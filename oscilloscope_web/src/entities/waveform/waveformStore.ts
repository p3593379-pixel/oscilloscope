import { create } from 'zustand';

/**
 * Holds metadata about the current waveform stream.
 * Actual sample data lives in the SharedArrayBuffer managed by the worker.
 */
interface WaveformState {
  isStreaming: boolean;
  sampleRate: number;         // Hz
  channelCount: number;
  frameCount: number;         // total frames received
  setStreaming:     (v: boolean) => void;
  setSampleRate:    (hz: number) => void;
  setChannelCount:  (n: number)  => void;
  incrementFrames:  ()           => void;
  reset:            ()           => void;
}

export const useWaveformStore = create<WaveformState>((set) => ({
  isStreaming:  false,
  sampleRate:   0,
  channelCount: 1,
  frameCount:   0,
  setStreaming:    (v)  => set({ isStreaming: v }),
  setSampleRate:   (hz) => set({ sampleRate: hz }),
  setChannelCount: (n)  => set({ channelCount: n }),
  incrementFrames: ()   => set((s) => ({ frameCount: s.frameCount + 1 })),
  reset: ()             => set({ isStreaming: false, sampleRate: 0, channelCount: 1, frameCount: 0 }),
}));
