import { create } from 'zustand';

interface WaveformState {
  isStreaming:   boolean;
  sampleRate:    number;
  channelCount:  number;
  frameCount:    number;
  /** Total samples received (channel 0 as reference). Used for live viewport. */
  totalSamples:  number;

  setStreaming:    (v: boolean) => void;
  setSampleRate:   (hz: number) => void;
  setChannelCount: (n: number)  => void;
  incrementFrames: ()           => void;
  addSamples:      (n: number)  => void;
  reset:           ()           => void;
}

export const useWaveformStore = create<WaveformState>((set) => ({
  isStreaming:  false,
  sampleRate:   0,
  channelCount: 1,
  frameCount:   0,
  totalSamples: 0,

  setStreaming:    (v)  => set({ isStreaming: v }),
  setSampleRate:   (hz) => set({ sampleRate: hz }),
  setChannelCount: (n)  => set({ channelCount: n }),
  incrementFrames: ()   => set((s) => ({ frameCount: s.frameCount + 1 })),
  addSamples:      (n)  => set((s) => ({ totalSamples: s.totalSamples + n })),
  reset:           ()   => set({ isStreaming: false, sampleRate: 0, channelCount: 1, frameCount: 0, totalSamples: 0 }),
}));
