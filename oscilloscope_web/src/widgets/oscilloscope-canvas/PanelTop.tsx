import { useSettingsStore, type ActiveTool } from '@/entities/oscilloscopeSettings/settingsStore';
import styles from './PanelTop.module.css';

interface Props {
    cursorX: string;
    cursorY: string;
}

// ── Inline SVG icons ──────────────────────────────────────────────────────────
const IconRectZoom = () => (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none" stroke="currentColor" strokeWidth="1.5">
        <rect x="2" y="3" width="10" height="8" rx="0.5" strokeDasharray="2 1.5"/>
        <line x1="7" y1="3" x2="7" y2="11"/>
    </svg>
);
const IconVertZoom = () => (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none" stroke="currentColor" strokeWidth="1.5">
        <line x1="4" y1="1" x2="4" y2="13"/>
        <line x1="10" y1="1" x2="10" y2="13"/>
        <path d="M1 7 L4 4 M1 7 L4 10" strokeWidth="1.2"/>
        <path d="M13 7 L10 4 M13 7 L10 10" strokeWidth="1.2"/>
    </svg>
);
const IconHorZoom = () => (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none" stroke="currentColor" strokeWidth="1.5">
        <line x1="1" y1="4" x2="13" y2="4"/>
        <line x1="1" y1="10" x2="13" y2="10"/>
        <path d="M7 1 L4 4 M7 1 L10 4" strokeWidth="1.2"/>
        <path d="M7 13 L4 10 M7 13 L10 10" strokeWidth="1.2"/>
    </svg>
);
const IconResetZoom = () => (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none" stroke="currentColor" strokeWidth="1.5">
        <rect x="2" y="2" width="10" height="10" rx="1"/>
        <path d="M5 7 L9 7 M7 5 L7 9" strokeWidth="1.2"/>
    </svg>
);
const IconPan = () => (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none" stroke="currentColor" strokeWidth="1.4">
        <path d="M7 1 L5 3 M7 1 L9 3"/>
        <path d="M7 13 L5 11 M7 13 L9 11"/>
        <path d="M1 7 L3 5 M1 7 L3 9"/>
        <path d="M13 7 L11 5 M13 7 L11 9"/>
        <line x1="7" y1="1" x2="7" y2="13"/>
        <line x1="1" y1="7" x2="13" y2="7"/>
    </svg>
);
const IconSum = () => (
    <svg width="14" height="14" viewBox="0 0 14 14" fill="none" stroke="currentColor" strokeWidth="1.5">
        <path d="M10 2 L3 2 L7 7 L3 12 L10 12"/>
    </svg>
);

// const CH0_COLOR = '#87cefa';
// const CH1_COLOR = '#f08080';

export function PanelTop({ cursorX, cursorY }: Props) {
    const {
        channelVisible, setChannelVisible,
        activeTool, setActiveTool,
        naturalUnits, setNaturalUnits,
        setXStart, setXShow, setYPeakToPeak,
    } = useSettingsStore();

    const pickTool = (t: ActiveTool) =>
        setActiveTool(activeTool === t ? 'none' : t);

    const resetZoom = () => {
        setActiveTool('none');
        setXStart(-1);
        setXShow(8192);
        setYPeakToPeak(8.0);
    };

    return (
        <div className={styles.bar}>
            {/* Channel toggles */}
            <button
                className={`${styles.chBtn} ${channelVisible[0] ? styles.ch0Active : styles.chInactive}`}
                onClick={() => setChannelVisible(0, !channelVisible[0])}
                title="Toggle IN0"
            >IN0</button>
            <button
                className={`${styles.chBtn} ${channelVisible[1] ? styles.ch1Active : styles.chInactive}`}
                onClick={() => setChannelVisible(1, !channelVisible[1])}
                title="Toggle IN1"
            >IN1</button>

            <div className={styles.sep} />

            {/* Zoom / Pan tools */}
            <button
                className={`${styles.toolBtn} ${activeTool === 'rectZoom' ? styles.toolActive : ''}`}
                onClick={() => pickTool('rectZoom')}
                title="Rectangle Zoom"
            ><IconRectZoom /></button>
            <button
                className={`${styles.toolBtn} ${activeTool === 'vertZoom' ? styles.toolActive : ''}`}
                onClick={() => pickTool('vertZoom')}
                title="Vertical Zoom (X axis)"
            ><IconVertZoom /></button>
            <button
                className={`${styles.toolBtn} ${activeTool === 'horZoom' ? styles.toolActive : ''}`}
                onClick={() => pickTool('horZoom')}
                title="Horizontal Zoom (Y axis)"
            ><IconHorZoom /></button>
            <button
                className={styles.toolBtn}
                onClick={resetZoom}
                title="Reset Zoom"
            ><IconResetZoom /></button>
            <button
                className={`${styles.toolBtn} ${activeTool === 'pan' ? styles.toolActive : ''}`}
                onClick={() => pickTool('pan')}
                title="Pan View"
            ><IconPan /></button>
            <button
                className={`${styles.toolBtn} ${activeTool === 'none' ? '' : ''}`}
                style={{ display: 'none' }}
                title="Sum Mode"
            ><IconSum /></button>

            <div className={styles.sep} />

            {/* Units toggle */}
            <span className={styles.label}>Units:</span>
            <div className={styles.segmented}>
                <button
                    className={`${styles.segBtn} ${!naturalUnits ? styles.segActive : ''}`}
                    onClick={() => setNaturalUnits(false)}
                >Samples</button>
                <button
                    className={`${styles.segBtn} ${naturalUnits ? styles.segActive : ''}`}
                    onClick={() => setNaturalUnits(true)}
                >Natural</button>
            </div>

            <div className={styles.spacer} />

            {/* Cursor coordinates */}
            <div className={styles.sep} />
            <span className={styles.coordLabel}>X:</span>
            <span className={styles.coordValue}>{cursorX}</span>
            <div className={styles.sep} />
            <span className={styles.coordLabel}>Y:</span>
            <span className={styles.coordValue}>{cursorY}</span>
        </div>
    );
}
