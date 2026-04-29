import { create } from 'zustand';
import { SessionMode, UserRole } from '@/generated/buf_connect_server_pb';

interface SessionConflictInfo {
  startedAtUtc: string;
  role: string;
  pendingCallToken: string;
}

interface AuthState {
  isAuthenticated: boolean;
  callToken:       string | null;
  streamToken:     string | null;
  sessionId:       string | null;
  tokenExp:        number;
  role:            UserRole;
  sessionMode:     SessionMode;
  sessionConflict: SessionConflictInfo | null;
  logoutReason:    string | null;

  setAuth: (params: {
    callToken:   string;
    role:        UserRole;
    sessionMode: SessionMode;
    sessionId:   string;
    tokenExp:    number;
  }) => void;
  setCallToken:        (token: string) => void;
  setStreamToken:      (token: string) => void;
  setSessionMode:      (mode: SessionMode) => void;
  setSessionConflict:  (info: SessionConflictInfo | null) => void;
  setLogoutReason:     (reason: string | null) => void;
  clearAuth:           () => void;
}

export const useAuthStore = create<AuthState>((set) => ({
  isAuthenticated: false,
  callToken:       null,
  streamToken:     null,
  sessionId:       null,
  tokenExp:        0,
  role:            UserRole.UNSPECIFIED,
  sessionMode:     SessionMode.UNSPECIFIED,
  sessionConflict: null,
  logoutReason:    null,

  setAuth: ({ callToken, role, sessionMode, sessionId, tokenExp }) =>
      set({ isAuthenticated: true, callToken, role, sessionMode, sessionId, tokenExp,
        sessionConflict: null, logoutReason: null }),

  setCallToken:     (token) => set({ callToken: token }),
  setStreamToken:     (token) => set({ streamToken: token }),
  setSessionMode:     (mode)  => set({ sessionMode: mode }),
  setSessionConflict: (info)  => set({ sessionConflict: info }),
  setLogoutReason:    (reason) => set({ logoutReason: reason }),

  clearAuth: () => set({
    isAuthenticated: false,
    callToken:       null,
    streamToken:     null,
    sessionId:       null,
    tokenExp:        0,
    role:            UserRole.UNSPECIFIED,
    sessionMode:     SessionMode.UNSPECIFIED,
    sessionConflict: null,
  }),
}));