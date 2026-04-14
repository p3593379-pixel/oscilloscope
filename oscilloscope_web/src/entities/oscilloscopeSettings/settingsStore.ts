import { create } from 'zustand';

export type TriggerMode = 'AUTO' | 'NORMAL' | 'SINGLE';
export type TriggerEdge = 'RISING' | 'FALLING';

interface OscilloscopeSettings {
  // Time base
  timePerDiv: number;         // seconds/div
  // Vertical (per channel, index = channel number)
  voltsPerDiv: number[];
  verticalOffset: number[];   // volts
  // Trigger
  triggerMode:    TriggerMode;
  triggerEdge:    TriggerEdge;
  triggerLevel:   number;     // volts
  triggerChannel: number;
  // Bandwidth hint
  maxBandwidthMbps: number;

  // Actions
  setTimePerDiv:      (v: number)         => void;
  setVoltsPerDiv:     (ch: number, v: number) => void;
  setVerticalOffset:  (ch: number, v: number) => void;
  setTriggerMode:     (m: TriggerMode)    => void;
  setTriggerEdge:     (e: TriggerEdge)    => void;
  setTriggerLevel:    (v: number)         => void;
  setTriggerChannel:  (ch: number)        => void;
  setMaxBandwidth:    (mbps: number)      => void;
}

export const useSettingsStore = create<OscilloscopeSettings>((set) => ({
  timePerDiv:       1e-3,
  voltsPerDiv:      [1.0],
  verticalOffset:   [0.0],
  triggerMode:      'AUTO',
  triggerEdge:      'RISING',
  triggerLevel:     0.0,
  triggerChannel:   0,
  maxBandwidthMbps: 0,

  setTimePerDiv:     (v)       => set({ timePerDiv: v }),
  setVoltsPerDiv:    (ch, v)   => set((s) => {
    const a = [...s.voltsPerDiv]; a[ch] = v; return { voltsPerDiv: a };
  }),
  setVerticalOffset: (ch, v)   => set((s) => {
    const a = [...s.verticalOffset]; a[ch] = v; return { verticalOffset: a };
  }),
  setTriggerMode:    (m)       => set({ triggerMode: m }),
  setTriggerEdge:    (e)       => set({ triggerEdge: e }),
  setTriggerLevel:   (v)       => set({ triggerLevel: v }),
  setTriggerChannel: (ch)      => set({ triggerChannel: ch }),
  setMaxBandwidth:   (mbps)    => set({ maxBandwidthMbps: mbps }),
}));
