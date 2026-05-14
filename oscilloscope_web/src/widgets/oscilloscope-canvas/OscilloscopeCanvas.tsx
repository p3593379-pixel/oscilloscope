import { useEffect, useRef, type RefObject } from 'react';
import { useDataStream }              from '@/features/data-stream/useDataStream';
import { useAuthStore }               from '@/entities/auth/authStore';
import { useSettingsStore }           from '@/entities/oscilloscopeSettings/settingsStore';
import styles                         from './OscilloscopeCanvas.module.css';

interface Props {
  workerRef: RefObject<Worker | null>;
}

// Inner component — only rendered when streamToken is guaranteed non-null.
function OscilloscopeCanvasInner({
                                   workerRef,
                                   streamToken,
                                 }: Props & { streamToken: string }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  const {
    xStart, xShow, yPeakToPeak,
    verticalOffset, channelVisible,
  } = useSettingsStore();

  useEffect(() => {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const ref = canvasRef as any;
    if (!ref.current) return;
    const offscreen = ref.current.transferControlToOffscreen();
    const worker = new Worker(
        new URL('../../features/data-stream/worker/streamWorker.ts', import.meta.url),
        { type: 'module' }
    );
    worker.postMessage(
        { type: 'init', canvas: offscreen, channelCount: 2 },
        [offscreen]
    );
    (workerRef as React.MutableRefObject<Worker | null>).current = worker;
    return () => {
      worker.postMessage({ type: 'stop' });
      worker.terminate();
      (workerRef as React.MutableRefObject<Worker | null>).current = null;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    workerRef.current?.postMessage({
      type: 'settings', xStart, xShow, yPeakToPeak,
      verticalOffset, channelVisible,
    });
  }, [xStart, xShow, yPeakToPeak, verticalOffset, channelVisible, workerRef]);

  useEffect(() => {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const ref = canvasRef as any;
    if (!ref.current) return;
    const ro = new ResizeObserver((entries) => {
      const { width, height } = entries[0].contentRect;
      workerRef.current?.postMessage({ type: 'resize',
        width: Math.round(width), height: Math.round(height) });
    });
    ro.observe(ref.current);
    return () => ro.disconnect();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useDataStream(workerRef, streamToken);

  return (
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      <canvas ref={canvasRef as any} className={styles.canvas}
              aria-label="Oscilloscope waveform display" />
  );
}

// Outer shell — delays mount until token is ready.
export function OscilloscopeCanvas({ workerRef }: Props) {
  const streamToken = useAuthStore(s => s.streamToken);
  if (!streamToken) return null;
  return <OscilloscopeCanvasInner workerRef={workerRef} streamToken={streamToken} />;
}
