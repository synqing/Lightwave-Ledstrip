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
  uniform float uDotRadius;

  varying vec2 vUv;

  float dotMask(vec2 uv, vec2 grid, float radius) {
    vec2 cell = fract(uv * grid) - 0.5;
    float d = length(cell);
    float aa = fwidth(d);
    return 1.0 - smoothstep(radius - aa, radius + aa, d);
  }

  void main() {
    // Same UV offset logic as ridge plot
    float yLookup = fract(uOffset - vUv.y);
    float s = texture2D(uAudioTex, vec2(vUv.x, yLookup)).r;
    float shaped = pow(s, 1.15);

    // Background dot lattice
    float bgDots = dotMask(vUv, uGrid, uDotRadius) * 0.12;

    // Foreground dots driven by audio
    float fgDots = dotMask(vUv, uGrid, uDotRadius) * (shaped * uGain);

    // Age fade
    float age = clamp(vUv.y, 0.0, 1.0);
    float fade = pow(1.0 - age, 1.5);

    float outv = bgDots + fgDots;
    outv *= (0.3 + 0.7 * fade);

    // Gamma
    outv = pow(outv, 1.0 / 2.2);

    gl_FragColor = vec4(vec3(outv), 1.0);
  }
`;
