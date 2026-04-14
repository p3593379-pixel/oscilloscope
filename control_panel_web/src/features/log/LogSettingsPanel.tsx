// FILE: control_panel_web/src/features/log/LogSettingsPanel.tsx
import { useEffect, useState } from 'react';
import { configClient, type LogConfig } from 'api/configClient';

const LOG_LEVELS      = ['trace', 'debug', 'info', 'warn', 'error'];
const LOG_DESTINATIONS = ['stdout', 'file', 'both'];

export default function LogSettingsPanel() {
  const [cfg, setCfg] = useState<LogConfig | null>(null);
  const [saving, setSaving] = useState(false);
  const [message, setMessage] = useState('');

  useEffect(() => { configClient.getLog().then(r => r.data && setCfg(r.data)); }, []);
  if (!cfg) return <p>Loading…</p>;

  const save = async () => {
    setSaving(true); setMessage('');
    const r = await configClient.putLog(cfg);
    setSaving(false);
    setMessage(r.error ? `Error: ${r.error}` : 'Applied immediately.');
  };

  return (
    <div>
      <h2>Log Settings</h2>
      <p style={{ fontSize: 13, color: '#6b7280', marginBottom: 12 }}>
        Log settings are applied immediately without restart.
      </p>
      <label style={{ display: 'block', marginBottom: 10 }}>
        Log level
        <select value={cfg.level}
          onChange={e => setCfg({ ...cfg, level: e.target.value })}
          style={{ marginLeft: 8 }}>
          {LOG_LEVELS.map(l => <option key={l}>{l}</option>)}
        </select>
      </label>
      <label style={{ display: 'block', marginBottom: 10 }}>
        Destination
        <select value={cfg.destination}
          onChange={e => setCfg({ ...cfg, destination: e.target.value })}
          style={{ marginLeft: 8 }}>
          {LOG_DESTINATIONS.map(d => <option key={d}>{d}</option>)}
        </select>
      </label>
      {(cfg.destination === 'file' || cfg.destination === 'both') && (
        <>
          <label style={{ display: 'block', marginBottom: 10 }}>
            File path
            <input value={cfg.file_path}
              onChange={e => setCfg({ ...cfg, file_path: e.target.value })}
              style={{ marginLeft: 8, width: 280 }} />
          </label>
          <label style={{ display: 'block', marginBottom: 10 }}>
            Max file size (MB)
            <input type="number" value={cfg.max_size_mb}
              onChange={e => setCfg({ ...cfg, max_size_mb: Number(e.target.value) })}
              style={{ marginLeft: 8, width: 80 }} />
          </label>
          <label style={{ display: 'block', marginBottom: 10 }}>
            Max files (rotation count)
            <input type="number" value={cfg.max_files}
              onChange={e => setCfg({ ...cfg, max_files: Number(e.target.value) })}
              style={{ marginLeft: 8, width: 80 }} />
          </label>
        </>
      )}
      <label style={{ display: 'flex', gap: 8, alignItems: 'center', marginBottom: 16 }}>
        <input type="checkbox" checked={cfg.access_log}
          onChange={e => setCfg({ ...cfg, access_log: e.target.checked })} />
        Access log
      </label>
      <div style={{ display: 'flex', gap: 10, alignItems: 'center' }}>
        <button onClick={save} disabled={saving} style={{ padding: '6px 18px', cursor: 'pointer' }}>
          {saving ? 'Applying…' : 'Apply'}
        </button>
        {message && <span style={{ fontSize: 13, color: '#166534' }}>{message}</span>}
      </div>
    </div>
  );
}
