export const dotMatrixVertexShader = `
  precision highp float;

  varying vec2 vUv;

  void main() {
    vUv = uv;
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
  }
`;

export const dotMatrixFragmentShader = `
  precision highp float;

  uniform sampler2D uAudioTex;
  uniform float uOffset;
  uniform vec2 uGrid;
  uniform float uGain;

  varying vec2 vUv;

  void main() {
    // 1. Grid Logic - create LCD-style dot cells
    vec2 gridUV = vUv * uGrid;
    vec2 cellUV = fract(gridUV);
    float dist = length(cellUV - 0.5);

    // 2. Audio Sampling
    // vUv.y=1 is TOP (newest), vUv.y=0 is BOTTOM (oldest)
    // Invert Y so data flows DOWNWARD from shared edge
    float timeScroll = fract(uOffset + (1.0 - vUv.y));
    float audio = texture2D(uAudioTex, vec2(vUv.x, timeScroll)).r;

    // 3. Dot Styling - size depends on audio amplitude
    float radius = 0.45 * smoothstep(0.01, 0.4, audio * uGain);
    float dot = 1.0 - smoothstep(radius - 0.05, radius + 0.05, dist);

    // 4. Color & Opacity
    // Fade out at bottom (oldest data), bright at top (newest)
    float opacity = dot * (0.1 + 0.9 * vUv.y);
    vec3 color = mix(vec3(0.4, 0.6, 1.0), vec3(1.0), vUv.y * audio);

    gl_FragColor = vec4(color, opacity);
  }
`;
