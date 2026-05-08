import { useEffect, useState } from 'react';
import { configClient, type NetworkConfig, type InterfaceConfig } from 'api/configClient';
import {TogglePill} from "../../components/TogglePill.tsx";

const TLS_VERSIONS = ['TLSv1.2', 'TLSv1.3'];
interface IfaceOption { name: string; address: string; status: string; speed_mbps: number; }

const inp: React.CSSProperties = {
    background: '#313244', border: '1px solid #45475a', color: '#cdd6f4',
    borderRadius: 4, padding: '4px 8px', fontSize: 14,
};
const labelTxt: React.CSSProperties = {
    display: 'block', fontSize: 12, color: '#6c7086', marginBottom: 3,
    textTransform: 'uppercase', letterSpacing: 0.5,
};
const saveBtn: React.CSSProperties = {
    background: '#89b4fa', color: '#1e1e2e', border: 'none',
    padding: '6px 20px', borderRadius: 4, cursor: 'pointer', fontSize: 14, fontWeight: 600,
};
const th: React.CSSProperties = {
    padding: '6px 10px', fontWeight: 600, fontSize: 12, color: '#6c7086', textAlign: 'left',
};
const td: React.CSSProperties = { padding: '7px 10px', color: '#cdd6f4' };

// ── IP address picker ─────────────────────────────────────────────────────
function IpPicker({ value, onChange, options }: {
    value: string; onChange: (v: string) => void; options: IfaceOption[];
}) {
    const [custom, setCustom] = useState(false);
    const presets = [
        { address: '0.0.0.0',   label: 'All interfaces (0.0.0.0)' },
        { address: '127.0.0.1', label: 'Loopback (127.0.0.1)' },
        ...options.map(o => ({ address: o.address, label: `${o.name} — ${o.address} [${o.status}]` })),
    ];
    const isPreset = presets.some(p => p.address === value);

    if (custom) return (
        <div style={{ display: 'flex', gap: 6 }}>
            <input value={value} onChange={e => onChange(e.target.value)}
                   style={{ ...inp, width: 180 }} />
            <button onClick={() => setCustom(false)}
                    style={{ ...inp, cursor: 'pointer', padding: '4px 8px', fontSize: 12 }}>✕</button>
        </div>
    );
    return (
        <select value={isPreset ? value : '__custom__'}
                onChange={e => e.target.value === '__custom__' ? setCustom(true) : onChange(e.target.value)}
                style={{ ...inp, width: 280 }}>
            {presets.map(p => <option key={p.address} value={p.address}>{p.label}</option>)}
            {!isPreset && <option value={value}>{value}</option>}
            <option value="__custom__">Custom…</option>
        </select>
    );
}

// ── Interface block ───────────────────────────────────────────────────────
/**
 * showEnabledToggle: false for the control plane (always on),
 *                   true for the data plane.
 */
function InterfaceBlock({ title, cfg, onChange, options, showEnabledToggle }: {
    title: string; cfg: InterfaceConfig;
    onChange: (c: InterfaceConfig) => void;
    options: IfaceOption[];
    showEnabledToggle: boolean;
}) {
    const set    = (p: Partial<InterfaceConfig>)        => onChange({ ...cfg, ...p });
    const setTls = (p: Partial<InterfaceConfig['tls']>) => onChange({ ...cfg, tls: { ...cfg.tls, ...p } });
    const blockId = title.toLowerCase().replace(/\s+/g, '-');

    return (
        <section style={{ background: '#313244', borderRadius: 6, padding: '14px 18px' }}>
            {/* Header row */}
            <div style={{ display: 'flex', alignItems: 'center', gap: 16, marginBottom: 14 }}>
                <h3 style={{ margin: 0, fontSize: 14, color: '#cdd6f4' }}>{title}</h3>
                {showEnabledToggle && (
                    <TogglePill
                        id={`${blockId}-enabled`}
                        checked={cfg.enabled}
                        onChange={v => set({ enabled: v })}
                        label="Enabled"
                    />
                )}
            </div>

            {/* Address + Port */}
            <div style={{opacity: cfg.enabled ? 1 : 0.4,
                        pointerEvents: cfg.enabled ? 'auto' : 'none'
            }}>
                <div style={{display: 'flex', gap: 16, alignItems: 'flex-end', marginBottom: 14}}>
                    <div>
                        <span style={labelTxt}>Bind address</span>
                        <IpPicker value={cfg.bind_address} onChange={v => set({bind_address: v})} options={options}/>
                    </div>
                    <div>
                        <span style={labelTxt}>Port</span>
                        <input type="number" min={1} max={65535} value={cfg.port}
                               onChange={e => set({port: Number(e.target.value)})}
                               style={{...inp, width: 80}}/>
                    </div>
                </div>

                {/* TLS */}
                <div style={{borderTop: '1px solid #45475a', paddingTop: 12}}>
                    {/* Line 1: TLS toggle + min-version */}
                    <div style={{display: 'flex', alignItems: 'center', gap: 14, marginBottom: 10}}>
                        <TogglePill
                            id={`${blockId}-tls`}
                            checked={cfg.tls.enabled}
                            onChange={v => setTls({enabled: v})}
                            label="TLS"
                            description="Leave off when behind a TLS-terminating proxy"
                        />
                        <select value={cfg.tls.min_version}
                                onChange={e => setTls({min_version: e.target.value})}
                                disabled={!cfg.tls.enabled}
                                style={{
                                    ...inp, width: 110, opacity: cfg.tls.enabled ? 1 : 0.4,
                                    flexShrink: 0
                                }}>
                            {TLS_VERSIONS.map(v => <option key={v}>{v}</option>)}
                        </select>
                    </div>
                    {/* Lines 2–3: cert + key — dimmed when TLS off */}
                    <div style={{
                        display: 'flex', flexDirection: 'column', gap: 8,
                        opacity: cfg.tls.enabled ? 1 : 0.35,
                        pointerEvents: cfg.tls.enabled ? 'auto' : 'none',
                    }}>
                        <div>
                            <span style={labelTxt}>Certificate path</span>
                            <input value={cfg.tls.cert_path}
                                   onChange={e => setTls({cert_path: e.target.value})}
                                   style={{...inp, width: '100%'}}/>
                        </div>
                        <div>
                            <span style={labelTxt}>Key path</span>
                            <input value={cfg.tls.key_path}
                                   onChange={e => setTls({key_path: e.target.value})}
                                   style={{...inp, width: '100%'}}/>
                        </div>
                    </div>
                </div>
            </div>

        </section>
    );
}

