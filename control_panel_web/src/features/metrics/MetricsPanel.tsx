// FILE: control_panel_web/src/features/metrics/MetricsPanel.tsx
import { useEffect, useState, useCallback } from 'react';
import { configClient, type MetricsConfig, type LiveMetrics } from 'api/configClient';
import { formatUptime, formatBytes } from 'api/statusClient';
import {TogglePill} from "../../components/TogglePill.tsx";

const inp: React.CSSProperties = {
    background: '#313244', border: '1px solid #45475a', color: '#cdd6f4',
    borderRadius: 4, padding: '4px 8px', fontSize: 14,
};
const saveBtn: React.CSSProperties = {
    background: '#89b4fa', color: '#1e1e2e', border: 'none',
    padding: '6px 20px', borderRadius: 4, cursor: 'pointer', fontSize: 14, fontWeight: 600,
};
const secLabel: React.CSSProperties = {
    fontSize: 13, textTransform: 'uppercase' as const,
    letterSpacing: 1, color: '#6c7086', marginBottom: 12,
};

function KpiCard({ label, value, sub }: { label: string; value: string; sub?: string }) {
    return (
        <div style={{ background: '#313244', borderRadius: 6, padding: '14px 18px', minWidth: 150 }}>
            <div style={{ fontSize: 11, color: '#6c7086', textTransform: 'uppercase', letterSpacing: 0.8, marginBottom: 6 }}>
                {label}
            </div>
            <div style={{ fontSize: 22, color: '#cdd6f4', fontVariantNumeric: 'tabular-nums', lineHeight: 1 }}>
                {value}
            </div>
            {sub && <div style={{ fontSize: 12, color: '#6c7086', marginTop: 4 }}>{sub}</div>}
        </div>
    );
}

export default function MetricsPanel() {
    const [live,    setLive]    = useState<LiveMetrics | null>(null);
    const [cfg,     setCfg]     = useState<MetricsConfig | null>(null);
    const [saving,  setSaving]  = useState(false);
    const [msg,     setMsg]     = useState('');
    const [lastRef, setLastRef] = useState<Date | null>(null);

    const refreshLive = useCallback(() => {
        configClient.getLiveMetrics().then(r => {
            if (r.data) { setLive(r.data); setLastRef(new Date()); }
        });
    }, []);

    useEffect(() => {
        refreshLive();
        const id = setInterval(refreshLive, 5000);
        return () => clearInterval(id);
    }, [refreshLive]);

    useEffect(() => { configClient.getMetrics().then(r => r.data && setCfg(r.data)); }, []);

    const save = async () => {
        if (!cfg) return;
        setSaving(true); setMsg('');
        const r = await configClient.putMetrics(cfg);
        setSaving(false);
        setMsg(r.error ? `Error: ${r.error}` : 'Saved.');
    };

    return (
        <div style={{ maxWidth: 680 }}>
            <div style={{ display: 'flex', alignItems: 'baseline', gap: 16, marginBottom: 20 }}>
                <h2 style={{ color: '#89b4fa', margin: 0 }}>Metrics</h2>
                {lastRef && (
                    <span style={{ fontSize: 12, color: '#6c7086' }}>
                        Last updated {lastRef.toLocaleTimeString()}
                    </span>
                )}
            </div>

            {/* ── Live counters ── */}
            <h3 style={{ ...secLabel, marginTop: 0 }}>Live</h3>
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
                <p style={{ fontSize: 13, color: '#6c7086', marginBottom: 28 }}>Loading live metrics…</p>
            )}

            <button onClick={refreshLive}
                    style={{ background: '#313244', color: '#cdd6f4', border: '1px solid #45475a',
                        padding: '5px 14px', borderRadius: 4, cursor: 'pointer', fontSize: 13, marginBottom: 32 }}>
                ↻ Refresh
            </button>

            <hr style={{ border: 'none', borderTop: '1px solid #313244', marginBottom: 28 }} />

            {/* ── Metrics logging config ── */}
            <h3 style={secLabel}>Metrics Log Configuration</h3>
            <p style={{ fontSize: 13, color: '#6c7086', marginBottom: 16 }}>
                When enabled, the server writes per-session metrics to rotating log files (e.g.{' '}
                <code style={{ background: '#313244', padding: '1px 4px', borderRadius: 3 }}>
                    metrics_session_from_1714000000_1.log
                </code>).
            </p>

            {cfg ? (
                <>
                    <label style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 16 }}>
                        <TogglePill
                            id="metrics-log-toggle"
                            checked={cfg.enabled}
                            onChange={v => setCfg({ ...cfg, enabled: v })}
                            label="Enable metrics logging"
                            description="Writes per-session counters to rotating files, e.g. metrics_session_from_1714000000_1.log"
                        />
                    </label>

                    <div style={{
                        opacity: cfg.enabled ? 1 : 0.4,
                        pointerEvents: cfg.enabled ? 'auto' : 'none'
                    }}>
                        <label style={{ display: 'block', marginBottom: 14 }}>
                            <span style={{ display: 'block', fontSize: 14, marginBottom: 4 }}>
                                Metrics log directory / path
                            </span>
                            <input value={cfg.metrics_log_path}
                                   onChange={e => setCfg({ ...cfg, metrics_log_path: e.target.value })}
                                   style={{ ...inp, width: 320 }} />
                        </label>
                        <label style={{ display: 'block', marginBottom: 20 }}>
                            <span style={{ display: 'block', fontSize: 14, marginBottom: 4 }}>
                                Log file name prefix
                                <span style={{ marginLeft: 8, fontSize: 12, color: '#6c7086' }}>
                                    (server start timestamp appended automatically)
                                </span>
                            </span>
                            <input value={cfg.metrics_log_file_prefix}
                                   onChange={e => setCfg({ ...cfg, metrics_log_file_prefix: e.target.value })}
                                   style={{ ...inp, width: 320 }} />
                        </label>
                    </div>

                    <div style={{ display: 'flex', gap: 12, alignItems: 'center' }}>
                        <button onClick={save} disabled={saving} style={saveBtn}>
                            {saving ? 'Saving…' : 'Save'}
                        </button>
                        {msg && (
                            <span style={{ fontSize: 13, color: msg.startsWith('Error') ? '#f38ba8' : '#a6e3a1' }}>
                                {msg}
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
