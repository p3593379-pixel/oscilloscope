import { useRef } from 'react';
import { useSettingsStore }     from '@/entities/oscilloscopeSettings/settingsStore';
import { useSpectrogramStore }  from '@/entities/spectrogram/spectrogramStore';
import { SpectrogramCanvas }    from './SpectrogramCanvas';
import styles                   from './SpectrogramWidget.module.css';

const IconPlay = () => (
    <svg width="11" height="11" viewBox="0 0 11 11" fill="currentColor">
        <path d="M2 1.5 L9.5 5.5 L2 9.5 Z"/>
    </svg>
);
const IconStop = () => (
    <svg width="10" height="10" viewBox="0 0 10 10" fill="currentColor">
        <rect x="1.5" y="1.5" width="7" height="7" rx="1"/>
    </svg>
);

export function SpectrogramWidget() {
    const workerRef = useRef<Worker | null>(null);

    const spectrogramEnabled    = useSettingsStore(s => s.spectrogramEnabled);
    const setSpectrogramEnabled = useSettingsStore(s => s.setSpectrogramEnabled);
    const isStreaming            = useSpectrogramStore(s => s.isStreaming);
    const frameCount             = useSpectrogramStore(s => s.frameCount);
    const meta                   = useSpectrogramStore(s => s.meta);

    return (
        <div className={styles.widget}>
            <div className={styles.toolbar}>
                <button
                    className={`${styles.streamBtn} ${spectrogramEnabled ? styles.streamBtnStop : styles.streamBtnStart}`}
                    onClick={() => setSpectrogramEnabled(!spectrogramEnabled)}
                    title={spectrogramEnabled ? 'Stop spectrogram' : 'Start spectrogram'}
                >
                    {spectrogramEnabled ? <><IconStop/>&nbsp;Stop</> : <><IconPlay/>&nbsp;Start</>}
                </button>

                <span className={`${styles.pill} ${isStreaming ? styles.pillLive : styles.pillStopped}`}>
                    <span className={isStreaming ? styles.liveDot : styles.stoppedDot}/>
                    {isStreaming ? 'LIVE' : 'STOPPED'}
                </span>

                {meta && (
                    <>
                        <span className={styles.stat}>
                            FFT&thinsp;{meta.fftSize}
                        </span>
                        <span className={styles.stat}>
                            {(meta.freqResolutionHz / 1000).toFixed(1)}&thinsp;kHz/bin
                        </span>
                    </>
                )}
                {frameCount > 0 && (
                    <span className={styles.stat}>{frameCount.toLocaleString()}&thinsp;fr</span>
                )}
            </div>

            <div className={styles.canvasArea}>
                <SpectrogramCanvas workerRef={workerRef} />
            </div>
        </div>
    );
}
