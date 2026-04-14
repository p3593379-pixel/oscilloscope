// FILE: control_panel_web/src/features/auth/AuthSettingsPanel.tsx
import { useEffect, useState } from 'react';
import { configClient, type AuthConfig } from 'api/configClient';

function generateSecret(): string {
  const arr = new Uint8Array(32);
  crypto.getRandomValues(arr);
  return Array.from(arr).map(b => b.toString(16).padStart(2, '0')).join('');
}

export default function AuthSettingsPanel() {
  const [cfg, setCfg] = useState<AuthConfig | null>(null);
  const [saving, setSaving] = useState(false);
  const [message, setMessage] = useState('');
  const [showSecretModal, setShowSecretModal] = useState(false);
  const [newSecret, setNewSecret] = useState('');
  const [secretError, setSecretError] = useState('');

  useEffect(() => {
    configClient.getAuth().then(r => r.data && setCfg(r.data));
  }, []);

  if (!cfg) return <p>Loading…</p>;

  const save = async () => {
    setSaving(true); setMessage('');
    const r = await configClient.putAuth(cfg);
    setSaving(false);
    setMessage(r.error ? `Error: ${r.error}` : 'Saved.');
  };

  const applySecret = async () => {
    if (newSecret.length < 32) { setSecretError('Secret must be at least 32 characters.'); return; }
    const r = await fetch('/api/config/auth', {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ ...cfg, jwt_secret: newSecret }),
    });
    if (r.ok) { setShowSecretModal(false); setNewSecret(''); setSecretError(''); setMessage('JWT secret updated.'); }
    else setSecretError('Failed to update secret.');
  };

  return (
    <div>
      <h2>Auth Settings</h2>
      <label style={{ display: 'block', marginBottom: 10 }}>
        Access Token TTL (seconds)
        <input type="number" value={cfg.access_token_ttl_seconds}
          onChange={e => setCfg({ ...cfg, access_token_ttl_seconds: Number(e.target.value) })}
          style={{ marginLeft: 8, width: 100 }} />
      </label>
      <label style={{ display: 'block', marginBottom: 10 }}>
        Refresh Token TTL (seconds)
        <input type="number" value={cfg.refresh_token_ttl_seconds}
          onChange={e => setCfg({ ...cfg, refresh_token_ttl_seconds: Number(e.target.value) })}
          style={{ marginLeft: 8, width: 120 }} />
        <span style={{ marginLeft: 8, color: '#6b7280', fontSize: 13 }}>
          ({(cfg.refresh_token_ttl_seconds / 86400).toFixed(1)} days)
        </span>
      </label>
      <label style={{ display: 'block', marginBottom: 10 }}>
        Stream Token TTL (seconds)
        <input type="number" value={cfg.stream_token_ttl_seconds}
          onChange={e => setCfg({ ...cfg, stream_token_ttl_seconds: Number(e.target.value) })}
          style={{ marginLeft: 8, width: 100 }} />
      </label>
      <div style={{ marginBottom: 16 }}>
        <button onClick={() => { setNewSecret(generateSecret()); setSecretError(''); setShowSecretModal(true); }}
          style={{ padding: '6px 14px', cursor: 'pointer' }}>
          Change JWT Secret…
        </button>
        <span style={{ marginLeft: 10, fontSize: 13, color: '#6b7280' }}>
          JWT secret is write-only and never displayed by the API.
        </span>
      </div>
      <div style={{ display: 'flex', gap: 10, alignItems: 'center' }}>
        <button onClick={save} disabled={saving} style={{ padding: '6px 18px', cursor: 'pointer' }}>
          {saving ? 'Saving…' : 'Save'}
        </button>
        {message && <span style={{ fontSize: 13, color: '#166534' }}>{message}</span>}
      </div>

      {showSecretModal && (
        <div style={{ position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.4)', display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 100 }}>
          <div style={{ background: '#fff', padding: 24, borderRadius: 8, width: 480 }}>
            <h3 style={{ marginTop: 0 }}>Change JWT Secret</h3>
            <p style={{ fontSize: 13, color: '#6b7280' }}>
              Enter a new secret (min 32 chars) or use the generated value.
            </p>
            <textarea value={newSecret} onChange={e => setNewSecret(e.target.value)}
              rows={3} style={{ width: '100%', fontFamily: 'monospace', fontSize: 13 }} />
            {secretError && <p style={{ color: '#dc2626', fontSize: 13 }}>{secretError}</p>}
            <div style={{ display: 'flex', gap: 8, marginTop: 12 }}>
              <button onClick={() => setNewSecret(generateSecret())}
                style={{ padding: '5px 12px', cursor: 'pointer' }}>
                Regenerate
              </button>
              <button onClick={applySecret}
                style={{ padding: '5px 14px', background: '#1d4ed8', color: '#fff', border: 'none', borderRadius: 4, cursor: 'pointer' }}>
                Apply
              </button>
              <button onClick={() => setShowSecretModal(false)}
                style={{ padding: '5px 12px', cursor: 'pointer' }}>
                Cancel
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
