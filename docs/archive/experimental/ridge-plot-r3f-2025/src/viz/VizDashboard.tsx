import { useEffect, useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import { audioEngine } from "../audio/AudioEngine";
import { AudioHistoryTexture } from "./AudioHistoryTexture";
import { RidgePlot } from "./RidgePlot";
import { DotMatrixSpectrogram } from "./DotMatrixSpectrogram";

const HISTORY_ROWS = 60;
const FFT_BINS = 256;

// Console Layout Constants
const PANEL_WIDTH = 20;
const RIDGE_HEIGHT = 12;
const GRID_HEIGHT = 8;
const TILT_ANGLE = -Math.PI / 4; // -45 degrees

// Pre-computed positions for console geometry
const RIDGE_Y_OFFSET = (RIDGE_HEIGHT / 2) * Math.sin(Math.abs(TILT_ANGLE));
const RIDGE_Z_OFFSET = -(RIDGE_HEIGHT / 2) * Math.cos(Math.abs(TILT_ANGLE));

export function VizDashboard() {
  const history = useMemo(() => new AudioHistoryTexture(FFT_BINS, HISTORY_ROWS), []);
  const offsetRef = useRef(0);

  useEffect(() => {
    return () => history.dispose();
  }, [history]);

  useFrame(() => {
    if (!audioEngine.ready) return;
    audioEngine.update();
    offsetRef.current = history.pushRow(audioEngine.frequencyData);
  });

  return (
    <group position={[0, 2, 0]}>
      {/* Ridge Plot - tilted "keyboard" surface, front edge at group origin */}
      <group
        rotation={[TILT_ANGLE, 0, 0]}
        position={[0, RIDGE_Y_OFFSET, RIDGE_Z_OFFSET]}
      >
        <RidgePlot
          history={history}
          offsetRef={offsetRef}
          width={PANEL_WIDTH}
          height={RIDGE_HEIGHT}
        />
      </group>

      {/* Dot Matrix - vertical "front panel", hangs from shared edge */}
      <group position={[0, -GRID_HEIGHT / 2, 0]}>
        <DotMatrixSpectrogram
          history={history}
          offsetRef={offsetRef}
          width={PANEL_WIDTH}
          height={GRID_HEIGHT}
        />
      </group>
    </group>
  );
}
