const NUM_LEDS = 320;
const STRIP_LENGTH = 160;
const canvas = document.getElementById('ledCanvas');
const ctx = canvas.getContext('2d');
const speedSlider = document.getElementById('speed');
const brightnessSlider = document.getElementById('brightness');
const colorShiftSlider = document.getElementById('colorShift');
const waveFreqSlider = document.getElementById('waveFreq');
const colorStartPicker = document.getElementById('colorStart');
const colorEndPicker = document.getElementById('colorEnd');
const patternSelector = document.getElementById('pattern');

let t = 0;

function hexToRgb(hex) {
  hex = hex.replace('#', '');
  if (hex.length === 3) hex = hex.split('').map(x => x + x).join('');
  const num = parseInt(hex, 16);
  return [num >> 16 & 255, num >> 8 & 255, num & 255];
}

function lerp(a, b, t) {
  return a + (b - a) * t;
}

function lerpColor(rgbA, rgbB, t) {
  return [
    Math.round(lerp(rgbA[0], rgbB[0], t)),
    Math.round(lerp(rgbA[1], rgbB[1], t)),
    Math.round(lerp(rgbA[2], rgbB[2], t))
  ];
}

function drawGradientWave() {
  let speed = parseFloat(speedSlider.value);
  let brightness = parseFloat(brightnessSlider.value);
  let colorShift = parseFloat(colorShiftSlider.value);
  let waveFreq = parseFloat(waveFreqSlider.value);
  let colorA = hexToRgb(colorStartPicker.value);
  let colorB = hexToRgb(colorEndPicker.value);

  ctx.clearRect(0, 0, canvas.width, canvas.height);
  for (let i = 0; i < NUM_LEDS; i++) {
    let row = i < STRIP_LENGTH ? 0 : 1;
    let col = i % STRIP_LENGTH;
    let x = 20 + col * 8;
    let y = 30 + row * 50;

    // Gradient position along strip
    let gradT = i / (NUM_LEDS - 1);
    // Wave modulation
    let wave = Math.sin(2 * Math.PI * (gradT * waveFreq + colorShift + t * 0.1 * speed));
    let waveT = (wave + 1) / 2; // 0..1
    let rgb = lerpColor(colorA, colorB, waveT);
    rgb = rgb.map(c => Math.floor(c * brightness));

    ctx.beginPath();
    ctx.arc(x, y, 6, 0, 2 * Math.PI);
    ctx.fillStyle = `rgb(${rgb[0]},${rgb[1]},${rgb[2]})`;
    ctx.shadowColor = ctx.fillStyle;
    ctx.shadowBlur = 10;
    ctx.fill();
    ctx.shadowBlur = 0;
  }
}

function drawPulsePattern() {
  // Placeholder: show a static color for now
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  for (let i = 0; i < NUM_LEDS; i++) {
    let row = i < STRIP_LENGTH ? 0 : 1;
    let col = i % STRIP_LENGTH;
    let x = 20 + col * 8;
    let y = 30 + row * 50;
    ctx.beginPath();
    ctx.arc(x, y, 6, 0, 2 * Math.PI);
    ctx.fillStyle = '#444';
    ctx.fill();
  }
  // TODO: Implement audio-reactive pulse pattern
}

function draw() {
  if (patternSelector.value === 'gradient') {
    drawGradientWave();
  } else if (patternSelector.value === 'pulse') {
    drawPulsePattern();
  }
  t += 1 / 60;
  requestAnimationFrame(draw);
}

draw(); 