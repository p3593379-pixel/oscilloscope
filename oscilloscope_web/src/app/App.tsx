import './styles/global.css';
import { AppRouter }       from './router';
import { useSession }      from '@/features/session/useSession';
import { useRefreshToken } from '@/features/auth/useRefreshToken';
import { useEffect }       from 'react';
import { useAuthStore }    from '@/entities/auth/authStore';

export default function App() {
  const { refresh, cancel } = useRefreshToken();
  const sessionId = useAuthStore((s) => s.sessionId);

  // Bootstrap: attempt silent restore on mount
  useEffect(() => {
    refresh();
    return () => cancel();
  }, []);   // eslint-disable-line react-hooks/exhaustive-deps

  // Best-effort session end on tab close
  useEffect(() => {
    const onBeforeUnload = (e: BeforeUnloadEvent) => {
      e.preventDefault();
      e.returnValue = '';   // triggers browser's generic "leave page?" dialog
    };
    const onPageHide = () => {
      if (sessionId) {
        // sendBeacon fires even as the page is being torn down
        navigator.sendBeacon(
            '/buf_connect_server.v2.SessionService/Heartbeat',
            // empty body — server just needs the session to exist;
            // proper logout RPC can be added later
        );
      }
    };
    window.addEventListener('beforeunload', onBeforeUnload);
    window.addEventListener('pagehide', onPageHide);
    return () => {
      window.removeEventListener('beforeunload', onBeforeUnload);
      window.removeEventListener('pagehide', onPageHide);
    };
  }, [sessionId]);

  useSession();
  return <AppRouter />;
}
