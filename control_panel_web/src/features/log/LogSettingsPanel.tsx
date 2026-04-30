// FILE: control_panel_web/src/features/log/LogSettingsPanel.tsx
import { useEffect, useState } from 'react';
import { configClient, type LogConfig } from 'api/configClient';

const LOG_LEVELS = ['trace', 'debug', 'info', 'warn', 'error', 'critical', 'off'];

const input: React.CSSProperties = {
    background: '#313244', border: '1px solid #45475a', color: '#cdd6f4',
    borderRadius: 4, padding: '4px 8px', fontSize: 14,
};

export default function LogSettingsPanel() {
    const [cfg, setCfg]       = useState<LogConfig | null>(null);
    const [saving, setSaving] = useState(false);
    const [message, setMsg]   = useState('');

    useEffect(() => { configClient.getLog().then(r => r.data && setCfg(r.data)); }, []);
    if (!cfg) return <p style={{ color: '#a6adc8' }}>Loading…</p>;

    const save = async () => {
        setSaving(true); setMsg('');
        const r = await configClient.putLog(cfg);
        setSaving(false);
        setMsg(r.error ? `Error: ${r.error}` : 'Applied immediately.');
    };

    return (
        <div style={{ maxWidth: 480 }}>
            <h2 style={{ marginBottom: 6, color: '#89b4fa' }}>Logging</h2>
            <p style={{ fontSize: 13, color: '#6c7086', marginBottom: 24 }}>
                Log level changes are applied immediately without restart.
            </p>

            <label style={{ display: 'block', marginBottom: 16 }}>
                <span style={{ display: 'block', marginBottom: 4, fontSize: 14 }}>Log level</span>
                <select value={cfg.level}
                        onChange={e => setCfg({ ...cfg, level: e.target.value })}
                        style={{ ...input, width: 160 }}>
                    {LOG_LEVELS.map(l => <option key={l}>{l}</option>)}
                </select>
            </label>

            <label style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 16 }}>
                <input type="checkbox" checked={cfg.console}
                       onChange={e => setCfg({ ...cfg, console: e.target.checked })} />
                <span style={{ fontSize: 14 }}>Log to console (stdout)</span>
            </label>

            <label style={{ display: 'block', marginBottom: 16 }}>
                <span style={{ display: 'block', marginBottom: 4, fontSize: 14 }}>
                    Log file name prefix
                    <span style={{ marginLeft: 8, fontSize: 12, color: '#6c7086' }}>
                        e.g. buf_connect_server → buf_connect_server_1.log
                    </span>
                </span>
                <input value={cfg.log_file_name_prefix}
                       onChange={e => setCfg({ ...cfg, log_file_name_prefix: e.target.value })}
                       style={{ ...input, width: 300 }} />
            </label>

            <div style={{ display: 'flex', gap: 16, marginBottom: 16 }}>
                <label style={{ display: 'block' }}>
                    <span style={{ display: 'block', marginBottom: 4, fontSize: 14 }}>Max file size (MB)</span>
                    <input type="number" value={cfg.max_size_mb}
                           onChange={e => setCfg({ ...cfg, max_size_mb: Number(e.target.value) })}
                           style={{ ...input, width: 100 }} />
                </label>
                <label style={{ display: 'block' }}>
                    <span style={{ display: 'block', marginBottom: 4, fontSize: 14 }}>Max files (rotation)</span>
                    <input type="number" value={cfg.max_files}
                           onChange={e => setCfg({ ...cfg, max_files: Number(e.target.value) })}
                           style={{ ...input, width: 100 }} />
                </label>
            </div>

            <div style={{ display: 'flex', gap: 12, alignItems: 'center' }}>
                <button onClick={save} disabled={saving} style={saveBtn}>
                    {saving ? 'Applying…' : 'Apply'}
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
