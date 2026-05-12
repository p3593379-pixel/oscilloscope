// src/features/oscilloscope-settings/useOscilloscopeSettings.ts
import { useCallback, useState } from 'react';
import { createClient }          from '@connectrpc/connect';
import { create }                from '@bufbuild/protobuf';
import { FieldMaskSchema }       from '@bufbuild/protobuf/wkt';
import { makeControlTransport }  from '@/shared/api/transport';
import { OscilloscopeService }   from '@/generated/oscilloscope_interface_pb';
import { useAuthStore }          from '@/entities/auth/authStore';
import {
    useSettingsStore,
    type TriggerMode,
    type TriggerEdge,
} from '@/entities/oscilloscopeSettings/settingsStore';

// Maps proto OscilloscopeSettings → zustand store
function applyServerSettings(s: import('@/generated/oscilloscope_interface_pb').OscilloscopeSettings) {
    const store = useSettingsStore.getState();
    if (s.targetFps)          store.setTargetFps(s.targetFps);
    if (s.maxBandwidthMbps !== undefined) store.setMaxBandwidth(s.maxBandwidthMbps);
    if (s.trigger) {
        const modeMap: Record<number, TriggerMode> = { 1: 'AUTO', 2: 'NORMAL', 3: 'SINGLE' };
        const edgeMap: Record<number, TriggerEdge> = { 1: 'RISING', 2: 'FALLING' };
        if (s.trigger.mode)    store.setTriggerMode(modeMap[s.trigger.mode] ?? 'AUTO');
        if (s.trigger.edge)    store.setTriggerEdge(edgeMap[s.trigger.edge] ?? 'RISING');
        if (s.trigger.levelV !== undefined) store.setTriggerLevel(s.trigger.levelV);
        if (s.trigger.channel !== undefined) store.setTriggerChannel(s.trigger.channel);
    }
    s.channels.forEach((ch, i) => {
        if (ch.enabled !== undefined)       store.setChannelVisible(i, ch.enabled);
        if (ch.voltsPerDiv !== undefined)   store.setVoltsPerDiv(i, ch.voltsPerDiv);
        if (ch.verticalOffset !== undefined) store.setVerticalOffset(i, ch.verticalOffset);
    });
    if (s.naturalUnits !== undefined) store.setNaturalUnits(s.naturalUnits);
}

export function useOscilloscopeSettings() {
    const callToken = useAuthStore(s => s.callToken);
    const [loading, setLoading] = useState(false);
    const [error,   setError]   = useState<string | null>(null);

    const fetchSettings = useCallback(async () => {
        if (!callToken) return;
        setLoading(true);
        setError(null);
        try {
            const client = createClient(OscilloscopeService, makeControlTransport(callToken));
            const res    = await client.getSettings({});
            if (res.settings) applyServerSettings(res.settings);
        } catch (e) {
            setError(e instanceof Error ? e.message : 'Failed to fetch settings');
        } finally {
            setLoading(false);
        }
    }, [callToken]);

    const pushSettings = useCallback(async (
        patch: import('@/generated/oscilloscope_interface_pb').OscilloscopeSettings,
        paths: string[],
    ): Promise<boolean> => {
        if (!callToken) return false;
        setLoading(true);
        setError(null);
        try {
            const client = createClient(OscilloscopeService, makeControlTransport(callToken));
            const res    = await client.setSettings({
                updateMask: create(FieldMaskSchema, { paths }),
                settings:   patch,
            });
            if (res.settings) applyServerSettings(res.settings);
            return res.streamRestartNeeded ?? false;
        } catch (e) {
            setError(e instanceof Error ? e.message : 'Failed to apply settings');
            return false;
        } finally {
            setLoading(false);
        }
    }, [callToken]);

    return { fetchSettings, pushSettings, loading, error };
}
