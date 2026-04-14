// All runtime-injected env values live here.
// VITE_DATA_PLANE_URL defaults to same origin (Nginx or single-interface mode).
export const CONTROL_PLANE_URL = import.meta.env.VITE_CONTROL_PLANE_URL ?? '';
export const DATA_PLANE_URL    = import.meta.env.VITE_DATA_PLANE_URL    ?? '';
