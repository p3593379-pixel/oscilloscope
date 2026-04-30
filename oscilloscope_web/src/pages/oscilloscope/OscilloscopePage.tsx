import { useNavigate }        from 'react-router';
import { useAuthStore }       from '@/entities/auth/authStore';
import { useWaveformStore }   from '@/entities/waveform/waveformStore';
import { SessionMode, UserRole } from '@/generated/buf_connect_server_pb';
import { OscWidget }          from '@/widgets/oscilloscope/OscWidget';
import styles                 from './OscilloscopePage.module.css';

// ── Inline SVG logo ──────────────────────────────────────────────────────────
const OscLogo = () => (
    <svg
        width="28" height="28"
        viewBox="0 0 28 28"
        fill="none"
        aria-label="Oscilloscope"
    >
        {/* screen bezel */}
        <rect x="2" y="4" width="24" height="18" rx="2"
              stroke="#222" strokeWidth="1.5" fill="none"/>
        {/* IN0 waveform — blue */}
        <polyline
            points="4,16 7,16 9,8 11,22 13,8 15,22 17,8 19,16 22,16 24,16"
            stroke="#87cefa" strokeWidth="1.5" fill="none"
            strokeLinejoin="round" strokeLinecap="round"/>
        {/* ground stub left */}
        <line x1="0" y1="24" x2="4" y2="24" stroke="#555" strokeWidth="1.2"/>
        <line x1="0" y1="26" x2="4" y2="26" stroke="#555" strokeWidth="1.2"/>
        {/* ground stub right */}
        <line x1="24" y1="24" x2="28" y2="24" stroke="#555" strokeWidth="1.2"/>
    </svg>
);

// ── Status pill ───────────────────────────────────────────────────────────────
function StreamPill({ isStreaming }: { isStreaming: boolean }) {
    return (
        <span className={`${styles.pill} ${isStreaming ? styles.pillLive : styles.pillStopped}`}>
            <span className={isStreaming ? styles.liveDot : styles.stoppedDot}/>
            {isStreaming ? 'LIVE' : 'STOPPED'}
        </span>
    );
}

function roleName(role: UserRole): string {
    switch (role) {
        case UserRole.ADMIN:    return 'ADMIN';
        case UserRole.ENGINEER: return 'ENGINEER';
        default:                return 'UNDEFINED';
    }
}

function sessionModeName(mode: SessionMode): string {
    switch (mode) {
        case SessionMode.ACTIVE:     return 'ACTIVE';
        case SessionMode.OBSERVER:   return 'OBSERVER';
        case SessionMode.ON_SERVICE: return 'SERVICE';
        default:                     return '';
    }
}

function fmtRate(hz: number): string {
    if (!hz) return '';
    return hz >= 1_000_000
        ? `${(hz / 1_000_000).toFixed(2)} MHz`
        : hz >= 1_000
            ? `${(hz / 1_000).toFixed(1)} kHz`
            : `${hz} Hz`;
}

// ── Menubar ───────────────────────────────────────────────────────────────────
function Menubar() {
    const navigate   = useNavigate();
    const role       = useAuthStore(s => s.role);
    const sessionMode = useAuthStore(s => s.sessionMode);
    const clearAuth  = useAuthStore(s => s.clearAuth);

    const isStreaming  = useWaveformStore(s => s.isStreaming);
    const sampleRate   = useWaveformStore(s => s.sampleRate);
    const frameCount   = useWaveformStore(s => s.frameCount);
    const channelCount = useWaveformStore(s => s.channelCount);

    const handleLogout = () => {
        clearAuth();
        navigate('/login', { replace: true });
    };

    return (
        <nav className={styles.menubar} aria-label="Application bar">
            {/* ── Left: brand ── */}
            <div className={styles.brand}>
                <OscLogo/>
                <span className={styles.brandName}>Oscilloscope</span>
            </div>

            {/* ── Right: status + session ── */}
            <div className={styles.statusArea}>
                <StreamPill isStreaming={isStreaming}/>

                {sampleRate > 0 && (
                    <span className={styles.metaStat}>
                        f<sub>s</sub> {fmtRate(sampleRate)}
                    </span>
                )}

                {channelCount > 0 && (
                    <span className={styles.metaStat}>
                        CH&thinsp;×&thinsp;{channelCount}
                    </span>
                )}

                {frameCount > 0 && (
                    <span className={styles.metaStat}>
                        {frameCount.toLocaleString()} fr
                    </span>
                )}

                <div className={styles.divider}/>

                <span className={styles.sessionBadge}>
                    {roleName(role)}
                    {sessionMode ? ` · ${sessionModeName(sessionMode)}` : ''}
                </span>

                <button
                    className={styles.logoutBtn}
                    onClick={handleLogout}
                    title="Sign out"
                    aria-label="Sign out"
                >
                    <svg width="15" height="15" viewBox="0 0 15 15"
                         fill="none" stroke="currentColor" strokeWidth="1.6">
                        <path d="M5.5 2H3a1 1 0 0 0-1 1v9a1 1 0 0 0 1 1h2.5"/>
                        <path d="M10 10.5 l3-3 -3-3" strokeLinecap="round" strokeLinejoin="round"/>
                        <line x1="13" y1="7.5" x2="5.5" y2="7.5" strokeLinecap="round"/>
                    </svg>
                    Sign out
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
