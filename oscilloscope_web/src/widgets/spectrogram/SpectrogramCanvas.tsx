import { useEffect, useRef, useState, type RefObject } from 'react';
import { useSpectrogramStream }  from '@/features/data-stream/useSpectrogramStream';
import { useAuthStore }          from '@/entities/auth/authStore';
import { useSettingsStore }      from '@/entities/oscilloscopeSettings/settingsStore';
import styles                    from './SpectrogramCanvas.module.css';

interface Props {
    workerRef: RefObject<Worker | null>;
}

function SpectrogramCanvasInner({
                                    workerRef,
                                    streamToken,
                                }: Props & { streamToken: string }) {
    const canvasRef      = useRef<HTMLCanvasElement>(null);
    const channelVisible = useSettingsStore(s => s.channelVisible);
    const totalChannels  = channelVisible.length;

    const [selectedChannel, setSelectedChannel] = useState(0);

    // Boot the OffscreenCanvas worker once on mount
    useEffect(() => {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const ref = canvasRef as any;
        if (!ref.current) return;
        const offscreen = ref.current.transferControlToOffscreen();
        const worker = new Worker(
            new URL('../../features/data-stream/worker/spectrogramWorker.ts', import.meta.url),
            { type: 'module' }
        );
        worker.postMessage(
            { type: 'init', canvas: offscreen, channelCount: totalChannels, selectedChannel: 0 },
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

    // Forward selected channel changes to worker
    useEffect(() => {
        workerRef.current?.postMessage({ type: 'selectChannel', channel: selectedChannel });
    }, [selectedChannel, workerRef]);

    // Forward settings changes to worker
    useEffect(() => {
        workerRef.current?.postMessage({ type: 'settings', channelVisible });
    }, [channelVisible, workerRef]);

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

    useSpectrogramStream(workerRef, streamToken);

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%', gap: '8px' }}>
            <div style={{ display: 'flex', gap: '6px', flexShrink: 0 }}>
                {Array.from({ length: totalChannels }, (_, i) => (
                    <button
                        key={i}
                        onClick={() => setSelectedChannel(i)}
                        style={{
                            padding: '3px 10px',
                            fontSize: '12px',
                            fontWeight: selectedChannel === i ? 700 : 400,
                            background: selectedChannel === i
                                ? 'var(--color-primary, #01696f)'
                                : 'var(--color-surface-offset, #edeae5)',
                            color: selectedChannel === i
                                ? 'var(--color-text-inverse, #f9f8f4)'
                                : 'var(--color-text, #28251d)',
                            border: 'none',
                            borderRadius: '4px',
                            cursor: 'pointer',
                        }}
                    >
                        CH{i + 1}
                    </button>
                ))}
            </div>
            {/* eslint-disable-next-line @typescript-eslint/no-explicit-any */}
            <canvas ref={canvasRef as any} className={styles.canvas}
                    style={{ flex: 1, minHeight: 0 }}
                    aria-label="Spectrogram waterfall display" />
        </div>
    );
}

export function SpectrogramCanvas({ workerRef }: Props) {
    const spectrogramToken = useAuthStore(s => s.spectrogramToken);
    if (!spectrogramToken) return null;
    return <SpectrogramCanvasInner workerRef={workerRef} streamToken={spectrogramToken} />;
}
