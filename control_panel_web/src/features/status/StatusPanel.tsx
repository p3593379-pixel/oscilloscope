// FILE: control_panel_web/src/features/status/StatusPanel.tsx
import { useEffect, useState, useCallback } from "react";
import { statusClient } from "api/statusClient";
import type { StatusResponse, InterfaceStatus } from "api/statusClient";

function formatUptime(seconds: number): string {
  const d = Math.floor(seconds / 86400);
  const h = Math.floor((seconds % 86400) / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = seconds % 60;
  return [d && `${d}d`, h && `${h}h`, m && `${m}m`, `${s}s`]
      .filter(Boolean)
      .join(" ");
}

function formatBytes(bytes: number): string {
  if (bytes >= 1e9) return `${(bytes / 1e9).toFixed(2)} GB`;
  if (bytes >= 1e6) return `${(bytes / 1e6).toFixed(2)} MB`;
  if (bytes >= 1e3) return `${(bytes / 1e3).toFixed(2)} KB`;
  return `${bytes} B`;
}

export function StatusPanel() {
  const [status, setStatus]         = useState<StatusResponse | null>(null);
  const [interfaces, setInterfaces] = useState<InterfaceStatus[]>([]);   // ← always an array
  const [error, setError]           = useState<string | null>(null);
  const [loading, setLoading]       = useState(true);

  const refresh = useCallback(async () => {
    try {
      const [st, ifaces] = await Promise.all([
        statusClient.getStatus(),
        statusClient.getInterfaces(),
      ]);
      setStatus(st);
      setInterfaces(Array.isArray(ifaces) ? ifaces : []);   // ← defensive
      setError(null);
    } catch (e) {
      setError(e instanceof Error ? e.message : "Failed to fetch status");
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    void refresh();
    const id = setInterval(() => { void refresh(); }, 5000);
    return () => clearInterval(id);
  }, [refresh]);

  if (loading) {
    return <div className="status-panel status-panel--loading">Loading status…</div>;
  }

  if (error) {
    return (
        <div className="status-panel status-panel--error">
          <p>⚠ {error}</p>
          <button onClick={() => { void refresh(); }}>Retry</button>
        </div>
    );
  }

  return (
      <section className="status-panel">
        <h2>System Status</h2>

        {status && (
            <dl className="status-kpis">
              <div>
                <dt>Uptime</dt>
                <dd>{formatUptime(status.uptime_seconds)}</dd>
              </div>
              <div>
                <dt>Active Sessions</dt>
                <dd>{status.active_sessions}</dd>
              </div>
              <div>
                <dt>Bytes Streamed</dt>
                <dd>{formatBytes(status.bytes_streamed)}</dd>
              </div>
              <div>
                <dt>Version</dt>
                <dd>{status.version}</dd>
              </div>
            </dl>
        )}

        <h3>Network Interfaces ({interfaces.length})</h3>
        {interfaces.length === 0 ? (
            <p className="status-panel__empty">No interfaces reported.</p>
        ) : (
            <table className="status-table">
              <thead>
              <tr>
                <th>Name</th>
                <th>Status</th>
                <th>Address</th>
                <th>Speed</th>
              </tr>
              </thead>
              <tbody>
              {interfaces.map((iface) => (
                  <tr key={iface.name}>
                    <td>{iface.name}</td>
                    <td>
                  <span className={`badge badge--${iface.status === "UP" ? "success" : "error"}`}>
                    {iface.status}
                  </span>
                    </td>
                    <td>{iface.address}</td>
                    <td>{iface.speed_mbps > 0 ? `${iface.speed_mbps} Mbps` : "—"}</td>
                  </tr>
              ))}
              </tbody>
            </table>
        )}

        <button className="btn btn-secondary" onClick={() => { void refresh(); }}>
          ↻ Refresh
        </button>
      </section>
  );
}
