"""Pydantic models for benchmark data storage."""

from datetime import datetime
from typing import Any
from uuid import UUID, uuid4

from pydantic import BaseModel, Field


class BenchmarkSample(BaseModel):
    """Single benchmark sample data point.

    Represents one parsed benchmark frame with metadata.
    """

    id: UUID = Field(default_factory=uuid4)
    run_id: UUID = Field(description="Associated benchmark run ID")
    timestamp_utc: datetime = Field(default_factory=datetime.utcnow)
    timestamp_ms: int = Field(description="Device timestamp (ms since boot)")

    # Timing metrics
    avg_total_us: float = Field(description="Average total processing time (µs)")
    avg_goertzel_us: float = Field(description="Average Goertzel time (µs)")
    peak_total_us: int = Field(description="Peak total processing time (µs)")
    peak_goertzel_us: int = Field(description="Peak Goertzel time (µs)")

    # System metrics
    cpu_load_percent: float = Field(description="CPU load percentage")

    # Configuration
    hop_count: int = Field(description="FFT hop count")
    goertzel_count: int = Field(description="Number of Goertzel filters")

    # Status
    flags: int = Field(description="Status flags bitfield")

    class Config:
        """Pydantic configuration."""
        json_encoders = {
            datetime: lambda v: v.isoformat(),
            UUID: lambda v: str(v),
        }

    def to_storage_dict(self) -> dict[str, Any]:
        """Convert to dictionary suitable for database storage.

        Returns:
            Dictionary with UUID and datetime as strings
        """
        return {
            "id": str(self.id),
            "run_id": str(self.run_id),
            "timestamp_utc": self.timestamp_utc.isoformat(),
            "timestamp_ms": self.timestamp_ms,
            "avg_total_us": self.avg_total_us,
            "avg_goertzel_us": self.avg_goertzel_us,
            "peak_total_us": self.peak_total_us,
            "peak_goertzel_us": self.peak_goertzel_us,
            "cpu_load_percent": self.cpu_load_percent,
            "hop_count": self.hop_count,
            "goertzel_count": self.goertzel_count,
            "flags": self.flags,
        }


class BenchmarkRun(BaseModel):
    """Benchmark run metadata and aggregated statistics.

    Represents a single benchmark collection session.
    """

    id: UUID = Field(default_factory=uuid4)
    name: str = Field(description="Human-readable run name")
    description: str = Field(default="", description="Optional run description")

    # Timing
    started_at: datetime = Field(default_factory=datetime.utcnow)
    completed_at: datetime | None = Field(default=None)
    duration_seconds: float | None = Field(default=None)

    # Collection metadata
    host: str = Field(description="ESP32 host address")
    sample_count: int = Field(default=0, description="Number of samples collected")

    # Device info
    firmware_version: str | None = Field(default=None)
    device_uptime_ms: int | None = Field(default=None)

    # Aggregated statistics (computed after collection)
    avg_total_us_mean: float | None = Field(default=None)
    avg_total_us_p50: float | None = Field(default=None)
    avg_total_us_p95: float | None = Field(default=None)
    avg_total_us_p99: float | None = Field(default=None)

    cpu_load_mean: float | None = Field(default=None)
    cpu_load_p95: float | None = Field(default=None)

    # Configuration
    hop_count: int | None = Field(default=None)
    goertzel_count: int | None = Field(default=None)

    # Tags for organization
    tags: list[str] = Field(default_factory=list)

    class Config:
        """Pydantic configuration."""
        json_encoders = {
            datetime: lambda v: v.isoformat(),
            UUID: lambda v: str(v),
        }

    @property
    def is_complete(self) -> bool:
        """Check if run is marked as complete."""
        return self.completed_at is not None

    def mark_complete(self) -> None:
        """Mark run as complete and compute duration."""
        self.completed_at = datetime.utcnow()
        if self.completed_at and self.started_at:
            self.duration_seconds = (self.completed_at - self.started_at).total_seconds()

    def to_storage_dict(self) -> dict[str, Any]:
        """Convert to dictionary suitable for database storage.

        Returns:
            Dictionary with UUID and datetime as strings
        """
        return {
            "id": str(self.id),
            "name": self.name,
            "description": self.description,
            "started_at": self.started_at.isoformat(),
            "completed_at": self.completed_at.isoformat() if self.completed_at else None,
            "duration_seconds": self.duration_seconds,
            "host": self.host,
            "sample_count": self.sample_count,
            "firmware_version": self.firmware_version,
            "device_uptime_ms": self.device_uptime_ms,
            "avg_total_us_mean": self.avg_total_us_mean,
            "avg_total_us_p50": self.avg_total_us_p50,
            "avg_total_us_p95": self.avg_total_us_p95,
            "avg_total_us_p99": self.avg_total_us_p99,
            "cpu_load_mean": self.cpu_load_mean,
            "cpu_load_p95": self.cpu_load_p95,
            "hop_count": self.hop_count,
            "goertzel_count": self.goertzel_count,
            "tags": ",".join(self.tags) if self.tags else "",
        }


class ComparisonResult(BaseModel):
    """Statistical comparison between two benchmark runs."""

    run_a_id: UUID
    run_a_name: str
    run_b_id: UUID
    run_b_name: str

    # Sample sizes
    n_samples_a: int
    n_samples_b: int

    # Mean differences
    avg_total_us_diff: float = Field(description="Mean difference in avg_total_us (B - A)")
    cpu_load_diff: float = Field(description="Mean difference in CPU load (B - A)")

    # Statistical significance (t-test)
    avg_total_us_pvalue: float = Field(description="p-value for avg_total_us t-test")
    cpu_load_pvalue: float = Field(description="p-value for CPU load t-test")

    # Effect size (Cohen's d)
    avg_total_us_cohens_d: float = Field(description="Cohen's d for avg_total_us")
    cpu_load_cohens_d: float = Field(description="Cohen's d for CPU load")

    # Significance thresholds
    alpha: float = Field(default=0.05, description="Significance level")

    @property
    def avg_total_us_significant(self) -> bool:
        """Check if avg_total_us difference is statistically significant."""
        return self.avg_total_us_pvalue < self.alpha

    @property
    def cpu_load_significant(self) -> bool:
        """Check if CPU load difference is statistically significant."""
        return self.cpu_load_pvalue < self.alpha

    @property
    def avg_total_us_effect_size(self) -> str:
        """Interpret Cohen's d effect size for avg_total_us."""
        d = abs(self.avg_total_us_cohens_d)
        if d < 0.2:
            return "negligible"
        if d < 0.5:
            return "small"
        if d < 0.8:
            return "medium"
        return "large"

    @property
    def cpu_load_effect_size(self) -> str:
        """Interpret Cohen's d effect size for CPU load."""
        d = abs(self.cpu_load_cohens_d)
        if d < 0.2:
            return "negligible"
        if d < 0.5:
            return "small"
        if d < 0.8:
            return "medium"
        return "large"

    class Config:
        """Pydantic configuration."""
        json_encoders = {
            UUID: lambda v: str(v),
        }
