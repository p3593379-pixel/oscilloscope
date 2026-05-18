import { create } from 'zustand';

export type TriggerMode = 'AUTO' | 'NORMAL' | 'SINGLE';
export type TriggerEdge = 'RISING' | 'FALLING';
export type ActiveTool  = 'none' | 'rectZoom' | 'vertZoom' | 'horZoom' | 'pan';
export type DaqMode     = 'DAQ_MODE_1' | 'DAQ_MODE_2';

interface OscilloscopeSettings {

  // ── Emulation pipeline ────────────────────────────────────────────────────
  sampleRate:        number;
  frameSize:         number;
  frameFrequency:    number;
  displayFrequency:  number;
  decimationRate:    number;

  // ── Spectrogram ───────────────────────────────────────────────────────────
  spectrogramEnabled:  boolean;
  spectrogramFftSize:  number;
  spectrogramFps:      number;

  // ── Viewport ──────────────────────────────────────────────────────────────
  xStart:      number;
  xShow:       number;
  yPeakToPeak: number;

  // ── Channels ──────────────────────────────────────────────────────────────
  channelVisible: boolean[];
  voltsPerDiv:    number[];
  verticalOffset: number[];

  // ── Trigger ───────────────────────────────────────────────────────────────
  triggerMode:    TriggerMode;
  triggerEdge:    TriggerEdge;
  triggerLevel:   number;
  triggerChannel: number;

  // ── Stream control ────────────────────────────────────────────────────────
  streamingEnabled: boolean;
  targetFps:        number;
  maxBandwidthMbps: number;

  // ── UI ────────────────────────────────────────────────────────────────────
  activeTool:   ActiveTool;
  naturalUnits: boolean;

  // ── Synchronisation / DAQ ─────────────────────────────────────────────────
  daqMode:         DaqMode;
  preDelaySamples: number;

  // ── Spectrogram device settings ───────────────────────────────────────────
  specSettingOne:   number;
  specSettingTwo:   number;
  specSettingThree: number;
  specSettingFour:  number;   // 0 | 1 — splitter index

  // ── Archive ───────────────────────────────────────────────────────────────
  writeAdc:                boolean;
  adcArchivePath:          string;
  writeSpectrogram:        boolean;
  spectrogramArchivePath:  string;

  // ── Actions ───────────────────────────────────────────────────────────────
  setSampleRate:       (hz: number)  => void;
  setFrameSize:        (n: number)   => void;
  setFrameFrequency:   (hz: number)  => void;
  setDisplayFrequency: (fps: number) => void;
  setDecimationRate:   (n: number)   => void;

  setSpectrogramEnabled: (v: boolean)  => void;
  setSpectrogramFftSize: (n: number)   => void;
  setSpectrogramFps:     (fps: number) => void;

  setXStart:         (v: number)              => void;
  setXShow:          (v: number)              => void;
  setYPeakToPeak:    (v: number)              => void;
  setXRange:         (start: number, show: number) => void;
  setChannelVisible: (ch: number, v: boolean) => void;
  setVoltsPerDiv:    (ch: number, v: number)  => void;
  setVerticalOffset: (ch: number, v: number)  => void;

  setTriggerMode:    (m: TriggerMode) => void;
  setTriggerEdge:    (e: TriggerEdge) => void;
  setTriggerLevel:   (v: number)      => void;
  setTriggerChannel: (ch: number)     => void;

  setStreamingEnabled: (v: boolean)   => void;
  setTargetFps:        (fps: number)  => void;
  setMaxBandwidth:     (mbps: number) => void;

  setActiveTool:   (t: ActiveTool) => void;
  setNaturalUnits: (v: boolean)    => void;

  setDaqMode:         (m: DaqMode) => void;
  setPreDelaySamples: (n: number)  => void;

  setSpecSettingOne:   (v: number) => void;
  setSpecSettingTwo:   (v: number) => void;
  setSpecSettingThree: (v: number) => void;
  setSpecSettingFour:  (v: number) => void;

