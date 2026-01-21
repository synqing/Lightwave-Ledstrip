"""Base pipeline interface for LED controller simulation."""

from abc import ABC, abstractmethod
import numpy as np
from typing import Dict, Any


class BasePipeline(ABC):
    """Base class for LED controller pipeline simulation."""
    
    def __init__(self, num_leds: int, config: Dict[str, Any]):
        """
        Initialize pipeline.
        
        Args:
            num_leds: Number of LEDs in the strip
            config: Pipeline-specific configuration
        """
        self.num_leds = num_leds
        self.config = config
    
    @abstractmethod
    def process_frame(self, input_float: np.ndarray) -> np.ndarray:
        """
        Process a single frame through the complete pipeline.
        
        Args:
            input_float: Input LED buffer, shape (num_leds, 3), float32, range [0..1]
        
        Returns:
            Output LED buffer, shape (num_leds, 3), uint8, range [0..255]
        """
        pass
    
    @abstractmethod
    def reset(self):
        """Reset pipeline state (for stateful quantisers)."""
        pass
    
    @abstractmethod
    def get_name(self) -> str:
        """Get pipeline name for reporting."""
        pass
    
    @abstractmethod
    def get_description(self) -> str:
        """Get detailed pipeline description."""
        pass
    
    def get_config(self) -> Dict[str, Any]:
        """Get current configuration."""
        return self.config.copy()
