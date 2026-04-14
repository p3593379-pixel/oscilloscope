import type { ReactNode } from 'react';
import { Navigate }       from 'react-router';
import { useAuthStore }   from '@/entities/auth/authStore';
import { Spinner }        from '@/shared/ui/Spinner';

export function AuthGuard({ children }: { children: ReactNode }) {
  const { isAuthenticated, bootstrapping } = useAuthStore();

  // While bootstrap refresh is in flight, render nothing —
  // LoginPage is already showing its intro animation
  if (bootstrapping) return (
      <Spinner style={{ position: 'fixed', top: '50%', left: '50%',
        transform: 'translate(-50%,-50%)' }} />
  );

  return isAuthenticated
      ? <>{children}</>
      : <Navigate to="/login" replace />;
}
