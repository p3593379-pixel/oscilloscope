import { useEffect, useRef }      from 'react';
import { createClient }           from '@connectrpc/connect';
import { makeControlTransport }   from '@/shared/api/transport';
import {
    SessionService,
    SessionMode,
    type SessionEvent,
} from '@/generated/buf_connect_server_pb';
import { useAuthStore }           from '@/entities/auth/authStore';

function applyEvent(event: SessionEvent) {
    const store = useAuthStore.getState();

    // Cast to a loose type so the switch compiles against both the current
    // (limited) generated union and the upcoming full proto regen.
    const ev = event.event as { case?: string; value?: unknown };

    switch (ev.case) {
        case 'onService':
            store.setSessionMode(SessionMode.ON_SERVICE);
            break;
        case 'serviceEnded':
            store.setSessionMode(SessionMode.ACTIVE);
            break;
        case 'vacantRole':
            store.setSessionMode(SessionMode.OBSERVER);
            break;
        case 'roleClaimed':
            store.setSessionMode(SessionMode.OBSERVER);
            break;
        case 'adminConflict':
            window.dispatchEvent(
                new CustomEvent('bcs:adminConflict', { detail: ev.value })
            );
            break;
        case 'conflictResolved':
            window.dispatchEvent(
                new CustomEvent('bcs:conflictResolved', { detail: ev.value })
            );
            break;
        case 'forcedLogout': {
            const payload  = ev.value as { reason?: string } | undefined;
            const reason   = payload?.reason ?? '';
            const msg =
                reason === 'taken_over'
                    ? 'Your session was ended because someone signed in to your account from another location.'
                    : reason === 'session_expired'
                        ? 'Your session expired due to inactivity.'
                        : 'You were signed out.';
            store.setLogoutReason(msg);
            store.clearAuth();
            break;
        }
        default:
            break;
    }
}

export function useSession() {
    const callToken       = useAuthStore(s => s.callToken);      // ← renamed from accessToken
    const isAuthenticated = useAuthStore(s => s.isAuthenticated);
    const setStreamToken  = useAuthStore(s => s.setStreamToken);
    const abortRef        = useRef<AbortController | null>(null);

    useEffect(() => {
        if (!isAuthenticated || !callToken) return;

        const abort  = new AbortController();
        abortRef.current = abort;

        const transport = makeControlTransport(callToken);
        const client    = createClient(SessionService, transport);

        const run = async () => {
            try {
                const { streamToken } = await client.getStreamToken({});
                setStreamToken(streamToken);
            } catch (e) {
                console.warn('[useSession] getStreamToken failed:', e);
            }

            try {
                for await (const event of client.watchSessionEvents(
                    {},
                    { signal: abort.signal }
                )) {
                    applyEvent(event);
                }
            } catch (e: unknown) {
                if ((e as Error)?.name !== 'AbortError') {
                    console.error('[useSession] stream error:', e);
                }
            }
        };

        run();
        return () => abort.abort();
    }, [isAuthenticated, callToken, setStreamToken]);
}
