import { useState }        from 'react';
import type { FormEvent } from 'react';
import { Spinner }         from '@/shared/ui/Spinner';
import { Button }          from '@/shared/ui/Button';
import styles              from './LoginForm.module.css';
import logo from '../../../../images/Logo.svg'

interface Props {
  onSubmit:   (c: { username: string; password: string }) => void;
  loading:    boolean;
  error:      string | null;
  statusText: string;
}

export function LoginForm({ onSubmit, loading, error, statusText }: Props) {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');

  const handleSubmit = (e: FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    if (!loading) onSubmit({ username, password });
  };

  return (
    <form className={styles.card} onSubmit={handleSubmit} noValidate>
      <div className={styles.logo_container} aria-label="Oscilloscope">
        <img src={logo} alt="Logo" className={styles.logo}/>
      </div>

      <h1 className={styles.title}>Oscilloscope</h1>

        <div>
            <label className={styles.label} htmlFor="login-user">Username</label>
            <input
                id="login-user"
                className={styles.input}
                type="text"
                autoComplete="username"
                value={username}
                onChange={(e) => setUsername(e.target.value)}
                disabled={loading}
                required
            />
        </div>

        <div>
            <label className={styles.label} htmlFor="login-pass">Password</label>
            <input
                id="login-pass"
                className={styles.input}
                type="password"
                autoComplete="current-password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                disabled={loading}
                required
            />
        </div>

        {error && <p className={styles.error} role="alert">{error}</p>}

        <Button type="submit" disabled={loading} className={styles.submitBtn}>
            {loading ? <Spinner size={16}/> : 'Sign in'}
        </Button>

        <p className={styles.status}>{statusText}</p>
    </form>
  );
}
