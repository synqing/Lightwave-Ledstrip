import { useEffect, useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import { audioEngine } from "../audio/AudioEngine";
import { AudioHistoryTexture } from "./AudioHistoryTexture";
import { RidgePlot } from "./RidgePlot";
import { DotMatrixSpectrogram } from "./DotMatrixSpectrogram";

const HISTORY_ROWS = 60;
const FFT_BINS = 256;

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
    <>
      <RidgePlot history={history} offsetRef={offsetRef} />
      <DotMatrixSpectrogram history={history} offsetRef={offsetRef} />
    </>
  );
}
