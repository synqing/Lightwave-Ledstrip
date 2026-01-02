export const ridgeVertexShader = `
  precision highp float;

  uniform sampler2D uAudioTex;
  uniform float uOffset;
  uniform float uGain;
  uniform float uHistory;

  varying float vElevation;
  varying vec2 vUv;
  varying float vAge;

  void main() {
    vUv = uv;

    // CRITICAL: Must SUBTRACT uOffset so newest data appears at front
    float yLookup = fract(uOffset - vUv.y);
    vec2 lookupUv = vec2(vUv.x, yLookup);

    float audioValue = texture2D(uAudioTex, lookupUv).r;

    float shaped = pow(audioValue, 1.35);
    float elevation = shaped * uGain;

    vec3 newPos = position;
    newPos.z += elevation;

    vElevation = elevation;
    vAge = fract(vUv.y + 1.0 - uOffset);

    gl_Position = projectionMatrix * modelViewMatrix * vec4(newPos, 1.0);
  }
`;

export const ridgeFragmentShader = `
  precision highp float;

  uniform float uHistory;

  varying float vElevation;
  varying vec2 vUv;
  varying float vAge;

  void main() {
    float rowPhase = fract(vUv.y * uHistory);

    float d = abs(rowPhase - 0.5);
    float lineWidth = 0.08;
    float aa = fwidth(d);
    float line = 1.0 - smoothstep(lineWidth - aa, lineWidth + aa, d);

    float intensity = smoothstep(0.0, 1.0, vElevation);

    float fade = 1.0 - vAge;
    fade = pow(fade, 1.6);

    float base = 0.06;
    float glow = line * (0.25 + 1.35 * intensity);

    float outv = base + glow;
    outv *= (0.35 + 0.65 * fade);

    // Gamma correction
    outv = pow(outv, 1.0 / 2.2);

    gl_FragColor = vec4(vec3(outv), 1.0);
  }
`;
