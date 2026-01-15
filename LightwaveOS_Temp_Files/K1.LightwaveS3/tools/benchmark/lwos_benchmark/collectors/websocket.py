"""WebSocket collector for binary benchmark frames from ESP32."""

import asyncio
import json
import logging
from typing import AsyncIterator
from uuid import UUID

import websockets
from websockets.client import WebSocketClientProtocol
from websockets.exceptions import ConnectionClosed, WebSocketException

from ..parsers.binary import BenchmarkFrame
from ..storage.database import BenchmarkDatabase
from ..storage.models import BenchmarkRun, BenchmarkSample

logger = logging.getLogger(__name__)


class WebSocketCollector:
    """Collects binary benchmark frames over WebSocket from ESP32.

    Connects to LightwaveOS WebSocket endpoint, subscribes to benchmark
    stream, and stores parsed frames to database.
    """

    def __init__(
        self,
        host: str,
        port: int = 80,
        database: BenchmarkDatabase | None = None,
        timeout: float = 10.0,
    ) -> None:
        """Initialize WebSocket collector.

        Args:
            host: ESP32 hostname or IP address
            port: WebSocket port (default 80)
            database: Optional BenchmarkDatabase instance
            timeout: Connection timeout in seconds
        """
        self.host = host
        self.port = port
        self.database = database
        self.timeout = timeout
        self.ws_url = f"ws://{host}:{port}/ws"

        self._ws: WebSocketClientProtocol | None = None
        self._running = False
        self._frame_count = 0

    async def connect(self) -> None:
        """Establish WebSocket connection to ESP32.

        Raises:
            WebSocketException: If connection fails
        """
        logger.info(f"Connecting to {self.ws_url}")
        try:
            self._ws = await asyncio.wait_for(
                websockets.connect(self.ws_url),
                timeout=self.timeout,
            )
            logger.info("WebSocket connected")
        except asyncio.TimeoutError as e:
            msg = f"Connection timeout after {self.timeout}s"
            raise WebSocketException(msg) from e

    async def subscribe(self) -> None:
        """Subscribe to benchmark data stream.

        Sends subscription message to ESP32:
        {"type": "benchmark.subscribe"}

        Raises:
            RuntimeError: If not connected
        """
        if not self._ws:
            msg = "Not connected - call connect() first"
            raise RuntimeError(msg)

        subscription = {"type": "benchmark.subscribe"}
        await self._ws.send(json.dumps(subscription))
        logger.info("Subscribed to benchmark stream")

    async def unsubscribe(self) -> None:
        """Unsubscribe from benchmark data stream.

        Sends unsubscription message:
        {"type": "benchmark.unsubscribe"}
        """
        if not self._ws:
            return

        try:
            unsubscription = {"type": "benchmark.unsubscribe"}
            await self._ws.send(json.dumps(unsubscription))
            logger.info("Unsubscribed from benchmark stream")
        except Exception as e:
            logger.warning(f"Error unsubscribing: {e}")

    async def collect(
        self,
        run: BenchmarkRun,
        duration: float | None = None,
        max_samples: int | None = None,
    ) -> int:
        """Collect benchmark data for a run.

        Args:
            run: BenchmarkRun instance to associate samples with
            duration: Collection duration in seconds (None for indefinite)
            max_samples: Maximum samples to collect (None for unlimited)

        Returns:
            Number of samples collected

        Raises:
            RuntimeError: If not connected or database not configured
        """
        if not self._ws:
            msg = "Not connected - call connect() first"
            raise RuntimeError(msg)

        if not self.database:
            msg = "Database not configured"
            raise RuntimeError(msg)

        self._running = True
        self._frame_count = 0
        start_time = asyncio.get_event_loop().time()

        logger.info(
            f"Starting collection for run '{run.name}' "
            f"(duration={duration}s, max_samples={max_samples})"
        )

        await self.subscribe()

        try:
            async for frame in self._receive_frames():
                # Store sample
                sample = BenchmarkSample(
                    run_id=run.id,
                    timestamp_ms=frame.timestamp_ms,
                    avg_total_us=frame.avg_total_us,
                    avg_goertzel_us=frame.avg_goertzel_us,
                    peak_total_us=frame.peak_total_us,
                    peak_goertzel_us=frame.peak_goertzel_us,
                    cpu_load_percent=frame.cpu_load_percent,
                    hop_count=frame.hop_count,
                    goertzel_count=frame.goertzel_count,
                    flags=frame.flags,
                )

                self.database.add_sample(sample)
                self._frame_count += 1

                if self._frame_count % 10 == 0:
                    logger.debug(f"Collected {self._frame_count} samples")

                # Check duration limit
                if duration is not None:
                    elapsed = asyncio.get_event_loop().time() - start_time
                    if elapsed >= duration:
                        logger.info(f"Duration limit reached: {elapsed:.1f}s")
                        break

                # Check sample limit
                if max_samples is not None and self._frame_count >= max_samples:
                    logger.info(f"Sample limit reached: {self._frame_count}")
                    break

                if not self._running:
                    logger.info("Collection stopped by user")
                    break

        finally:
            await self.unsubscribe()

        logger.info(f"Collection complete: {self._frame_count} samples")
        return self._frame_count

    async def _receive_frames(self) -> AsyncIterator[BenchmarkFrame]:
        """Receive and parse binary benchmark frames.

        Yields:
            Parsed BenchmarkFrame instances

        Raises:
            ConnectionClosed: If WebSocket connection closes
        """
        if not self._ws:
            msg = "Not connected"
            raise RuntimeError(msg)

        try:
            async for message in self._ws:
                # Binary messages are benchmark frames
                if isinstance(message, bytes):
                    try:
                        frame = BenchmarkFrame.from_bytes(message)
                        if frame.is_valid:
                            yield frame
                        else:
                            logger.warning(f"Invalid frame magic: 0x{frame.magic:08X}")
                    except ValueError as e:
                        logger.error(f"Frame parse error: {e}")
                        continue

                # Text messages are JSON (control/status)
                elif isinstance(message, str):
                    try:
                        data = json.loads(message)
                        logger.debug(f"Received JSON: {data}")
                    except json.JSONDecodeError as e:
                        logger.warning(f"JSON decode error: {e}")

        except ConnectionClosed:
            logger.info("WebSocket connection closed")
            raise

    def stop(self) -> None:
        """Stop collection loop."""
        self._running = False

    async def disconnect(self) -> None:
        """Close WebSocket connection."""
        if self._ws:
            await self._ws.close()
            self._ws = None
            logger.info("WebSocket disconnected")

    @property
    def is_connected(self) -> bool:
        """Check if WebSocket is connected."""
        return self._ws is not None and not self._ws.closed

    @property
    def frame_count(self) -> int:
        """Get number of frames collected in current session."""
        return self._frame_count
