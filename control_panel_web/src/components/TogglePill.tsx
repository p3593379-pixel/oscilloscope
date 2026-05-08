export function TogglePill({ id, checked, onChange, label, description }: {
    id: string; checked: boolean; onChange: (v: boolean) => void;
    label: string; description?: string;
}) {
    return (
        <label htmlFor={id} style={{ display: 'flex', alignItems: 'flex-start', gap: 14,
            cursor: 'pointer', userSelect: 'none' }}>
            <input id={id} type="checkbox" checked={checked} onChange={e => onChange(e.target.checked)}
                   style={{ position: 'absolute', opacity: 0, width: 0, height: 0 }} />
            <div style={{
                flexShrink: 0, marginTop: 2, width: 40, height: 22, borderRadius: 11,
                background: checked ? '#89b4fa' : '#45475a',
                position: 'relative', transition: 'background 180ms',
            }}>
                <div style={{
                    position: 'absolute', top: 3, left: checked ? 21 : 3,
                    width: 16, height: 16, borderRadius: '50%',
                    background: checked ? '#1e1e2e' : '#cdd6f4', transition: 'left 180ms',
                }} />
            </div>
            <div>
                <div style={{ fontSize: 14, color: '#cdd6f4', lineHeight: 1.4 }}>{label}</div>
                {description && (
                    <div style={{ fontSize: 12, color: '#6c7086', marginTop: 2 }}>{description}</div>
                )}
            </div>
        </label>
    );
}
