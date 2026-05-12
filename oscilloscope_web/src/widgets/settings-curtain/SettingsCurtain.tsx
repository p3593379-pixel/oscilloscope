// src/widgets/settings-curtain/SettingsCurtain.tsx
import { useState, useEffect } from 'react';
import { useSettingsStore, type TriggerMode, type TriggerEdge }
    from '@/entities/oscilloscopeSettings/settingsStore';
import { useOscilloscopeSettings } from '@/features/oscilloscope-settings/useOscilloscopeSettings';
import { create } from '@bufbuild/protobuf';
import {
    OscilloscopeSettingsSchema,
    TriggerConfigSchema,
    ChannelConfigSchema,
    TriggerMode   as ProtoTriggerMode,
    TriggerEdge   as ProtoTriggerEdge,
    Interpolation as ProtoInterpolation,
} from '@/generated/oscilloscope_interface_pb';
import styles from './SettingsCurtain.module.css';

interface Props {
    open:           boolean;
    onClose:        () => void;
}

export function SettingsCurtain({ open, onClose }: Props) {
    const store                              = useSettingsStore();
    const { fetchSettings, pushSettings, loading, error } = useOscilloscopeSettings();

    // ── Draft state ───────────────────────────────────────────────────────────
    const [triggerMode,    setTriggerMode]    = useState(store.triggerMode);
    const [triggerEdge,    setTriggerEdge]    = useState(store.triggerEdge);
    const [triggerLevel,   setTriggerLevel]   = useState(store.triggerLevel);
    const [triggerChannel, setTriggerChannel] = useState(store.triggerChannel);
    const [targetFps,      setTargetFps]      = useState(store.targetFps);
    const [maxBandwidth,   setMaxBandwidth]   = useState(store.maxBandwidthMbps);
    const [naturalUnits,   setNaturalUnits]   = useState(store.naturalUnits);
    const [ch0Visible,     setCh0Visible]     = useState(store.channelVisible[0]);
    const [ch1Visible,     setCh1Visible]     = useState(store.channelVisible[1]);
    const [ch0VPD,         setCh0VPD]         = useState(store.voltsPerDiv[0]);
    const [ch1VPD,         setCh1VPD]         = useState(store.voltsPerDiv[1]);
    const [ch0Offset,      setCh0Offset]      = useState(store.verticalOffset[0]);
    const [ch1Offset,      setCh1Offset]      = useState(store.verticalOffset[1]);
    const [interpolation,  setInterpolation]  = useState<'linear'|'sinc'|'step'>('sinc');
    const [persistence,    setPersistence]    = useState(false);
    const [persistDecay,   setPersistDecay]   = useState(0.85);
    const [gridOpacity,    setGridOpacity]    = useState(0.35);
    const [theme,          setTheme]          = useState<'dark'|'amber'|'green'>('dark');

    useEffect(() => {
        if (!open) return;

        // Sync draft from store + fetch fresh from server
        setTriggerMode(store.triggerMode);
        setTriggerEdge(store.triggerEdge);
        setTriggerLevel(store.triggerLevel);
        setTriggerChannel(store.triggerChannel);
        setTargetFps(store.targetFps);
        setMaxBandwidth(store.maxBandwidthMbps);
        setNaturalUnits(store.naturalUnits);
        setCh0Visible(store.channelVisible[0]);
        setCh1Visible(store.channelVisible[1]);
        setCh0VPD(store.voltsPerDiv[0]);
        setCh1VPD(store.voltsPerDiv[1]);
        setCh0Offset(store.verticalOffset[0]);
        setCh1Offset(store.verticalOffset[1]);
        fetchSettings();  // ← GET /GetSettings, result flows into store, next open will re-sync
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [open]);

    // Escape key
    useEffect(() => {
        if (!open) return;
        const h = (e: KeyboardEvent) => { if (e.key === 'Escape') onClose(); };
        window.addEventListener('keydown', h);
        return () => window.removeEventListener('keydown', h);
    }, [open, onClose]);

    // ── Apply ─────────────────────────────────────────────────────────────────
    const apply = async () => {
        const triggerModeMap: Record<TriggerMode, ProtoTriggerMode> = {
            AUTO:   ProtoTriggerMode.AUTO,
            NORMAL: ProtoTriggerMode.NORMAL,
            SINGLE: ProtoTriggerMode.SINGLE,
        };
        const triggerEdgeMap: Record<TriggerEdge, ProtoTriggerEdge> = {
            RISING:  ProtoTriggerEdge.RISING,
            FALLING: ProtoTriggerEdge.FALLING,
        };
        const interpolationMap: Record<string, ProtoInterpolation> = {
            linear: ProtoInterpolation.LINEAR,
            sinc:   ProtoInterpolation.SINC,
            step:   ProtoInterpolation.STEP,
        };

        const patch = create(OscilloscopeSettingsSchema, {
            targetFps:         targetFps,
            maxBandwidthMbps:  maxBandwidth,
            naturalUnits:      naturalUnits,
            gridOpacity:       gridOpacity,
            persistenceEnabled: persistence,
            persistenceDecay:  persistDecay,
            displayTheme:      theme === 'dark' ? 0 : theme === 'amber' ? 1 : 2,
            interpolation:     interpolationMap[interpolation],
            trigger: create(TriggerConfigSchema, {
                mode:    triggerModeMap[triggerMode],
                edge:    triggerEdgeMap[triggerEdge],
                levelV:  triggerLevel,
                channel: triggerChannel,
            }),
            channels: [
                create(ChannelConfigSchema, { enabled: ch0Visible, voltsPerDiv: ch0VPD, verticalOffset: ch0Offset }),
                create(ChannelConfigSchema, { enabled: ch1Visible, voltsPerDiv: ch1VPD, verticalOffset: ch1Offset }),
            ],
        });

        // Build field mask paths for every field we're sending
        const paths = [
            'target_fps', 'max_bandwidth_mbps', 'natural_units',
            'grid_opacity', 'persistence_enabled', 'persistence_decay',
            'display_theme', 'interpolation',
            'trigger.mode', 'trigger.edge', 'trigger.level_v', 'trigger.channel',
            'channels',
        ];

        const restartNeeded = await pushSettings(patch, paths);
        if (restartNeeded) {
            // Signal the data stream to re-negotiate (dispatch a custom event;
            // useDataStream can listen for it and restart the stream)
            window.dispatchEvent(new CustomEvent('osc:streamRestartNeeded'));
        }

        // Commit draft to local store
        store.setTriggerMode(triggerMode);
        store.setTriggerEdge(triggerEdge);
        store.setTriggerLevel(triggerLevel);
        store.setTriggerChannel(triggerChannel);
        store.setTargetFps(targetFps);
        store.setMaxBandwidth(maxBandwidth);
        store.setNaturalUnits(naturalUnits);
        store.setChannelVisible(0, ch0Visible);
        store.setChannelVisible(1, ch1Visible);
        store.setVoltsPerDiv(0, ch0VPD);
        store.setVoltsPerDiv(1, ch1VPD);
        store.setVerticalOffset(0, ch0Offset);
        store.setVerticalOffset(1, ch1Offset);
        onClose();
    };

    return (
        <>
            {/* Backdrop */}
            <div
                className={`${styles.backdrop} ${open ? styles.backdropVisible : ''}`}
                onClick={onClose}
                aria-hidden="true"
            />

            <div
                className={`${styles.curtain} ${open ? styles.curtainOpen : ''}`}
                role="dialog"
                aria-modal="true"
                aria-label="Acquisition settings"
            >
                {/* Header */}
                <div className={styles.curtainHeader}>
                    <span className={styles.curtainTitle}>Acquisition Settings</span>
                    <button className={styles.closeBtn} onClick={onClose} aria-label="Close settings">
                        <IconX />
                    </button>
                </div>

                {/* Error banner */}
                {error && (
                    <div className={styles.errorBanner}>⚠ {error}</div>
                )}

                {/* Body grid */}
                <div className={styles.body}>

                    {/* ══ Trigger ══ */}
                    <section className={styles.section}>
                        <h3 className={styles.sectionTitle}>Trigger</h3>

                        <div className={styles.field}>
                            <label className={styles.label}>Mode</label>
                            <div className={styles.segmented}>
                                {(['AUTO', 'NORMAL', 'SINGLE'] as TriggerMode[]).map(m => (
                                    <button key={m}
                                            className={`${styles.seg} ${triggerMode === m ? styles.segActive : ''}`}
                                            onClick={() => setTriggerMode(m)}>{m}</button>
                                ))}
                            </div>
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>Edge</label>
                            <div className={styles.segmented}>
                                <button
                                    className={`${styles.seg} ${triggerEdge === 'RISING' ? styles.segActive : ''}`}
                                    onClick={() => setTriggerEdge('RISING')}
                                ><IconRising /> Rising</button>
                                <button
                                    className={`${styles.seg} ${triggerEdge === 'FALLING' ? styles.segActive : ''}`}
                                    onClick={() => setTriggerEdge('FALLING')}
                                ><IconFalling /> Falling</button>
                            </div>
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>
                                Level <span className={styles.labelVal}>{triggerLevel.toFixed(3)} V</span>
                            </label>
                            <input type="range" min="-5" max="5" step="0.001"
                                   value={triggerLevel}
                                   onChange={e => setTriggerLevel(parseFloat(e.target.value))}
                                   className={styles.slider} />
                            <div className={styles.sliderTicks}><span>−5 V</span><span>0</span><span>+5 V</span></div>
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>Source</label>
                            <div className={styles.radioPills}>
                                {[0, 1].map(ch => (
                                    <label key={ch}
                                           className={`${styles.radioPill} ${triggerChannel === ch ? styles.radioPillActive : ''}`}>
                                        <input type="radio" name="trigCh" value={ch}
                                               checked={triggerChannel === ch}
                                               onChange={() => setTriggerChannel(ch)}
                                               className={styles.srOnly} />
                                        CH {ch + 1}
                                    </label>
                                ))}
                            </div>
                        </div>
                    </section>

                    {/* ══ Channels ══ */}
                    <section className={styles.section}>
                        <h3 className={styles.sectionTitle}>Channels</h3>
                        {([
                            { label: 'CH 1', color: '#1565c0', visible: ch0Visible, setVisible: setCh0Visible,
                                vpd: ch0VPD, setVpd: setCh0VPD, offset: ch0Offset, setOffset: setCh0Offset },
                            { label: 'CH 2', color: '#c62828', visible: ch1Visible, setVisible: setCh1Visible,
                                vpd: ch1VPD, setVpd: setCh1VPD, offset: ch1Offset, setOffset: setCh1Offset },
                        ]).map(ch => (
                            <div key={ch.label} className={styles.channelRow}>
                                <div className={styles.channelLabel}>
                                    <span className={styles.chDot} style={{ background: ch.color }} />
                                    {ch.label}
                                    <label className={styles.toggle}>
                                        <input type="checkbox" checked={ch.visible}
                                               onChange={e => ch.setVisible(e.target.checked)} />
                                        <span className={styles.toggleTrack}><span className={styles.toggleThumb} /></span>
                                    </label>
                                </div>
                                <div className={styles.channelControls}>
                                    <div className={styles.inlineField}>
                                        <span className={styles.inlineLabel}>V/div</span>
                                        <select className={styles.select} value={ch.vpd}
                                                onChange={e => ch.setVpd(parseFloat(e.target.value))}>
                                            {[0.05, 0.1, 0.2, 0.5, 1, 2, 5].map(v =>
                                                <option key={v} value={v}>{v} V</option>)}
                                        </select>
                                    </div>
                                    <div className={styles.inlineField}>
                                        <span className={styles.inlineLabel}>Offset</span>
                                        <input type="number" className={styles.numInput} step="0.01"
                                               value={ch.offset}
                                               onChange={e => ch.setOffset(parseFloat(e.target.value))} />
                                        <span className={styles.unit}>V</span>
                                    </div>
                                </div>
                            </div>
                        ))}
                    </section>

                    {/* ══ Acquisition ══ */}
                    <section className={styles.section}>
                        <h3 className={styles.sectionTitle}>Acquisition</h3>

                        <div className={styles.field}>
                            <label className={styles.label}>Interpolation</label>
                            <div className={styles.segmented}>
                                {(['linear', 'sinc', 'step'] as const).map(m => (
                                    <button key={m}
                                            className={`${styles.seg} ${interpolation === m ? styles.segActive : ''}`}
                                            onClick={() => setInterpolation(m)}>{m}</button>
                                ))}
                            </div>
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>
                                Target FPS <span className={styles.labelVal}>{targetFps} fps</span>
                            </label>
                            <div className={styles.stepper}>
                                <button className={styles.stepBtn}
                                        onClick={() => setTargetFps(Math.max(1, targetFps - 5))}>−</button>
                                <span className={styles.stepVal}>{targetFps}</span>
                                <button className={styles.stepBtn}
                                        onClick={() => setTargetFps(Math.min(120, targetFps + 5))}>+</button>
                            </div>
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>
                                Max bandwidth
                                <span className={styles.labelVal}>
                  {maxBandwidth === 0 ? '∞ unlimited' : `${maxBandwidth} Mbps`}
                </span>
                            </label>
                            <input type="range" min="0" max="1000" step="10"
                                   value={maxBandwidth}
                                   onChange={e => setMaxBandwidth(parseInt(e.target.value))}
                                   className={styles.slider} />
                            <div className={styles.sliderTicks}><span>∞</span><span>500</span><span>1000 Mbps</span></div>
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>
                                Waveform persistence
                            </label>
                            <label className={styles.toggle}>
                                <input type="checkbox" checked={persistence}
                                       onChange={e => setPersistence(e.target.checked)} />
                                <span className={styles.toggleTrack}><span className={styles.toggleThumb} /></span>
                            </label>
                        </div>

                        {persistence && (
                            <div className={styles.field}>
                                <label className={styles.label}>
                                    Decay <span className={styles.labelVal}>{persistDecay.toFixed(2)}</span>
                                </label>
                                <input type="range" min="0.5" max="0.99" step="0.01"
                                       value={persistDecay}
                                       onChange={e => setPersistDecay(parseFloat(e.target.value))}
                                       className={styles.slider} />
                            </div>
                        )}
                    </section>

                    {/* ══ Display ══ */}
                    <section className={styles.section}>
                        <h3 className={styles.sectionTitle}>Display</h3>

                        <div className={styles.field}>
                            <label className={styles.label}>Screen theme</label>
                            <div className={styles.swatches}>
                                {([
                                    { key: 'dark',  bg: '#0d1117', ring: '#4f98a3' },
                                    { key: 'amber', bg: '#1a0f00', ring: '#f59e0b' },
                                    { key: 'green', bg: '#001a0f', ring: '#22c55e' },
                                ] as const).map(t => (
                                    <button key={t.key}
                                            className={`${styles.swatch} ${theme === t.key ? styles.swatchActive : ''}`}
                                            style={{ background: t.bg, '--swatch-ring': t.ring } as React.CSSProperties}
                                            onClick={() => setTheme(t.key)}
                                            title={t.key} />
                                ))}
                            </div>
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>
                                Grid opacity <span className={styles.labelVal}>{Math.round(gridOpacity * 100)}%</span>
                            </label>
                            <input type="range" min="0" max="1" step="0.01"
                                   value={gridOpacity}
                                   onChange={e => setGridOpacity(parseFloat(e.target.value))}
                                   className={styles.slider} />
                        </div>

                        <div className={styles.field}>
                            <label className={styles.label}>Natural units (Hz/s)</label>
                            <label className={styles.toggle}>
                                <input type="checkbox" checked={naturalUnits}
                                       onChange={e => setNaturalUnits(e.target.checked)} />
                                <span className={styles.toggleTrack}><span className={styles.toggleThumb} /></span>
                            </label>
                        </div>
                    </section>

                </div>{/* /body */}

                {/* Footer */}
                <div className={styles.footer}>
                    <button className={styles.cancelBtn} onClick={onClose}>Cancel</button>
                    <button className={styles.applyBtn} onClick={apply} disabled={loading}>
                        {loading ? <Spinner /> : 'Apply'}
                    </button>
                </div>
            </div>
        </>
    );
}

// ── Inline icons ──────────────────────────────────────────────────────────────
const IconX = () => (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none"
         stroke="currentColor" strokeWidth="2" strokeLinecap="round">
        <line x1="2" y1="2" x2="12" y2="12"/><line x1="12" y1="2" x2="2" y2="12"/>
    </svg>
);
const IconRising = () => (
    <svg width="12" height="12" viewBox="0 0 12 12" fill="none"
         stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round">
        <polyline points="1,9 4,9 4,3 8,3 8,9 11,9"/>
    </svg>
);
const IconFalling = () => (
    <svg width="12" height="12" viewBox="0 0 12 12" fill="none"
         stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round">
        <polyline points="1,3 4,3 4,9 8,9 8,3 11,3"/>
    </svg>
);
const Spinner = () => (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none"
         stroke="currentColor" strokeWidth="2" strokeLinecap="round"
         style={{ animation: 'spin 0.7s linear infinite' }}>
        <path d="M7 1.5 A5.5 5.5 0 1 1 1.5 7"/>
    </svg>
);