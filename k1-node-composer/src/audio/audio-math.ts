/**
 * @file audio-math.ts
 * @brief Frame-rate-independent EMA alpha computation utilities.
 *
 * Ported from firmware-v3/src/audio/AudioMath.h.
 *
 * Converts between time constants (seconds) and per-frame EMA alphas,
 * enabling a single set of perceptual tuning values to work across
 * any hop rate (50 Hz, 100 Hz, 125 Hz, etc.).
 *
 * Usage:
 *   const alpha = computeEmaAlpha(0.2, 125);  // 200ms tau @ 125 Hz
 *   smoothed = smoothed + alpha * (raw - smoothed); // standard EMA
 */

/**
 * Compute EMA alpha from a time constant and frame rate.
 *
 * alpha = 1 - exp(-1 / (tau_seconds * hop_rate_hz))
 *
 * @param tauSeconds  Time constant in seconds (63% settling time)
 * @param hopRateHz   Frame/hop rate in Hz
 * @returns Per-frame alpha in [0, 1]
 */
export function computeEmaAlpha(tauSeconds: number, hopRateHz: number): number {
  if (tauSeconds <= 0 || hopRateHz <= 0) return 1.0;
  return 1.0 - Math.exp(-1.0 / (tauSeconds * hopRateHz));
}

/**
 * Reverse-engineer the time constant from an existing alpha and frame rate.
 *
 * tau = -1 / (hop_rate_hz * ln(1 - alpha))
 *
 * Useful for documenting what time constant a hardcoded alpha corresponds to.
 *
 * @param alpha       Per-frame EMA alpha in (0, 1)
 * @param hopRateHz   Frame/hop rate in Hz
 * @returns Time constant in seconds
 */
export function tauFromAlpha(alpha: number, hopRateHz: number): number {
  if (alpha <= 0 || alpha >= 1.0 || hopRateHz <= 0) return 0.0;
  return -1.0 / (hopRateHz * Math.log(1.0 - alpha));
}

/**
 * Compute EMA alpha from half-life in frames.
 *
 * alpha = 1 - exp(-ln(2) / half_life_frames)
 *
 * @param halfLifeFrames  Number of frames for signal to decay to 50%
 * @returns Per-frame alpha in [0, 1]
 */
export function alphaFromHalfLife(halfLifeFrames: number): number {
  if (halfLifeFrames <= 0) return 1.0;
  return 1.0 - Math.exp(-0.693147 / halfLifeFrames);
}

/**
 * Re-scale an alpha tuned at one frame rate to another.
 *
 * Preserves the perceptual time constant across frame rates.
 * This is the key function used throughout the adapter to retune
 * firmware alphas (tuned at 50 Hz) to the node-composer hop rate (125 Hz).
 *
 * @param alphaAtRef  Original alpha value
 * @param refHz       Frame rate the original alpha was tuned for
 * @param targetHz    Frame rate to retune to
 * @returns Retuned alpha for targetHz
 */
export function retunedAlpha(alphaAtRef: number, refHz: number, targetHz: number): number {
  const tau = tauFromAlpha(alphaAtRef, refHz);
  return computeEmaAlpha(tau, targetHz);
}
