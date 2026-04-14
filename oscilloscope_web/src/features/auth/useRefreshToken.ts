// useRefreshToken.ts
import { useCallback, useRef } from 'react';
import { createClient }         from '@connectrpc/connect';
import { makeControlTransport } from '@/shared/api/transport';
import { AuthService, SessionService } from '@/generated/buf_connect_server_pb';
import { useAuthStore }         from '@/entities/auth/authStore';
import { decodeJwtPayload }     from '@/shared/lib/jwtUtils';

export function useRefreshToken() {
    const setAuth         = useAuthStore((s) => s.setAuth);
    const clearAuth       = useAuthStore((s) => s.clearAuth);
    const setBootstrapping = useAuthStore((s) => s.setBootstrapping);
    const timerRef        = useRef<ReturnType<typeof setTimeout> | null>(null);

    const scheduleNext = useCallback((expSec: number) => {
        if (timerRef.current) clearTimeout(timerRef.current);
        const delaySec = Math.max(expSec - Date.now() / 1000 - 60, 10);
        timerRef.current = setTimeout(() => refresh(), delaySec * 1000);
    }, []);  // refresh added below via closure after declaration

    const refresh = useCallback(async (): Promise<boolean> => {
        try {
            const client   = createClient(AuthService, makeControlTransport());
            const response = await client.refresh({});

            const payload = decodeJwtPayload<{ exp: number }>(response.accessToken);
            setAuth({
                accessToken: response.accessToken,
                role:        response.role,
                sessionMode: response.sessionMode,
                sessionId:   response.sessionId,    // ← new
                tokenExp:    payload.exp,           // ← new
            });

            // Send heartbeat immediately after refresh so SessionManager knows
            // which session_id this tab maps to
            try {
                const sessionClient = createClient(
                    SessionService, makeControlTransport(response.accessToken));
                await sessionClient.heartbeat({ sessionId: response.sessionId });
            } catch { /* non-fatal */ }

            scheduleNext(payload.exp);
            return true;
        } catch {
            clearAuth();
            return false;
        } finally {
            setBootstrapping(false);   // ← always clear bootstrap flag
        }
    }, [setAuth, clearAuth, setBootstrapping, scheduleNext]);

    // Expose cancel so App can clean up on unmount
    const cancel = useCallback(() => {
        if (timerRef.current) clearTimeout(timerRef.current);
    }, []);

    return { refresh, cancel };
}
