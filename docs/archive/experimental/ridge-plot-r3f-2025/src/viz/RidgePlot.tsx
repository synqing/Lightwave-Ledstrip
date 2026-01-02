import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { AudioHistoryTexture } from "./AudioHistoryTexture";
import { ridgeVertexShader, ridgeFragmentShader } from "./ridgeShaders";

const PLANE_WIDTH = 20;
const PLANE_HEIGHT = 12;
const DEFAULT_GAIN = 2.2;
const ROTATION_X = -Math.PI * 0.35;

type Props = {
  history: AudioHistoryTexture;
  offsetRef: React.MutableRefObject<number>;
};

export function RidgePlot({ history, offsetRef }: Props) {
  const meshRef = useRef<THREE.Mesh>(null);

  const uniforms = useMemo(
    () => ({
      uAudioTex: { value: history.texture },
      uOffset: { value: 0.0 },
      uGain: { value: DEFAULT_GAIN },
      uHistory: { value: history.height },
    }),
    [history.texture, history.height]
  );

  useFrame(() => {
    const mat = meshRef.current?.material as THREE.ShaderMaterial | undefined;
    if (mat) {
      mat.uniforms.uOffset.value = offsetRef.current;
    }
  });

  const bins = history.width;
  const rows = history.height;

  return (
    <mesh
      ref={meshRef}
      frustumCulled={false}
      rotation={[ROTATION_X, 0, 0]}
      position={[0, -2.0, 0]}
    >
      <planeGeometry args={[PLANE_WIDTH, PLANE_HEIGHT, bins - 1, rows - 1]} />
      <shaderMaterial
        uniforms={uniforms}
        vertexShader={ridgeVertexShader}
        fragmentShader={ridgeFragmentShader}
        side={THREE.DoubleSide}
      />
    </mesh>
  );
}
