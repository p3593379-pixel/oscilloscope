// Opens the WatchSessionEvents stream as soon as the user is authenticated,
// keeps it alive for the lifetime of the session, and fetches a stream token
// immediately after the stream is established.
//
// Incoming session events are applied to the auth store so that every
// consumer (AuthGuard, Oscilloscope page, conflict dialogs) reacts in
// real time without extra polling.

import { useEffect, useRef }     from 'react';
import { createClient }          from '@connectrpc/connect';
import { makeControlTransport }  from '@/shared/api/transport';
import {
    SessionService,
    SessionMode,
    type SessionEvent,
} from '@/generated/buf_connect_server_pb';
import { useAuthStore }          from '@/entities/auth/authStore';

function applyEvent(event: SessionEvent) {
    const store = useAuthStore.getState();

    switch (event.event.case) {
        case 'onService':
            store.setSessionMode(SessionMode.ON_SERVICE);
            break;
        case 'serviceEnded':
            store.setSessionMode(SessionMode.ACTIVE);
            break;
        case 'vacantRole':
            // Server signals the role is free; UI may prompt the user to claim it
            store.setSessionMode(SessionMode.OBSERVER);
            break;
        case 'roleClaimed':
            // Another engineer claimed active — demote self to observer
            store.setSessionMode(SessionMode.OBSERVER);
            break;
        case 'adminConflict':
            // Current admin is notified — dispatch a custom DOM event so that
            // the AdminConflictDialog component can render independently.
            window.dispatchEvent(
                new CustomEvent('bcs:adminConflict', { detail: event.event.value })
            );
            break;
        case 'conflictResolved':
            window.dispatchEvent(
                new CustomEvent('bcs:conflictResolved', { detail: event.event.value })
            );
            break;
        case 'forcedLogout':
            store.clearAuth();
            break;
        default:
            break;
    }
}

export function useSession() {
    const accessToken     = useAuthStore((s) => s.accessToken);
    const isAuthenticated = useAuthStore((s) => s.isAuthenticated);
    const setStreamToken  = useAuthStore((s) => s.setStreamToken);
    const abortRef        = useRef<AbortController | null>(null);

    useEffect(() => {
        if (!isAuthenticated || !accessToken) return;

        const abort  = new AbortController();
        abortRef.current = abort;

        const transport = makeControlTransport(accessToken);
        const client    = createClient(SessionService, transport);

        const run = async () => {
            // ── 1. Fetch a stream token immediately (Option A: tier-agnostic) ──
            try {
                const { streamToken } = await client.getStreamToken({});
                setStreamToken(streamToken);
            } catch (e) {
                console.warn('[useSession] getStreamToken failed:', e);
            }

            // ── 2. Open WatchSessionEvents — blocks until disconnected ──────────
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
    }, [isAuthenticated, accessToken, setStreamToken]);
}
