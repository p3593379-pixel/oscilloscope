import { useCallback }              from 'react';
import { createClient }             from '@connectrpc/connect';
import { create }                   from '@bufbuild/protobuf';
import { FieldMaskSchema }          from '@bufbuild/protobuf/wkt';
import { makeControlTransport }     from '@/shared/api/transport';
import { OscilloscopeService }      from '@/generated/oscilloscope_interface_pb';
import { useAuthStore }             from '@/entities/auth/authStore';
import {
    useSettingsStore,
    type TriggerMode,
    type TriggerEdge,
    type DaqMode,
} from '@/entities/oscilloscopeSettings/settingsStore';
import type { OscilloscopeSettings } from '@/generated/oscilloscope_interface_pb';

// ── Proto → Zustand store ─────────────────────────────────────────────────────
// Always writes every field — no falsy guards that would silently skip
// fields that legitimately carry a zero/false value.
function applyServerSettings(s: OscilloscopeSettings) {
    const store = useSettingsStore.getState();

    store.setSampleRate(Number(s.sampleRateHz));
    store.setFrameSize(s.frameSizeSamples);
    store.setFrameFrequency(s.frameFrequencyHz);
    store.setDisplayFrequency(s.displayFrequencyHz);
    store.setDecimationRate(s.decimationRate);
    store.setSpectrogramFftSize(s.spectrogramFftSize);
    store.setSpectrogramFps(s.spectrogramFps);

    if (s.trigger) {
        const modeMap: Record<number, TriggerMode> = { 1: 'AUTO', 2: 'NORMAL', 3: 'SINGLE' };
        const edgeMap: Record<number, TriggerEdge> = { 1: 'RISING', 2: 'FALLING' };
        store.setTriggerMode(modeMap[s.trigger.mode]    ?? 'AUTO');
        store.setTriggerEdge(edgeMap[s.trigger.edge]    ?? 'RISING');
        store.setTriggerLevel(s.trigger.levelV);
        store.setTriggerChannel(s.trigger.channel);
    }

    s.channels.forEach((ch, i) => {
        store.setChannelVisible(i, ch.enabled);
        store.setVoltsPerDiv(i, ch.voltsPerDiv);
        store.setVerticalOffset(i, ch.verticalOffset);
    });

    store.setNaturalUnits(s.naturalUnits);

    const daqModeMap: Record<number, DaqMode> = { 1: 'DAQ_MODE_1', 2: 'DAQ_MODE_2' };
    store.setDaqMode(daqModeMap[s.daqMode] ?? 'DAQ_MODE_1');
    store.setPreDelaySamples(s.preDelaySamples);
    store.setSpecSettingOne(s.specSettingOne);
    store.setSpecSettingTwo(s.specSettingTwo);
    store.setSpecSettingThree(s.specSettingThree);
    store.setSpecSettingFour(s.specSettingFour);
    store.setWriteAdc(s.writeAdc);
    if (s.adcArchivePath)          store.setAdcArchivePath(s.adcArchivePath);
    store.setWriteSpectrogram(s.writeSpectrogram);
    if (s.spectrogramArchivePath)  store.setSpectrogramArchivePath(s.spectrogramArchivePath);
}

// ── Hook — pure async functions, no loading state ─────────────────────────────
// Loading state is owned by the component so it can never bleed across
// concurrent operations or survive a curtain close/reopen cycle.
export function useOscilloscopeSettings() {
    const callToken = useAuthStore(s => s.callToken);

    const fetchSettings = useCallback(async (): Promise<void> => {
        if (!callToken) return;
        const client = createClient(OscilloscopeService, makeControlTransport(callToken));
        const res    = await client.getSettings({});
        if (res.settings) applyServerSettings(res.settings);
    }, [callToken]);

    const pushSettings = useCallback(async (
        patch: OscilloscopeSettings,
        paths: string[],
    ): Promise<boolean> => {
        if (!callToken) return false;
        const client = createClient(OscilloscopeService, makeControlTransport(callToken));
        const res    = await client.setSettings({
            updateMask: create(FieldMaskSchema, { paths }),
            settings:   patch,
        });
        if (res.settings) applyServerSettings(res.settings);
        return res.streamRestartNeeded ?? false;
    }, [callToken]);

    const browseDirectory = useCallback(async (path: string) => {
        if (!callToken) throw new Error('Not authenticated');
        const client = createClient(OscilloscopeService, makeControlTransport(callToken));
        return await client.browseDirectory({ path });
    }, [callToken]);

    return { fetchSettings, pushSettings, browseDirectory };
}
