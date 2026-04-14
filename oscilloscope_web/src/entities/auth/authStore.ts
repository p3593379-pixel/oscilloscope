import { create } from 'zustand';
import { SessionMode, UserRole } from '@/generated/buf_connect_server_pb';

interface AuthState {
  bootstrapping:   boolean;
  isAuthenticated: boolean;
  accessToken:     string | null;
  streamToken:     string | null;
  sessionId:       string | null;  // per-tab identity
  tokenExp:        number;         // Unix epoch seconds, for scheduling refresh
  role:            UserRole;
  sessionMode:     SessionMode;

  setAuth: (params: {
    accessToken: string;
    role:        UserRole;
    sessionMode: SessionMode;
    sessionId:   string;
    tokenExp:    number;
  }) => void;
  setBootstrapping: (v: boolean) => void;
  setStreamToken:  (token: string) => void;
  setSessionMode:  (mode: SessionMode) => void;
  clearAuth:       () => void;
}

export const useAuthStore = create<AuthState>((set) => ({
  bootstrapping: true,
  isAuthenticated: false,
  accessToken:     null,
  streamToken:     null,
  sessionId:     null,
  tokenExp:      0,
  role:            UserRole.UNSPECIFIED,
  sessionMode:     SessionMode.UNSPECIFIED,

  setAuth: ({ accessToken, role, sessionMode, sessionId, tokenExp }) =>
      set({ isAuthenticated: true, accessToken, role, sessionMode, sessionId, tokenExp }),

  setBootstrapping: (v) => set({ bootstrapping: v }),

  setStreamToken: (token) => set({ streamToken: token }),

  setSessionMode: (mode) => set({ sessionMode: mode }),

  clearAuth: () =>
      set({
        isAuthenticated: false,
        accessToken:     null,
        streamToken:     null,
        role:            UserRole.UNSPECIFIED,
        sessionMode:     SessionMode.UNSPECIFIED,
      }),
}));