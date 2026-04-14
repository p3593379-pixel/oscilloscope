// FILE: control_panel_web/src/features/network/NetworkSettingsPanel.tsx
import { useEffect, useState } from 'react';
import { configClient, type NetworkConfig, type InterfaceConfig } from 'api/configClient';

const TLS_VERSIONS = ['TLSv1.2', 'TLSv1.3'];

function InterfaceForm({
  label, value, onChange, disabled = false,
}: {
  label: string;
  value: InterfaceConfig;
  onChange: (v: InterfaceConfig) => void;
  disabled?: boolean;
}) {
  const set = (patch: Partial<InterfaceConfig>) => onChange({ ...value, ...patch });
  const setTls = (patch: Partial<InterfaceConfig['tls']>) =>
    set({ tls: { ...value.tls, ...patch } });

  return (
    <fieldset style={{ border: '1px solid #ccc', padding: 12, marginBottom: 12, borderRadius: 6 }}>
      <legend><strong>{label}</strong></legend>
      {label === 'Data Plane' && (
        <label style={{ display: 'flex', gap: 8, alignItems: 'center', marginBottom: 8 }}>
          <input type="checkbox" checked={value.enabled}
            onChange={e => set({ enabled: e.target.checked })} disabled={disabled} />
          Enabled
        </label>
      )}
      <label style={{ display: 'block', marginBottom: 6 }}>
        Bind Address
        <input value={value.bind_address} disabled={disabled || !value.enabled}
          onChange={e => set({ bind_address: e.target.value })}
          style={{ marginLeft: 8, padding: '2px 6px' }} />
      </label>
      <label style={{ display: 'block', marginBottom: 6 }}>
        Port
        <input type="number" value={value.port} disabled={disabled || !value.enabled}
          onChange={e => set({ port: Number(e.target.value) })}
          style={{ marginLeft: 8, width: 80, padding: '2px 6px' }} />
      </label>
      <label style={{ display: 'flex', gap: 8, alignItems: 'center', marginBottom: 6 }}>
        <input type="checkbox" checked={value.tls.enabled}
          disabled={disabled || !value.enabled}
          onChange={e => setTls({ enabled: e.target.checked })} />
        TLS
      </label>
      {value.tls.enabled && (
        <>
          <label style={{ display: 'block', marginBottom: 6 }}>
            Cert Path
            <input value={value.tls.cert_path} disabled={disabled || !value.enabled}
              onChange={e => setTls({ cert_path: e.target.value })}
              style={{ marginLeft: 8, width: 260, padding: '2px 6px' }} />
          </label>
          <label style={{ display: 'block', marginBottom: 6 }}>
            Key Path
            <input value={value.tls.key_path} disabled={disabled || !value.enabled}
              onChange={e => setTls({ key_path: e.target.value })}
              style={{ marginLeft: 8, width: 260, padding: '2px 6px' }} />
          </label>
          <label style={{ display: 'block', marginBottom: 6 }}>
            Min TLS Version
            <select value={value.tls.min_version} disabled={disabled || !value.enabled}
              onChange={e => setTls({ min_version: e.target.value })}
              style={{ marginLeft: 8 }}>
              {TLS_VERSIONS.map(v => <option key={v}>{v}</option>)}
            </select>
          </label>
        </>
      )}
    </fieldset>
  );
}

export default function NetworkSettingsPanel() {
  const [cfg, setCfg] = useState<NetworkConfig | null>(null);
  const [saving, setSaving] = useState(false);
  const [message, setMessage] = useState('');
  const [restartNeeded, setRestartNeeded] = useState(false);

  useEffect(() => {
    configClient.getNetwork().then(r => r.data && setCfg(r.data));
  }, []);

  if (!cfg) return <p>Loading…</p>;

    const save = async () => {
        setSaving(true);
        const r = await configClient.putNetwork(cfg);
        if (r.error) {
            setMessage(`Error: ${r.error}`);
        } else if (r.data?.restart_required) {
            setRestartNeeded(true);
            setMessage('Saved. Restart required.');
        } else {
            setMessage('Saved.');
        }
        setSaving(false);
        setTimeout(() => setMessage(''), 4000);
    };

    const restart = async () => {
    await configClient.restart();
    setRestartNeeded(false);
    setMessage('Restart requested.');
  };

  return (
    <div>
      <h2>Network Settings</h2>
      <label style={{ display: 'flex', gap: 8, alignItems: 'center', marginBottom: 12 }}>
        <input type="checkbox" checked={cfg.single_interface_mode}
          onChange={e => setCfg({ ...cfg, single_interface_mode: e.target.checked })} />
        Single-interface mode
        {cfg.single_interface_mode && (
          <span style={{ color: '#b45309', fontSize: 13 }}>
            ⚠ Data streaming shares control plane bandwidth
          </span>
        )}
      </label>
      <InterfaceForm label="Control Plane" value={cfg.control_plane}
        onChange={v => setCfg({ ...cfg, control_plane: v })} />
      <InterfaceForm label="Data Plane" value={cfg.data_plane}
        onChange={v => setCfg({ ...cfg, data_plane: v })}
        disabled={cfg.single_interface_mode} />
      <div style={{ display: 'flex', gap: 10, alignItems: 'center' }}>
        <button onClick={save} disabled={saving}
          style={{ padding: '6px 18px', cursor: 'pointer' }}>
          {saving ? 'Saving…' : 'Save'}
        </button>
        {restartNeeded && (
          <button onClick={restart}
            style={{ padding: '6px 18px', background: '#b45309', color: '#fff', cursor: 'pointer', border: 'none', borderRadius: 4 }}>
            Restart Server
          </button>
        )}
        {message && <span style={{ fontSize: 13, color: restartNeeded ? '#b45309' : '#166534' }}>{message}</span>}
      </div>
    </div>
  );
}
