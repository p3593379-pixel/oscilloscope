import { BrowserRouter, Routes, Route, Navigate } from 'react-router';
import { lazy, Suspense } from 'react';
import { LoginPage }      from '@/pages/login/LoginPage';
import { AuthGuard }      from './AuthGuard';
import { Spinner }        from '@/shared/ui/Spinner';

const OscilloscopePage = lazy(() =>
    import('@/pages/oscilloscope/OscilloscopePage')
        .then((m) => ({ default: m.OscilloscopePage }))
);

export function AppRouter() {
    return (
        <BrowserRouter>
            <Routes>
                <Route path="/login" element={<LoginPage />} />
                <Route
                    path="/oscilloscope"
                    element={
                        <AuthGuard>
                            <Suspense fallback={
                                <Spinner style={{
                                    position: 'fixed', top: '50%', left: '50%',
                                    transform: 'translate(-50%,-50%)'
                                }} />
                            }>
                                <OscilloscopePage />
                            </Suspense>
                        </AuthGuard>
                    }
                />
                <Route path="*" element={<Navigate to="/login" replace />} />
            </Routes>
        </BrowserRouter>
    );
}
