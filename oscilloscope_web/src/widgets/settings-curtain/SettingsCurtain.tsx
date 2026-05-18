import { useState, useEffect, useRef, useCallback } from 'react';
import { create }                       from '@bufbuild/protobuf';
import {
    OscilloscopeSettingsSchema,
    DaqMode as ProtoDaqMode,
}                                       from '@/generated/oscilloscope_interface_pb';
import { useSettingsStore }             from '@/entities/oscilloscopeSettings/settingsStore';
import { useOscilloscopeSettings }      from '@/features/oscilloscope-settings/useOscilloscopeSettings';
import type { DaqMode }                 from '@/entities/oscilloscopeSettings/settingsStore';
import styles                           from './SettingsCurtain.module.css';

const SAMPLES_PER_SECOND = 4000;

function fmtHz(hz: number): string {
    if (hz >= 1e9) return `${(hz / 1e9).toPrecision(4)} GHz`;
    if (hz >= 1e6) return `${(hz / 1e6).toPrecision(4)} MHz`;
    if (hz >= 1e3) return `${(hz / 1e3).toPrecision(4)} kHz`;
    return `${hz} Hz`;
}
function fmtInt(n: number): string {
    return Math.round(n).toString().replace(/\B(?=(\d{3})+(?!\d))/g, "'");
}
function parseDelimited(s: string): number {
    return parseInt(s.replace(/'/g, ''), 10);
}

// ── Icons ─────────────────────────────────────────────────────────────────────

const IconX = () => (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none" stroke="currentColor" strokeWidth="1.8">
        <path d="M2 2 L12 12 M12 2 L2 12"/>
    </svg>
);
const IconUp = () => (
    <svg width="12" height="12" viewBox="0 0 12 12" fill="none" stroke="currentColor" strokeWidth="1.6">
        <path d="M6 10 L6 2 M2 6 L6 2 L10 6"/>
    </svg>
);
const IconFolder = () => (
    <svg width="13" height="13" viewBox="0 0 13 13" fill="none" stroke="currentColor" strokeWidth="1.4">
        <path d="M1 3.5 L1 11 L12 11 L12 5 L6 5 L4.5 3.5 Z"/>
    </svg>
);
const IconFile = () => (
    <svg width="13" height="13" viewBox="0 0 13 13" fill="none" stroke="currentColor" strokeWidth="1.4">
        <path d="M3 1 L8 1 L11 4 L11 12 L3 12 Z M8 1 L8 4 L11 4"/>
    </svg>
);
const IconNav = () => (
    <svg width="13" height="13" viewBox="0 0 13 13" fill="none" stroke="currentColor" strokeWidth="1.4">
        <path d="M1 3.5 L1 11 L12 11 L12 5 L6 5 L4.5 3.5 Z"/>
        <path d="M8 8 L10.5 8 M9.5 6.5 L11 8 L9.5 9.5" strokeLinecap="round" strokeLinejoin="round"/>
    </svg>
);
const IconSpinner = () => (
    <svg width="12" height="12" viewBox="0 0 12 12" fill="none" stroke="currentColor" strokeWidth="1.6"
         style={{ animation: 'spin 0.7s linear infinite', display: 'block' }}>
        <path d="M6 1 A5 5 0 0 1 11 6" strokeLinecap="round"/>
    </svg>
);
const IconTick = () => (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none"
         stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
        <path d="M2 7 L5.5 10.5 L12 3.5"/>
    </svg>
);

// ── Splitter ──────────────────────────────────────────────────────────────────

interface SplitterProps {
    options:  [string, string];
    value:    0 | 1;
    onChange: (v: 0 | 1) => void;
}
function Splitter({ options, value, onChange }: SplitterProps) {
    return (
        <div className={styles.segmented}>
            <button
                className={`${styles.seg} ${value === 0 ? styles.segActive : ''}`}
                onClick={() => onChange(0)}
            >{options[0]}</button>
            <button
                className={`${styles.seg} ${value === 1 ? styles.segActive : ''}`}
                onClick={() => onChange(1)}
            >{options[1]}</button>
        </div>
    );
}

// ── Spinbox ───────────────────────────────────────────────────────────────────

interface SpinboxProps {
    value:      number;
    onChange:   (v: number) => void;
    min?:       number;
    max?:       number;
    step?:      number;
    unit?:      string;
    delimited?: boolean;
    width?:     number;
}
function Spinbox({ value, onChange, min, max, step = 1, unit, delimited, width = 100 }: SpinboxProps) {
    const [raw, setRaw] = useState(delimited ? fmtInt(value) : String(value));
    const inputRef = useRef<HTMLInputElement>(null);

    useEffect(() => {
        setRaw(delimited ? fmtInt(value) : String(value));
    }, [value, delimited]);

    const clamp = (n: number) => {
        if (min !== undefined && n < min) return min;
        if (max !== undefined && n > max) return max;
        return n;
    };
    const commit = (s: string) => {
        const n = delimited ? parseDelimited(s) : parseFloat(s);
        if (isNaN(n)) { setRaw(delimited ? fmtInt(value) : String(value)); return; }
        const next = clamp(n);
        onChange(next);
        setRaw(delimited ? fmtInt(next) : String(next));
    };
    const step_ = (dir: 1 | -1) => {
        const next = clamp(value + dir * step);
        onChange(next);
        setRaw(delimited ? fmtInt(next) : String(next));
        inputRef.current?.focus();
    };

    return (
        <div className={styles.spinbox}>
            <div className={styles.spinboxField}>
                <input
                    ref={inputRef}
                    type="text"
                    inputMode="numeric"
                    className={styles.spinboxInput}
                    style={{ width }}
                    value={raw}
                    onChange={e => setRaw(e.target.value)}
                    onBlur={e  => commit(e.target.value)}
                    onKeyDown={e => {
                        if (e.key === 'Enter')     { commit((e.target as HTMLInputElement).value); }
                        if (e.key === 'ArrowUp')   { e.preventDefault(); step_(1);  }
                        if (e.key === 'ArrowDown') { e.preventDefault(); step_(-1); }
                    }}
                />
                <div className={styles.spinboxArrows}>
                    <button
                        className={styles.spinboxArrow}
                        onMouseDown={e => { e.preventDefault(); step_(1); }}
                        tabIndex={-1}
                        aria-label="Increment"
                    >
                        <svg width="7" height="5" viewBox="0 0 7 5" fill="currentColor">
                            <path d="M3.5 0 L7 5 L0 5 Z"/>
                        </svg>
                    </button>
                    <button
                        className={styles.spinboxArrow}
                        onMouseDown={e => { e.preventDefault(); step_(-1); }}
                        tabIndex={-1}
                        aria-label="Decrement"
                    >
                        <svg width="7" height="5" viewBox="0 0 7 5" fill="currentColor">
                            <path d="M3.5 5 L7 0 L0 0 Z"/>
                        </svg>
                    </button>
                </div>
            </div>
            {unit && <span className={styles.unit}>{unit}</span>}
        </div>
    );
}

// ── DirPicker ─────────────────────────────────────────────────────────────────

interface DirPickerProps {
    initialPath: string;
    onSelect:    (path: string) => void;
    onClose:     () => void;
    browse:      (path: string) => Promise<{
        currentPath: string;
        parentPath:  string;
        entries:     { name: string; isDir: boolean }[];
    }>;
}

function DirPicker({ initialPath, onSelect, onClose, browse }: DirPickerProps) {
    const [currentPath, setCurrentPath] = useState(initialPath || '/');
    const [entries,     setEntries]     = useState<{ name: string; isDir: boolean }[]>([]);
    const [loading,     setLoading]     = useState(false);
    const [error,       setError]       = useState<string | null>(null);
    const [parentPath,  setParentPath]  = useState('');

    // Each in-flight navigate call owns an abort ref cell passed by object so
    // it can be synchronously cancelled before any await resolves.
    // This is immune to Strict Mode's double-invocation because the abort cell
    // is created fresh per navigate() call, not per component mount.
    const navigate = useCallback(async (path: string) => {
        const abort = { cancelled: false };

        setLoading(true);
        setError(null);

        try {
            const res = await browse(path);
            if (abort.cancelled) return abort;
            setCurrentPath(res.currentPath);
            setParentPath(res.parentPath);
            setEntries(res.entries.map(e => ({ name: e.name, isDir: e.isDir })));
        } catch (e) {
            if (abort.cancelled) return abort;
            setError(e instanceof Error ? e.message : 'Browse failed');
        } finally {
            if (!abort.cancelled) setLoading(false);
        }
        return abort;
    }, [browse]);

    // Initial load. The useEffect cleanup receives the abort cell synchronously
    // via the ref below — set before the first await, so Strict Mode's cleanup
    // can always cancel the in-flight call before it touches state.
    const abortRef = useRef<{ cancelled: boolean } | null>(null);

    useEffect(() => {
        // Strict Mode fires this twice. On the first run we kick off navigate()
        // and store the abort cell in abortRef synchronously via the returned
        // promise's microtask — but that's too late for the cleanup.
        // Instead we store a "pending abort" stub synchronously, then navigate
        // fills it in when it starts running.
        const pendingAbort = { cancelled: false };
        abortRef.current = pendingAbort;

        navigate(initialPath || '/').then(abort => {
            // Replace stub with the real abort cell from this navigate call
            // so future cancellations target the right closure.
            abortRef.current = abort ?? null;
        });

        return () => {
            // Cancel whichever cell we have — stub or real.
            // If the real abort cell hasn't arrived yet (still in microtask queue),
            // the pending stub is already marked cancelled so navigate will
            // check abort.cancelled === true on its very first line after await.
            pendingAbort.cancelled = true;
            if (abortRef.current) abortRef.current.cancelled = true;
        };
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, []);

    return (
        <div className={styles.pickerBackdrop} onClick={onClose}>
            <div className={styles.pickerModal} onClick={e => e.stopPropagation()}>
                <div className={styles.pickerHeader}>
                    <span className={styles.curtainTitle}>Choose Directory</span>
                    <button className={styles.closeBtn} onClick={onClose} aria-label="Close">
                        <IconX />
                    </button>
                </div>

                <div className={styles.pickerPath}>
                    {parentPath && (
                        <button className={styles.pickerUpBtn} onClick={() => navigate(parentPath)}>
                            <IconUp /> ..
                        </button>
                    )}
                    <span className={styles.pickerCurrentPath}>{currentPath}</span>
                </div>

                {error && <div className={styles.errorBanner}>⚠ {error}</div>}

                <div className={styles.pickerList}>
                    {loading && <div className={styles.pickerLoading}>Loading…</div>}
                    {!loading && entries.length === 0 && (
                        <div className={styles.pickerEmpty}>Empty directory</div>
                    )}
                    {!loading && entries.map(e => (
                        <button
                            key={e.name}
                            className={`${styles.pickerEntry} ${e.isDir ? styles.pickerEntryDir : styles.pickerEntryFile}`}
                            onClick={() => {
                                if (!e.isDir) return;
                                const next = currentPath === '/'
                                    ? `/${e.name}`
                                    : `${currentPath}/${e.name}`;
                                navigate(next);
                            }}
                            disabled={!e.isDir}
                        >
                            {e.isDir ? <IconFolder /> : <IconFile />}
                            <span>{e.name}</span>
                        </button>
                    ))}
                </div>

                <div className={styles.pickerFooter}>
                    <button className={styles.cancelBtn} onClick={onClose}>Cancel</button>
                    <button
                        className={styles.applyBtn}
                        onClick={() => { onSelect(currentPath); onClose(); }}
                    >
                        Select
                    </button>
                </div>
            </div>
        </div>
    );
}

// ── Main curtain ──────────────────────────────────────────────────────────────

type ApplyState = 'idle' | 'busy' | 'applied';

interface Props {
    open:    boolean;
    onClose: () => void;
}

export function SettingsCurtain({ open, onClose }: Props) {
    const { fetchSettings, pushSettings, browseDirectory } = useOscilloscopeSettings();

    // ── Transient UI state — resets on every open ─────────────────────────────
    const [applyState, setApplyState] = useState<ApplyState>('idle');
    const [error,      setError]      = useState<string | null>(null);
    const appliedTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

    // ── Draft settings ────────────────────────────────────────────────────────
    const [daqMode,        setDaqMode]        = useState<DaqMode>('DAQ_MODE_1');
    const [frameFrequency, setFrameFrequency] = useState(1000);
    const [naturalUnits,   setNaturalUnits]   = useState(false);
    const [frameSize,      setFrameSize]      = useState(2500);
    const [preDelay,       setPreDelay]       = useState(0);
    const [sampleRate,     setSampleRate]     = useState(2_500_000_000);
    const [specOne,        setSpecOne]        = useState(0);
    const [specTwo,        setSpecTwo]        = useState(0);
    const [specThree,      setSpecThree]      = useState(0);
    const [specFour,       setSpecFour]       = useState<0 | 1>(0);
    const [writeAdc,       setWriteAdc]       = useState(false);
    const [adcPath,        setAdcPath]        = useState('/opt/osc_archive/adc');
    const [writeSpec,      setWriteSpec]      = useState(false);
    const [specPath,       setSpecPath]       = useState('/opt/osc_archive/spectrogram');

    // ── Dirty-tracking wrappers ───────────────────────────────────────────────
    // Any user interaction resets a visible 'applied' tick back to 'idle'.
    const markDirty = useCallback(() => {
        setApplyState(s => {
            if (s === 'applied') {
                if (appliedTimerRef.current) {
                    clearTimeout(appliedTimerRef.current);
                    appliedTimerRef.current = null;
                }
                return 'idle';
            }
            return s;
        });
    }, []);

    const setDaqModeD        = (v: DaqMode)  => { markDirty(); setDaqMode(v);        };
    const setFrameFreqD      = (v: number)   => { markDirty(); setFrameFrequency(v); };
    const setNaturalUnitsD   = (v: boolean)  => { markDirty(); setNaturalUnits(v);   };
    const setFrameSizeD      = (v: number)   => { markDirty(); setFrameSize(v);      };
    const setPreDelayD       = (v: number)   => { markDirty(); setPreDelay(v);       };
    const setSampleRateD     = (v: number)   => { markDirty(); setSampleRate(v);     };
    const setSpecOneD        = (v: number)   => { markDirty(); setSpecOne(v);        };
    const setSpecTwoD        = (v: number)   => { markDirty(); setSpecTwo(v);        };
    const setSpecThreeD      = (v: number)   => { markDirty(); setSpecThree(v);      };
    const setSpecFourD       = (v: 0 | 1)    => { markDirty(); setSpecFour(v);       };
    const setWriteAdcD       = (v: boolean)  => { markDirty(); setWriteAdc(v);       };
    const setAdcPathD        = (v: string)   => { markDirty(); setAdcPath(v);        };
    const setWriteSpecD      = (v: boolean)  => { markDirty(); setWriteSpec(v);      };
    const setSpecPathD       = (v: string)   => { markDirty(); setSpecPath(v);       };

    // ── Directory picker state ────────────────────────────────────────────────
    const [pickerOpen,   setPickerOpen]   = useState(false);
    const [pickerTarget, setPickerTarget] = useState<'adc' | 'spec'>('adc');

    // ── Snapshot store → draft ────────────────────────────────────────────────
    const snapshotDraft = useCallback(() => {
        const s = useSettingsStore.getState();
        setDaqMode(s.daqMode);
        setFrameFrequency(s.frameFrequency);
        setNaturalUnits(s.naturalUnits);
        setFrameSize(s.frameSize);
        setPreDelay(s.preDelaySamples);
        setSampleRate(s.sampleRate);
        setSpecOne(s.specSettingOne);
        setSpecTwo(s.specSettingTwo);
        setSpecThree(s.specSettingThree);
        setSpecFour(s.specSettingFour as 0 | 1);
        setWriteAdc(s.writeAdc);
        setAdcPath(s.adcArchivePath);
        setWriteSpec(s.writeSpectrogram);
        setSpecPath(s.spectrogramArchivePath);
    }, []);

    // On open: reset transient state, snapshot store, fetch from server,
    // then snapshot again so draft reflects whatever the server returned.
    useEffect(() => {
        if (!open) return;
        setApplyState('idle');
        setError(null);
        if (appliedTimerRef.current) {
            clearTimeout(appliedTimerRef.current);
            appliedTimerRef.current = null;
        }
        snapshotDraft();
        fetchSettings()
            .then(() => snapshotDraft())
            .catch(e => setError(e instanceof Error ? e.message : 'Failed to load settings'));
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [open]);

    // Escape key
    useEffect(() => {
        if (!open) return;
        const h = (e: KeyboardEvent) => { if (e.key === 'Escape' && !pickerOpen) onClose(); };
        window.addEventListener('keydown', h);
        return () => window.removeEventListener('keydown', h);
    }, [open, onClose, pickerOpen]);

    // Cleanup timer on unmount
    useEffect(() => () => {
        if (appliedTimerRef.current) clearTimeout(appliedTimerRef.current);
    }, []);

    // ── Derived display values ────────────────────────────────────────────────
    const frameSizeDisplay = naturalUnits ? frameSize  / SAMPLES_PER_SECOND : frameSize;
    const preDelayDisplay  = naturalUnits ? preDelay   / SAMPLES_PER_SECOND : preDelay;
    const frameSizeUnit    = naturalUnits ? 's'        : 'samples';
    const preDelayUnit     = naturalUnits ? 's'        : 'samples';

    // ── Apply ─────────────────────────────────────────────────────────────────
    const apply = async () => {
        setApplyState('busy');
        setError(null);
        try {
            const daqModeProto = daqMode === 'DAQ_MODE_1'
                ? ProtoDaqMode.DAQ_MODE_1
                : ProtoDaqMode.DAQ_MODE_2;

            const patch = create(OscilloscopeSettingsSchema, {
                frameFrequencyHz:       frameFrequency,
                sampleRateHz:           BigInt(sampleRate),
                frameSizeSamples:       frameSize,
                naturalUnits:           naturalUnits,
                preDelaySamples:        preDelay,
                daqMode:                daqModeProto,
                specSettingOne:         specOne,
                specSettingTwo:         specTwo,
                specSettingThree:       specThree,
                specSettingFour:        specFour,
                writeAdc:               writeAdc,
                adcArchivePath:         adcPath,
                writeSpectrogram:       writeSpec,
                spectrogramArchivePath: specPath,
            });

            const paths = [
                'frame_frequency_hz', 'sample_rate_hz', 'frame_size_samples',
                'natural_units', 'pre_delay_samples', 'daq_mode',
                'spec_setting_one', 'spec_setting_two', 'spec_setting_three',
                'spec_setting_four', 'write_adc', 'adc_archive_path',
                'write_spectrogram', 'spectrogram_archive_path',
            ];

            const restartNeeded = await pushSettings(patch, paths);
            if (restartNeeded)
                window.dispatchEvent(new CustomEvent('osc:streamRestartNeeded'));

            setApplyState('applied');
            appliedTimerRef.current = setTimeout(() => {
                setApplyState('idle');
                appliedTimerRef.current = null;
            }, 1500);

        } catch (e) {
            setError(e instanceof Error ? e.message : 'Failed to apply settings');
            setApplyState('idle');
        }
    };

    // ── Dir picker helpers ────────────────────────────────────────────────────
    const openPicker = (target: 'adc' | 'spec') => {
        setPickerTarget(target);
        setPickerOpen(true);
    };
    const handlePickerSelect = (path: string) => {
        if (pickerTarget === 'adc') setAdcPathD(path);
        else                        setSpecPathD(path);
    };

    // ── Render ────────────────────────────────────────────────────────────────
    return (
        <>
            <div
                className={`${styles.backdrop} ${open ? styles.backdropVisible : ''}`}
                onClick={onClose}
                aria-hidden="true"
            />

            <div
                className={`${styles.curtain} ${open ? styles.curtainOpen : ''}`}
                role="dialog"
                aria-modal="true"
                aria-label="Device settings"
            >
                <div className={styles.curtainHeader}>
                    <span className={styles.curtainTitle}>Device Settings</span>
                    <button className={styles.closeBtn} onClick={onClose} aria-label="Close">
                        <IconX />
                    </button>
                </div>

                {error && <div className={styles.errorBanner}>⚠ {error}</div>}

                <div className={styles.body}>

                    {/* ══ Synchronisation Settings ════════════════════════ */}
                    <section className={styles.section}>
                        <h3 className={styles.sectionTitle}>Synchronisation Settings</h3>

                        <div className={styles.field}>
                            <label className={styles.label}>DAQ Mode</label>
                            <Splitter
                                options={['daq_mode_1', 'daq_mode_2']}
                                value={daqMode === 'DAQ_MODE_1' ? 0 : 1}
                                onChange={v => setDaqModeD(v === 0 ? 'DAQ_MODE_1' : 'DAQ_MODE_2')}
                            />
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>Frame Frequency</label>
                            <Spinbox
                                value={frameFrequency}
                                onChange={setFrameFreqD}
                                min={1} max={10000} step={1}
                                unit="Hz"
                                delimited
                            />
                            <span className={styles.hint}>
                                Rate at which the ADC stream is chopped into frames
                            </span>
                        </div>
                    </section>

                    {/* ══ Frame Settings ══════════════════════════════════ */}
                    <section className={styles.section}>
                        <h3 className={styles.sectionTitle}>Frame Settings</h3>

                        <div className={styles.field}>
                            <label className={styles.label}>Units</label>
                            <Splitter
                                options={['natural', 'samples']}
                                value={naturalUnits ? 0 : 1}
                                onChange={v => setNaturalUnitsD(v === 0)}
                            />
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>Frame Size</label>
                            <Spinbox
                                value={frameSizeDisplay}
                                onChange={v => setFrameSizeD(
                                    naturalUnits ? Math.round(v * SAMPLES_PER_SECOND) : Math.round(v)
                                )}
                                min={0}
                                step={naturalUnits ? 0.001 : 1}
                                unit={frameSizeUnit}
                                delimited={!naturalUnits}
                            />
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>Pre-delay</label>
                            <Spinbox
                                value={preDelayDisplay}
                                onChange={v => setPreDelayD(
                                    naturalUnits ? Math.round(v * SAMPLES_PER_SECOND) : Math.round(v)
                                )}
                                min={0}
                                step={naturalUnits ? 0.001 : 1}
                                unit={preDelayUnit}
                                delimited={!naturalUnits}
                            />
                        </div>
                    </section>

                    {/* ══ ADC Settings ════════════════════════════════════ */}
                    <section className={styles.section}>
                        <h3 className={styles.sectionTitle}>ADC Settings</h3>

                        <div className={styles.field}>
                            <label className={styles.label}>
                                ADC Sample Rate
                                <span className={styles.labelVal}>{fmtHz(sampleRate)}</span>
                            </label>
                            <Spinbox
                                value={sampleRate}
                                onChange={setSampleRateD}
                                min={1_000_000} max={5_000_000_000} step={250_000_000}
                                unit="Hz"
                                delimited
                                width={150}
                            />
                        </div>
                    </section>

                    {/* ══ Spectrogram Settings ════════════════════════════ */}
                    <section className={styles.section}>
                        <h3 className={styles.sectionTitle}>Spectrogram Settings</h3>

                        <div className={styles.field}>
                            <label className={styles.label}>spec_setting_one</label>
                            <Spinbox value={specOne}   onChange={setSpecOneD}   step={0.1} unit="m" />
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>spec_setting_two</label>
                            <Spinbox value={specTwo}   onChange={setSpecTwoD}   step={0.1} />
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>spec_setting_three</label>
                            <Spinbox value={specThree} onChange={setSpecThreeD} step={0.1} />
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>spec_setting_four</label>
                            <Splitter
                                options={['opt_a', 'opt_b']}
                                value={specFour}
                                onChange={setSpecFourD}
                            />
                        </div>
                    </section>

                    {/* ══ Archive Settings ════════════════════════════════ */}
                    <section className={styles.section}>
                        <h3 className={styles.sectionTitle}>Archive Settings</h3>

                        <div className={styles.field}>
                            <label className={styles.label}>Write ADC</label>
                            <Splitter
                                options={['Yes', 'No']}
                                value={writeAdc ? 0 : 1}
                                onChange={v => setWriteAdcD(v === 0)}
                            />
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>ADC Archive Location</label>
                            <div className={styles.pathRow}>
                                <input
                                    type="text"
                                    className={styles.pathInput}
                                    value={adcPath}
                                    onChange={e => setAdcPathD(e.target.value)}
                                    spellCheck={false}
                                />
                                <button
                                    className={styles.navBtn}
                                    onClick={() => openPicker('adc')}
                                    aria-label="Browse ADC archive directory"
                                >
                                    <IconNav />
                                </button>
                            </div>
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>Write Spectrogram</label>
                            <Splitter
                                options={['Yes', 'No']}
                                value={writeSpec ? 0 : 1}
                                onChange={v => setWriteSpecD(v === 0)}
                            />
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>Spectrogram Archive Location</label>
                            <div className={styles.pathRow}>
                                <input
                                    type="text"
                                    className={styles.pathInput}
                                    value={specPath}
                                    onChange={e => setSpecPathD(e.target.value)}
                                    spellCheck={false}
                                />
                                <button
                                    className={styles.navBtn}
                                    onClick={() => openPicker('spec')}
                                    aria-label="Browse spectrogram archive directory"
                                >
                                    <IconNav />
                                </button>
                            </div>
                        </div>
                    </section>

                </div>{/* /body */}

                <div className={styles.footer}>
                    <button className={styles.cancelBtn} onClick={onClose}>
                        Cancel
                    </button>
                    <button
                        className={`${styles.applyBtn} ${applyState === 'applied' ? styles.applyBtnApplied : ''}`}
                        onClick={apply}
                        disabled={applyState === 'busy'}
                    >
                        {applyState === 'busy'    && <IconSpinner />}
                        {applyState === 'applied' && <IconTick />}
                        {applyState === 'idle'    && 'Apply'}
                    </button>
                </div>
            </div>

            {pickerOpen && (
                <DirPicker
                    initialPath={pickerTarget === 'adc' ? adcPath : specPath}
                    onSelect={handlePickerSelect}
                    onClose={() => setPickerOpen(false)}
                    browse={browseDirectory}
                />
            )}
        </>
    );
}
