import {
  useState, useEffect, useRef, useCallback,
} from 'react';
import { useNavigate }             from 'react-router';
import SnakeLines                  from './components/SnakeLines';
import type { SnakeLinesHandle }   from './components/SnakeLines';
import { LoginForm }               from './components/LoginForm';
import { useLogin }                from '@/features/auth/useLogin';
import { useAuthStore }            from '@/entities/auth/authStore';
import styles                      from './LoginPage.module.css';

type UiState = 'intro' | 'login' | 'login_outro' | 'done';

export function LoginPage() {
  const navigate = useNavigate();
  const { submit, loading, error } = useLogin();
  const isAuthenticated = useAuthStore((s) => s.isAuthenticated);

  const [uiState,    setUiState]    = useState<UiState>('intro');
  const [statusText, setStatusText] = useState('Initializing…');
  const [isNarrow,   setIsNarrow]   = useState(window.innerWidth <= 900);

  const snakesRef = useRef<SnakeLinesHandle | null>(null);

  setTimeout(() => {
    const store = useAuthStore.getState();
    if (store.bootstrapping) {
      // Refresh still in flight — show form optimistically;
      // the outro effect will fire when refresh resolves
      setUiState('login');
      setStatusText('Ready');
    } else if (store.isAuthenticated) {
      // Refresh already resolved during intro — go straight to outro
      setUiState('login_outro');
      setStatusText('Session restored');
      snakesRef.current?.startOutroAnimation();
    } else {
      // No valid session — show login form
      setUiState('login');
      setStatusText('Ready');
    }
  }, 1600);

  // ── Viewport tracking ─────────────────────────────────────────────────────
  useEffect(() => {
    const onResize = () => setIsNarrow(window.innerWidth <= 900);
    window.addEventListener('resize', onResize);
    return () => window.removeEventListener('resize', onResize);
  }, []);

  // ── Intro: play once on mount ─────────────────────────────────────────────
  useEffect(() => {
    const timer = setTimeout(() => {
      snakesRef.current?.startIntroAnimation();
      setTimeout(() => {
        // If refresh already succeeded during intro, go straight to outro
        if (useAuthStore.getState().isAuthenticated) {
          setUiState('login_outro');
          setStatusText('Session restored');
          snakesRef.current?.startOutroAnimation();
        } else {
          setUiState('login');
          setStatusText('Ready');
        }
      }, 1600);
    }, 100);
    return () => clearTimeout(timer);
  }, []);

// ── Outro: trigger as soon as auth succeeds AND intro is done ─────────────
  useEffect(() => {
    if (!isAuthenticated) return;

    if (uiState === 'login') {
      // Normal path: user submitted the form
      setUiState('login_outro');
      setStatusText('Login successful');
      snakesRef.current?.startOutroAnimation();
    } else if (uiState === 'intro') {
      // Refresh resolved before intro finished — skip straight to outro
      // when intro completes (handled in the intro timer below)
      setStatusText('Session restored');
    }
  }, [isAuthenticated, uiState]);

  const handleOutroFinished = useCallback(() => {
    if (isAuthenticated) {
      setUiState('done');
      navigate('/oscilloscope', { replace: true });
    }
  }, [isAuthenticated, navigate]);

  const handleSubmit = async (credentials: { username: string; password: string }) => {
    setStatusText('Authenticating…');
    await submit(credentials.username, credentials.password);
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
            <LoginForm
                onSubmit={handleSubmit}
                loading={loading}
                error={error}
                statusText={statusText}
            />
          </div>
        </div>
      )}
    </div>
  );
}
