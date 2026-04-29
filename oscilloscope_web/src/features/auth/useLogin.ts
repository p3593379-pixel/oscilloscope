import { useState, useCallback }  from 'react';
import { createClient }           from '@connectrpc/connect';
import { makeControlTransport }   from '@/shared/api/transport';
import { AuthService }            from '@/generated/buf_connect_server_pb';
import { useAuthStore }           from '@/entities/auth/authStore';
import { useRenewCallToken }      from './useRenewCallToken';
import { decodeJwtPayload }       from '@/shared/lib/jwtUtils';

export type LoginResult =
    | { ok: true }
    | { ok: false; conflict: true }
    | { ok: false; conflict: false; error: string };

export function useLogin() {
  const [loading, setLoading] = useState(false);
  const setAuth            = useAuthStore(s => s.setAuth);
  const setSessionConflict = useAuthStore(s => s.setSessionConflict);
  const { scheduleRenewal } = useRenewCallToken();

  const submit = useCallback(async (
      username: string, password: string
  ): Promise<LoginResult> => {
    setLoading(true);
    try {
      const client   = createClient(AuthService, makeControlTransport());
      const response = await client.login({ username, password });

      if (response.sessionConflict) {
        setSessionConflict({
          startedAtUtc: response.conflictInfo?.startedAtUtc ?? '',
          role:         response.conflictInfo?.role ?? '',
          pendingCallToken: response.callToken,
        });
        return { ok: false, conflict: true };
      }

      const payload = decodeJwtPayload<{ exp: number }>(response.callToken);
      setAuth({
        callToken:   response.callToken,
        role:        response.role,
        sessionMode: response.sessionMode,
        sessionId:   response.sessionId,
        tokenExp:    payload.exp,
      });
      scheduleRenewal(payload.exp);
      return { ok: true };
    } catch (e: unknown) {
      return {
        ok: false, conflict: false,
        error: e instanceof Error ? e.message : 'Login failed',
      };
    } finally {
      setLoading(false);
    }
  }, [setAuth, setSessionConflict, scheduleRenewal]);

  return { submit, loading };
}
