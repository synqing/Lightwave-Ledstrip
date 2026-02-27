#pragma once
// Lightweight radix-2 Cooley-Tukey FFT — single header, no dependencies.
// Provides real FFT and magnitude extraction for ESP32-S3 (single-precision FPU).
// N must be a power of 2.

#include <cmath>
#include <cstddef>

namespace fft {

// Bit-reversal permutation (in-place)
inline void bitReverse(float* buf, size_t N) {
  size_t j = 0;
  for (size_t i = 1; i < N; ++i) {
    size_t bit = N >> 1;
    while (j & bit) {
      j ^= bit;
      bit >>= 1;
    }
    j ^= bit;
    if (i < j) {
      float tmp = buf[i];
      buf[i] = buf[j];
      buf[j] = tmp;
    }
  }
}

// In-place real FFT.  Input: N real values in buf[].
// Output: N floats as interleaved complex pairs:
//   buf[0] = DC real,  buf[1] = Nyquist real  (packed)
//   buf[2] = Re[1],    buf[3] = Im[1]
//   buf[4] = Re[2],    buf[5] = Im[2]  ...
// This matches CMSIS-DSP rfft packing convention.
inline void rfft(float* buf, size_t N) {
  // Step 1: N/2-point complex FFT on interleaved even/odd samples
  const size_t half = N / 2;
  constexpr float kPi = 3.14159265358979323846f;

  // Bit-reverse the N/2 complex pairs (stride-2 reversal)
  {
    size_t j = 0;
    for (size_t i = 1; i < half; ++i) {
      size_t bit = half >> 1;
      while (j & bit) { j ^= bit; bit >>= 1; }
      j ^= bit;
      if (i < j) {
        float tr = buf[2 * i];     float ti = buf[2 * i + 1];
        buf[2 * i]     = buf[2 * j];     buf[2 * i + 1] = buf[2 * j + 1];
        buf[2 * j]     = tr;             buf[2 * j + 1] = ti;
      }
    }
  }

  // Butterfly passes for N/2-point complex FFT
  for (size_t len = 2; len <= half; len <<= 1) {
    const float angle = -2.0f * kPi / static_cast<float>(len);
    const float wRe = cosf(angle);
    const float wIm = sinf(angle);
    for (size_t i = 0; i < half; i += len) {
      float curRe = 1.0f, curIm = 0.0f;
      for (size_t j = 0; j < len / 2; ++j) {
        size_t u = 2 * (i + j);
        size_t v = 2 * (i + j + len / 2);
        float tRe = curRe * buf[v] - curIm * buf[v + 1];
        float tIm = curRe * buf[v + 1] + curIm * buf[v];
        buf[v]     = buf[u] - tRe;
        buf[v + 1] = buf[u + 1] - tIm;
        buf[u]     += tRe;
        buf[u + 1] += tIm;
        float newRe = curRe * wRe - curIm * wIm;
        curIm = curRe * wIm + curIm * wRe;
        curRe = newRe;
      }
    }
  }

  // Step 2: Unpack N/2-point complex FFT into N-point real FFT
  // F[k] = 0.5*(Z[k] + Z*[N/2-k]) - 0.5j*W^k*(Z[k] - Z*[N/2-k])
  // where Z[] is the complex FFT result, W = exp(-2πi/N)
  float* z = buf;  // alias
  float dcRe = z[0] + z[1];
  float nyRe = z[0] - z[1];

  for (size_t k = 1; k <= half / 2; ++k) {
    size_t k2 = half - k;
    float zRe  = z[2 * k];
    float zIm  = z[2 * k + 1];
    float z2Re = z[2 * k2];
    float z2Im = z[2 * k2 + 1];

    // Even part: 0.5 * (Z[k] + conj(Z[N/2-k]))
    float eRe = 0.5f * (zRe + z2Re);
    float eIm = 0.5f * (zIm - z2Im);

    // Odd part: 0.5 * (Z[k] - conj(Z[N/2-k]))
    float oRe = 0.5f * (zRe - z2Re);
    float oIm = 0.5f * (zIm + z2Im);

    // Twiddle: W^k = exp(-2πik/N)
    float angle = -2.0f * kPi * static_cast<float>(k) / static_cast<float>(N);
    float twRe = cosf(angle);
    float twIm = sinf(angle);

    // -j * W^k * odd = -(twIm*oRe - twRe*oIm) + j*(twRe*oRe + twIm*oIm)  ... wait
    // -j * (twRe + j*twIm) * (oRe + j*oIm)
    // = -j * (twRe*oRe - twIm*oIm + j*(twRe*oIm + twIm*oRe))
    // = (twRe*oIm + twIm*oRe) - j*(twRe*oRe - twIm*oIm)
    float tRe =  twRe * oIm + twIm * oRe;
    float tIm = -(twRe * oRe - twIm * oIm);

    float fkRe = eRe + tRe;
    float fkIm = eIm + tIm;
    float fk2Re = eRe - tRe;   // F[N/2-k] = conj mirror
    float fk2Im = -(eIm - tIm);

    z[2 * k]      = fkRe;
    z[2 * k + 1]  = fkIm;
    z[2 * k2]     = fk2Re;
    z[2 * k2 + 1] = fk2Im;
  }

  // Pack DC and Nyquist into first two slots (CMSIS convention)
  z[0] = dcRe;
  z[1] = nyRe;
}

// Extract magnitudes from rfft output.
// Input: buf[N] from rfft().  Output: mag[N/2] magnitudes for bins 0..N/2-1.
inline void magnitudes(const float* buf, float* mag, size_t N) {
  const size_t half = N / 2;
  // DC bin (real only, packed in buf[0])
  mag[0] = fabsf(buf[0]);
  // Bins 1..N/2-1
  for (size_t k = 1; k < half; ++k) {
    float re = buf[2 * k];
    float im = buf[2 * k + 1];
    mag[k] = sqrtf(re * re + im * im);
  }
}

}  // namespace fft
