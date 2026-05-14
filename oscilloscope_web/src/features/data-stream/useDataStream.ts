import { useEffect, useRef }      from 'react';
import type { RefObject }          from 'react';
import { createClient }            from '@connectrpc/connect';
import { makeDataTransport }       from '@/shared/api/transport';
import {
  OscilloscopeService,
  DecimationTier,
} from '@/generated/oscilloscope_interface_pb';
import { useWaveformStore }        from '@/entities/waveform/waveformStore';
import { useSettingsStore }        from '@/entities/oscilloscopeSettings/settingsStore';

function deinterleave(raw: Uint8Array, numChannels: number): Float32Array[] {
  const aligned = raw.byteOffset % 4 === 0
      ? raw
      : new Uint8Array(raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength));
  const totalFloats    = aligned.byteLength / 4;
  const samplesPerChan = Math.floor(totalFloats / numChannels);
  const src = new Float32Array(aligned.buffer, aligned.byteOffset, totalFloats);
  const out: Float32Array[] = Array.from(
      { length: numChannels },
      () => new Float32Array(samplesPerChan)
  );
  for (let i = 0; i < samplesPerChan; i++)
    for (let ch = 0; ch < numChannels; ch++)
      out[ch][i] = src[i * numChannels + ch];
  return out;
}

export function useDataStream(
    workerRef: RefObject<Worker | null>,
    streamToken: string,             // caller guarantees non-null
) {
  const streamingEnabled = useSettingsStore(s => s.streamingEnabled);

  const workerRefRef = useRef(workerRef);
  workerRefRef.current = workerRef;

  useEffect(() => {
    if (!streamingEnabled) return;

    const { xShow, targetFps } = useSettingsStore.getState();
    const abort   = new AbortController();
    const waveform = useWaveformStore.getState();

    const run = async () => {
      const transport = makeDataTransport();
      const client    = createClient(OscilloscopeService, transport);

      waveform.setStreaming(true);
      try {
        for await (const chunk of client.streamData(
            {
              requestedTier:    DecimationTier.FULL,
              maxBandwidthMbps: 0,
              streamToken,
              frameSize:        xShow,
              targetFps,
            },
            { signal: abort.signal }
        )) {
          const numChannels = chunk.channels;
          const raw         = chunk.samples;
          if (numChannels === 0 || raw.byteLength === 0) continue;

          const perChannel = deinterleave(raw, numChannels);

          waveform.addSamples(perChannel[0].length);
          if (chunk.sampleRateHz)                        waveform.setSampleRate(chunk.sampleRateHz);
          if (numChannels !== waveform.channelCount) waveform.setChannelCount(numChannels);

          for (let ch = 0; ch < perChannel.length; ch++) {
            const f32 = perChannel[ch];
            const buf = f32.buffer.slice(f32.byteOffset, f32.byteOffset + f32.byteLength);
            const transferable = new Float32Array(buf);
            workerRefRef.current.current?.postMessage(
                { type: 'samples', channel: ch,
                  sampleRate: chunk.sampleRateHz,
                  sequence: chunk.sequenceNumber,
                  timestampNs: chunk.timestampNs,
                  data: transferable },
                [transferable.buffer]
            );
          }
          waveform.incrementFrames();
        }
      } catch (err) {
        if ((err as Error).name !== 'AbortError')
          console.error('[useDataStream] stream error:', err);
      } finally {
        waveform.setStreaming(false);
      }
    };

    run();
    return () => abort.abort();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [streamingEnabled, streamToken]);
}
