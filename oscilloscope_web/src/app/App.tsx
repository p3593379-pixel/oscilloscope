import './styles/global.css';
import { AppRouter }    from './router';
import { useSession }   from '@/features/session/useSession';
import { useEffect }    from 'react';
import { useAuthStore } from '@/entities/auth/authStore';

export default function App() {
  const sessionId = useAuthStore((s) => s.sessionId);

  useEffect(() => {
    const onBeforeUnload = (e: BeforeUnloadEvent) => {
      e.preventDefault();
      e.returnValue = '';
    };
    const onPageHide = () => {
      if (sessionId) {
        navigator.sendBeacon('/buf_connect_server.v2.SessionService/Heartbeat');
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
