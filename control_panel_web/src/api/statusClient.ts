// FILE: control_panel_web/src/api/statusClient.ts

export interface InterfaceStatus {
    name: string;
    address: string;
    port: number;
    up: boolean;
    status: 'UP' | 'DOWN';   // ← add this
    speed_mbps: number;
}

// export interface SessionEntry {
//     connection_id: string;
//     user_id: string;
//     role: string;
//     mode: string;
//     connected_at: number;
// }

// export interface ServerStatus {
//     uptime_seconds: number;
//     active_sessions: number;
//     bytes_streamed: number;
//     version: string;
//     sessions: SessionEntry[];
//     interfaces: InterfaceStatus[];
// }

// export type ApiResult<T> = Promise<{ data: T | null; error: string | null }>;

// async function get<T>(path: string): ApiResult<T> {
//     try {
//         const res = await fetch(path);
//         if (!res.ok) return { data: null, error: `HTTP ${res.status}` };
//         return { data: (await res.json()) as T, error: null };
//     } catch (e) {
//         return { data: null, error: String(e) };
//     }
// }

function formatUptime(seconds: number): string {
    const d = Math.floor(seconds / 86400);
    const h = Math.floor((seconds % 86400) / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = seconds % 60;
    if (d > 0) return `${d}d ${h}h ${m}m`;
    if (h > 0) return `${h}h ${m}m ${s}s`;
    return `${m}m ${s}s`;
}

function formatBytes(bytes: number): string {
    if (bytes >= 1e9) return `${(bytes / 1e9).toFixed(2)} GB`;
    if (bytes >= 1e6) return `${(bytes / 1e6).toFixed(2)} MB`;
    if (bytes >= 1e3) return `${(bytes / 1e3).toFixed(2)} KB`;
    return `${bytes} B`;
}

// export const statusClient = {
//     getStatus:     () => get<ServerStatus>('/api/status'),
//     getInterfaces: () => get<{ interfaces: InterfaceStatus[] }>('/api/status/interfaces'),
//     formatUptime,
//     formatBytes,
// };

// FILE: control_panel_web/src/api/statusClient.ts
const API_BASE = "/api";

async function request<T>(path: string, fallback?: T): Promise<T> {
    const res = await fetch(`${API_BASE}${path}`);
    if (!res.ok) {
        if (fallback !== undefined) return fallback;
        throw new Error(`HTTP ${res.status} on ${path}`);
    }
    return res.json() as Promise<T>;
}

export interface StatusResponse {
    uptime_seconds:  number;
    active_sessions: number;
    bytes_streamed:  number;
    version:         string;
}

export interface InterfaceStatus {
    name:       string;
    status:     "UP" | "DOWN";
    address:    string;
    speed_mbps: number;
}

export const statusClient = {
    getStatus:     () => request<StatusResponse>("/status"),
    // Pass [] as fallback so a 404 never crashes the component
    getInterfaces: () => request<InterfaceStatus[]>("/status/interfaces", []),
    formatUptime,
    formatBytes,
};
