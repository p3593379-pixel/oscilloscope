// FILE: control_panel_web/src/api/configClient.ts

export interface TlsConfig {
    enabled:     boolean;
    cert_path:   string;
    key_path:    string;
    min_version: string;
}
export interface InterfaceConfig {
    bind_address: string;
    port:         number;
    enabled:      boolean;
    tls:          TlsConfig;
}
export interface NetworkConfig {
    control_plane: InterfaceConfig;
    data_plane:    InterfaceConfig;
}
export interface NetworkSaveResponse extends NetworkConfig { restart_required?: boolean; }

export interface SessionConfig {
    admin_conflict_timeout_seconds: number;
    snooze_duration_seconds:        number;
    max_concurrent_sessions:        number;
    call_token_renew_period:        number;
    grace_period_admin_seconds:     number;
    grace_period_engineer_seconds:  number;
}
export interface LogConfig {
    level:                string;
    log_file_name_prefix: string;
    console:              boolean;
    max_size_mb:          number;
    max_files:            number;
}
export interface MetricsConfig {
    enabled:                 boolean;
    metrics_log_path:        string;
    metrics_log_file_prefix: string;
}
export interface LiveMetrics {
    uptime_seconds:  number;
    active_sessions: number;
    total_sessions:  number;
    peak_sessions:   number;
    bytes_streamed:  number;
    active_streams:  number;
    rps:             number;
}
export type UserRole = 'admin' | 'engineer';
export interface UserInfo {
    user_id:    number;
    username:   string;
    role:       UserRole;
    created_at: number;
    last_login: number;
}
export interface CreateUserRequest { username: string; password: string; role: UserRole; }
export interface UserDbConfig { user_db_path: string; restart_required?: boolean; }

/** Full config — StreamingConfig removed */
export interface ServerConfig {
    control_plane: InterfaceConfig;
    data_plane:    InterfaceConfig;
    session:       SessionConfig;
    log:           LogConfig;
    metrics:       MetricsConfig;
    user_db_path:  string;
}

// ── HTTP helpers ──────────────────────────────────────────────────────────
type ApiResult<T> = Promise<{ data: T | null; error: string | null }>;

async function get<T>(path: string): ApiResult<T> {
    try {
        const res = await fetch(path);
        if (!res.ok) return { data: null, error: `HTTP ${res.status}` };
        return { data: (await res.json()) as T, error: null };
    } catch (e) { return { data: null, error: String(e) }; }
}
async function put<T>(path: string, body: unknown): ApiResult<T> {
    try {
        const res = await fetch(path, {
            method: 'PUT', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body),
        });
        if (!res.ok) return { data: null, error: `HTTP ${res.status}` };
        return { data: (await res.json()) as T, error: null };
    } catch (e) { return { data: null, error: String(e) }; }
}
async function post<T>(path: string, body: unknown): ApiResult<T> {
    try {
        const res = await fetch(path, {
            method: 'POST', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body),
        });
        if (!res.ok) {
            const err = await res.json().catch(() => ({}));
            return { data: null, error: (err as { error?: string }).error ?? `HTTP ${res.status}` };
        }
        return { data: (await res.json()) as T, error: null };
    } catch (e) { return { data: null, error: String(e) }; }
}
async function del<T>(path: string): ApiResult<T> {
    try {
        const res = await fetch(path, { method: 'DELETE' });
        if (!res.ok) {
            const err = await res.json().catch(() => ({}));
            return { data: null, error: (err as { error?: string }).error ?? `HTTP ${res.status}` };
        }
        if (res.status === 204) return { data: null, error: null };
        return { data: (await res.json()) as T, error: null };
    } catch (e) { return { data: null, error: String(e) }; }
}

// ── Client ────────────────────────────────────────────────────────────────
export const configClient = {
    getAll:  ()                => get<ServerConfig>('/api/config'),
    putAll:  (c: ServerConfig) => put<ServerConfig>('/api/config', c),

    getNetwork:  ()                 => get<NetworkConfig>('/api/config/network'),
    putNetwork:  (c: NetworkConfig) => put<NetworkSaveResponse>('/api/config/network', c),

    getSession:  ()                 => get<SessionConfig>('/api/config/session'),
    putSession:  (c: SessionConfig) => put<SessionConfig>('/api/config/session', c),

    getLog:  ()             => get<LogConfig>('/api/config/log'),
    putLog:  (c: LogConfig) => put<LogConfig>('/api/config/log', c),

    getMetrics:  ()                 => get<MetricsConfig>('/api/config/metrics'),
    putMetrics:  (c: MetricsConfig) => put<MetricsConfig>('/api/config/metrics', c),

    getLiveMetrics: ()              => get<LiveMetrics>('/api/metrics/live'),

    getUserDb: ()                   => get<UserDbConfig>('/api/config/userdb'),

    listNetworkInterfaces: ()       =>
        get<{ name: string; address: string; status: string; speed_mbps: number }[]>('/api/status/interfaces'),

    listUsers:     ()                            => get<{ users: UserInfo[] }>('/api/users'),
    createUser:    (c: CreateUserRequest)        => post<UserInfo>('/api/users', c),
    deleteUser:    (userId: string)              => del<{ success: boolean }>(`/api/users/${userId}`),
    resetPassword: (userId: string, pwd: string) =>
        put<{ success: boolean }>(`/api/users/${userId}/password`, { new_password: pwd }),

    exportConfig: async () => {
        const { data } = await get<ServerConfig>('/api/config');
        if (!data) return;
        const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
        const a = document.createElement('a');
        a.href = URL.createObjectURL(blob); a.download = 'config.json'; a.click();
    },

    importConfig: async (file: File): Promise<{ error: string | null }> => {
        const text = await file.text();
        try {
            const { error } = await put<ServerConfig>('/api/config', JSON.parse(text));
            return { error };
        } catch { return { error: 'Invalid JSON' }; }
    },
};
