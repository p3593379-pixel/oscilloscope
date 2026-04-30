// FILE: control_panel_web/src/api/configClient.ts

// ---- Types aligned with canonical server_config.hpp ----

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

export interface NetworkSaveResponse extends NetworkConfig {
    restart_required?: boolean;
}

/** Matches SessionConfig in server_config.hpp */
export interface SessionConfig {
    admin_conflict_timeout_seconds: number;
    snooze_duration_seconds:        number;
    max_concurrent_sessions:        number;
    call_token_renew_period:        number;
    grace_period_admin_seconds:     number;
    grace_period_engineer_seconds:  number;
}

/** Matches StreamingConfig — simplified, future fields TBD */
export interface StreamingConfig {
    compression_enabled:   boolean;
    compression_algorithm: string;
}

/** Matches LogConfig in server_config.hpp */
export interface LogConfig {
    level:                string;
    log_file_name_prefix: string;
    console:              boolean;
    max_size_mb:          number;
    max_files:            number;
}

/** Matches MetricsConfig in server_config.hpp */
export interface MetricsConfig {
    enabled:                 boolean;
    metrics_log_path:        string;
    metrics_log_file_prefix: string;
}

/** Live runtime counters returned by /api/metrics/live */
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

export interface CreateUserRequest {
    username: string;
    password: string;
    role:     UserRole;
}

/** Full config — no auth, no single_interface_mode */
export interface ServerConfig {
    control_plane: InterfaceConfig;
    data_plane:    InterfaceConfig;
    session:       SessionConfig;
    streaming:     StreamingConfig;
    log:           LogConfig;
    metrics:       MetricsConfig;
    user_db_path:  string;
}

// ---- HTTP helpers ----

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
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body),
        });
        if (!res.ok) return { data: null, error: `HTTP ${res.status}` };
        return { data: (await res.json()) as T, error: null };
    } catch (e) { return { data: null, error: String(e) }; }
}

async function post<T>(path: string, body: unknown): ApiResult<T> {
    try {
        const res = await fetch(path, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
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

// ---- Client ----

export const configClient = {
    // Full config
    getAll:  ()                  => get<ServerConfig>('/api/config'),
    putAll:  (c: ServerConfig)   => put<ServerConfig>('/api/config', c),

    // Network
    getNetwork:  ()                      => get<NetworkConfig>('/api/config/network'),
    putNetwork:  (c: NetworkConfig)      => put<NetworkSaveResponse>('/api/config/network', c),

    // Session
    getSession:  ()                      => get<SessionConfig>('/api/config/session'),
    putSession:  (c: SessionConfig)      => put<SessionConfig>('/api/config/session', c),

    // Streaming
    getStreaming: ()                     => get<StreamingConfig>('/api/config/streaming'),
    putStreaming: (c: StreamingConfig)   => put<StreamingConfig>('/api/config/streaming', c),

    // Log
    getLog:  ()                          => get<LogConfig>('/api/config/log'),
    putLog:  (c: LogConfig)              => put<LogConfig>('/api/config/log', c),

    // Metrics config
    getMetrics:  ()                      => get<MetricsConfig>('/api/config/metrics'),
    putMetrics:  (c: MetricsConfig)      => put<MetricsConfig>('/api/config/metrics', c),

    // Live metrics  (read-only)
    getLiveMetrics: ()                   => get<LiveMetrics>('/api/metrics/live'),

    // Network interfaces for IP picker
    listNetworkInterfaces: ()            => get<{ name: string; address: string; status: string }[]>(
        '/api/status/interfaces'),

    // Users
    listUsers:     ()                              => get<{ users: UserInfo[] }>('/api/users'),
    createUser:    (c: CreateUserRequest)          => post<UserInfo>('/api/users', c),
    deleteUser:    (userId: string)                => del<{ success: boolean }>(`/api/users/${userId}`),
    resetPassword: (userId: string, pwd: string)   =>
        put<{ success: boolean }>(`/api/users/${userId}/password`, { new_password: pwd }),

    // Import / export
    exportConfig: async () => {
        const { data } = await get<ServerConfig>('/api/config');
        if (!data) return;
        const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
        const a    = document.createElement('a');
        a.href     = URL.createObjectURL(blob);
        a.download = 'config.json';
        a.click();
    },

    importConfig: async (file: File): Promise<{ error: string | null }> => {
        const text = await file.text();
        try {
            const { error } = await post<{ valid: boolean }>('/api/config/import', JSON.parse(text));
            return { error };
        } catch {
            return { error: 'Invalid JSON' };
        }
    },
};