// ── Panel ─────────────────────────────────────────────────────────────────
export default function NetworkSettingsPanel() {
    const [cfg, setCfg] = useState<NetworkConfig | null>(null);
    const [ifaces, setIfaces] = useState<IfaceOption[]>([]);
    const [applying, setApplying] = useState(false);
    const [msg, setMsg] = useState('');

    useEffect(() => {
        configClient.getNetwork().then(r => r.data && setCfg(r.data));
        configClient.listNetworkInterfaces().then(r => r.data && setIfaces(r.data));
    }, []);

    const save = async () => {
        if (!cfg) return;
        setApplying(true);
        setMsg('');
        const r = await configClient.putNetwork(cfg);
        setApplying(false);
        setMsg(r.error ? `Error: ${r.error}` : 'Hot-applied new interfaces configuration');
    };

    if (!cfg) return <p style={{color: '#a6adc8'}}>Loading…</p>;

    return (
        <div style={{maxWidth: 1100}}>
            <h2 style={{marginBottom: 4, color: '#89b4fa'}}>Network</h2>
            <p style={{fontSize: 13, color: '#6c7086', marginBottom: 18}}>
                Bind addresses, ports, and TLS for both planes. Changes require a server restart.
            </p>

            {/* Two-column: plane blocks left, interfaces table right */}
            <div style={{display: 'flex', gap: 20, alignItems: 'flex-start'}}>

                {/* Left: blocks + save */}
                <div style={{
                    display: 'flex', flexDirection: 'column', gap: 14,
                    flex: '0 0 auto', minWidth: 400
                }}>
                    <InterfaceBlock
                        title="Control Plane"
                        cfg={cfg.control_plane}
                        onChange={cp => setCfg({...cfg, control_plane: cp})}
                        options={ifaces}
                        showEnabledToggle={false}
                    />
                    <InterfaceBlock
                        title="Data Plane"
                        cfg={cfg.data_plane}
                        onChange={dp => setCfg({...cfg, data_plane: dp})}
                        options={ifaces}
                        showEnabledToggle={true}
                    />
                    <div style={{display: 'flex', gap: 12, alignItems: 'center'}}>
                        <button onClick={save} disabled={applying} style={saveBtn}>
                            {applying ? 'Applying…' : 'Apply'}
                        </button>
                        {msg && (
                            <span style={{
                                fontSize: 13,
                                color: msg.startsWith('Error') ? '#f38ba8' : '#a6e3a1'
                            }}>
                                {msg}
                            </span>
                        )}
                    </div>
                </div>

                {/* Right: interface reference table */}
                <div style={{ flex: '1 1 0', minWidth: 0 }}>
                    <h3 style={{ fontSize: 12, textTransform: 'uppercase', letterSpacing: 0.8,
                        color: '#6c7086', marginBottom: 8 }}>
                        System interfaces ({ifaces.length})
                    </h3>
                    <p style={{ fontSize: 12, color: '#6c7086', marginBottom: 10 }}>
                        Detected at runtime — reference when setting bind addresses.
                    </p>
                    {ifaces.length === 0
                        ? <p style={{ color: '#6c7086', fontSize: 13 }}>None reported.</p>
                        : (
                            <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: 13 }}>
                                <thead>
                                <tr>
                                    <th style={th}>Name</th>
                                    <th style={th}>Status</th>
                                    <th style={th}>Address</th>
                                    <th style={th}>Speed</th>
                                </tr>
                                </thead>
                                <tbody>
                                {ifaces.map(iface => (
                                    <tr key={iface.name} style={{ borderTop: '1px solid #313244' }}>
                                        <td style={td}>
                                            <code style={{ color: '#89dceb' }}>{iface.name}</code>
                                        </td>
                                        <td style={td}>
                                                <span style={{
                                                    background: iface.status === 'UP' ? '#1c3a2a' : '#3a1c2a',
                                                    color:      iface.status === 'UP' ? '#a6e3a1' : '#f38ba8',
                                                    padding: '2px 8px', borderRadius: 999, fontSize: 11,
                                                }}>{iface.status}</span>
                                        </td>
                                        <td style={td}>{iface.address}</td>
                                        <td style={td}>
                                            {iface.speed_mbps > 0 ? `${iface.speed_mbps} Mbps` : '—'}
                                        </td>
                                    </tr>
                                ))}
                                </tbody>
                            </table>
                        )
                    }
                </div>
            </div>
        </div>
    );
}
