// FILE: control_panel_web/src/features/streaming/StreamingSettingsPanel.tsx
import { useEffect, useState } from 'react';
import { configClient, type StreamingConfig } from 'api/configClient';
import { PanelLayout, SaveButton, Field, Select } from '../_shared/PanelComponents';

const FRAME_SIZES = [8192, 32768, 65536, 262144];
const ALGORITHMS  = ['zstd', 'gzip'];
const POLICIES    = [
    { value: 'drop_oldest',        label: 'Drop oldest'        },
    { value: 'block',              label: 'Block producer'     },
    { value: 'disconnect',         label: 'Disconnect client'  },
];

const defaults: StreamingConfig = {
    max_clients: 0,
    frame_size_bytes: 65536,
    compression_enabled: false,
    compression_algorithm: 'zstd',
    compression_threshold_bytes: 1024,
    backpressure_policy: 'disconnect',
};

export default function StreamingSettingsPanel() {
    const [cfg, setCfg]       = useState<StreamingConfig>(defaults);
    const [saving, setSaving] = useState(false);
    const [msg, setMsg]       = useState('');

    useEffect(() => {
        configClient.getStreaming().then(r => { if (r.data) setCfg(r.data); });
    }, []);

    const save = async () => {
        setSaving(true);
        const r = await configClient.putStreaming(cfg);
        setMsg(r.error ? `Error: ${r.error}` : 'Saved');
        setSaving(false);
        setTimeout(() => setMsg(''), 3000);
    };

    return (
        <PanelLayout title="Streaming Settings">
            <Field label="Max Streaming Clients (0 = unlimited)">
                <input type="number" min={0} value={cfg.max_clients}
                       onChange={e => setCfg(p => ({ ...p, max_clients: +e.target.value }))}
                       style={inputStyle} />
            </Field>
            <Field label="Frame Size">
                <Select value={String(cfg.frame_size_bytes)}
                        onChange={v => setCfg(p => ({ ...p, frame_size_bytes: +v }))}
                        options={FRAME_SIZES.map(s => ({ value: String(s), label: `${s / 1024} KiB` }))} />
            </Field>
            <Field label="Compression">
                <label style={{ display: 'flex', alignItems: 'center', gap: 8, color: '#cdd6f4' }}>
                    <input type="checkbox" checked={cfg.compression_enabled}
                           onChange={e => setCfg(p => ({ ...p, compression_enabled: e.target.checked }))} />
                    Enable compression
                </label>
            </Field>
            {cfg.compression_enabled && <>
                <Field label="Algorithm">
                    <Select value={cfg.compression_algorithm}
                            onChange={v => setCfg(p => ({ ...p, compression_algorithm: v }))}
                            options={ALGORITHMS.map(a => ({ value: a, label: a }))} />
                </Field>
                <Field label="Threshold (bytes)">
                    <input type="number" min={0} value={cfg.compression_threshold_bytes}
                           onChange={e => setCfg(p => ({ ...p, compression_threshold_bytes: +e.target.value }))}
                           style={inputStyle} />
                </Field>
            </>}
            <Field label="Backpressure Policy">
                <Select value={cfg.backpressure_policy}
                        onChange={v => setCfg(p => ({ ...p, backpressure_policy: v }))}
                        options={POLICIES} />
            </Field>
            <SaveButton onClick={save} saving={saving} msg={msg} />
        </PanelLayout>
    );
}

const inputStyle: React.CSSProperties = {
    background: '#313244', border: '1px solid #45475a', color: '#cdd6f4',
    padding: '0.4rem 0.6rem', borderRadius: 4, width: '100%', boxSizing: 'border-box',
};
