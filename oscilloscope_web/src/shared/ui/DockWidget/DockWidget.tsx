import {
    useCallback, useEffect, useId, useRef, useState, type ReactNode,
} from 'react';
import { useDockArea, type DockSide } from './DockWidgetContext';
import styles from './DockWidget.module.css';

const BASE_Z = 10;

const IconPin = ({ active }: { active: boolean }) => (
    <svg width="10" height="10" viewBox="0 0 10 10" fill="none"
         stroke="currentColor" strokeWidth="1.4" strokeLinecap="round"
         style={{ transform: active ? 'rotate(45deg)' : 'none', transition: 'transform 200ms' }}>
        <line x1="5" y1="1" x2="5" y2="9" />
        <line x1="2" y1="3" x2="8" y2="3" />
        <line x1="3" y1="3" x2="3" y2="7" />
        <line x1="7" y1="3" x2="7" y2="7" />
    </svg>
);

const IconClose = () => (
    <svg width="9" height="9" viewBox="0 0 9 9" fill="none"
         stroke="currentColor" strokeWidth="1.5" strokeLinecap="round">
        <line x1="1.5" y1="1.5" x2="7.5" y2="7.5" />
        <line x1="7.5" y1="1.5" x2="1.5" y2="7.5" />
    </svg>
);

type ResizeDir = 'n' | 's' | 'e' | 'w' | 'nw' | 'ne' | 'sw' | 'se';
const DIR_CLASS: Record<ResizeDir, string> = {
    n: styles.resizeN, s: styles.resizeS, e: styles.resizeE, w: styles.resizeW,
    nw: styles.resizeNW, ne: styles.resizeNE, sw: styles.resizeSW, se: styles.resizeSE,
};

function ResizeHandle({ dir, onResize }: {
    dir: ResizeDir;
    onResize: (dir: ResizeDir, dx: number, dy: number) => void;
}) {
    const onPD = useCallback((e: React.PointerEvent) => {
        e.stopPropagation();
        e.currentTarget.setPointerCapture(e.pointerId);
        e.preventDefault();
        let lx = e.clientX, ly = e.clientY;
        const mv = (ev: PointerEvent) => {
            onResize(dir, ev.clientX - lx, ev.clientY - ly);
            lx = ev.clientX; ly = ev.clientY;
        };
        const up = () => {
            window.removeEventListener('pointermove', mv);
            window.removeEventListener('pointerup', up);
        };
        window.addEventListener('pointermove', mv);
        window.addEventListener('pointerup', up);
    }, [dir, onResize]);
    return <div className={DIR_CLASS[dir]} onPointerDown={onPD} />;
}

export interface DockWidgetProps {
    title: string;
    initialX?: number;
    initialY?: number;
    initialWidth?: number;
    initialHeight?: number;
    minWidth?: number;
    minHeight?: number;
    defaultPinned?: boolean;
    defaultPinnedSide?: DockSide;
    children?: ReactNode;
}

