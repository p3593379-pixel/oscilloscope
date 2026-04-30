import { useEffect, type RefObject } from 'react';
import { useDataStream }              from '@/features/data-stream/useDataStream';
import { useSettingsStore }           from '@/entities/oscilloscopeSettings/settingsStore';
import styles                         from './OscilloscopeCanvas.module.css';

interface Props {
  /** Owned by OscWidget; passed here so the parent can also postMessage. */
  workerRef: RefObject<Worker | null>;
}

export function OscilloscopeCanvas({ workerRef }: Props) {
  const canvasRef = typeof document !== 'undefined'
      ? require('react').useRef<HTMLCanvasElement>()
      : { current: null };

  const {
    xStart, xShow, yPeakToPeak,
    verticalOffset, channelVisible,
  } = useSettingsStore();

  // Boot the OffscreenCanvas worker once on mount
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

  // Forward settings to worker whenever they change
  useEffect(() => {
    workerRef.current?.postMessage({
      type: 'settings', xStart, xShow, yPeakToPeak,
      verticalOffset, channelVisible,
    });
  }, [xStart, xShow, yPeakToPeak, verticalOffset, channelVisible, workerRef]);

  // Keep canvas pixel size in sync with CSS size
  useEffect(() => {
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const ref = canvasRef as any;
    if (!ref.current) return;
    const ro = new ResizeObserver((entries) => {
      const { width, height } = entries[0].contentRect;
      workerRef.current?.postMessage({
        type: 'resize',
        width:  Math.round(width),
        height: Math.round(height),
      });
    });
    ro.observe(ref.current);
    return () => ro.disconnect();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useDataStream(workerRef);

  return (
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      <canvas ref={canvasRef as any} className={styles.canvas}
              aria-label="Oscilloscope waveform display" />
  );
}
