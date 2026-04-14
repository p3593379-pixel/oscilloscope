// FILE: control_panel_web/src/components/Sidebar.tsx
import type { PanelId } from '../App';

const navItems: { id: PanelId; label: string; icon: string }[] = [
    { id: 'status',    label: 'Status',    icon: '📊' },
    { id: 'network',   label: 'Network',   icon: '🌐' },
    { id: 'auth',      label: 'Auth',      icon: '🔑' },
    { id: 'streaming', label: 'Streaming', icon: '📡' },
    { id: 'session',   label: 'Session',   icon: '🔗' },
    { id: 'log',       label: 'Logging',   icon: '📋' },
    { id: 'metrics',   label: 'Metrics',   icon: '📈' },
    { id: 'users',     label: 'Users',     icon: '👥' },
];

interface Props {
    active: PanelId;
    onNavigate: (id: PanelId) => void;
}

export default function Sidebar({ active, onNavigate }: Props) {
    return (
        <nav style={{
            width: 200, background: '#1e1e2e', color: '#cdd6f4',
            display: 'flex', flexDirection: 'column', padding: '1rem 0',
            height: '100vh', flexShrink: 0,
        }}>
            <div style={{ padding: '0 1rem 1.5rem', fontWeight: 700, fontSize: 15, color: '#89b4fa' }}>
                Oscilloscope
            </div>
            {navItems.map(t => (
                <button key={t.id} onClick={() => onNavigate(t.id)} style={{
                    background: active === t.id ? '#313244' : 'transparent',
                    color: active === t.id ? '#cdd6f4' : '#a6adc8',
                    border: 'none', textAlign: 'left', padding: '0.6rem 1rem',
                    cursor: 'pointer', fontSize: 14, display: 'flex', gap: 8, alignItems: 'center',
                }}>
                    <span>{t.icon}</span>{t.label}
                </button>
            ))}
        </nav>
    );
}
