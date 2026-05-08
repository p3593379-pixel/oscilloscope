// FILE: control_panel_web/src/App.tsx
import { useState } from 'react';
import NetworkSettingsPanel  from './features/network/NetworkSettingsPanel';
import SessionsAndUsersPanel from './features/session/SessionsAndUsersPanel';
import LogSettingsPanel      from './features/log/LogSettingsPanel';
import MetricsPanel          from './features/metrics/MetricsPanel';
import { configClient }      from './api/configClient';

const TABS = [
    { id: 'network', label: 'Network'          },
    { id: 'session', label: 'Sessions & Users' },
    { id: 'log',     label: 'Logging'          },
    { id: 'metrics', label: 'Metrics'          },
] as const;

export type TabId = typeof TABS[number]['id'];

export default function App() {
    const [tab, setTab] = useState<TabId>('network');

    return (
        <div style={{ fontFamily: 'system-ui, sans-serif', display: 'flex', height: '100vh' }}>
            <nav style={{
                width: 210, background: '#1e1e2e', color: '#cdd6f4',
                display: 'flex', flexDirection: 'column', padding: '1rem 0', flexShrink: 0,
            }}>
                <div style={{ padding: '0 1rem 1.5rem', fontWeight: 700, fontSize: 15, color: '#89b4fa' }}>
                    buf_connect
                </div>
                {TABS.map(t => (
                    <button key={t.id} onClick={() => setTab(t.id)} style={{
                        background:  tab === t.id ? '#313244' : 'transparent',
                        color:       tab === t.id ? '#cdd6f4'  : '#a6adc8',
                        border:      'none',
                        borderLeft:  tab === t.id ? '2px solid #89b4fa' : '2px solid transparent',
                        textAlign:   'left',
                        padding:     '0.6rem 1rem',
                        cursor:      'pointer',
                        fontSize:    14,
                    }}>{t.label}</button>
                ))}
                <div style={{ flex: 1 }} />
                <div style={{ padding: '0 1rem', display: 'flex', flexDirection: 'column', gap: 8 }}>
                    <button onClick={() => configClient.exportConfig()} style={actionBtn}>
                        ↓ Export Config
                    </button>
                    <label style={{ ...actionBtn, textAlign: 'center', cursor: 'pointer' }}>
                        ↑ Import Config
                        <input type="file" accept=".json" style={{ display: 'none' }}
                               onChange={async e => {
                                   const file = e.target.files?.[0];
                                   if (!file) return;
                                   const result = await configClient.importConfig(file);
                                   if (result.error) alert('Import error: ' + result.error);
                                   else alert('Config imported and saved.');
                               }} />
                    </label>
                </div>
            </nav>

            <main style={{ flex: 1, overflow: 'auto', background: '#1a1a2e', padding: '2rem', color: '#cdd6f4' }}>
                {tab === 'network' && <NetworkSettingsPanel />}
                {tab === 'session' && <SessionsAndUsersPanel />}
                {tab === 'log'     && <LogSettingsPanel />}
                {tab === 'metrics' && <MetricsPanel />}
            </main>
        </div>
    );
}

const actionBtn: React.CSSProperties = {
    background: '#313244', color: '#cdd6f4', border: 'none',
    padding: '0.5rem', borderRadius: 4, cursor: 'pointer', fontSize: 13,
};
