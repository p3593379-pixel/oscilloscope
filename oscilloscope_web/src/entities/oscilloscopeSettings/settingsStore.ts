import { create } from 'zustand';

export type TriggerMode = 'AUTO' | 'NORMAL' | 'SINGLE';
export type TriggerEdge = 'RISING' | 'FALLING';
export type ActiveTool  = 'none' | 'rectZoom' | 'vertZoom' | 'horZoom' | 'pan';

interface OscilloscopeSettings {
  // ── Viewport ─────────────────────────────────────────────────────────────
  xStart:      number;   // -1 = live/auto-follow; ≥0 = offset from end (samples)
  xShow:       number;   // samples visible across the full canvas width
  yPeakToPeak: number;   // full visible Y range in volts

  // ── Channels ─────────────────────────────────────────────────────────────
  channelVisible: boolean[];
  voltsPerDiv:    number[];
  verticalOffset: number[];

  // ── Trigger ───────────────────────────────────────────────────────────────
  triggerMode:    TriggerMode;
  triggerEdge:    TriggerEdge;
  triggerLevel:   number;
  triggerChannel: number;

  // ── Stream control ────────────────────────────────────────────────────────
  streamingEnabled: boolean;  // user-controlled start/stop
  targetFps:        number;   // desired server frame rate (currently fixed at 30)
  maxBandwidthMbps: number;

  // ── UI ────────────────────────────────────────────────────────────────────
  activeTool:   ActiveTool;
  naturalUnits: boolean;

  // ── Actions ───────────────────────────────────────────────────────────────
  setXStart:            (v: number)       => void;
  setXShow:             (v: number)       => void;
  setYPeakToPeak:       (v: number)       => void;
  setXRange:            (start: number, show: number) => void;
  setChannelVisible:    (ch: number, v: boolean) => void;
  setVoltsPerDiv:       (ch: number, v: number)  => void;
  setVerticalOffset:    (ch: number, v: number)  => void;
  setTriggerMode:       (m: TriggerMode)  => void;
  setTriggerEdge:       (e: TriggerEdge)  => void;
  setTriggerLevel:      (v: number)       => void;
  setTriggerChannel:    (ch: number)      => void;
  setStreamingEnabled:  (v: boolean)      => void;
  setTargetFps:         (fps: number)     => void;
  setMaxBandwidth:      (mbps: number)    => void;
  setActiveTool:        (t: ActiveTool)   => void;
  setNaturalUnits:      (v: boolean)      => void;
}

export const useSettingsStore = create<OscilloscopeSettings>((set) => ({
  xStart:       -1,
  xShow:        8192,
  yPeakToPeak:  2.5,   // ±1.25 V, matches ±1 V sinewave amplitude with headroom

  channelVisible:  [true, true],
  voltsPerDiv:     [1.0, 1.0],
  verticalOffset:  [0.0, 0.0],

  triggerMode:    'AUTO',
  triggerEdge:    'RISING',
  triggerLevel:   0.0,
  triggerChannel: 0,

  streamingEnabled: false,
  targetFps:        30,
  maxBandwidthMbps: 0,

  activeTool:   'none',
  naturalUnits: false,

  setXStart:           (v)     => set({ xStart: v }),
  setXShow:            (v)     => set({ xShow: v }),
  setYPeakToPeak:      (v)     => set({ yPeakToPeak: v }),
  setXRange:           (s, sh) => set({ xStart: s, xShow: sh }),
  setChannelVisible:   (ch, v) => set((s) => { const a = [...s.channelVisible]; a[ch] = v; return { channelVisible: a }; }),
  setVoltsPerDiv:      (ch, v) => set((s) => { const a = [...s.voltsPerDiv];    a[ch] = v; return { voltsPerDiv: a };    }),
  setVerticalOffset:   (ch, v) => set((s) => { const a = [...s.verticalOffset]; a[ch] = v; return { verticalOffset: a }; }),
  setTriggerMode:      (m)     => set({ triggerMode: m }),
  setTriggerEdge:      (e)     => set({ triggerEdge: e }),
  setTriggerLevel:     (v)     => set({ triggerLevel: v }),
  setTriggerChannel:   (ch)    => set({ triggerChannel: ch }),
  setStreamingEnabled: (v)     => set({ streamingEnabled: v }),
  setTargetFps:        (fps)   => set({ targetFps: fps }),
  setMaxBandwidth:     (mbps)  => set({ maxBandwidthMbps: mbps }),
  setActiveTool:       (t)     => set({ activeTool: t }),
  setNaturalUnits:     (v)     => set({ naturalUnits: v }),
}));
