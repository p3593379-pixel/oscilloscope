/** Decode a JWT payload without verifying the signature (client-side only). */
export function decodeJwtPayload<T = Record<string, unknown>>(token: string): T {
  const base64 = token.split('.')[1];
  if (!base64) throw new Error('Invalid JWT');
  const json = atob(base64.replace(/-/g, '+').replace(/_/g, '/'));
  return JSON.parse(json) as T;
}

export function isTokenExpired(token: string): boolean {
  try {
    const { exp } = decodeJwtPayload<{ exp: number }>(token);
    return Date.now() / 1000 >= exp;
  } catch {
    return true;
  }
}
