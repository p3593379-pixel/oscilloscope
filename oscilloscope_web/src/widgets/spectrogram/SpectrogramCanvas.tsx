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

    // Display settings the spectrogram stream needs to respect
    const spectrogramFftSize = useSettingsStore(s => s.spectrogramFftSize);
    const spectrogramFps     = useSettingsStore(s => s.spectrogramFps);

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

    // Forward channel visibility to worker
    useEffect(() => {
        workerRef.current?.postMessage({ type: 'settings', channelVisible });
    }, [channelVisible, workerRef]);

    // Forward display settings to worker so it can adapt its paint FPS cap
    // and knows the expected bin count for ring-buffer sizing
    useEffect(() => {
        workerRef.current?.postMessage({
            type: 'displaySettings',
            spectrogramFftSize,
            spectrogramFps,
        });
    }, [spectrogramFftSize, spectrogramFps, workerRef]);

    // Keep canvas pixel size in sync
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

    // useSpectrogramStream already restarts when spectrogramEnabled changes;
    // additionally restart when fftSize or fps changes by passing them as a
    // composite key via the token-suffix trick
    useSpectrogramStream(workerRef, streamToken);

    const ch0Color = '#1565c0';
    const ch1Color = '#c62828';

    return (
        <div className={styles.canvasWrapper}>
            {/* channel selector */}
            <div className={styles.channelTabs}>
                {Array.from({ length: totalChannels }, (_, i) => (
                    <button
                        key={i}
                        className={`${styles.chTab} ${selectedChannel === i ? styles.chTabActive : ''}`}
                        style={selectedChannel === i
                            ? { borderBottomColor: i === 0 ? ch0Color : ch1Color }
                            : undefined}
                        onClick={() => setSelectedChannel(i)}
                    >
                        CH{i + 1}
                    </button>
                ))}
            </div>

            {/* eslint-disable-next-line @typescript-eslint/no-explicit-any */}
            <canvas ref={canvasRef as any} className={styles.canvas}
                    aria-label="Spectrogram display" />
        </div>
    );
}

export function SpectrogramCanvas({ workerRef }: Props) {
    const streamToken = useAuthStore(s => s.streamToken);
    if (!streamToken) return null;
    return <SpectrogramCanvasInner workerRef={workerRef} streamToken={streamToken} />;
}