  setWriteAdc:               (v: boolean) => void;
  setAdcArchivePath:         (p: string)  => void;
  setWriteSpectrogram:       (v: boolean) => void;
  setSpectrogramArchivePath: (p: string)  => void;
}

export const useSettingsStore = create<OscilloscopeSettings>((set) => ({
  sampleRate:        2_500_000_000,
  frameSize:         2500,
  frameFrequency:    1000,
  displayFrequency:  30,
  decimationRate:    1,

  spectrogramEnabled:  false,
  spectrogramFftSize:  512,
  spectrogramFps:      10,

  xStart:       -1,
  xShow:        8192,
  yPeakToPeak:  2.5,

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

  daqMode:         'DAQ_MODE_1',
  preDelaySamples: 0,

  specSettingOne:   0,
  specSettingTwo:   0,
  specSettingThree: 0,
  specSettingFour:  0,

  writeAdc:               false,
  adcArchivePath:         '/opt/osc_archive/adc',
  writeSpectrogram:       false,
  spectrogramArchivePath: '/opt/osc_archive/spectrogram',

  setSampleRate:       (hz)  => set({ sampleRate: hz }),
  setFrameSize:        (n)   => set({ frameSize: n }),
  setFrameFrequency:   (hz)  => set({ frameFrequency: hz }),
  setDisplayFrequency: (fps) => set({ displayFrequency: fps }),
  setDecimationRate:   (n)   => set({ decimationRate: n }),

  setSpectrogramEnabled: (v)   => set({ spectrogramEnabled: v }),
  setSpectrogramFftSize: (n)   => set({ spectrogramFftSize: n }),
  setSpectrogramFps:     (fps) => set({ spectrogramFps: fps }),

  setXStart:         (v)      => set({ xStart: v }),
  setXShow:          (v)      => set({ xShow: v }),
  setYPeakToPeak:    (v)      => set({ yPeakToPeak: v }),
  setXRange:         (s, sh)  => set({ xStart: s, xShow: sh }),
  setChannelVisible: (ch, v)  => set((s) => { const a = [...s.channelVisible]; a[ch] = v; return { channelVisible: a }; }),
  setVoltsPerDiv:    (ch, v)  => set((s) => { const a = [...s.voltsPerDiv];    a[ch] = v; return { voltsPerDiv: a };    }),
  setVerticalOffset: (ch, v)  => set((s) => { const a = [...s.verticalOffset]; a[ch] = v; return { verticalOffset: a }; }),

  setTriggerMode:    (m)  => set({ triggerMode: m }),
  setTriggerEdge:    (e)  => set({ triggerEdge: e }),
  setTriggerLevel:   (v)  => set({ triggerLevel: v }),
  setTriggerChannel: (ch) => set({ triggerChannel: ch }),

  setStreamingEnabled: (v)    => set({ streamingEnabled: v }),
  setTargetFps:        (fps)  => set({ targetFps: fps }),
  setMaxBandwidth:     (mbps) => set({ maxBandwidthMbps: mbps }),

  setActiveTool:   (t) => set({ activeTool: t }),
  setNaturalUnits: (v) => set({ naturalUnits: v }),

  setDaqMode:         (m) => set({ daqMode: m }),
  setPreDelaySamples: (n) => set({ preDelaySamples: n }),

  setSpecSettingOne:   (v) => set({ specSettingOne: v }),
  setSpecSettingTwo:   (v) => set({ specSettingTwo: v }),
  setSpecSettingThree: (v) => set({ specSettingThree: v }),
  setSpecSettingFour:  (v) => set({ specSettingFour: v }),

  setWriteAdc:               (v) => set({ writeAdc: v }),
  setAdcArchivePath:         (p) => set({ adcArchivePath: p }),
  setWriteSpectrogram:       (v) => set({ writeSpectrogram: v }),
  setSpectrogramArchivePath: (p) => set({ spectrogramArchivePath: p }),
}));
