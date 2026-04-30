// FILE: control_panel_web/src/features/metrics/MetricsSettingsPanel.tsx
import { useEffect, useState, useCallback } from 'react';
import { configClient, type MetricsConfig, type LiveMetrics } from 'api/configClient';

function formatBytes(b: number) {
    if (b >= 1e9) return `${(b / 1e9).toFixed(2)} GB`;
    if (b >= 1e6) return `${(b / 1e6).toFixed(2)} MB`;
    if (b >= 1e3) return `${(b / 1e3).toFixed(2)} KB`;
    return `${b} B`;
}
function formatUptime(s: number) {
    const d = Math.floor(s / 86400), h = Math.floor((s % 86400) / 3600),
        m = Math.floor((s % 3600) / 60), sec = s % 60;
    return [d && `${d}d`, h && `${h}h`, m && `${m}m`, `${sec}s`].filter(Boolean).join(' ');
}

const inputStyle: React.CSSProperties = {
    background: '#313244', border: '1px solid #45475a', color: '#cdd6f4',
    borderRadius: 4, padding: '4px 8px', fontSize: 14,
};

function KpiCard({ label, value }: { label: string; value: string }) {
    return (
        <div style={{ background: '#313244', borderRadius: 6, padding: '12px 16px', minWidth: 160 }}>
            <div style={{ fontSize: 12, color: '#6c7086', marginBottom: 4, textTransform: 'uppercase', letterSpacing: 0.8 }}>
                {label}
            </div>
            <div style={{ fontSize: 22, color: '#cdd6f4', fontVariantNumeric: 'tabular-nums' }}>
                {value}
            </div>
        </div>
    );
}

export default function MetricsSettingsPanel() {
    const [cfg, setCfg]           = useState<MetricsConfig | null>(null);
    const [live, setLive]         = useState<LiveMetrics | null>(null);
    const [saving, setSaving]     = useState(false);
    const [message, setMsg]       = useState('');

    // Config load
    useEffect(() => { configClient.getMetrics().then(r => r.data && setCfg(r.data)); }, []);

    // Live metrics polling (5 s)
    const refreshLive = useCallback(() => {
        configClient.getLiveMetrics().then(r => r.data && setLive(r.data));
    }, []);
    useEffect(() => {
        refreshLive();
        const id = setInterval(refreshLive, 5000);
        return () => clearInterval(id);
    }, [refreshLive]);

    const save = async () => {
        if (!cfg) return;
        setSaving(true); setMsg('');
        const r = await configClient.putMetrics(cfg);
        setSaving(false);
        setMsg(r.error ? `Error: ${r.error}` : 'Saved.');
    };

    return (
        <div style={{ maxWidth: 640 }}>
            <h2 style={{ marginBottom: 6, color: '#89b4fa' }}>Metrics</h2>

            {/* ── Live metrics ──────────────────────────────────── */}
            <h3 style={{ fontSize: 13, textTransform: 'uppercase', letterSpacing: 1,
                color: '#6c7086', marginBottom: 12, marginTop: 4 }}>Live</h3>
            {live ? (
                <div style={{ display: 'flex', flexWrap: 'wrap', gap: 10, marginBottom: 28 }}>
                    <KpiCard label="Uptime"          value={formatUptime(live.uptime_seconds)} />
                    <KpiCard label="Active Sessions" value={String(live.active_sessions)} />
                    <KpiCard label="Total Sessions"  value={String(live.total_sessions)} />
                    <KpiCard label="Peak Sessions"   value={String(live.peak_sessions)} />
                    <KpiCard label="Data Streamed"   value={formatBytes(live.bytes_streamed)} />
                    <KpiCard label="Active Streams"  value={String(live.active_streams)} />
                    <KpiCard label="RPS"             value={live.rps.toFixed(1)} />
                </div>
            ) : (
                <p style={{ fontSize: 13, color: '#6c7086', marginBottom: 28 }}>
                    Loading live metrics…
                </p>
            )}

            {/* ── Logging config ────────────────────────────────── */}
            <h3 style={{ fontSize: 13, textTransform: 'uppercase', letterSpacing: 1,
                color: '#6c7086', marginBottom: 12 }}>Metrics Log Configuration</h3>
            <p style={{ fontSize: 13, color: '#6c7086', marginBottom: 16 }}>
                When enabled, the server writes per-session metrics to rotating log files
                (e.g. <code style={{ background: '#313244', padding: '1px 4px', borderRadius: 3 }}>
                metrics_session_from_1714000000_1.log
            </code>).
            </p>

            {cfg ? (
                <>
                    <label style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 16 }}>
                        <input type="checkbox" checked={cfg.enabled}
                               onChange={e => setCfg({ ...cfg, enabled: e.target.checked })} />
                        <span style={{ fontSize: 14 }}>Enable metrics logging</span>
                    </label>

                    {cfg.enabled && (
                        <>
                            <label style={{ display: 'block', marginBottom: 14 }}>
                                <span style={{ display: 'block', marginBottom: 4, fontSize: 14 }}>
                                    Metrics log directory / path
                                </span>
                                <input value={cfg.metrics_log_path}
                                       onChange={e => setCfg({ ...cfg, metrics_log_path: e.target.value })}
                                       style={{ ...inputStyle, width: 300 }} />
                            </label>

                            <label style={{ display: 'block', marginBottom: 20 }}>
                                <span style={{ display: 'block', marginBottom: 4, fontSize: 14 }}>
                                    Log file name prefix
                                    <span style={{ marginLeft: 8, fontSize: 12, color: '#6c7086' }}>
                                        + server start timestamp is appended
                                    </span>
                                </span>
                                <input value={cfg.metrics_log_file_prefix}
                                       onChange={e => setCfg({ ...cfg, metrics_log_file_prefix: e.target.value })}
                                       style={{ ...inputStyle, width: 300 }} />
                            </label>
                        </>
                    )}

                    <div style={{ display: 'flex', gap: 12, alignItems: 'center' }}>
                        <button onClick={save} disabled={saving} style={saveBtn}>
                            {saving ? 'Saving…' : 'Save'}
                        </button>
                        {message && (
                            <span style={{ fontSize: 13, color: message.startsWith('Error') ? '#f38ba8' : '#a6e3a1' }}>
                                {message}
                            </span>
                        )}
                    </div>
                </>
            ) : (
                <p style={{ fontSize: 13, color: '#6c7086' }}>Loading config…</p>
            )}
        </div>
    );
}

const saveBtn: React.CSSProperties = {
    background: '#89b4fa', color: '#1e1e2e', border: 'none',
    padding: '6px 20px', borderRadius: 4, cursor: 'pointer', fontSize: 14, fontWeight: 600,
};
