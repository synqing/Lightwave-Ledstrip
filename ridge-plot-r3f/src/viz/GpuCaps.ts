import * as THREE from "three";

export function getMaxVertexTextureUnits(renderer: THREE.WebGLRenderer): number {
  const gl = renderer.getContext();
  // MAX_VERTEX_TEXTURE_IMAGE_UNITS is queried via getParameter. :contentReference[oaicite:7]{index=7}
  return gl.getParameter(gl.MAX_VERTEX_TEXTURE_IMAGE_UNITS) as number;
}
