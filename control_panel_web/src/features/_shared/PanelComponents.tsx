// FILE: control_panel_web/src/features/_shared/PanelComponents.tsx
import React from 'react';

export function PanelLayout({ title, children }: { title: string; children: React.ReactNode }) {
    return (
        <div style={{ maxWidth: 640, color: '#cdd6f4' }}>
            <h2 style={{ marginBottom: '1.5rem', fontSize: 20, fontWeight: 600, color: '#89b4fa' }}>
                {title}
            </h2>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '1rem' }}>
                {children}
            </div>
        </div>
    );
}

export function Field({ label, children }: { label: string; children: React.ReactNode }) {
    return (
        <div>
            <label style={{ display: 'block', marginBottom: 4, fontSize: 13, color: '#a6adc8' }}>
                {label}
            </label>
            {children}
        </div>
    );
}

interface SelectOption { value: string; label: string; }
export function Select({ value, onChange, options }: {
    value: string;
    onChange: (v: string) => void;
    options: SelectOption[];
}) {
    return (
        <select value={value} onChange={e => onChange(e.target.value)} style={{
            background: '#313244', border: '1px solid #45475a', color: '#cdd6f4',
            padding: '0.4rem 0.6rem', borderRadius: 4, width: '100%',
        }}>
            {options.map(o => <option key={o.value} value={o.value}>{o.label}</option>)}
        </select>
    );
}

export function SaveButton({ onClick, saving, msg }: {
    onClick: () => void;
    saving: boolean;
    msg: string;
}) {
    return (
        <div style={{ display: 'flex', alignItems: 'center', gap: 12, marginTop: 8 }}>
            <button onClick={onClick} disabled={saving} style={{
                background: '#89b4fa', color: '#1e1e2e', border: 'none',
                padding: '0.5rem 1.5rem', borderRadius: 4, cursor: saving ? 'wait' : 'pointer',
                fontWeight: 600, fontSize: 14,
            }}>
                {saving ? 'Saving…' : 'Save'}
            </button>
            {msg && <span style={{ fontSize: 13, color: msg.startsWith('Error') ? '#f38ba8' : '#a6e3a1' }}>
        {msg}
      </span>}
        </div>
    );
}

export const inputStyle: React.CSSProperties = {
    background: '#313244', border: '1px solid #45475a', color: '#cdd6f4',
    padding: '0.4rem 0.6rem', borderRadius: 4, width: '100%', boxSizing: 'border-box',
};
