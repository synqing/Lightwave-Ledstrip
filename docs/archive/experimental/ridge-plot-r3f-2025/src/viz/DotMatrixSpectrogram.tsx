import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { AudioHistoryTexture } from "./AudioHistoryTexture";
import { dotMatrixVertexShader, dotMatrixFragmentShader } from "./dotMatrixShaders";

type Props = {
  history: AudioHistoryTexture;
  offsetRef: React.MutableRefObject<number>;
  width?: number;
  height?: number;
};

export function DotMatrixSpectrogram({ history, offsetRef, width = 20, height = 8 }: Props) {
  const meshRef = useRef<THREE.Mesh>(null);

  const uniforms = useMemo(() => ({
    uAudioTex: { value: history.texture },
    uOffset: { value: 0.0 },
    uGrid: { value: new THREE.Vector2(80, 30) },
    uGain: { value: 1.6 },
    uDotRadius: { value: 0.22 },
  }), [history.texture]);

  useFrame(() => {
    const mat = meshRef.current?.material as THREE.ShaderMaterial | undefined;
    if (mat) mat.uniforms.uOffset.value = offsetRef.current;
  });

  return (
    <mesh ref={meshRef} frustumCulled={false}>
      <planeGeometry args={[width, height, 1, 1]} />
      <shaderMaterial
        uniforms={uniforms}
        vertexShader={dotMatrixVertexShader}
        fragmentShader={dotMatrixFragmentShader}
        transparent={true}
        side={THREE.DoubleSide}
      />
    </mesh>
  );
}
