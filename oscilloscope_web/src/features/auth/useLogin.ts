// src/features/auth/useLogin.ts
import { useState, useCallback }  from 'react';
import { createClient }           from '@connectrpc/connect';
import { makeControlTransport }   from '@/shared/api/transport';
import { AuthService }            from '@/generated/buf_connect_server_pb';
import { useAuthStore }           from '@/entities/auth/authStore';
import { decodeJwtPayload }       from '@/shared/lib/jwtUtils';

export function useLogin() {
  const [loading, setLoading] = useState(false);
  const [error,   setError]   = useState<string | null>(null);
  const setAuth = useAuthStore((s) => s.setAuth);

  const submit = useCallback(async (username: string, password: string) => {
    setLoading(true);
    setError(null);
    try {
      const client   = createClient(AuthService, makeControlTransport());
      const response = await client.login({ username, password });

      const payload = decodeJwtPayload<{ exp: number }>(response.accessToken);
      setAuth({
        accessToken: response.accessToken,
        role:        response.role,
        sessionMode: response.sessionMode,
        sessionId:   response.sessionId,   // from LoginResponse.session_id
        tokenExp:    payload.exp,
      });
    } catch (e: unknown) {
      setError(e instanceof Error ? e.message : 'Login failed');
    } finally {
      setLoading(false);
    }
  }, [setAuth]);

  return { submit, loading, error };
}
