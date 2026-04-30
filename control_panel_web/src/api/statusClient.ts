// FILE: control_panel_web/src/api/statusClient.ts

export interface StatusResponse {
    uptime_seconds:  number;
    active_sessions: number;
    bytes_streamed:  number;
    version:         string;
}

export interface InterfaceStatus {
    name:       string;
    status:     'UP' | 'DOWN';
    address:    string;
    speed_mbps: number;
}

export function formatUptime(seconds: number): string {
    const d = Math.floor(seconds / 86400);
    const h = Math.floor((seconds % 86400) / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = seconds % 60;
    return [d && `${d}d`, h && `${h}h`, m && `${m}m`, `${s}s`]
        .filter(Boolean).join(' ');
}

export function formatBytes(bytes: number): string {
    if (bytes >= 1e9) return `${(bytes / 1e9).toFixed(2)} GB`;
    if (bytes >= 1e6) return `${(bytes / 1e6).toFixed(2)} MB`;
    if (bytes >= 1e3) return `${(bytes / 1e3).toFixed(2)} KB`;
    return `${bytes} B`;
}

async function request<T>(path: string, fallback?: T): Promise<T> {
    const res = await fetch(`/api${path}`);
    if (!res.ok) {
        if (fallback !== undefined) return fallback;
        throw new Error(`HTTP ${res.status} on /api${path}`);
    }
    return res.json() as Promise<T>;
}

export const statusClient = {
    getStatus:     () => request<StatusResponse>('/status'),
    getInterfaces: () => request<InterfaceStatus[]>('/status/interfaces', []),
};
