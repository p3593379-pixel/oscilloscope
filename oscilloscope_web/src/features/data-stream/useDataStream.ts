import { useEffect, useRef }        from 'react';
import type { RefObject }            from 'react';
import { createClient }              from '@connectrpc/connect';
import { makeDataTransport }         from '@/shared/api/transport';
import {
  OscilloscopeService,
  DecimationTier,
} from '@/generated/oscilloscope_interface_pb';
import { useAuthStore }              from '@/entities/auth/authStore';
import { useWaveformStore }          from '@/entities/waveform/waveformStore';

function deinterleave(raw: Uint8Array, numChannels: number): Float32Array[] {
  const totalFloats    = raw.byteLength / 4;
  const samplesPerChan = Math.floor(totalFloats / numChannels);
  const src            = new Float32Array(raw.buffer, raw.byteOffset, totalFloats);
  const channels: Float32Array[] = Array.from(
      { length: numChannels },
      () => new Float32Array(samplesPerChan)
  );
  for (let i = 0; i < samplesPerChan; i++) {
    for (let ch = 0; ch < numChannels; ch++) {
      channels[ch][i] = src[i * numChannels + ch];
    }
  }
  return channels;
}

export function useDataStream(workerRef: RefObject<Worker | null>) {
  const abortRef = useRef<AbortController | null>(null);

  useEffect(() => {
    const abort = new AbortController();
    abortRef.current = abort;

    const run = async () => {
      const { streamToken } = useAuthStore.getState();
      if (!streamToken) return;

      const transport = makeDataTransport();
      const client    = createClient(OscilloscopeService, transport);
      const waveform  = useWaveformStore.getState();

      waveform.setStreaming(true);
      try {
        for await (const chunk of client.streamData(
            { requestedTier: DecimationTier.FULL, maxBandwidthMbps: 0, streamToken },
            { signal: abort.signal }
        )) {
          const numChannels = chunk.channels;
          const raw         = chunk.samples;
          if (numChannels === 0 || raw.byteLength === 0) continue;

          const perChannel = deinterleave(raw, numChannels);

          // Track total samples (use ch0 as reference)
          waveform.addSamples(perChannel[0].length);
          if (chunk.sampleRateHz) waveform.setSampleRate(chunk.sampleRateHz);
          if (numChannels !== waveform.channelCount) waveform.setChannelCount(numChannels);

          for (let ch = 0; ch < perChannel.length; ch++) {
            const buf = perChannel[ch].buffer.slice(
                perChannel[ch].byteOffset,
                perChannel[ch].byteOffset + perChannel[ch].byteLength
            );
            workerRef.current?.postMessage(
                { type: 'samples', channel: ch, sampleRate: chunk.sampleRateHz,
                  sequence: chunk.sequenceNumber, timestampNs: chunk.timestampNs, data: buf },
                [buf]
            );
          }
          waveform.incrementFrames();
        }
      } catch (err) {
        if ((err as Error).name !== 'AbortError') {
          console.error('[useDataStream] stream error:', err);
        }
      } finally {
        waveform.setStreaming(false);
      }
    };

    run();
    return () => abort.abort();
  }, [workerRef]);
}
