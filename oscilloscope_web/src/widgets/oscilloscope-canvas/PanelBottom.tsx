import { useState } from 'react';
import { useSettingsStore } from '@/entities/oscilloscopeSettings/settingsStore';
import styles from './PanelBottom.module.css';

const STEP_OPTIONS = [16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536];
const DEFAULT_STEP_IDX = 6; // 1024

const IconLeft = () => (
    <svg width="12" height="12" viewBox="0 0 12 12" fill="none" stroke="currentColor" strokeWidth="1.5">
        <path d="M8 2 L3 6 L8 10"/>
    </svg>
);
const IconRight = () => (
    <svg width="12" height="12" viewBox="0 0 12 12" fill="none" stroke="currentColor" strokeWidth="1.5">
        <path d="M4 2 L9 6 L4 10"/>
    </svg>
);

export function PanelBottom() {
    const {
        xStart, xShow, yPeakToPeak, naturalUnits,
        setXStart, setXShow, setYPeakToPeak,
    } = useSettingsStore();

    const [step, setStep] = useState(STEP_OPTIONS[DEFAULT_STEP_IDX]);

    const units = naturalUnits ? 'm' : 'smps';

    const handleXStartBlur = (v: string) => {
        const n = parseInt(v, 10);
        if (!isNaN(n)) setXStart(n);
    };
    const handleXShowBlur = (v: string) => {
        const n = parseInt(v, 10);
        if (!isNaN(n) && n > 0) setXShow(n);
    };
    const handleYPPBlur = (v: string) => {
        const n = parseFloat(v);
        if (!isNaN(n) && n > 0) setYPeakToPeak(n);
    };

    // "Move left" → older data → larger offset from end → increase xStart
    const moveLeft = () => {
        const eff = xStart < 0 ? 0 : xStart;
        setXStart(eff + step);
    };
    // "Move right" → newer data → smaller offset from end → decrease xStart
    const moveRight = () => {
        const eff = xStart < 0 ? 0 : xStart;
        setXStart(Math.max(0, eff - step));
    };

    return (
        <div className={styles.bar}>
            {/* ── X start ── */}
            <span className={styles.label}>X start:</span>
            <input
                type="number"
                className={styles.spinBox}
                defaultValue={xStart}
                key={xStart}          /* re-mount when external change resets to live */
                onBlur={e => handleXStartBlur(e.target.value)}
                onKeyDown={e => e.key === 'Enter' && handleXStartBlur((e.target as HTMLInputElement).value)}
                step={step}
            />
            <span className={styles.units}>{units}</span>

            <div className={styles.sep}/>

            {/* ── X show ── */}
            <span className={styles.label}>Show:</span>
            <input
                type="number"
                className={styles.spinBox}
                defaultValue={xShow}
                key={`show-${xShow}`}
                min={1}
                onBlur={e => handleXShowBlur(e.target.value)}
                onKeyDown={e => e.key === 'Enter' && handleXShowBlur((e.target as HTMLInputElement).value)}
                step={step}
            />
            <span className={styles.units}>{units}</span>

            <div className={styles.sep}/>

            {/* ── Move by ── */}
            <span className={styles.label}>Move by:</span>
            <button className={styles.arrowBtn} onClick={moveLeft} title="Move view left (older data)">
                <IconLeft/>
            </button>
            <select
                className={styles.stepSelect}
                value={step}
                onChange={e => setStep(parseInt(e.target.value, 10))}
            >
                {STEP_OPTIONS.map(v => <option key={v} value={v}>{v}</option>)}
            </select>
            <span className={styles.units}>{units}</span>
            <button className={styles.arrowBtn} onClick={moveRight} title="Move view right (newer data)">
                <IconRight/>
            </button>

            <div className={styles.sep}/>

            {/* ── Y peak-to-peak ── */}
            <span className={styles.label}>Y peak-to-peak:</span>
            <input
                type="number"
                className={styles.spinBox}
                defaultValue={yPeakToPeak}
                key={`ypp-${yPeakToPeak}`}
                min={0.001}
                step={0.5}
                onBlur={e => handleYPPBlur(e.target.value)}
                onKeyDown={e => e.key === 'Enter' && handleYPPBlur((e.target as HTMLInputElement).value)}
            />
            <span className={styles.units}>V</span>
        </div>
    );
}
