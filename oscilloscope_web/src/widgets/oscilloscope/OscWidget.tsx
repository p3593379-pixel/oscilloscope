import { useRef, useState, useCallback } from 'react';
import { OscilloscopeCanvas }  from '@/widgets/oscilloscope-canvas/OscilloscopeCanvas';
import { PanelTop }             from '@/widgets/oscilloscope-canvas/PanelTop';
import { PanelBottom }          from '@/widgets/oscilloscope-canvas/PanelBottom';
import { InteractionOverlay }   from '@/widgets/oscilloscope-canvas/InteractionOverlay';
import styles                   from './OscWidget.module.css';

export function OscWidget() {
    const workerRef = useRef<Worker | null>(null);
    const [cursorX, setCursorX] = useState('—');
    const [cursorY, setCursorY] = useState('—');

    const handleCursorChange = useCallback((x: string, y: string) => {
        setCursorX(x);
        setCursorY(y);
    }, []);

    return (
        <div className={styles.widget}>
            {/* 5-px white strip matching OscWidget.qml: Rectangle { height: 5; color: "white" } */}
            <div className={styles.topStrip}/>

            <PanelTop cursorX={cursorX} cursorY={cursorY}/>

            <div className={styles.canvasArea}>
                <OscilloscopeCanvas workerRef={workerRef}/>
                <InteractionOverlay onCursorChange={handleCursorChange}/>
            </div>

            <PanelBottom/>
        </div>
    );
}