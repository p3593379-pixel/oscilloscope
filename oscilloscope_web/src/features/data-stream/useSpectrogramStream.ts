import { useEffect, useRef }                   from 'react';
import type { RefObject }                       from 'react';
import { createClient }                         from '@connectrpc/connect';
import { createConnectTransport }               from '@connectrpc/connect-web';
import { DATA_PLANE_URL }                       from '@/shared/config/env';
import { OscilloscopeService }                  from '@/generated/oscilloscope_interface_pb';
import { useSpectrogramStore }                  from '@/entities/spectrogram/spectrogramStore';
import { useSettingsStore }                     from '@/entities/oscilloscopeSettings/settingsStore';

function deinterleaveSpectrum(raw: Uint8Array, numChannels: number): Float32Array[] {
    const aligned = raw.byteOffset % 4 === 0
        ? raw
        : new Uint8Array(raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength));
    const totalFloats = aligned.byteLength / 4;
    const binsPerChan = Math.floor(totalFloats / numChannels);
    const src = new Float32Array(aligned.buffer, aligned.byteOffset, totalFloats);
    const out: Float32Array[] = Array.from(
        { length: numChannels },
        () => new Float32Array(binsPerChan)
    );
    for (let bin = 0; bin < binsPerChan; bin++)
        for (let ch = 0; ch < numChannels; ch++)
            out[ch][bin] = src[bin * numChannels + ch];
    return out;
}

export function useSpectrogramStream(
    workerRef: RefObject<Worker | null>,
    spectrogramToken: string,
) {
    const spectrogramEnabled = useSettingsStore(s => s.spectrogramEnabled);

    const workerRefRef = useRef(workerRef);
    workerRefRef.current = workerRef;

    useEffect(() => {
        if (!spectrogramEnabled) return;

        const { spectrogramFftSize, spectrogramFps } = useSettingsStore.getState();
        const abort     = new AbortController();
        const specStore = useSpectrogramStore.getState();

        console.log('%c[useSpectrogramStream] starting', 'color:#4af; font-weight:bold', {
            spectrogramFftSize, spectrogramFps,
            tokenPrefix: spectrogramToken.slice(0, 20) + '…',
        });

        const run = async () => {
            // Use the debug-intercepting transport only for spectrogram
            const transport = createConnectTransport({
                baseUrl:         DATA_PLANE_URL,
                useBinaryFormat: true,
            });
            const client = createClient(OscilloscopeService, transport);

            specStore.setStreaming(true);
            let frameCount = 0;
            try {
                for await (const chunk of client.streamSpectrogram(
                    { spectrogramToken, fftSize: spectrogramFftSize, targetFps: spectrogramFps },
                    { signal: abort.signal }
                )) {
                    frameCount++;
                    console.log(`[useSpectrogramStream] chunk #${frameCount}:`, {
                        seq:      chunk.sequenceNumber,
                        channels: chunk.channels,
                        fftSize:  chunk.fftSize,
                        bytes:    chunk.magnitudesDb.byteLength,
                        freqRes:  chunk.freqResolutionHz,
                        sr:       chunk.sampleRateHz,
                    });

                    const numChannels = chunk.channels;
                    const raw         = chunk.magnitudesDb;
                    if (numChannels === 0 || raw.byteLength === 0) {
                        console.warn(`[useSpectrogramStream] chunk #${frameCount}: skipped (empty)`);
                        continue;
                    }

                    let perChannel: Float32Array[];
                    try {
                        perChannel = deinterleaveSpectrum(raw, numChannels);
                    } catch (e) {
                        console.error(`[useSpectrogramStream] chunk #${frameCount}: deinterleave failed`, e);
                        continue;
                    }

                    specStore.setMeta({
                        fftSize:          chunk.fftSize,
                        freqResolutionHz: chunk.freqResolutionHz,
                        sampleRateHz:     chunk.sampleRateHz,
                        channelCount:     numChannels,
                    });

                    workerRefRef.current.current?.postMessage({
                        type: 'channelCount',
                        channelCount: numChannels,
                    });

                    for (let ch = 0; ch < perChannel.length; ch++) {
                        const f32          = perChannel[ch];
                        const buf          = f32.buffer.slice(f32.byteOffset, f32.byteOffset + f32.byteLength);
                        const transferable = new Float32Array(buf);
                        workerRefRef.current.current?.postMessage(
                            { type: 'spectrum', channel: ch, data: transferable },
                            [transferable.buffer]
                        );
                    }
                    specStore.incrementFrames();
                }
                console.log(`%c[useSpectrogramStream] for-await loop exited cleanly after ${frameCount} frames`, 'color:#fa0');
            } catch (err) {
                const e = err as Error;
                console.error(
                    `%c[useSpectrogramStream] stream error after ${frameCount} frame(s):`,
                    'color:#f66; font-weight:bold',
                    e.name, e.message,
                    // Print the full cause chain
                    (e as { cause?: unknown }).cause,
                );
            } finally {
                console.log(`[useSpectrogramStream] finally: setStreaming(false), frameCount=${frameCount}`);
                specStore.setStreaming(false);
            }
        };

        run();
        return () => {
            console.log('[useSpectrogramStream] cleanup: aborting');
            abort.abort();
        };
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [spectrogramEnabled, spectrogramToken]);
}
