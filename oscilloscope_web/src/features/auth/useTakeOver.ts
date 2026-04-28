import { useCallback, useState } from 'react';
import { createClient }          from '@connectrpc/connect';
import { makeControlTransport }  from '@/shared/api/transport';
import { AuthService }           from '@/generated/buf_connect_server_pb';
import { useAuthStore }          from '@/entities/auth/authStore';
import { useRenewCallToken }     from './useRenewCallToken';
import { decodeJwtPayload }      from '@/shared/lib/jwtUtils';

export function useTakeOver() {
    const [loading, setLoading] = useState(false);
    const [error, setError]     = useState<string | null>(null);
    const setAuth               = useAuthStore(s => s.setAuth);
    const setSessionConflict    = useAuthStore(s => s.setSessionConflict);
    const { scheduleRenewal }   = useRenewCallToken();

    const takeOver = useCallback(async (): Promise<boolean> => {
        setLoading(true);
        setError(null);
        try {
            // The session_ticket cookie set during Login (even on conflict) is sent automatically
            const client   = createClient(AuthService, makeControlTransport());
            const response = await client.takeOver({});

            const payload = decodeJwtPayload<{ exp: number }>(response.callToken);
            setAuth({
                callToken:   response.callToken,
                role:        response.role,
                sessionMode: response.sessionMode,
                sessionId:   response.sessionId,
                tokenExp:    payload.exp,
            });
            setSessionConflict(null);
            scheduleRenewal(payload.exp);
            return true;
        } catch (e: unknown) {
            setError(e instanceof Error ? e.message : 'Take-over failed');
            return false;
        } finally {
            setLoading(false);
        }
    }, [setAuth, setSessionConflict, scheduleRenewal]);

    return { takeOver, loading, error };
}
