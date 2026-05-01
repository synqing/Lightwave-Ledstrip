import type { LedFrame } from '../types';

export type RendererOptions = {
  width: number;
  height: number;
  ledSize: number;
};

export class ByteStripRenderer {
  private readonly ctx: CanvasRenderingContext2D;
  private readonly options: RendererOptions;

  constructor(private readonly canvas: HTMLCanvasElement, options?: Partial<RendererOptions>) {
    const ctx = canvas.getContext('2d');
    if (!ctx) {
      throw new Error('Canvas 2D context unavailable');
    }
    this.ctx = ctx;
    this.options = {
      width: options?.width ?? 1120,
      height: options?.height ?? 260,
      ledSize: options?.ledSize ?? 5,
    };
    this.canvas.width = this.options.width;
    this.canvas.height = this.options.height;
  }

  render(frame: LedFrame | null): void {
    const { width, height, ledSize } = this.options;
    this.ctx.clearRect(0, 0, width, height);
    this.ctx.fillStyle = '#05070a';
    this.ctx.fillRect(0, 0, width, height);

    this.drawPlate();
    if (frame) {
      this.drawFrame(frame, 0, 78);
      this.drawFrame(frame, 160, 158);
      this.drawBadge(frame.source);
    } else {
      this.ctx.fillStyle = '#5b6472';
      this.ctx.font = '14px ui-monospace, SFMono-Regular, Menlo, monospace';
      this.ctx.fillText('No capture loaded', 36, height - 30);
    }

    this.drawCentreMarkers(78, ledSize);
    this.drawCentreMarkers(158, ledSize);
  }

  private drawPlate(): void {
    const gradient = this.ctx.createLinearGradient(0, 0, this.options.width, 0);
    gradient.addColorStop(0, '#05070a');
    gradient.addColorStop(0.5, '#101722');
    gradient.addColorStop(1, '#05070a');
    this.ctx.fillStyle = gradient;
    this.ctx.fillRect(24, 42, this.options.width - 48, 170);
    this.ctx.strokeStyle = 'rgba(129, 230, 217, 0.18)';
    this.ctx.lineWidth = 1;
    this.ctx.strokeRect(24.5, 42.5, this.options.width - 49, 170);
  }

  private drawFrame(frame: LedFrame, stripStart: number, y: number): void {
    const ledSize = this.options.ledSize;
    const gap = 1;
    const startX = 44;
    for (let i = 0; i < 160; i += 1) {
      const ledIndex = stripStart + i;
      const rgb = ledIndex * 3;
      const r = frame.leds[rgb] ?? 0;
      const g = frame.leds[rgb + 1] ?? 0;
      const b = frame.leds[rgb + 2] ?? 0;
      this.ctx.fillStyle = `rgb(${r}, ${g}, ${b})`;
      this.ctx.fillRect(startX + i * (ledSize + gap), y, ledSize, 28);
    }
  }

  private drawCentreMarkers(y: number, ledSize: number): void {
    const gap = 1;
    const startX = 44;
    const left = startX + 79 * (ledSize + gap) - 2;
    const right = startX + 80 * (ledSize + gap) + ledSize + 2;
    this.ctx.strokeStyle = 'rgba(255, 184, 77, 0.95)';
    this.ctx.lineWidth = 2;
    this.ctx.beginPath();
    this.ctx.moveTo(left, y - 10);
    this.ctx.lineTo(left, y + 40);
    this.ctx.moveTo(right, y - 10);
    this.ctx.lineTo(right, y + 40);
    this.ctx.stroke();
  }

  private drawBadge(source: string): void {
    const label = source === 'predicted-workbench'
      ? 'PREDICTED - NOT HARDWARE TRUTH'
      : source === 'schematic-workbench'
        ? 'SCHEMATIC - NOT HARDWARE TRUTH'
        : source.toUpperCase();
    const provisional = source === 'predicted-workbench' || source === 'schematic-workbench';
    this.ctx.font = '12px ui-monospace, SFMono-Regular, Menlo, monospace';
    const width = Math.min(310, this.ctx.measureText(label).width + 34);
    const x = this.options.width - width - 28;
    this.ctx.fillStyle = provisional ? 'rgba(255, 184, 77, 0.18)' : 'rgba(34, 211, 238, 0.16)';
    this.ctx.fillRect(x, 16, width, 28);
    this.ctx.strokeStyle = provisional ? 'rgba(255, 184, 77, 0.65)' : 'rgba(34, 211, 238, 0.55)';
    this.ctx.strokeRect(x + 0.5, 16.5, width, 28);
    this.ctx.fillStyle = provisional ? '#ffcf7a' : '#67e8f9';
    this.ctx.textAlign = 'right';
    this.ctx.fillText(label, this.options.width - 42, 35);
    this.ctx.textAlign = 'left';
  }
}
