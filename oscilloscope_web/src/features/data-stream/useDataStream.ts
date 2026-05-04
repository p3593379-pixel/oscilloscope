import { useEffect, useRef }      from 'react';
import type { RefObject }          from 'react';
import { createClient }            from '@connectrpc/connect';
import { makeDataTransport }       from '@/shared/api/transport';
import {
  OscilloscopeService,
  DecimationTier,
} from '@/generated/oscilloscope_interface_pb';
import { useAuthStore }            from '@/entities/auth/authStore';
import { useWaveformStore }        from '@/entities/waveform/waveformStore';
import { useSettingsStore }        from '@/entities/oscilloscopeSettings/settingsStore';

// De-interleave packed float32 bytes: CH0[0],CH1[0],CH0[1],CH1[1],...
function deinterleave(raw: Uint8Array, numChannels: number): Float32Array[] {
  const totalFloats    = raw.byteLength / 4;
  const samplesPerChan = Math.floor(totalFloats / numChannels);
  const src = new Float32Array(raw.buffer, raw.byteOffset, totalFloats);
  const out: Float32Array[] = Array.from(
      { length: numChannels },
      () => new Float32Array(samplesPerChan)
  );
  for (let i = 0; i < samplesPerChan; i++)
    for (let ch = 0; ch < numChannels; ch++)
      out[ch][i] = src[i * numChannels + ch];
  return out;
}

export function useDataStream(workerRef: RefObject<Worker | null>) {
  const streamingEnabled = useSettingsStore(s => s.streamingEnabled);

  // Keep a ref so the async loop can read the latest worker without re-running
  const workerRefRef = useRef(workerRef);
  workerRefRef.current = workerRef;
  const streamToken = useAuthStore(s => s.streamToken);

  useEffect(() => {
    if (!streamingEnabled || !streamToken) return;

    // Snapshot frame parameters at start time — changing xShow while streaming
    // does NOT restart the connection; the viewport just scrolls into the buffer.
    const { xShow, targetFps } = useSettingsStore.getState();

    const abort   = new AbortController();
    const waveform = useWaveformStore.getState();

    const run = async () => {
      const { streamToken } = useAuthStore.getState();
      if (!streamToken) return;

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
          console.log("iterating")
          const numChannels = chunk.channels;
          const raw         = chunk.samples;
          if (numChannels === 0 || raw.byteLength === 0) continue;

          const perChannel = deinterleave(raw, numChannels);

          waveform.addSamples(perChannel[0].length);
          if (chunk.sampleRateHz)               waveform.setSampleRate(chunk.sampleRateHz);
          if (numChannels !== waveform.channelCount) waveform.setChannelCount(numChannels);

          for (let ch = 0; ch < perChannel.length; ch++) {
            // Create a clean copy as Float32Array (slice gives us ownership for transfer)
            const f32 = perChannel[ch];                          // already Float32Array
            const buf = f32.buffer.slice(
                f32.byteOffset,
                f32.byteOffset + f32.byteLength
            );
            const transferable = new Float32Array(buf);          // Float32Array over the copy
            workerRef.current?.postMessage(
                { type: 'samples', channel: ch,
                  sampleRate: chunk.sampleRateHz,
                  sequence: chunk.sequenceNumber,
                  timestampNs: chunk.timestampNs,
                  data: transferable },                           // ← Float32Array, not ArrayBuffer
                [transferable.buffer]                            // transfer the underlying buffer
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
    // streamingEnabled is the sole reconnect trigger.
    // xShow / targetFps are snapshotted at start — no restart needed for viewport changes.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [streamingEnabled, streamToken]);
}
