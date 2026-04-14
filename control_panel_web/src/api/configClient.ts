// FILE: control_panel_web/src/api/configClient.ts

export interface TlsConfig {
    enabled: boolean;
    cert_path: string;
    key_path: string;
    min_version: string;
}

export interface InterfaceConfig {
    bind_address: string;
    port: number;
    enabled: boolean;
    tls: TlsConfig;
}

export interface NetworkConfig {
    control_plane: InterfaceConfig;
    data_plane: InterfaceConfig;
    single_interface_mode: boolean;
}

export interface NetworkSaveResponse extends NetworkConfig {
    restart_required?: boolean;
}

export interface AuthConfig {
    access_token_ttl_seconds: number;
    refresh_token_ttl_seconds: number;
    stream_token_ttl_seconds: number;
    // jwt_secret intentionally omitted — never returned by API
}

export interface SessionConfig {
    admin_conflict_timeout_seconds: number;
    snooze_duration_seconds: number;
    max_concurrent_sessions: number;
}

export interface StreamingConfig {
    max_clients: number;
    frame_size_bytes: number;
    compression_enabled: boolean;
    compression_algorithm: string;
    compression_threshold_bytes: number;
    backpressure_policy: string;
}

export interface LogConfig {
    level: string;
    destination: string;
    file_path: string;
    max_size_mb: number;
    max_files: number;
    access_log: boolean;
}

export interface MetricsConfig {
    enabled: boolean;
    path: string;
}

export type UserRole = 'admin' | 'engineer';

export interface UserInfo {
    user_id: number;
    username: string;
    role: UserRole;
    created_at: number;   // unix seconds
    last_login: number;   // unix seconds, 0 = never
}

export interface CreateUserRequest {
    username: string;
    password: string;
    role: UserRole;
}

export interface ResetPasswordRequest {
    username: string;
    new_password: string;
}

export interface ServerConfig {
    control_plane: InterfaceConfig;
    data_plane: InterfaceConfig;
    single_interface_mode: boolean;
    auth: AuthConfig;
    session: SessionConfig;
    streaming: StreamingConfig;
    log: LogConfig;
    metrics: MetricsConfig;
    users: UserInfo[];
}

type ApiResult<T> = Promise<{ data: T | null; error: string | null }>;

async function get<T>(path: string): ApiResult<T> {
    try {
        const res = await fetch(path);
        if (!res.ok) return { data: null, error: `HTTP ${res.status}` };
        return { data: (await res.json()) as T, error: null };
    } catch (e) {
        return { data: null, error: String(e) };
    }
}

async function put<T>(path: string, body: T): ApiResult<T> {
    try {
        const res = await fetch(path, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body),
        });
        if (!res.ok) return { data: null, error: `HTTP ${res.status}` };
        return { data: (await res.json()) as T, error: null };
    } catch (e) {
        return { data: null, error: String(e) };
    }
}

async function post<T>(path: string, body: unknown): ApiResult<T> {
    try {
        const res = await fetch(path, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body),
        });
        if (!res.ok) {
            const err = await res.json().catch(() => ({ error: `HTTP ${res.status}` }));
            return { data: null, error: (err as { error?: string }).error ?? `HTTP ${res.status}` };
        }
        return { data: (await res.json()) as T, error: null };
    } catch (e) {
        return { data: null, error: String(e) };
    }
}

async function del<T>(path: string): ApiResult<T> {
    try {
        const res = await fetch(path, { method: 'DELETE' });
        if (!res.ok) {
            const err = await res.json().catch(() => ({ error: `HTTP ${res.status}` }));
            return { data: null, error: (err as { error?: string }).error ?? `HTTP ${res.status}` };
        }
        if (res.status === 204) return { data: null, error: null };
        return { data: (await res.json()) as T, error: null };
    } catch (e) {
        return { data: null, error: String(e) };
    }
}

export const configClient = {
    getAll:      ()                     => get<ServerConfig>('/api/config'),
    putAll:      (c: ServerConfig)      => put<ServerConfig>('/api/config', c),
    getNetwork:  ()                     => get<NetworkConfig>('/api/config/network'),
    putNetwork: (c: NetworkConfig)      => put<NetworkSaveResponse>('/api/config/network', c),
    getAuth:     ()                     => get<AuthConfig>('/api/config/auth'),
    putAuth:     (c: AuthConfig)        => put<AuthConfig>('/api/config/auth', c),
    getStreaming:()                     => get<StreamingConfig>('/api/config/streaming'),
    putStreaming: (c: StreamingConfig)  => put<StreamingConfig>('/api/config/streaming', c),
    getSession:  ()                     => get<SessionConfig>('/api/config/session'),
    putSession:  (c: SessionConfig)     => put<SessionConfig>('/api/config/session', c),
    getLog:      ()                     => get<LogConfig>('/api/config/log'),
    putLog:      (c: LogConfig)         => put<LogConfig>('/api/config/log', c),
    getMetrics:  ()                     => get<MetricsConfig>('/api/config/metrics'),
    putMetrics:  (c: MetricsConfig)     => put<MetricsConfig>('/api/config/metrics', c),
    listUsers:     ()                             => get<{ users: UserInfo[] }>('/api/users'),
    createUser:    (c: CreateUserRequest)       => post<UserInfo>('/api/users', c),
    deleteUser:    (userId: string)               => del<{ success: boolean }>(`/api/users/${userId}`),

    resetPassword: async (c: ResetPasswordRequest) => {
        let r = await get<{ users: UserInfo[] }>('/api/users');
        let userId = -1;
        for (let i = 0; i < (r.data?.users.length ?? 0); i++) {
            if (r.data?.users[i].username == c.username)
                userId = r.data?.users[i].user_id;
        }
        let success = false;
        return put<{ success: boolean }>(`/api/users/${userId}/password`, {success})
    },

    setJwtSecret: (secret: string) =>
        fetch('/api/config/auth/jwt-secret', {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ jwt_secret: secret }),
        }),

    exportConfig: async (): Promise<void> => {
        const res = await fetch('/api/config/export', { method: 'POST' });
        const blob = await res.blob();
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'config.json';
        a.click();
        URL.revokeObjectURL(url);
    },

    importConfig: async (file: File): ApiResult<{ valid: boolean }> => {
        const text = await file.text();
        try {
            const res = await fetch('/api/config/import', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: text,
            });
            if (!res.ok) {
                const err = await res.json().catch(() => ({ error: `HTTP ${res.status}` }));
                return { data: null, error: (err as { error: string }).error };
            }
            return { data: await res.json() as { valid: boolean }, error: null };
        } catch (e) {
            return { data: null, error: String(e) };
        }
    },

    restart: () => fetch('/api/restart', { method: 'POST' }),
};
