// FILE: control_panel_web/src/features/metrics/MetricsSettingsPanel.tsx
import { useEffect, useState } from 'react';
import { configClient, type MetricsConfig } from 'api/configClient';

export default function MetricsSettingsPanel() {
  const [cfg, setCfg] = useState<MetricsConfig | null>(null);
  const [saving, setSaving] = useState(false);
  const [message, setMessage] = useState('');

  useEffect(() => { configClient.getMetrics().then(r => r.data && setCfg(r.data)); }, []);
  if (!cfg) return <p>Loading…</p>;

  const save = async () => {
    setSaving(true); setMessage('');
    const r = await configClient.putMetrics(cfg);
    setSaving(false);
    setMessage(r.error ? `Error: ${r.error}` : 'Saved.');
  };

  return (
    <div>
      <h2>Metrics Settings</h2>
      <label style={{ display: 'flex', gap: 8, alignItems: 'center', marginBottom: 10 }}>
        <input type="checkbox" checked={cfg.enabled}
          onChange={e => setCfg({ ...cfg, enabled: e.target.checked })} />
        Prometheus metrics endpoint enabled
      </label>
      {cfg.enabled && (
        <label style={{ display: 'block', marginBottom: 16 }}>
          Metrics path
          <input value={cfg.path}
            onChange={e => setCfg({ ...cfg, path: e.target.value })}
            style={{ marginLeft: 8, width: 200 }} />
        </label>
      )}
      <div style={{ display: 'flex', gap: 10, alignItems: 'center' }}>
        <button onClick={save} disabled={saving} style={{ padding: '6px 18px', cursor: 'pointer' }}>
          {saving ? 'Saving…' : 'Save'}
        </button>
        {message && <span style={{ fontSize: 13, color: '#166534' }}>{message}</span>}
      </div>
    </div>
  );
}
