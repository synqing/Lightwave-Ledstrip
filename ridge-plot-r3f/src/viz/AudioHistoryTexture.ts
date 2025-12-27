import * as THREE from "three";

/**
 * AudioHistoryTexture - Ring buffer backed by THREE.DataTexture
 * Using RedFormat which was working in v2.
 */
export class AudioHistoryTexture {
  readonly width: number;
  readonly height: number;
  readonly data: Uint8Array;
  readonly texture: THREE.DataTexture;

  private writeRow = 0;

  constructor(width: number, height: number) {
    this.width = width;
    this.height = height;

    this.data = new Uint8Array(width * height);

    this.texture = new THREE.DataTexture(
      this.data,
      width,
      height,
      THREE.RedFormat,
      THREE.UnsignedByteType
    );

    this.texture.unpackAlignment = 1;
    this.texture.magFilter = THREE.LinearFilter;
    this.texture.minFilter = THREE.LinearFilter;
    this.texture.wrapS = THREE.ClampToEdgeWrapping;
    this.texture.wrapT = THREE.RepeatWrapping;
    this.texture.generateMipmaps = false;
    this.texture.needsUpdate = true;
  }

  pushRow(row: Uint8Array | Uint8Array<ArrayBuffer>): number {
    const start = this.writeRow * this.width;
    const slice = row.length >= this.width ? row.subarray(0, this.width) : row;
    this.data.set(slice, start);

    // Full texture upload - simple and reliable
    this.texture.needsUpdate = true;

    const offsetNorm = this.writeRow / this.height;
    this.writeRow = (this.writeRow + 1) % this.height;

    return offsetNorm;
  }

  dispose(): void {
    this.texture.dispose();
  }
}
