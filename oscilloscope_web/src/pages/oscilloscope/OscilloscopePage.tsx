import { useNavigate }           from 'react-router';
import { useAuthStore }          from '@/entities/auth/authStore';
import { useWaveformStore }      from '@/entities/waveform/waveformStore';
import { useSettingsStore }      from '@/entities/oscilloscopeSettings/settingsStore';
import { SessionMode, UserRole } from '@/generated/buf_connect_server_pb';
import { OscWidget }             from '@/widgets/oscilloscope/OscWidget';
import styles                    from './OscilloscopePage.module.css';

// ── SVG logo ──────────────────────────────────────────────────────────────────
const OscLogo = () => (
    <svg width="28" height="28" viewBox="0 0 28 28" fill="none"
         aria-label="Oscilloscope logo">
        <rect x="2" y="4" width="24" height="18" rx="2" stroke="#222" strokeWidth="1.5"/>
        <polyline
            points="4,16 7,16 8,9 10,21 12,9 14,21 16,9 18,16 24,16"
            stroke="#1565c0" strokeWidth="1.5" fill="none"
            strokeLinejoin="round" strokeLinecap="round"/>
        <polyline
            points="4,13 8,13 10,17 12,10 14,17 16,10 18,13 24,13"
            stroke="#c62828" strokeWidth="1" fill="none" opacity="0.55"
            strokeLinejoin="round" strokeLinecap="round"/>
    </svg>
);

// ── helpers ───────────────────────────────────────────────────────────────────
function fmtRate(hz: number): string {
    if (!hz) return '';
    if (hz >= 1_000_000) return `${(hz / 1_000_000).toFixed(2)} MHz`;
    if (hz >= 1_000)     return `${(hz / 1_000).toFixed(1)} kHz`;
    return `${hz} Hz`;
}
function roleName(r: UserRole): string {
    switch (r) {
        case UserRole.ADMIN:    return 'ADMIN';
        case UserRole.ENGINEER: return 'ENGINEER';
        default:                return 'UNDEFINED';
    }
}
function sessionLabel(m: SessionMode): string {
    switch (m) {
        case SessionMode.ACTIVE:     return 'ACTIVE';
        case SessionMode.OBSERVER:   return 'OBSERVER';
        case SessionMode.ON_SERVICE: return 'SERVICE';
        default:                     return '';
    }
}

// ── Icons ─────────────────────────────────────────────────────────────────────
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
const IconSignOut = () => (
    <svg width="14" height="14" viewBox="0 0 14 14"
         fill="none" stroke="currentColor" strokeWidth="1.6">
        <path d="M5 2H3a1 1 0 0 0-1 1v8a1 1 0 0 0 1 1h2" strokeLinecap="round"/>
        <path d="M9.5 9.5 l3-2.5-3-2.5" strokeLinecap="round" strokeLinejoin="round"/>
        <line x1="12.5" y1="7" x2="5" y2="7" strokeLinecap="round"/>
    </svg>
);

// ── Menubar ───────────────────────────────────────────────────────────────────
function Menubar() {
    const navigate   = useNavigate();
    const role       = useAuthStore(s => s.role);
    const sessMode   = useAuthStore(s => s.sessionMode);
    const clearAuth  = useAuthStore(s => s.clearAuth);

    const isStreaming  = useWaveformStore(s => s.isStreaming);
    const sampleRate   = useWaveformStore(s => s.sampleRate);
    const frameCount   = useWaveformStore(s => s.frameCount);
    const channelCount = useWaveformStore(s => s.channelCount);

    const streamingEnabled    = useSettingsStore(s => s.streamingEnabled);
    const setStreamingEnabled = useSettingsStore(s => s.setStreamingEnabled);
    const targetFps           = useSettingsStore(s => s.targetFps);

    const sessTxt = sessionLabel(sessMode);

    const logout = () => { clearAuth(); navigate('/login', { replace: true }); };

    return (
        <nav className={styles.menubar} aria-label="Application bar">

            {/* ── Left: brand ── */}
            <div className={styles.brand}>
                <OscLogo/>
                <span className={styles.brandName}>Oscilloscope</span>
            </div>

            {/* ── Centre: start / stop ── */}
            <div className={styles.centre}>
                <button
                    className={`${styles.streamBtn} ${streamingEnabled ? styles.streamBtnStop : styles.streamBtnStart}`}
                    onClick={() => setStreamingEnabled(!streamingEnabled)}
                    title={streamingEnabled ? 'Stop streaming' : 'Start streaming'}
                >
                    {streamingEnabled ? <><IconStop/>&nbsp;Stop</> : <><IconPlay/>&nbsp;Start</>}
                </button>
            </div>

            {/* ── Right: telemetry + session ── */}
            <div className={styles.statusArea}>

                {/* Live / stopped indicator */}
                <span className={`${styles.pill} ${isStreaming ? styles.pillLive : styles.pillStopped}`}>
                    <span className={isStreaming ? styles.liveDot : styles.stoppedDot}/>
                    {isStreaming ? 'LIVE' : 'STOPPED'}
                </span>

                {sampleRate > 0 && (
                    <span className={styles.stat}>
                        f<sub>s</sub>&nbsp;{fmtRate(sampleRate)}
                    </span>
                )}

                {channelCount > 0 && (
                    <span className={styles.stat}>CH&thinsp;×&thinsp;{channelCount}</span>
                )}

                {frameCount > 0 && (
                    <span className={styles.stat}>{frameCount.toLocaleString()}&thinsp;fr</span>
                )}

                <span className={styles.stat}>{targetFps}&thinsp;fps</span>

                <div className={styles.divider}/>

                <span className={styles.sessionBadge}>
                    {roleName(role)}{sessTxt ? ` · ${sessTxt}` : ''}
                </span>

                <button className={styles.logoutBtn} onClick={logout} title="Sign out">
                    <IconSignOut/>
                    Sign&nbsp;out
                </button>
            </div>
        </nav>
    );
}

// ── Page ──────────────────────────────────────────────────────────────────────
export function OscilloscopePage() {
    return (
        <div className={styles.page}>
            <Menubar/>
            <div className={styles.widgetShell}>
                <OscWidget/>
            </div>
        </div>
    );
}
