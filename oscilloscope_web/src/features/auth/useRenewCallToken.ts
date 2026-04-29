import { useCallback, useRef }  from 'react';
import { createClient }         from '@connectrpc/connect';
import { makeControlTransport } from '@/shared/api/transport';
import { AuthService }          from '@/generated/buf_connect_server_pb';
import { useAuthStore }         from '@/entities/auth/authStore';
import { decodeJwtPayload }     from '@/shared/lib/jwtUtils';

export function useRenewCallToken() {
    const setCallToken     = useAuthStore(s => s.setCallToken);
    const clearAuth       = useAuthStore(s => s.clearAuth);
    const setLogoutReason = useAuthStore(s => s.setLogoutReason);
    const timerRef        = useRef<ReturnType<typeof setTimeout> | null>(null);

    const cancel = useCallback(() => {
        if (timerRef.current) clearTimeout(timerRef.current);
    }, []);

    // Declared with useRef so renew() and scheduleRenewal() can mutually reference each other.
    const scheduleRenewalRef = useRef<(expSec: number) => void>(() => {});

    const renew = useCallback(async (): Promise<boolean> => {
        try {
            const currentToken = useAuthStore.getState().callToken;
            if (!currentToken) {
                clearAuth();
                return false;
            }
            const client = createClient(AuthService, makeControlTransport(currentToken));
            const response = await client.renewCallToken({});
            console.log("renewCallToken")
            const payload = decodeJwtPayload<{ exp: number }>(response.callToken);
            setCallToken(response.callToken);
            scheduleRenewalRef.current(payload.exp);
            return true;
        } catch {
            clearAuth();
            setLogoutReason('Your session was ended from another location.');
            return false;
        }
    }, [setCallToken, clearAuth, setLogoutReason]);

    const scheduleRenewal = useCallback((expSec: number) => {
        cancel();
        const nowSec   = Date.now() / 1000;
        const ttlSec   = expSec - nowSec;               // how long until expiry
        const leadSec  = Math.max(ttlSec * 0.2, 5);     // renew at 80% of TTL, min 5s lead
        const delaySec = Math.max(ttlSec - leadSec, 2);
        timerRef.current = setTimeout(() => renew(), delaySec * 1000);
    }, [cancel, renew]);

    // Sync the ref so renew() can call the latest scheduleRenewal
    scheduleRenewalRef.current = scheduleRenewal;

    return { renew, scheduleRenewal, cancel };
}
