"""Accuracy measurement tools.

Quantifies how well quantised output matches high-precision target.
"""

import numpy as np


def measure_accuracy(frames: np.ndarray, reference: np.ndarray) -> dict:
    """
    Measure accuracy of quantised output vs high-precision reference.
    
    Args:
        frames: Quantised output, shape (num_frames, num_leds, 3), uint8
        reference: High-precision reference, shape (num_leds, 3), float32, [0..1]
    
    Returns:
        dict with metrics:
        - mae: Mean Absolute Error
        - rmse: Root Mean Squared Error
        - max_error: Maximum single-pixel error
        - time_averaged_mae: MAE of time-averaged output vs reference
    """
    if frames.ndim == 2:
        # Single frame
        frames = frames[np.newaxis, ...]
    
    num_frames, num_leds, num_channels = frames.shape
    
    # Convert reference to uint8 scale for comparison
    reference_uint8 = (reference * 255.0).astype(np.float32)
    
    # Per-frame errors
    frames_float = frames.astype(np.float32)
    
    # Broadcast reference to match frames
    reference_broadcast = np.broadcast_to(reference_uint8[np.newaxis, :, :], 
                                         frames_float.shape)
    
    # Error metrics
    errors = frames_float - reference_broadcast
    mae = np.mean(np.abs(errors))
    rmse = np.sqrt(np.mean(errors ** 2))
    max_error = np.max(np.abs(errors))
    
    # Time-averaged accuracy
    time_averaged = np.mean(frames_float, axis=0)  # Average across frames
    time_averaged_errors = time_averaged - reference_uint8
    time_averaged_mae = np.mean(np.abs(time_averaged_errors))
    
    # Accuracy score: low MAE = high accuracy
    # Normalize to [0..1] where 1 = perfect accuracy
    accuracy_score = 1.0 / (1.0 + mae / 5.0)
    
    return {
        'mae': float(mae),
        'rmse': float(rmse),
        'max_error': float(max_error),
        'time_averaged_mae': float(time_averaged_mae),
        'accuracy_score': float(np.clip(accuracy_score, 0.0, 1.0)),
    }
