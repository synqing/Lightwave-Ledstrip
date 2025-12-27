"""
WebSocket collector for LightwaveOS validation frames.

Connects to the device's WebSocket endpoint, subscribes to binary validation frames,
and stores samples to SQLite database.
"""

import asyncio
import logging
from datetime import datetime
from pathlib import Path
from typing import Optional

import websockets
from rich.console import Console
from rich.progress import Progress, SpinnerColumn, TextColumn, BarColumn, TaskProgressColumn
from sqlite_utils import Database

from lwos_test_audio.models import EffectValidationSample, ValidationSession

logger = logging.getLogger(__name__)


class ValidationCollector:
    """
    Collects binary validation frames from LightwaveOS via WebSocket.

    Subscribes to validation frames, parses the binary data, and stores
    samples to a SQLite database for later analysis.
    """

    def __init__(
        self,
        host: str,
        db_path: Path,
        console: Optional[Console] = None,
    ):
        """
        Initialize the validation collector.

        Args:
            host: Device hostname or IP (e.g., 'lightwaveos.local')
            db_path: Path to SQLite database file
            console: Rich console for output (optional)
        """
        self.host = host
        self.db_path = db_path
        self.console = console or Console()
        self.db: Optional[Database] = None
        self.session: Optional[ValidationSession] = None
        self.websocket: Optional[websockets.WebSocketClientProtocol] = None
        self.running = False
        self.sample_count = 0

    def _init_database(self) -> None:
        """Initialize SQLite database with schema."""
        self.db = Database(self.db_path)

        # Create sessions table
        if "sessions" not in self.db.table_names():
            self.db["sessions"].create(
                {
                    "session_id": int,
                    "device_host": str,
                    "start_time": str,
                    "end_time": str,
                    "effect_name": str,
                    "effect_id": int,
                    "sample_count": int,
                    "notes": str,
                },
                pk="session_id",
            )

        # Create samples table
        if "samples" not in self.db.table_names():
            self.db["samples"].create(
                {
                    "id": int,
                    "session_id": int,
                    "timestamp": int,
                    "effect_id": int,
                    "effect_type": str,
                    "captured_at": str,
                    # Audio metrics
                    "bass_energy": float,
                    "mid_energy": float,
                    "treble_energy": float,
                    "total_energy": float,
                    "peak_frequency": float,
                    "spectral_centroid": float,
                    "spectral_spread": float,
                    "zero_crossing_rate": float,
                    # Performance metrics
                    "frame_time_us": int,
                    "render_time_us": int,
                    "fps": float,
                    "heap_free": int,
                    "heap_fragmentation": float,
                    "cpu_usage": float,
                    # Effect data
                    "effect_data": bytes,
                },
                pk="id",
            )
            self.db["samples"].create_index(["session_id"])
            self.db["samples"].create_index(["effect_id"])
            self.db["samples"].create_index(["timestamp"])

    def _create_session(self, effect_name: Optional[str] = None) -> ValidationSession:
        """Create a new validation session."""
        assert self.db is not None

        # Get next session ID
        sessions_table = self.db["sessions"]
        last_session = list(sessions_table.rows_where(order_by="-session_id", limit=1))
        session_id = (last_session[0]["session_id"] + 1) if last_session else 1

        session = ValidationSession(
            session_id=session_id,
            device_host=self.host,
            start_time=datetime.now(),
            effect_name=effect_name,
        )

        # Insert session record
        sessions_table.insert(
            {
                "session_id": session.session_id,
                "device_host": session.device_host,
                "start_time": session.start_time.isoformat(),
                "end_time": None,
                "effect_name": effect_name,
                "effect_id": None,
                "sample_count": 0,
                "notes": "",
            }
        )

        return session

    def _store_sample(self, sample: EffectValidationSample) -> None:
        """Store a validation sample to the database."""
        assert self.db is not None
        assert self.session is not None

        sample.session_id = self.session.session_id

        # Flatten sample data for SQLite
        row = {
            "session_id": sample.session_id,
            "timestamp": sample.timestamp,
            "effect_id": sample.effect_id,
            "effect_type": sample.effect_type.name,
            "captured_at": sample.captured_at.isoformat(),
            # Audio metrics
            "bass_energy": sample.audio_metrics.bass_energy,
            "mid_energy": sample.audio_metrics.mid_energy,
            "treble_energy": sample.audio_metrics.treble_energy,
            "total_energy": sample.audio_metrics.total_energy,
            "peak_frequency": sample.audio_metrics.peak_frequency,
            "spectral_centroid": sample.audio_metrics.spectral_centroid,
            "spectral_spread": sample.audio_metrics.spectral_spread,
            "zero_crossing_rate": sample.audio_metrics.zero_crossing_rate,
            # Performance metrics
            "frame_time_us": sample.performance_metrics.frame_time_us,
            "render_time_us": sample.performance_metrics.render_time_us,
            "fps": sample.performance_metrics.fps,
            "heap_free": sample.performance_metrics.heap_free,
            "heap_fragmentation": sample.performance_metrics.heap_fragmentation,
            "cpu_usage": sample.performance_metrics.cpu_usage,
            # Effect data
            "effect_data": sample.effect_data,
        }

        self.db["samples"].insert(row)
        self.sample_count += 1

        # Update session effect_id if not set
        if self.session.effect_id is None:
            self.session.effect_id = sample.effect_id
            self.db["sessions"].update(
                self.session.session_id, {"effect_id": sample.effect_id}
            )

    def _finalize_session(self) -> None:
        """Finalize the current session."""
        assert self.db is not None
        assert self.session is not None

        self.session.end_time = datetime.now()
        self.session.sample_count = self.sample_count

        self.db["sessions"].update(
            self.session.session_id,
            {
                "end_time": self.session.end_time.isoformat(),
                "sample_count": self.session.sample_count,
            },
        )

    async def _subscribe_validation_frames(self) -> None:
        """Send WebSocket command to subscribe to validation frames."""
        assert self.websocket is not None

        subscribe_msg = '{"command": "validation.subscribe"}'
        await self.websocket.send(subscribe_msg)
        logger.info("Subscribed to validation frames")

    async def collect(
        self,
        duration: Optional[int] = None,
        max_samples: Optional[int] = None,
        effect_name: Optional[str] = None,
    ) -> None:
        """
        Collect validation frames from the device.

        Args:
            duration: Maximum collection duration in seconds (None = unlimited)
            max_samples: Maximum number of samples to collect (None = unlimited)
            effect_name: Name of the effect being validated (optional)
        """
        self._init_database()
        self.session = self._create_session(effect_name)
        self.running = True
        self.sample_count = 0

        ws_url = f"ws://{self.host}/ws"
        self.console.print(f"[cyan]Connecting to {ws_url}...[/cyan]")

        start_time = asyncio.get_event_loop().time()

        try:
            async with websockets.connect(ws_url) as websocket:
                self.websocket = websocket
                await self._subscribe_validation_frames()

                self.console.print(
                    f"[green]Connected! Session ID: {self.session.session_id}[/green]"
                )
                self.console.print("[yellow]Collecting frames... Press Ctrl+C to stop[/yellow]")

                with Progress(
                    SpinnerColumn(),
                    TextColumn("[progress.description]{task.description}"),
                    BarColumn(),
                    TaskProgressColumn(),
                    console=self.console,
                ) as progress:
                    task = progress.add_task(
                        "Collecting samples...",
                        total=max_samples if max_samples else 100,
                    )

                    while self.running:
                        # Check duration limit
                        if duration and (asyncio.get_event_loop().time() - start_time) >= duration:
                            self.console.print("[yellow]Duration limit reached[/yellow]")
                            break

                        # Check sample limit
                        if max_samples and self.sample_count >= max_samples:
                            self.console.print("[yellow]Sample limit reached[/yellow]")
                            break

                        try:
                            # Receive binary frame (timeout after 5 seconds)
                            message = await asyncio.wait_for(websocket.recv(), timeout=5.0)

                            if isinstance(message, bytes):
                                # Parse and store sample
                                sample = EffectValidationSample.from_binary_frame(message)
                                self._store_sample(sample)

                                # Update progress
                                progress.update(
                                    task,
                                    completed=self.sample_count,
                                    description=f"Samples: {self.sample_count} | FPS: {sample.performance_metrics.fps:.1f}",
                                )

                        except asyncio.TimeoutError:
                            # No frame received, continue waiting
                            continue

        except websockets.exceptions.WebSocketException as e:
            self.console.print(f"[red]WebSocket error: {e}[/red]")
            logger.error(f"WebSocket error: {e}")
        except KeyboardInterrupt:
            self.console.print("\n[yellow]Collection stopped by user[/yellow]")
        finally:
            self.running = False
            self._finalize_session()
            self.console.print(
                f"\n[green]✓ Collected {self.sample_count} samples in session {self.session.session_id}[/green]"
            )
            self.console.print(f"[cyan]Database: {self.db_path}[/cyan]")
