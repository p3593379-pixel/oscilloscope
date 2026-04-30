// FILE: control_panel_web/src/features/status/StatusPanel.tsx
import { useEffect, useState, useCallback } from 'react';
import { statusClient, formatUptime, formatBytes } from 'api/statusClient';
import type { StatusResponse, InterfaceStatus } from 'api/statusClient';

function KpiCard({ label, value, sub }: { label: string; value: string; sub?: string }) {
  return (
      <div style={{ background: '#313244', borderRadius: 6, padding: '14px 18px', minWidth: 160 }}>
        <div style={{ fontSize: 11, color: '#6c7086', textTransform: 'uppercase', letterSpacing: 0.8, marginBottom: 6 }}>
          {label}
        </div>
        <div style={{ fontSize: 22, color: '#cdd6f4', fontVariantNumeric: 'tabular-nums', lineHeight: 1 }}>
          {value}
        </div>
        {sub && <div style={{ fontSize: 12, color: '#6c7086', marginTop: 4 }}>{sub}</div>}
      </div>
  );
}

export function StatusPanel() {
  const [status, setStatus]         = useState<StatusResponse | null>(null);
  const [interfaces, setInterfaces] = useState<InterfaceStatus[]>([]);
  const [error, setError]           = useState<string | null>(null);
  const [loading, setLoading]       = useState(true);
  const [lastRefresh, setLastRefresh] = useState<Date | null>(null);

  const refresh = useCallback(async () => {
    try {
      const [st, ifaces] = await Promise.all([
        statusClient.getStatus(),
        statusClient.getInterfaces(),
      ]);
      setStatus(st);
      setInterfaces(Array.isArray(ifaces) ? ifaces : []);
      setError(null);
      setLastRefresh(new Date());
    } catch (e) {
      setError(e instanceof Error ? e.message : 'Failed to fetch status');
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    void refresh();
    const id = setInterval(() => void refresh(), 5000);
    return () => clearInterval(id);
  }, [refresh]);

  if (loading) {
    return <p style={{ color: '#a6adc8' }}>Loading status…</p>;
  }

  if (error) {
    return (
        <div>
          <p style={{ color: '#f38ba8', marginBottom: 12 }}>⚠ {error}</p>
          <button onClick={() => void refresh()} style={refreshBtn}>Retry</button>
        </div>
    );
  }

  const upIfaces  = interfaces.filter(i => i.status === 'UP').length;
  const downIfaces = interfaces.length - upIfaces;

  return (
      <div style={{ maxWidth: 700 }}>
        <div style={{ display: 'flex', alignItems: 'baseline', gap: 16, marginBottom: 20 }}>
          <h2 style={{ color: '#89b4fa', margin: 0 }}>System Status</h2>
          {lastRefresh && (
              <span style={{ fontSize: 12, color: '#6c7086' }}>
                        Last updated {lastRefresh.toLocaleTimeString()}
                    </span>
          )}
        </div>

        {status && (
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: 10, marginBottom: 28 }}>
              <KpiCard label="Uptime"          value={formatUptime(status.uptime_seconds)} />
              <KpiCard label="Active Sessions" value={String(status.active_sessions)} />
              <KpiCard label="Data Streamed"   value={formatBytes(status.bytes_streamed)} />
              <KpiCard label="Version"         value={status.version} />
              <KpiCard label="Interfaces"
                       value={`${upIfaces} UP`}
                       sub={downIfaces > 0 ? `${downIfaces} DOWN` : 'all healthy'} />
            </div>
        )}

        <h3 style={{ fontSize: 13, textTransform: 'uppercase', letterSpacing: 1,
          color: '#6c7086', marginBottom: 10 }}>
          Network Interfaces ({interfaces.length})
        </h3>

        {interfaces.length === 0 ? (
            <p style={{ color: '#6c7086', fontSize: 14 }}>No interfaces reported.</p>
        ) : (
            <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: 14 }}>
              <thead>
              <tr style={{ color: '#6c7086', textAlign: 'left' }}>
                <th style={th}>Name</th>
                <th style={th}>Status</th>
                <th style={th}>Address</th>
                <th style={th}>Speed</th>
              </tr>
              </thead>
              <tbody>
              {interfaces.map(iface => (
                  <tr key={iface.name} style={{ borderTop: '1px solid #313244' }}>
                    <td style={td}><code style={{ color: '#89dceb' }}>{iface.name}</code></td>
                    <td style={td}>
                                    <span style={{
                                      background: iface.status === 'UP' ? '#1c3a2a' : '#3a1c2a',
                                      color:      iface.status === 'UP' ? '#a6e3a1' : '#f38ba8',
                                      padding: '2px 8px', borderRadius: 999, fontSize: 12,
                                    }}>
                                        {iface.status}
                                    </span>
                    </td>
                    <td style={td}>{iface.address}</td>
                    <td style={td}>{iface.speed_mbps > 0 ? `${iface.speed_mbps} Mbps` : '—'}</td>
                  </tr>
              ))}
              </tbody>
            </table>
        )}

        <button onClick={() => void refresh()} style={{ ...refreshBtn, marginTop: 20 }}>
          ↻ Refresh
        </button>
      </div>
  );
}

const th: React.CSSProperties = { padding: '6px 12px', fontWeight: 600, fontSize: 12 };
const td: React.CSSProperties = { padding: '8px 12px', color: '#cdd6f4' };
const refreshBtn: React.CSSProperties = {
  background: '#313244', color: '#cdd6f4', border: '1px solid #45475a',
  padding: '5px 14px', borderRadius: 4, cursor: 'pointer', fontSize: 13,
};
