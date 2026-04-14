import { useEffect, useRef }      from 'react';
import { useDataStream }           from '@/features/data-stream/useDataStream';
import { useSettingsStore }        from '@/entities/oscilloscopeSettings/settingsStore';

export function OscilloscopeCanvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const workerRef = useRef<Worker | null>(null);

  const { voltsPerDiv, verticalOffset } = useSettingsStore();

  // Boot the OffscreenCanvas worker once on mount
  useEffect(() => {
    if (!canvasRef.current) return;
    const offscreen = canvasRef.current.transferControlToOffscreen();
    const worker = new Worker(
      new URL('../../features/data-stream/worker/streamWorker.ts', import.meta.url),
      { type: 'module' }
    );
    worker.postMessage(
      { type: 'init', canvas: offscreen, channelCount: 2 },
      [offscreen]     // transfer ownership — zero-copy
    );
    workerRef.current = worker;
    return () => {
      worker.postMessage({ type: 'stop' });
      worker.terminate();
    };
  }, []);

  // Forward settings changes to the worker
  useEffect(() => {
    workerRef.current?.postMessage({ type: 'settings', voltsPerDiv, verticalOffset });
  }, [voltsPerDiv, verticalOffset]);

  // Keep canvas pixel size in sync with its CSS size
  useEffect(() => {
    if (!canvasRef.current) return;
    const ro = new ResizeObserver((entries) => {
      const { width, height } = entries[0].contentRect;
      workerRef.current?.postMessage({
        type: 'resize',
        width:  Math.round(width),
        height: Math.round(height),
      });
    });
    ro.observe(canvasRef.current);
    return () => ro.disconnect();
  }, []);

  useDataStream(workerRef);

  return (
    <canvas
      ref={canvasRef}
      style={{ display: 'block', width: '100%', height: '100%', background: '#0d1117', borderRadius: 4 }}
      aria-label="Oscilloscope waveform display"
    />
  );
}
