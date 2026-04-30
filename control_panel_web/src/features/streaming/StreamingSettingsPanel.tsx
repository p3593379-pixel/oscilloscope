// FILE: control_panel_web/src/features/streaming/StreamingSettingsPanel.tsx
import { useEffect, useState } from 'react';
import { configClient, type StreamingConfig } from 'api/configClient';

const ALGORITHMS = ['zstd', 'lz4', 'snappy', 'gzip'];

const input: React.CSSProperties = {
    background: '#313244', border: '1px solid #45475a', color: '#cdd6f4',
    borderRadius: 4, padding: '4px 8px', fontSize: 14,
};

export default function StreamingSettingsPanel() {
    const [cfg, setCfg]       = useState<StreamingConfig | null>(null);
    const [saving, setSaving] = useState(false);
    const [message, setMsg]   = useState('');

    useEffect(() => { configClient.getStreaming().then(r => r.data && setCfg(r.data)); }, []);
    if (!cfg) return <p style={{ color: '#a6adc8' }}>Loading…</p>;

    const save = async () => {
        setSaving(true); setMsg('');
        const r = await configClient.putStreaming(cfg);
        setSaving(false);
        setMsg(r.error ? `Error: ${r.error}` : 'Saved.');
    };

    return (
        <div style={{ maxWidth: 480 }}>
            <h2 style={{ marginBottom: 6, color: '#89b4fa' }}>Streaming</h2>
            <p style={{ fontSize: 13, color: '#6c7086', marginBottom: 24 }}>
                Data-plane compression settings. More options (frame size, backpressure, TLS) will be added in a future release.
            </p>

            <label style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 16 }}>
                <input type="checkbox" checked={cfg.compression_enabled}
                       onChange={e => setCfg({ ...cfg, compression_enabled: e.target.checked })} />
                <span style={{ fontSize: 14 }}>Enable compression</span>
            </label>

            {cfg.compression_enabled && (
                <label style={{ display: 'block', marginBottom: 16 }}>
                    <span style={{ display: 'block', marginBottom: 4, fontSize: 14 }}>Algorithm</span>
                    <select value={cfg.compression_algorithm}
                            onChange={e => setCfg({ ...cfg, compression_algorithm: e.target.value })}
                            style={{ ...input, width: 160 }}>
                        {ALGORITHMS.map(a => <option key={a}>{a}</option>)}
                    </select>
                </label>
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
        </div>
    );
}

const saveBtn: React.CSSProperties = {
    background: '#89b4fa', color: '#1e1e2e', border: 'none',
    padding: '6px 20px', borderRadius: 4, cursor: 'pointer', fontSize: 14, fontWeight: 600,
};
