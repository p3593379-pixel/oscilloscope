import type { ReactNode } from 'react';
import { Navigate }       from 'react-router';
import { useAuthStore }   from '@/entities/auth/authStore';

export function AuthGuard({ children }: { children: ReactNode }) {
    const isAuthenticated = useAuthStore((s) => s.isAuthenticated);
    return isAuthenticated ? <>{children}</> : <Navigate to="/login" replace />;
}
