import { useMemo, useState } from "react";
import { Canvas } from "@react-three/fiber";
import { OrbitControls, Stats } from "@react-three/drei";
import * as THREE from "three";
import { audioEngine } from "./audio/AudioEngine";
import { getMaxVertexTextureUnits } from "./viz/GpuCaps";
import { VizDashboard } from "./viz/VizDashboard";

type Status =
  | { phase: "idle" }
  | { phase: "starting"; mode: "synth" | "mic" }
  | { phase: "running"; mode: "synth" | "mic"; maxVTF: number }
  | { phase: "error"; message: string };

export default function App() {
  const [status, setStatus] = useState<Status>({ phase: "idle" });

  const overlay = useMemo(() => {
    if (status.phase === "idle") {
      return (
        <div className="panel">
          <h1>Ridge + Dot-Matrix Spectrogram</h1>
          <p>Click to start audio visualization.</p>
          <div className="row">
            <button
              className="btn"
              onClick={async () => {
                try {
                  setStatus({ phase: "starting", mode: "synth" });
                  await audioEngine.initSynth();
                  setStatus({ phase: "running", mode: "synth", maxVTF: -1 });
                } catch (e: unknown) {
                  const message = e instanceof Error ? e.message : String(e);
                  setStatus({ phase: "error", message });
                }
              }}
            >
              START (SYNTH)
            </button>
            <button
              className="btn secondary"
              onClick={async () => {
                try {
                  setStatus({ phase: "starting", mode: "mic" });
                  await audioEngine.initMic();
                  setStatus({ phase: "running", mode: "mic", maxVTF: -1 });
                } catch (e: unknown) {
                  const message = e instanceof Error ? e.message : String(e);
                  setStatus({ phase: "error", message });
                }
              }}
            >
              START (MIC)
            </button>
          </div>
        </div>
      );
    }

    if (status.phase === "starting") {
      return (
        <div className="panel">
          <h1>Initializing audio…</h1>
          <p>Mode: {status.mode}</p>
        </div>
      );
    }

    if (status.phase === "error") {
      return (
        <div className="panel">
          <h1 className="bad">Error</h1>
          <p>{status.message}</p>
          <div className="row">
            <button className="btn" onClick={() => setStatus({ phase: "idle" })}>
              RESET
            </button>
          </div>
        </div>
      );
    }

    if (status.phase === "running") {
      return (
        <div className="panel">
          <h1>Running</h1>
          <p>Mode: <b>{status.mode}</b></p>
          <p className="kv">Drag to orbit, scroll to zoom.</p>
        </div>
      );
    }

    return null;
  }, [status]);

  return (
    <div style={{ width: "100vw", height: "100vh" }}>
      <div className="overlay">{overlay}</div>

      <Canvas
        camera={{ position: [0, 4.5, 16], fov: 45, near: 0.1, far: 200 }}
        onCreated={({ gl }) => {
          const maxVTF = getMaxVertexTextureUnits(gl as unknown as THREE.WebGLRenderer);
          setStatus((prev) => {
            if (prev.phase !== "running") return prev;
            return { ...prev, maxVTF };
          });
        }}
      >
        <color attach="background" args={["#000000"]} />

        {status.phase === "running" && <VizDashboard />}

        <OrbitControls enableDamping dampingFactor={0.08} />
        <Stats />
      </Canvas>
    </div>
  );
}
