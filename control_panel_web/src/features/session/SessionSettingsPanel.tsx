// FILE: control_panel_web/src/features/session/SessionSettingsPanel.tsx
import { useEffect, useState } from 'react';
import { configClient, type SessionConfig } from 'api/configClient';

export default function SessionSettingsPanel() {
  const [cfg, setCfg] = useState<SessionConfig | null>(null);
  const [saving, setSaving] = useState(false);
  const [message, setMessage] = useState('');

  useEffect(() => { configClient.getSession().then(r => r.data && setCfg(r.data)); }, []);
  if (!cfg) return <p>Loading…</p>;

  const save = async () => {
    setSaving(true); setMessage('');
    const r = await configClient.putSession(cfg);
    setSaving(false);
    setMessage(r.error ? `Error: ${r.error}` : 'Saved.');
  };

  return (
    <div>
      <h2>Session Settings</h2>
      <label style={{ display: 'block', marginBottom: 10 }}>
        Admin conflict timeout (seconds)
        <input type="number" value={cfg.admin_conflict_timeout_seconds}
          onChange={e => setCfg({ ...cfg, admin_conflict_timeout_seconds: Number(e.target.value) })}
          style={{ marginLeft: 8, width: 100 }} />
      </label>
      <label style={{ display: 'block', marginBottom: 10 }}>
        Snooze duration (seconds)
        <input type="number" value={cfg.snooze_duration_seconds}
          onChange={e => setCfg({ ...cfg, snooze_duration_seconds: Number(e.target.value) })}
          style={{ marginLeft: 8, width: 100 }} />
      </label>
      <label style={{ display: 'block', marginBottom: 16 }}>
        Max concurrent sessions (0 = unlimited)
        <input type="number" value={cfg.max_concurrent_sessions}
          onChange={e => setCfg({ ...cfg, max_concurrent_sessions: Number(e.target.value) })}
          style={{ marginLeft: 8, width: 100 }} />
      </label>
      <div style={{ display: 'flex', gap: 10, alignItems: 'center' }}>
        <button onClick={save} disabled={saving} style={{ padding: '6px 18px', cursor: 'pointer' }}>
          {saving ? 'Saving…' : 'Save'}
        </button>
        {message && <span style={{ fontSize: 13, color: '#166534' }}>{message}</span>}
      </div>
    </div>
  );
}
