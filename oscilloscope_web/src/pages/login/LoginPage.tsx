import {
  useState, useEffect, useRef, useCallback,
} from 'react';
import { useNavigate }       from 'react-router';
import SnakeLines            from './components/SnakeLines';
import type { SnakeLinesHandle } from './components/SnakeLines';
import { LoginForm }         from './components/LoginForm';
import { useLogin }          from '@/features/auth/useLogin';
import { useTakeOver }       from '@/features/auth/useTakeOver';
import { useAuthStore }      from '@/entities/auth/authStore';
import styles                from './LoginPage.module.css';

type UiState = 'intro' | 'login' | 'login_outro' | 'done';

function SessionConflictDialog({
                                 startedAtUtc,
                                 role,
                                 onTakeOver,
                                 onCancel,
                                 loading,
                                 error,
                               }: {
  startedAtUtc: string;
  role: string;
  onTakeOver: () => void;
  onCancel: () => void;
  loading: boolean;
  error: string | null;
}) {
  return (
      <div style={{
        position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.6)',
        display: 'flex', alignItems: 'center', justifyContent: 'center',
        zIndex: 100,
      }}>
        <div style={{
          background: '#1a1a2e', border: '1px solid #444', borderRadius: 8,
          padding: '2rem', maxWidth: 400, color: '#eee',
        }}>
          <h3 style={{ marginTop: 0 }}>Active session detected</h3>
          <p>
            This account already has an active session
            {role ? ` (${role})` : ''} started at{' '}
            <strong>{startedAtUtc}</strong>.
          </p>
          <p>Would you like to take control? The other session will be disconnected.</p>
          {error && <p style={{ color: '#f66' }}>{error}</p>}
          <div style={{ display: 'flex', gap: '1rem', marginTop: '1.5rem' }}>
            <button onClick={onTakeOver} disabled={loading}
                    style={{ flex: 1, padding: '0.6rem', cursor: 'pointer',
                      background: '#e05', color: '#fff', border: 'none', borderRadius: 4 }}>
              {loading ? 'Taking control…' : 'Take control'}
            </button>
            <button onClick={onCancel} disabled={loading}
                    style={{ flex: 1, padding: '0.6rem', cursor: 'pointer',
                      background: '#333', color: '#eee', border: 'none', borderRadius: 4 }}>
              Stay here
            </button>
          </div>
        </div>
      </div>
  );
}

export function LoginPage() {
  const navigate = useNavigate();
  const { submit, loading } = useLogin();
  const { takeOver, loading: takeOverLoading, error: takeOverError } = useTakeOver();
  const isAuthenticated  = useAuthStore(s => s.isAuthenticated);
  const sessionConflict  = useAuthStore(s => s.sessionConflict);
  const logoutReason     = useAuthStore(s => s.logoutReason);
  const setSessionConflict = useAuthStore(s => s.setSessionConflict);
  const setLogoutReason  = useAuthStore(s => s.setLogoutReason);

  const [uiState,    setUiState]    = useState<UiState>('intro');
  const [statusText, setStatusText] = useState('Initializing…');
  const [formError,  setFormError]  = useState<string | null>(null);
  const [isNarrow,   setIsNarrow]   = useState(window.innerWidth <= 900);
  const snakesRef = useRef<SnakeLinesHandle | null>(null);

  // Viewport tracking
  useEffect(() => {
    const onResize = () => setIsNarrow(window.innerWidth <= 900);
    window.addEventListener('resize', onResize);
    return () => window.removeEventListener('resize', onResize);
  }, []);

  // Intro animation on mount — always ends at login form
  useEffect(() => {
    const timer = setTimeout(() => {
      snakesRef.current?.startIntroAnimation();
      setTimeout(() => {
        setUiState('login');
        setStatusText('Ready');
      }, 1600);
    }, 100);
    return () => clearTimeout(timer);
  }, []);

  // Outro: trigger when auth succeeds
  useEffect(() => {
    if (!isAuthenticated) return;
    if (uiState === 'login') {
      setUiState('login_outro');
      setStatusText('Login successful');
      snakesRef.current?.startOutroAnimation();
    }
  }, [isAuthenticated, uiState]);

  const handleOutroFinished = useCallback(() => {
    if (isAuthenticated) {
      setUiState('done');
      navigate('/oscilloscope', { replace: true });
    }
  }, [isAuthenticated, navigate]);

  const handleSubmit = async (credentials: { username: string; password: string }) => {
    setFormError(null);
    setLogoutReason(null);
    setStatusText('Authenticating…');
    const result = await submit(credentials.username, credentials.password);
    if (result.ok) {
      // outro triggered by isAuthenticated effect above
    } else if (result.conflict) {
      setStatusText('Ready');
      // sessionConflict is set in store by useLogin — dialog renders below
    } else {
      setFormError(result.error);
      setStatusText('Ready');
    }
  };

  const handleTakeOver = async () => {
    const ok = await takeOver();
    if (ok) {
      // isAuthenticated will flip true → outro effect fires
    }
  };

  return (
      <div className={styles.root}>
        <SnakeLines
            ref={snakesRef}
            onOutroFinished={handleOutroFinished}
            isNarrow={isNarrow}
        />

        {uiState === 'login' && (
            <div className={styles.formLayer}>
              <div className={styles.loginPanel}>
                {logoutReason && (
                    <div style={{
                      background: '#3a1a1a', border: '1px solid #a44',
                      borderRadius: 4, padding: '0.75rem 1rem',
                      marginBottom: '1rem', color: '#f99', fontSize: '0.875rem',
                    }}>
                      {logoutReason}
                    </div>
                )}
                <LoginForm
                    onSubmit={handleSubmit}
                    loading={loading}
                    error={formError}
                    statusText={statusText}
                />
              </div>
            </div>
        )}

        {sessionConflict && (
            <SessionConflictDialog
                startedAtUtc={sessionConflict.startedAtUtc}
                role={sessionConflict.role}
                onTakeOver={handleTakeOver}
                onCancel={() => setSessionConflict(null)}
                loading={takeOverLoading}
                error={takeOverError}
            />
        )}
      </div>
  );
}
