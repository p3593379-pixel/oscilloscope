import { create } from 'zustand';

interface SpectrogramMeta {
    fftSize:          number;
    freqResolutionHz: number;
    sampleRateHz:     number;
    channelCount:     number;
}

interface SpectrogramState {
    isStreaming:  boolean;
    frameCount:   number;
    meta:         SpectrogramMeta | null;

    setStreaming:    (v: boolean)          => void;
    incrementFrames: ()                   => void;
    setMeta:         (m: SpectrogramMeta) => void;
    reset:           ()                   => void;
}

export const useSpectrogramStore = create<SpectrogramState>((set) => ({
    isStreaming:  false,
    frameCount:   0,
    meta:         null,

    setStreaming:     (v) => set({ isStreaming: v }),
    incrementFrames:  ()  => set((s) => ({ frameCount: s.frameCount + 1 })),
    setMeta:          (m) => set({ meta: m }),
    reset:            ()  => set({ isStreaming: false, frameCount: 0, meta: null }),
}));
