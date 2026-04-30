// FILE: control_panel_web/src/features/network/NetworkSettingsPanel.tsx
import { useEffect, useState } from 'react';
import { configClient, type NetworkConfig, type InterfaceConfig } from 'api/configClient';

const TLS_VERSIONS = ['TLSv1.2', 'TLSv1.3'];

interface IfaceOption { name: string; address: string; status: string; }

const inputStyle: React.CSSProperties = {
    background: '#313244', border: '1px solid #45475a', color: '#cdd6f4',
    borderRadius: 4, padding: '4px 8px', fontSize: 14,
};

function IpPicker({
                      value, onChange, options, disabled,
                  }: { value: string; onChange: (v: string) => void; options: IfaceOption[]; disabled?: boolean }) {
    const [custom, setCustom] = useState(false);
    const presets = [
        { address: '0.0.0.0', name: 'All interfaces (0.0.0.0)' },
        { address: '127.0.0.1', name: 'Loopback (127.0.0.1)' },
        ...options.map(o => ({ address: o.address, name: `${o.name} — ${o.address} [${o.status}]` })),
    ];
    const isPreset = presets.some(p => p.address === value);

    return (
        <div style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
            {!custom ? (
                <select value={isPreset ? value : '__custom__'}
                        disabled={disabled}
                        onChange={e => {
                            if (e.target.value === '__custom__') { setCustom(true); }
                            else onChange(e.target.value);
                        }}
                        style={{ ...inputStyle, width: 300 }}>
                    {presets.map(p => (
                        <option key={p.address} value={p.address}>{p.name}</option>
                    ))}
                    {!isPreset && <option value={value}>{value} (current)</option>}
                    <option value="__custom__">Custom…</option>
                </select>
            ) : (
                <input autoFocus value={value} disabled={disabled}
                       onChange={e => onChange(e.target.value)}
                       style={{ ...inputStyle, width: 200 }}
                       placeholder="e.g. 192.168.1.10" />
            )}
            <button onClick={() => setCustom(c => !c)} disabled={disabled} style={{
                background: 'transparent', border: '1px solid #45475a', color: '#a6adc8',
                borderRadius: 4, padding: '3px 8px', cursor: 'pointer', fontSize: 12,
            }}>{custom ? '↩ pick' : '✎ type'}</button>
        </div>
    );
}

function InterfaceForm({
                           label, value, onChange, disabled, ifaceOptions,
                       }: {
    label: string; value: InterfaceConfig;
    onChange: (v: InterfaceConfig) => void;
    disabled?: boolean;
    ifaceOptions: IfaceOption[];
}) {
    const set    = (p: Partial<InterfaceConfig>) => onChange({ ...value, ...p });
    const setTls = (p: Partial<InterfaceConfig['tls']>) => set({ tls: { ...value.tls, ...p } });

    return (
        <fieldset style={{ border: '1px solid #45475a', padding: 16, marginBottom: 16, borderRadius: 6 }}>
            <legend style={{ color: '#89b4fa', padding: '0 6px', fontWeight: 600 }}>{label}</legend>

            {label === 'Data Plane' && (
                <label style={{ display: 'flex', gap: 8, alignItems: 'center', marginBottom: 10 }}>
                    <input type="checkbox" checked={value.enabled} disabled={disabled}
                           onChange={e => set({ enabled: e.target.checked })} />
                    <span style={{ fontSize: 14 }}>Enabled</span>
                </label>
            )}

            <label style={{ display: 'block', marginBottom: 10 }}>
                <span style={{ display: 'block', marginBottom: 4, fontSize: 14 }}>Bind Address</span>
                <IpPicker value={value.bind_address} disabled={disabled || !value.enabled}
                          options={ifaceOptions}
                          onChange={v => set({ bind_address: v })} />
            </label>

            <label style={{ display: 'block', marginBottom: 10 }}>
                <span style={{ display: 'block', marginBottom: 4, fontSize: 14 }}>Port</span>
                <input type="number" value={value.port} disabled={disabled || !value.enabled}
                       onChange={e => set({ port: Number(e.target.value) })}
                       style={{ ...inputStyle, width: 100 }} />
            </label>

            <label style={{ display: 'flex', gap: 8, alignItems: 'center', marginBottom: 8 }}>
                <input type="checkbox" checked={value.tls.enabled}
                       disabled={disabled || !value.enabled}
                       onChange={e => setTls({ enabled: e.target.checked })} />
                <span style={{ fontSize: 14 }}>TLS</span>
            </label>

            {value.tls.enabled && (
                <div style={{ paddingLeft: 20 }}>
                    <label style={{ display: 'block', marginBottom: 8 }}>
                        <span style={{ display: 'block', marginBottom: 4, fontSize: 14 }}>Cert Path</span>
                        <input value={value.tls.cert_path} disabled={disabled || !value.enabled}
                               onChange={e => setTls({ cert_path: e.target.value })}
                               style={{ ...inputStyle, width: 280 }} />
                    </label>
                    <label style={{ display: 'block', marginBottom: 8 }}>
                        <span style={{ display: 'block', marginBottom: 4, fontSize: 14 }}>Key Path</span>
                        <input value={value.tls.key_path} disabled={disabled || !value.enabled}
                               onChange={e => setTls({ key_path: e.target.value })}
                               style={{ ...inputStyle, width: 280 }} />
                    </label>
                    <label style={{ display: 'block', marginBottom: 8 }}>
                        <span style={{ display: 'block', marginBottom: 4, fontSize: 14 }}>Min TLS Version</span>
                        <select value={value.tls.min_version} disabled={disabled || !value.enabled}
                                onChange={e => setTls({ min_version: e.target.value })}
                                style={{ ...inputStyle, width: 140 }}>
                            {TLS_VERSIONS.map(v => <option key={v}>{v}</option>)}
                        </select>
                    </label>
                </div>
            )}
        </fieldset>
    );
}

export default function NetworkSettingsPanel() {
    const [cfg, setCfg]               = useState<NetworkConfig | null>(null);
    const [ifaceOpts, setIfaceOpts]   = useState<IfaceOption[]>([]);
    const [saving, setSaving]         = useState(false);
    const [message, setMsg]           = useState('');

    useEffect(() => {
        configClient.getNetwork().then(r => r.data && setCfg(r.data));
        // Fetch available IPs from the server for the picker
        configClient.listNetworkInterfaces().then(r => {
            if (r.data) setIfaceOpts(Array.isArray(r.data) ? r.data : []);
        });
    }, []);

    if (!cfg) return <p style={{ color: '#a6adc8' }}>Loading…</p>;

    const save = async () => {
        setSaving(true); setMsg('');
        const r = await configClient.putNetwork(cfg);
        setSaving(false);
        if (r.error) { setMsg(`Error: ${r.error}`); return; }
        setMsg(r.data?.restart_required ? 'Saved — restart required to apply.' : 'Saved.');
    };

    return (
        <div style={{ maxWidth: 540 }}>
            <h2 style={{ marginBottom: 6, color: '#89b4fa' }}>Network</h2>
            <p style={{ fontSize: 13, color: '#6c7086', marginBottom: 20 }}>
                Changes require a server restart to take effect.
            </p>

            <InterfaceForm label="Control Plane" value={cfg.control_plane}
                           ifaceOptions={ifaceOpts}
                           onChange={v => setCfg({ ...cfg, control_plane: v })} />

            <InterfaceForm label="Data Plane" value={cfg.data_plane}
                           ifaceOptions={ifaceOpts}
                           onChange={v => setCfg({ ...cfg, data_plane: v })} />

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
