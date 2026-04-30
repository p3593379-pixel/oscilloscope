import { useRef, useState, useCallback, useEffect } from 'react';
import { useSettingsStore } from '@/entities/oscilloscopeSettings/settingsStore';
import styles from './InteractionOverlay.module.css';

interface Props {
  onCursorChange: (x: string, y: string) => void;
}

interface SelRect { left: number; top: number; width: number; height: number; }

export function InteractionOverlay({ onCursorChange }: Props) {
  const overlayRef  = useRef<HTMLDivElement>(null);
  const isDragging  = useRef(false);
  const dragStart   = useRef({ x: 0, y: 0 });
  const [selRect, setSelRect] = useState<SelRect | null>(null);

  // Keep a live ref to settings so event handlers never capture stale state
  const settingsRef = useRef(useSettingsStore.getState());
  useEffect(() => useSettingsStore.subscribe(s => { settingsRef.current = s; }), []);

  // ── coordinate helpers ──────────────────────────────────────────────────────

  const pixelToData = useCallback((px: number, py: number, W: number, H: number) => {
    const { xStart, xShow, yPeakToPeak } = settingsRef.current;
    const effXStart = xStart < 0 ? 0 : xStart;
    // left edge  = effXStart + xShow (oldest visible sample, furthest from end)
    // right edge = effXStart         (newest visible sample)
    const xData = effXStart + xShow * (1 - px / W);
    const yData = (H / 2 - py) * yPeakToPeak / H;
    return { xData, yData };
  }, []);

  const formatX = (v: number) => {
    const nat = settingsRef.current.naturalUnits;
    return nat ? `${(v / 1000).toFixed(3)} m` : `${Math.round(v)} smp`;
  };
  const formatY = (v: number) => `${v.toFixed(3)} V`;

  // ── mouse handlers ──────────────────────────────────────────────────────────

  const onMouseDown = useCallback((e: React.MouseEvent<HTMLDivElement>) => {
    const el = overlayRef.current;
    if (!el || e.button !== 0) return;
    const rect = el.getBoundingClientRect();
    const px   = e.clientX - rect.left;
    const py   = e.clientY - rect.top;
    isDragging.current  = true;
    dragStart.current   = { x: px, y: py };
    setSelRect(null);
  }, []);

  const onMouseMove = useCallback((e: React.MouseEvent<HTMLDivElement>) => {
    const el = overlayRef.current;
    if (!el) return;
    const rect  = el.getBoundingClientRect();
    const px    = e.clientX - rect.left;
    const py    = e.clientY - rect.top;
    const W     = rect.width;
    const H     = rect.height;

    // Update cursor display
    const { xData, yData } = pixelToData(px, py, W, H);
    onCursorChange(formatX(xData), formatY(yData));

    if (!isDragging.current) return;

    const ds                                    = dragStart.current;
    const { activeTool, xStart, xShow, yPeakToPeak } = settingsRef.current;

    if (activeTool === 'pan') {
      const dx        = px - ds.x;
      const dy        = py - ds.y;
      const effXStart = xStart < 0 ? 0 : xStart;
      // drag right → see older data → increase xStart (offset from end)
      const newXStart = Math.max(0, effXStart + dx * xShow / W);
      settingsRef.current.setXStart(newXStart);
      // drag down → waveform shifts up → verticalOffset increases
      const yDelta = dy * yPeakToPeak / H;
      const vo     = settingsRef.current.verticalOffset;
      settingsRef.current.setVerticalOffset(0, (vo[0] ?? 0) + yDelta);
      settingsRef.current.setVerticalOffset(1, (vo[1] ?? 0) + yDelta);
      dragStart.current = { x: px, y: py }; // incremental delta
    } else if (activeTool === 'rectZoom') {
      const x1 = Math.min(ds.x, px); const y1 = Math.min(ds.y, py);
      setSelRect({ left: x1, top: y1, width: Math.abs(px - ds.x), height: Math.abs(py - ds.y) });
    } else if (activeTool === 'vertZoom') {
      const x1 = Math.min(ds.x, px);
      setSelRect({ left: x1, top: 0, width: Math.abs(px - ds.x), height: H });
    } else if (activeTool === 'horZoom') {
      const y1 = Math.min(ds.y, py);
      setSelRect({ left: 0, top: y1, width: W, height: Math.abs(py - ds.y) });
    }
  }, [onCursorChange, pixelToData]);

  const onMouseUp = useCallback((e: React.MouseEvent<HTMLDivElement>) => {
    const el = overlayRef.current;
    if (!el || !isDragging.current) return;
    isDragging.current = false;

    const rect = el.getBoundingClientRect();
    const px   = e.clientX - rect.left;
    const py   = e.clientY - rect.top;
    const W    = rect.width;
    const H    = rect.height;
    const ds   = dragStart.current;
    const { activeTool, xStart, xShow, yPeakToPeak } = settingsRef.current;
    const effXStart = xStart < 0 ? 0 : xStart;

    if (activeTool === 'rectZoom') {
      const x1 = Math.min(ds.x, px); const x2 = Math.max(ds.x, px);
      const y1 = Math.min(ds.y, py); const y2 = Math.max(ds.y, py);
      if (x2 - x1 > 5 && y2 - y1 > 5) {
        const newXShow  = Math.max(1, Math.round(xShow * (x2 - x1) / W));
        // right edge of selection (x2) maps to newXStart (offset from end)
        const newXStart = Math.max(0, effXStart + Math.round(xShow * (1 - x2 / W)));
        const newYPP    = Math.max(0.001, yPeakToPeak * (y2 - y1) / H);
        settingsRef.current.setXRange(newXStart, newXShow);
        settingsRef.current.setYPeakToPeak(newYPP);
      }
    } else if (activeTool === 'vertZoom') {
      const x1 = Math.min(ds.x, px); const x2 = Math.max(ds.x, px);
      if (x2 - x1 > 5) {
        const newXShow  = Math.max(1, Math.round(xShow * (x2 - x1) / W));
        const newXStart = Math.max(0, effXStart + Math.round(xShow * (1 - x2 / W)));
        settingsRef.current.setXRange(newXStart, newXShow);
      }
    } else if (activeTool === 'horZoom') {
      const y1 = Math.min(ds.y, py); const y2 = Math.max(ds.y, py);
      if (y2 - y1 > 5) {
        settingsRef.current.setYPeakToPeak(Math.max(0.001, yPeakToPeak * (y2 - y1) / H));
      }
    }
    setSelRect(null);
  }, []);

  const onMouseLeave = useCallback(() => {
    isDragging.current = false;
    setSelRect(null);
    onCursorChange('—', '—');
  }, [onCursorChange]);

  // ── scroll wheel zoom ───────────────────────────────────────────────────────
  const onWheel = useCallback((e: React.WheelEvent<HTMLDivElement>) => {
    e.preventDefault();
    const el = overlayRef.current;
    if (!el) return;
    const rect  = el.getBoundingClientRect();
    const px    = e.clientX - rect.left;
    const W     = rect.width;
    const { xStart, xShow } = settingsRef.current;
    const effXStart = xStart < 0 ? 0 : xStart;

    const factor  = e.deltaY > 0 ? 1.25 : 0.8; // scroll down = zoom out
    const pivot   = effXStart + xShow * (1 - px / W); // data coord under cursor
    const newXShow   = Math.max(1, Math.round(xShow * factor));
    // keep pivot fixed: newXStart = pivot - newXShow*(1-px/W)
    const newXStart  = Math.max(0, Math.round(pivot - newXShow * (1 - px / W)));
    settingsRef.current.setXRange(newXStart, newXShow);
  }, []);

  // ── cursor style ────────────────────────────────────────────────────────────
  const activeTool = useSettingsStore(s => s.activeTool);
  const cursor =
    activeTool === 'pan'      ? 'grab'      :
    activeTool === 'rectZoom' ? 'crosshair' :
    activeTool === 'vertZoom' ? 'col-resize':
    activeTool === 'horZoom'  ? 'row-resize':
    'crosshair';

  return (
    <div
      ref={overlayRef}
      className={styles.overlay}
      style={{ cursor }}
      onMouseDown={onMouseDown}
      onMouseMove={onMouseMove}
      onMouseUp={onMouseUp}
      onMouseLeave={onMouseLeave}
      onWheel={onWheel}
    >
      {selRect && (
        <div
          className={styles.selRect}
          style={{
            left:   selRect.left,
            top:    selRect.top,
            width:  selRect.width,
            height: selRect.height,
          }}
        />
      )}
    </div>
  );
}
