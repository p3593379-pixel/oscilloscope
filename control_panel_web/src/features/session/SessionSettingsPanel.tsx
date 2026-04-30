// FILE: control_panel_web/src/features/session/SessionSettingsPanel.tsx
import { useEffect, useState } from 'react';
import { configClient, type SessionConfig } from 'api/configClient';

function Row({ label, hint, children }: { label: string; hint?: string; children: React.ReactNode }) {
    return (
        <div style={{ marginBottom: 16 }}>
            <label style={{ display: 'block', marginBottom: 4, fontSize: 14, color: '#cdd6f4' }}>
                {label}
                {hint && <span style={{ marginLeft: 8, fontSize: 12, color: '#6c7086' }}>{hint}</span>}
            </label>
            {children}
        </div>
    );
}

const input: React.CSSProperties = {
    background: '#313244', border: '1px solid #45475a', color: '#cdd6f4',
    borderRadius: 4, padding: '4px 8px', width: 120, fontSize: 14,
};

export default function SessionSettingsPanel() {
    const [cfg, setCfg]       = useState<SessionConfig | null>(null);
    const [saving, setSaving] = useState(false);
    const [message, setMsg]   = useState('');

    useEffect(() => { configClient.getSession().then(r => r.data && setCfg(r.data)); }, []);
    if (!cfg) return <p style={{ color: '#a6adc8' }}>Loading…</p>;

    const set = (patch: Partial<SessionConfig>) => setCfg(c => c ? { ...c, ...patch } : c);

    const save = async () => {
        setSaving(true); setMsg('');
        const r = await configClient.putSession(cfg);
        setSaving(false);
        setMsg(r.error ? `Error: ${r.error}` : 'Saved.');
    };

    return (
        <div style={{ maxWidth: 560 }}>
            <h2 style={{ marginBottom: 6, color: '#89b4fa' }}>Session Settings</h2>
            <p style={{ fontSize: 13, color: '#6c7086', marginBottom: 24 }}>
                Controls session lifecycle, concurrency, and token timing.
            </p>

            <h3 style={{ fontSize: 13, textTransform: 'uppercase', letterSpacing: 1,
                color: '#6c7086', marginBottom: 12 }}>Concurrency</h3>

            <Row label="Admin conflict timeout" hint="seconds">
                <input type="number" style={input} value={cfg.admin_conflict_timeout_seconds}
                       onChange={e => set({ admin_conflict_timeout_seconds: Number(e.target.value) })} />
            </Row>
            <Row label="Snooze duration" hint="seconds — pause before kicking idle admin">
                <input type="number" style={input} value={cfg.snooze_duration_seconds}
                       onChange={e => set({ snooze_duration_seconds: Number(e.target.value) })} />
            </Row>
            <Row label="Max concurrent sessions" hint="0 = unlimited">
                <input type="number" style={input} value={cfg.max_concurrent_sessions}
                       onChange={e => set({ max_concurrent_sessions: Number(e.target.value) })} />
            </Row>

            <h3 style={{ fontSize: 13, textTransform: 'uppercase', letterSpacing: 1,
                color: '#6c7086', marginBottom: 12, marginTop: 24 }}>Token & Grace Periods</h3>

            <Row label="Call token renew period" hint="seconds — how often streaming call tokens are reissued">
                <input type="number" style={input} value={cfg.call_token_renew_period}
                       onChange={e => set({ call_token_renew_period: Number(e.target.value) })} />
            </Row>
            <Row label="Grace period — admin" hint="seconds — admin keep-alive window">
                <input type="number" style={input} value={cfg.grace_period_admin_seconds}
                       onChange={e => set({ grace_period_admin_seconds: Number(e.target.value) })} />
            </Row>
            <Row label="Grace period — engineer" hint="seconds — engineer keep-alive window">
                <input type="number" style={input} value={cfg.grace_period_engineer_seconds}
                       onChange={e => set({ grace_period_engineer_seconds: Number(e.target.value) })} />
            </Row>

            <div style={{ display: 'flex', gap: 12, alignItems: 'center', marginTop: 8 }}>
                <button onClick={save} disabled={saving} style={saveBtn}>
                    {saving ? 'Saving…' : 'Save'}
                </button>
                {message && (
                    <span style={{ fontSize: 13, color: message.startsWith('Error') ? '#f38ba8' : '#a6e3a1' }}>
                        {message}
                    </span>
                )}
            </div>
        </div>
    );
}

const saveBtn: React.CSSProperties = {
    background: '#89b4fa', color: '#1e1e2e', border: 'none',
    padding: '6px 20px', borderRadius: 4, cursor: 'pointer', fontSize: 14, fontWeight: 600,
};
