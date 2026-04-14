// FILE: control_panel_web/src/App.tsx
import { useState } from 'react';
import NetworkSettingsPanel  from './features/network/NetworkSettingsPanel';
import AuthSettingsPanel     from './features/auth/AuthSettingsPanel';
import StreamingSettingsPanel from './features/streaming/StreamingSettingsPanel';
import SessionSettingsPanel  from './features/session/SessionSettingsPanel';
import LogSettingsPanel      from './features/log/LogSettingsPanel';
import MetricsSettingsPanel  from './features/metrics/MetricsSettingsPanel';
import UserManagementPanel   from './features/users/UserManagementPanel';
import { StatusPanel }           from './features/status/StatusPanel';
import { configClient }      from './api/configClient';

const TABS = [
    { id: 'network',   label: 'Network'   },
    { id: 'auth',      label: 'Auth'      },
    { id: 'streaming', label: 'Streaming' },
    { id: 'session',   label: 'Session'   },
    { id: 'log',       label: 'Logging'   },
    { id: 'metrics',   label: 'Metrics'   },
    { id: 'users',     label: 'Users'     },
    { id: 'status',    label: 'Status'    },
] as const;

// In App.tsx — add this export near the top
export type PanelId =
    | 'status' | 'network' | 'auth' | 'streaming'
    | 'session' | 'log' | 'metrics' | 'users';

type TabId = typeof TABS[number]['id'];

export default function App() {
    const [tab, setTab] = useState<TabId>('status');

    return (
        <div style={{ fontFamily: 'system-ui, sans-serif', display: 'flex', height: '100vh' }}>
            {/* Sidebar */}
            <nav style={{
                width: 200, background: '#1e1e2e', color: '#cdd6f4',
                display: 'flex', flexDirection: 'column', padding: '1rem 0',
            }}>
                <div style={{ padding: '0 1rem 1.5rem', fontWeight: 700, fontSize: 15, color: '#89b4fa' }}>
                    Oscilloscope
                </div>
                {TABS.map(t => (
                    <button key={t.id} onClick={() => setTab(t.id)} style={{
                        background: tab === t.id ? '#313244' : 'transparent',
                        color: tab === t.id ? '#cdd6f4' : '#a6adc8',
                        border: 'none', textAlign: 'left', padding: '0.6rem 1rem',
                        cursor: 'pointer', fontSize: 14,
                    }}>{t.label}</button>
                ))}
                <div style={{ flex: 1 }} />
                <div style={{ padding: '0 1rem', display: 'flex', flexDirection: 'column', gap: 8 }}>
                    <button onClick={() => configClient.exportConfig()} style={actionBtn}>
                        Export Config
                    </button>
                    <label style={{ ...actionBtn, textAlign: 'center', cursor: 'pointer' }}>
                        Import Config
                        <input type="file" accept=".json" style={{ display: 'none' }}
                               onChange={async e => {
                                   const file = e.target.files?.[0];
                                   if (!file) return;
                                   const result = await configClient.importConfig(file);
                                   if (result.error) alert('Import error: ' + result.error);
                                   else alert('Config valid — apply via Save buttons');
                               }} />
                    </label>
                </div>
            </nav>

            {/* Main */}
            <main style={{ flex: 1, overflow: 'auto', background: '#1a1a2e', padding: '2rem' }}>
                {tab === 'network'   && <NetworkSettingsPanel />}
                {tab === 'auth'      && <AuthSettingsPanel />}
                {tab === 'streaming' && <StreamingSettingsPanel />}
                {tab === 'session'   && <SessionSettingsPanel />}
                {tab === 'log'       && <LogSettingsPanel />}
                {tab === 'metrics'   && <MetricsSettingsPanel />}
                {tab === 'users'     && <UserManagementPanel />}
                {tab === 'status'    && <StatusPanel />}
            </main>
        </div>
    );
}

const actionBtn: React.CSSProperties = {
    background: '#313244', color: '#cdd6f4', border: 'none',
    padding: '0.5rem', borderRadius: 4, cursor: 'pointer', fontSize: 13,
};