export function DockWidget({
                               title,
                               initialX = 80, initialY = 80,
                               initialWidth = 320, initialHeight = 220,
                               minWidth = 150, minHeight = 100,
                               defaultPinned = false,
                               defaultPinnedSide = 'left',
                               children,
                           }: DockWidgetProps) {
    const rawId = useId();
    const uid   = rawId.replace(/[^a-zA-Z0-9_-]/g, '_');

    const ctx = useDockArea();
    const {
        registerFloating, unregisterFloating, bringToFront,
        pinWidget, unpinWidget,
        notifyDragStart, notifyDragEnd,
        registerDragHandlers, unregisterDragHandlers,
        pinnedIds, zMap, registerElement,
    } = ctx;

    const [visible, setVisible] = useState(true);
    const [pos,  setPos]  = useState({ x: initialX,    y: initialY });
    const [size, setSize] = useState({ w: initialWidth, h: initialHeight });
    const saved    = useRef({ x: initialX, y: initialY, w: initialWidth, h: initialHeight });
    const lastSide = useRef<DockSide>(defaultPinnedSide);

    const isPinned = pinnedIds.has(uid);
    const zIndex   = zMap[uid] ?? BASE_Z;

    // ── Mount ──────────────────────────────────────────────────────────────────
    useEffect(() => {
        if (defaultPinned) {
            lastSide.current = defaultPinnedSide;
            pinWidget(uid, defaultPinnedSide, initialWidth, initialHeight);
        } else {
            registerFloating(uid);
            bringToFront(uid);
        }
        return () => { unregisterFloating(uid); unpinWidget(uid); };
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, []);

    // ── Unpin to floating ──────────────────────────────────────────────────────
    const doUnpin = useCallback((floatX: number, floatY: number) => {
        unpinWidget(uid);
        registerFloating(uid);
        bringToFront(uid);
        setPos({ x: Math.max(0, floatX), y: Math.max(0, floatY) });
        setSize({ w: saved.current.w, h: saved.current.h });
    }, [uid, unpinWidget, registerFloating, bringToFront]);

    // ── Pin-button toggle ──────────────────────────────────────────────────────
    const handlePinToggle = useCallback(() => {
        if (isPinned) {
            doUnpin(saved.current.x, saved.current.y);
        } else {
            saved.current = { x: pos.x, y: pos.y, w: size.w, h: size.h };
            unregisterFloating(uid);
            pinWidget(uid, lastSide.current, size.w, size.h);
        }
    }, [isPinned, uid, pos, size, pinWidget, unregisterFloating, doUnpin]);

    // ── Close ──────────────────────────────────────────────────────────────────
    const handleClose = useCallback(() => {
        if (isPinned) unpinWidget(uid); else unregisterFloating(uid);
        setVisible(false);
    }, [isPinned, uid, unpinWidget, unregisterFloating]);

    // ── Drag ───────────────────────────────────────────────────────────────────
    const dragState = useRef<{
        startMx: number; startMy: number;
        startWx: number; startWy: number;
        wasPinned: boolean;
        unpinned: boolean;
    } | null>(null);

    const isPinnedRef = useRef(isPinned);  isPinnedRef.current = isPinned;
    const posRef      = useRef(pos);       posRef.current      = pos;
    const sizeRef     = useRef(size);      sizeRef.current     = size;

    const handleGlobalMove = useCallback((mx: number, my: number) => {
        const ds = dragState.current;
        if (!ds) return;

        if (ds.wasPinned && !ds.unpinned) {
            // Defer unpin to first actual move — mirrors QML onDragStarted
            ds.unpinned = true;
            const floatX = mx - sizeRef.current.w / 2;
            const floatY = my - 13;
            ds.startWx = Math.max(0, floatX);
            ds.startWy = Math.max(0, floatY);
            ds.startMx = mx;
            ds.startMy = my;
            doUnpin(ds.startWx, ds.startWy);
            return;
        }

        setPos({
            x: ds.startWx + (mx - ds.startMx),
            y: ds.startWy + (my - ds.startMy),
        });
    }, [doUnpin]);

    const handleGlobalUp = useCallback(() => {
        dragState.current = null;
        notifyDragEnd(uid);
    }, [uid, notifyDragEnd]);

    useEffect(() => {
        registerDragHandlers(uid, handleGlobalMove, handleGlobalUp);
        return () => unregisterDragHandlers(uid);
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [uid, handleGlobalMove, handleGlobalUp]);

    const handleTitlePD = useCallback((e: React.PointerEvent<HTMLDivElement>) => {
        if ((e.target as HTMLElement).closest('button')) return;
        e.preventDefault();
        e.stopPropagation();

        if (!isPinnedRef.current) bringToFront(uid);

        dragState.current = {
            startMx: e.clientX, startMy: e.clientY,
            startWx: posRef.current.x, startWy: posRef.current.y,
            wasPinned: isPinnedRef.current,
            unpinned: false,
        };

        if (isPinnedRef.current) {
            saved.current = { ...saved.current, w: sizeRef.current.w, h: sizeRef.current.h };
        }

        notifyDragStart(uid);
    }, [uid, bringToFront, notifyDragStart]);

    // ── Resize ─────────────────────────────────────────────────────────────────
    const handleResize = useCallback((dir: ResizeDir, dx: number, dy: number) => {
        const cw = (v: number) => Math.max(minWidth,  v);
        const ch = (v: number) => Math.max(minHeight, v);
        setSize(prev => ({
            w: dir.includes('e') ? cw(prev.w + dx) : dir.includes('w') ? cw(prev.w - dx) : prev.w,
            h: dir.includes('s') ? ch(prev.h + dy) : dir.includes('n') ? ch(prev.h - dy) : prev.h,
        }));
        setPos(prev => ({
            x: dir.includes('w') ? prev.x + dx : prev.x,
            y: dir.includes('n') ? prev.y + dy : prev.y,
        }));
    }, [minWidth, minHeight]);

    if (!visible) return null;

    const titleBarJsx = (pinned: boolean) => (
        <div
            className={`${styles.titleBar} ${pinned ? styles.titleBarPinned : ''}`}
            onPointerDown={handleTitlePD}
        >
            <span className={styles.titleText}>{title}</span>
            <div className={styles.btnRow}>
                <button
                    className={styles.titleBtn}
                    onPointerDown={e => e.stopPropagation()}
                    onClick={handlePinToggle}
                    title={pinned ? 'Unpin' : 'Pin to side'}
                >
                    <IconPin active={pinned} />
                </button>
                <button
                    className={`${styles.titleBtn} ${styles.titleBtnClose}`}
                    onPointerDown={e => e.stopPropagation()}
                    onClick={handleClose}
                    title="Close"
                >
                    <IconClose />
                </button>
            </div>
        </div>
    );

    if (isPinned) {
        registerElement(uid, (
            <div className={styles.widgetPinned}>
                {titleBarJsx(true)}
                <div className={styles.content}>{children}</div>
            </div>
        ));
        return null;
    }

    return (
        <div
            className={styles.widget}
            style={{ left: pos.x, top: pos.y, width: size.w, height: size.h, zIndex }}
            onPointerDown={() => bringToFront(uid)}
        >
            {titleBarJsx(false)}
            <div className={styles.content}>{children}</div>
            {(['nw', 'ne', 'sw', 'se', 'n', 's', 'w', 'e'] as ResizeDir[]).map(d => (
                <ResizeHandle key={d} dir={d} onResize={handleResize} />
            ))}
        </div>
    );
}
