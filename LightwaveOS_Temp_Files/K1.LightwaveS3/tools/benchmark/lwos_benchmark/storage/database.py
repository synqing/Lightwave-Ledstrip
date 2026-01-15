"""SQLite database layer for benchmark data persistence."""

import sqlite3
from datetime import datetime
from pathlib import Path
from typing import Any
from uuid import UUID

from .models import BenchmarkRun, BenchmarkSample


class BenchmarkDatabase:
    """SQLite database for benchmark run and sample storage.

    Provides persistent storage with schema versioning and migration support.
    """

    SCHEMA_VERSION = 1

    def __init__(self, db_path: Path | str) -> None:
        """Initialize database connection.

        Args:
            db_path: Path to SQLite database file
        """
        self.db_path = Path(db_path)
        self.db_path.parent.mkdir(parents=True, exist_ok=True)
        self.conn = sqlite3.connect(str(self.db_path))
        self.conn.row_factory = sqlite3.Row
        self._initialize_schema()

    def _initialize_schema(self) -> None:
        """Create database schema if not exists."""
        cursor = self.conn.cursor()

        # Schema version tracking
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS schema_version (
                version INTEGER PRIMARY KEY,
                applied_at TEXT NOT NULL
            )
        """)

        # Check current version
        cursor.execute("SELECT MAX(version) FROM schema_version")
        current_version = cursor.fetchone()[0] or 0

        if current_version < self.SCHEMA_VERSION:
            self._migrate_schema(current_version)

    def _migrate_schema(self, from_version: int) -> None:
        """Apply schema migrations.

        Args:
            from_version: Current schema version
        """
        cursor = self.conn.cursor()

        if from_version < 1:
            # Initial schema
            cursor.execute("""
                CREATE TABLE benchmark_runs (
                    id TEXT PRIMARY KEY,
                    name TEXT NOT NULL,
                    description TEXT,
                    started_at TEXT NOT NULL,
                    completed_at TEXT,
                    duration_seconds REAL,
                    host TEXT NOT NULL,
                    sample_count INTEGER DEFAULT 0,
                    firmware_version TEXT,
                    device_uptime_ms INTEGER,
                    avg_total_us_mean REAL,
                    avg_total_us_p50 REAL,
                    avg_total_us_p95 REAL,
                    avg_total_us_p99 REAL,
                    cpu_load_mean REAL,
                    cpu_load_p95 REAL,
                    hop_count INTEGER,
                    goertzel_count INTEGER,
                    tags TEXT
                )
            """)

            cursor.execute("""
                CREATE TABLE benchmark_samples (
                    id TEXT PRIMARY KEY,
                    run_id TEXT NOT NULL,
                    timestamp_utc TEXT NOT NULL,
                    timestamp_ms INTEGER NOT NULL,
                    avg_total_us REAL NOT NULL,
                    avg_goertzel_us REAL NOT NULL,
                    peak_total_us INTEGER NOT NULL,
                    peak_goertzel_us INTEGER NOT NULL,
                    cpu_load_percent REAL NOT NULL,
                    hop_count INTEGER NOT NULL,
                    goertzel_count INTEGER NOT NULL,
                    flags INTEGER NOT NULL,
                    FOREIGN KEY (run_id) REFERENCES benchmark_runs(id)
                )
            """)

            # Indexes for common queries
            cursor.execute("""
                CREATE INDEX idx_samples_run_id ON benchmark_samples(run_id)
            """)
            cursor.execute("""
                CREATE INDEX idx_samples_timestamp ON benchmark_samples(timestamp_utc)
            """)
            cursor.execute("""
                CREATE INDEX idx_runs_started_at ON benchmark_runs(started_at)
            """)

            cursor.execute(
                "INSERT INTO schema_version (version, applied_at) VALUES (?, ?)",
                (1, datetime.utcnow().isoformat()),
            )

        self.conn.commit()

    def create_run(self, run: BenchmarkRun) -> UUID:
        """Create a new benchmark run.

        Args:
            run: BenchmarkRun instance

        Returns:
            UUID of created run
        """
        cursor = self.conn.cursor()
        data = run.to_storage_dict()

        cursor.execute("""
            INSERT INTO benchmark_runs (
                id, name, description, started_at, completed_at, duration_seconds,
                host, sample_count, firmware_version, device_uptime_ms,
                avg_total_us_mean, avg_total_us_p50, avg_total_us_p95, avg_total_us_p99,
                cpu_load_mean, cpu_load_p95, hop_count, goertzel_count, tags
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            data["id"], data["name"], data["description"], data["started_at"],
            data["completed_at"], data["duration_seconds"], data["host"],
            data["sample_count"], data["firmware_version"], data["device_uptime_ms"],
            data["avg_total_us_mean"], data["avg_total_us_p50"],
            data["avg_total_us_p95"], data["avg_total_us_p99"],
            data["cpu_load_mean"], data["cpu_load_p95"],
            data["hop_count"], data["goertzel_count"], data["tags"],
        ))

        self.conn.commit()
        return run.id

    def update_run(self, run: BenchmarkRun) -> None:
        """Update an existing benchmark run.

        Args:
            run: BenchmarkRun instance with updated values
        """
        cursor = self.conn.cursor()
        data = run.to_storage_dict()

        cursor.execute("""
            UPDATE benchmark_runs SET
                name = ?, description = ?, completed_at = ?, duration_seconds = ?,
                sample_count = ?, firmware_version = ?, device_uptime_ms = ?,
                avg_total_us_mean = ?, avg_total_us_p50 = ?,
                avg_total_us_p95 = ?, avg_total_us_p99 = ?,
                cpu_load_mean = ?, cpu_load_p95 = ?,
                hop_count = ?, goertzel_count = ?, tags = ?
            WHERE id = ?
        """, (
            data["name"], data["description"], data["completed_at"],
            data["duration_seconds"], data["sample_count"], data["firmware_version"],
            data["device_uptime_ms"], data["avg_total_us_mean"],
            data["avg_total_us_p50"], data["avg_total_us_p95"],
            data["avg_total_us_p99"], data["cpu_load_mean"], data["cpu_load_p95"],
            data["hop_count"], data["goertzel_count"], data["tags"], data["id"],
        ))

        self.conn.commit()

    def get_run(self, run_id: UUID | str) -> BenchmarkRun | None:
        """Retrieve a benchmark run by ID.

        Args:
            run_id: Run UUID or string representation

        Returns:
            BenchmarkRun instance or None if not found
        """
        cursor = self.conn.cursor()
        cursor.execute("SELECT * FROM benchmark_runs WHERE id = ?", (str(run_id),))
        row = cursor.fetchone()

        if not row:
            return None

        return self._row_to_run(dict(row))

    def list_runs(
        self,
        limit: int = 100,
        offset: int = 0,
        tags: list[str] | None = None,
    ) -> list[BenchmarkRun]:
        """List benchmark runs with optional filtering.

        Args:
            limit: Maximum number of runs to return
            offset: Number of runs to skip
            tags: Filter by tags (OR logic)

        Returns:
            List of BenchmarkRun instances
        """
        cursor = self.conn.cursor()

        if tags:
            # Build WHERE clause for tag filtering
            tag_conditions = " OR ".join(["tags LIKE ?" for _ in tags])
            query = f"""
                SELECT * FROM benchmark_runs
                WHERE {tag_conditions}
                ORDER BY started_at DESC
                LIMIT ? OFFSET ?
            """
            params = [f"%{tag}%" for tag in tags] + [limit, offset]
        else:
            query = """
                SELECT * FROM benchmark_runs
                ORDER BY started_at DESC
                LIMIT ? OFFSET ?
            """
            params = [limit, offset]

        cursor.execute(query, params)
        rows = cursor.fetchall()

        return [self._row_to_run(dict(row)) for row in rows]

    def add_sample(self, sample: BenchmarkSample) -> UUID:
        """Add a benchmark sample to a run.

        Args:
            sample: BenchmarkSample instance

        Returns:
            UUID of created sample
        """
        cursor = self.conn.cursor()
        data = sample.to_storage_dict()

        cursor.execute("""
            INSERT INTO benchmark_samples (
                id, run_id, timestamp_utc, timestamp_ms,
                avg_total_us, avg_goertzel_us, peak_total_us, peak_goertzel_us,
                cpu_load_percent, hop_count, goertzel_count, flags
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            data["id"], data["run_id"], data["timestamp_utc"], data["timestamp_ms"],
            data["avg_total_us"], data["avg_goertzel_us"],
            data["peak_total_us"], data["peak_goertzel_us"],
            data["cpu_load_percent"], data["hop_count"],
            data["goertzel_count"], data["flags"],
        ))

        # Update run sample count
        cursor.execute("""
            UPDATE benchmark_runs
            SET sample_count = sample_count + 1
            WHERE id = ?
        """, (str(sample.run_id),))

        self.conn.commit()
        return sample.id

    def get_samples(
        self,
        run_id: UUID | str,
        limit: int | None = None,
        offset: int = 0,
    ) -> list[BenchmarkSample]:
        """Retrieve samples for a benchmark run.

        Args:
            run_id: Run UUID or string representation
            limit: Maximum number of samples (None for all)
            offset: Number of samples to skip

        Returns:
            List of BenchmarkSample instances
        """
        cursor = self.conn.cursor()

        if limit is not None:
            query = """
                SELECT * FROM benchmark_samples
                WHERE run_id = ?
                ORDER BY timestamp_utc
                LIMIT ? OFFSET ?
            """
            cursor.execute(query, (str(run_id), limit, offset))
        else:
            query = """
                SELECT * FROM benchmark_samples
                WHERE run_id = ?
                ORDER BY timestamp_utc
            """
            cursor.execute(query, (str(run_id),))

        rows = cursor.fetchall()
        return [self._row_to_sample(dict(row)) for row in rows]

    def delete_run(self, run_id: UUID | str) -> bool:
        """Delete a benchmark run and all associated samples.

        Args:
            run_id: Run UUID or string representation

        Returns:
            True if run was deleted, False if not found
        """
        cursor = self.conn.cursor()

        # Delete samples first (foreign key constraint)
        cursor.execute("DELETE FROM benchmark_samples WHERE run_id = ?", (str(run_id),))

        # Delete run
        cursor.execute("DELETE FROM benchmark_runs WHERE id = ?", (str(run_id),))

        deleted = cursor.rowcount > 0
        self.conn.commit()
        return deleted

    def close(self) -> None:
        """Close database connection."""
        self.conn.close()

    def _row_to_run(self, row: dict[str, Any]) -> BenchmarkRun:
        """Convert database row to BenchmarkRun instance.

        Args:
            row: SQLite row as dictionary

        Returns:
            BenchmarkRun instance
        """
        return BenchmarkRun(
            id=UUID(row["id"]),
            name=row["name"],
            description=row["description"] or "",
            started_at=datetime.fromisoformat(row["started_at"]),
            completed_at=datetime.fromisoformat(row["completed_at"]) if row["completed_at"] else None,
            duration_seconds=row["duration_seconds"],
            host=row["host"],
            sample_count=row["sample_count"],
            firmware_version=row["firmware_version"],
            device_uptime_ms=row["device_uptime_ms"],
            avg_total_us_mean=row["avg_total_us_mean"],
            avg_total_us_p50=row["avg_total_us_p50"],
            avg_total_us_p95=row["avg_total_us_p95"],
            avg_total_us_p99=row["avg_total_us_p99"],
            cpu_load_mean=row["cpu_load_mean"],
            cpu_load_p95=row["cpu_load_p95"],
            hop_count=row["hop_count"],
            goertzel_count=row["goertzel_count"],
            tags=row["tags"].split(",") if row["tags"] else [],
        )

    def _row_to_sample(self, row: dict[str, Any]) -> BenchmarkSample:
        """Convert database row to BenchmarkSample instance.

        Args:
            row: SQLite row as dictionary

        Returns:
            BenchmarkSample instance
        """
        return BenchmarkSample(
            id=UUID(row["id"]),
            run_id=UUID(row["run_id"]),
            timestamp_utc=datetime.fromisoformat(row["timestamp_utc"]),
            timestamp_ms=row["timestamp_ms"],
            avg_total_us=row["avg_total_us"],
            avg_goertzel_us=row["avg_goertzel_us"],
            peak_total_us=row["peak_total_us"],
            peak_goertzel_us=row["peak_goertzel_us"],
            cpu_load_percent=row["cpu_load_percent"],
            hop_count=row["hop_count"],
            goertzel_count=row["goertzel_count"],
            flags=row["flags"],
        )
