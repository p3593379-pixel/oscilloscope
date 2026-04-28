import { createConnectTransport } from '@connectrpc/connect-web';
import {CONTROL_PLANE_URL, DATA_PLANE_URL} from "@/shared/config/env.ts";

/**
 * Control-plane transport — used for AuthService, SessionService, AdminService,
 * and all device-specific RPCs that go through the nginx control plane.
 * The Authorization header is injected per-call by the hooks that use this.
 */
export function makeControlTransport(callToken?: string) {
    return createConnectTransport({
        baseUrl: CONTROL_PLANE_URL,
        useBinaryFormat: true,
        interceptors: callToken
            ? [(next) => (req) => {
                req.header.set('Authorization', `Bearer ${callToken}`);
                return next(req);
            }]
            : [],
    });
}

/**
 * Data-plane transport — used only for OscilloscopeService/StreamData.
 * No Authorization header: authentication is carried in the stream_token
 * message field, validated on the data plane without a DB lookup.
 */
export function makeDataTransport() {
  return createConnectTransport({
    baseUrl: DATA_PLANE_URL,   // nginx routes /oscilloscope_interface.v2.* to the data plane
    useBinaryFormat: true,
  });
}
