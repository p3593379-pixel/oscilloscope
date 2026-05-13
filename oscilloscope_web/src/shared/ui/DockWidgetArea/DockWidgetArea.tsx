import React, {
    useCallback, useRef, useState,
    type CSSProperties, type ReactElement, type ReactNode,
} from 'react';
import {
    DockAreaContext,
    type DockAreaContextValue,
    type DockSide,
} from '../DockWidget/DockWidgetContext';
import { insertWidget, removeWidget, type LayoutNode } from './layoutTree';
import styles from './DockWidgetArea.module.css';

const BASE_Z    = 10;
const DROP_PX   = 60;   // hit-test band width
const DROP_SIZE = 60;   // visual zone + shrink amount (px) — matches DROP_PX
const MIN_FLEX  = 0.05;

// ─── Splitter ─────────────────────────────────────────────────────────────────
function SplitterHandle({ axis, onDelta }: { axis: 'h' | 'v'; onDelta: (d: number) => void }) {
    const [active, setActive] = useState(false);
    const onPD = (e: React.PointerEvent) => {
        e.currentTarget.setPointerCapture(e.pointerId);
        e.preventDefault();
        setActive(true);
        let last = axis === 'h' ? e.clientX : e.clientY;
        const mv = (ev: PointerEvent) => { const c = axis === 'h' ? ev.clientX : ev.clientY; onDelta(c - last); last = c; };
        const up = () => { setActive(false); window.removeEventListener('pointermove', mv); window.removeEventListener('pointerup', up); };
        window.addEventListener('pointermove', mv);
        window.addEventListener('pointerup', up);
    };
    return (
        <div
            className={`${styles.splitter} ${axis === 'h' ? styles.splitterH : styles.splitterV} ${active ? styles.splitterActive : ''}`}
            onPointerDown={onPD}
        />
    );
}

// ─── Drop zone ────────────────────────────────────────────────────────────────
function DropZone({ side, visible, isOver }: { side: DockSide; visible: boolean; isOver: boolean }) {
    const cls = {
        left:   styles.dropZoneLeft,
        right:  styles.dropZoneRight,
        top:    styles.dropZoneTop,
        bottom: styles.dropZoneBottom,
    }[side];
    return (
        <div className={`${styles.dropZone} ${cls} ${visible ? styles.dropZoneVisible : ''} ${isOver ? styles.dropZoneOver : ''}`}>
            <div className={styles.dropZoneFill} />
        </div>
    );
}

// ─── Layout tree renderer ─────────────────────────────────────────────────────
function splitKey(node: LayoutNode): string {
    if (node.kind === 'widget') return `w:${node.id}`;
    return `s(${node.dir}):${node.children.map(splitKey).join('|')}`;
}

function LayoutRenderer({ node, widgetMap, flexShares, onFlexChange }: {
    node: LayoutNode;
    widgetMap: Map<string, ReactElement>;
    flexShares: React.MutableRefObject<Record<string, number[]>>;
    onFlexChange: (key: string, rightIdx: number, delta: number, containerPx: number) => void;
}) {
    const divRef = useRef<HTMLDivElement>(null);

    if (node.kind === 'widget') {
        return (
            <div className={styles.widgetSlot}>
                {widgetMap.get(node.id) ?? null}
            </div>
        );
    }

    const key = splitKey(node);
    if (!flexShares.current[key]) {
        flexShares.current[key] = node.children.map(() => 1);
    }
    const shares = flexShares.current[key];
    while (shares.length < node.children.length) shares.push(1);
    const total = shares.reduce((s, v) => s + v, 0);

    return (
        <div ref={divRef} className={node.dir === 'h' ? styles.splitH : styles.splitV}>
            {node.children.map((child, idx) => (
                <React.Fragment key={splitKey(child)}>
                    {idx > 0 && (
                        <SplitterHandle
                            axis={node.dir}
                            onDelta={delta => {
                                const px = divRef.current
                                    ? (node.dir === 'h' ? divRef.current.offsetWidth : divRef.current.offsetHeight)
                                    : 600;
                                onFlexChange(key, idx, delta, px);
                            }}
                        />
                    )}
                    <div className={styles.splitChild} style={{ flex: shares[idx] / total }}>
                        <LayoutRenderer node={child} widgetMap={widgetMap} flexShares={flexShares} onFlexChange={onFlexChange} />
                    </div>
                </React.Fragment>
            ))}
        </div>
    );
}

// ─── Compute pinnedRoot inset style ──────────────────────────────────────────
// Mirrors QML: anchors.leftMargin = containsDrag ? dropZoneSize : 0, with CSS transition.
// We shrink toward the hovered side only; all other sides stay at 0.
function pinnedRootStyle(isDragging: boolean, hoveredSide: DockSide | null): CSSProperties {
    const S = isDragging && hoveredSide ? DROP_SIZE : 0;
    return {
        top:    hoveredSide === 'top'    ? S : 0,
        left:   hoveredSide === 'left'   ? S : 0,
        right:  hoveredSide === 'right'  ? S : 0,
        bottom: hoveredSide === 'bottom' ? S : 0,
    };
}

// ─── DockWidgetArea ───────────────────────────────────────────────────────────
export function DockWidgetArea({ children }: { children?: ReactNode }) {

    // ── z-map (QML model) ──────────────────────────────────────────────────────
    const zMapRef = useRef<Record<string, number>>({});
    const [zMap, setZMap] = useState<Record<string, number>>({});

    const commitZMap = useCallback((next: Record<string, number>) => {
        zMapRef.current = next;
        setZMap({ ...next });
    }, []);

    const repackZIndices = useCallback((map: Record<string, number>): Record<string, number> => {
        const entries = Object.entries(map).sort((a, b) => a[1] - b[1]);
        const packed: Record<string, number> = {};
        entries.forEach(([id], i) => { packed[id] = BASE_Z + i; });
        return packed;
    }, []);

    const registerFloating = useCallback((id: string) => {
        const cur = zMapRef.current;
        if (id in cur) return;
        const maxZ = Object.values(cur).reduce((m, v) => Math.max(m, v), BASE_Z - 1);
        commitZMap({ ...cur, [id]: maxZ + 1 });
    }, [commitZMap]);

    const unregisterFloating = useCallback((id: string) => {
        const cur = zMapRef.current;
        if (!(id in cur)) return;
        const next = { ...cur };
        delete next[id];
        commitZMap(repackZIndices(next));
    }, [commitZMap, repackZIndices]);

    const bringToFront = useCallback((id: string) => {
        const cur = zMapRef.current;
        if (!(id in cur)) return;
        const currentZ = cur[id];
        const maxZ = BASE_Z + Object.keys(cur).length - 1;
        if (currentZ === maxZ) return;
        const next: Record<string, number> = {};
        for (const [k, z] of Object.entries(cur)) {
            if (k === id)      next[k] = maxZ;
            else if (z > currentZ) next[k] = z - 1;
            else               next[k] = z;
        }
        commitZMap(next);
    }, [commitZMap]);

    // ── layout tree ────────────────────────────────────────────────────────────
    const [layoutRoot, setLayoutRoot] = useState<LayoutNode | null>(null);
    const [pinnedIds,  setPinnedIds]  = useState<ReadonlySet<string>>(new Set());
    const flexShares = useRef<Record<string, number[]>>({});

    const collectIds = (node: LayoutNode | null): Set<string> => {
        if (!node) return new Set();
        if (node.kind === 'widget') return new Set([node.id]);
        return node.children.reduce((acc, c) => {
            collectIds(c).forEach(id => acc.add(id));
            return acc;
        }, new Set<string>());
    };

    const pinWidget = useCallback((id: string, side: DockSide, _w: number, _h: number) => {
        setLayoutRoot(prev => {
            const next = insertWidget(prev, id, side);
            setPinnedIds(collectIds(next));
            return next;
        });
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, []);

    const unpinWidget = useCallback((id: string) => {
        setLayoutRoot(prev => {
            const next = removeWidget(prev, id);
            setPinnedIds(collectIds(next));
            return next;
        });
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, []);

    const handleFlexChange = useCallback((key: string, rightIdx: number, deltaPx: number, containerPx: number) => {
        if (!flexShares.current[key] || containerPx === 0) return;
        const shares    = [...flexShares.current[key]];
        const total     = shares.reduce((s, v) => s + v, 0);
        const deltaFlex = (deltaPx / containerPx) * total;
        const li        = rightIdx - 1;
        const minShare  = MIN_FLEX * total;
        shares[li]       = Math.max(minShare, shares[li] + deltaFlex);
        shares[rightIdx] = Math.max(minShare, shares[rightIdx] - deltaFlex);
        flexShares.current[key] = shares;
        setLayoutRoot(r => r ? { ...r } : r);
    }, []);

    // ── Drag coordination ──────────────────────────────────────────────────────
    const areaRef         = useRef<HTMLDivElement>(null);
    const [draggingId,  setDraggingId]  = useState<string | null>(null);
    const [hoveredSide, setHoveredSide] = useState<DockSide | null>(null);
    const draggingIdRef = useRef<string | null>(null);
    draggingIdRef.current = draggingId;

    type DragHandlers = { onMove: (mx: number, my: number) => void; onUp: () => void };
    const dragHandlers = useRef<Map<string, DragHandlers>>(new Map());

    const registerDragHandlers = useCallback((id: string, onMove: (mx: number, my: number) => void, onUp: () => void) => {
        dragHandlers.current.set(id, { onMove, onUp });
    }, []);

    const unregisterDragHandlers = useCallback((id: string) => {
        dragHandlers.current.delete(id);
    }, []);

    const notifyDragStart = useCallback((id: string) => {
        setDraggingId(id);
    }, []);

    const lastPtrRef = useRef<{ x: number; y: number } | null>(null);

    const notifyDragEnd = useCallback((id: string) => {
        const last = lastPtrRef.current;
        if (last && areaRef.current) {
            const rect = areaRef.current.getBoundingClientRect();
            const lx = last.x - rect.left;
            const ly = last.y - rect.top;
            let side: DockSide | null = null;
            if      (lx < DROP_PX)               side = 'left';
            else if (lx > rect.width  - DROP_PX) side = 'right';
            else if (ly < DROP_PX)               side = 'top';
            else if (ly > rect.height - DROP_PX) side = 'bottom';
            if (side) {
                unregisterFloating(id);
                pinWidget(id, side, 300, 200);
            }
        }
        setDraggingId(null);
        setHoveredSide(null);
        lastPtrRef.current = null;
    }, [pinWidget, unregisterFloating]);

    const handleAreaPointerMove = useCallback((e: React.PointerEvent) => {
        const id = draggingIdRef.current;
        if (!id) return;
        lastPtrRef.current = { x: e.clientX, y: e.clientY };
        if (areaRef.current) {
            const rect = areaRef.current.getBoundingClientRect();
            const lx = e.clientX - rect.left;
            const ly = e.clientY - rect.top;
            if      (lx < DROP_PX)               setHoveredSide('left');
            else if (lx > rect.width  - DROP_PX) setHoveredSide('right');
            else if (ly < DROP_PX)               setHoveredSide('top');
            else if (ly > rect.height - DROP_PX) setHoveredSide('bottom');
            else                                 setHoveredSide(null);
        }
        dragHandlers.current.get(id)?.onMove(e.clientX, e.clientY);
    }, []);

    const handleAreaPointerUp = useCallback((_e: React.PointerEvent) => {
        const id = draggingIdRef.current;
        if (!id) return;
        dragHandlers.current.get(id)?.onUp();
    }, []);

    // ── Element registry ───────────────────────────────────────────────────────
    const elementMapRef = useRef<Map<string, ReactElement>>(new Map());

    const registerElement = useCallback((id: string, el: ReactElement) => {
        elementMapRef.current.set(id, el);
    }, []);

    const ctx: DockAreaContextValue = {
        registerFloating, unregisterFloating, bringToFront,
        pinWidget, unpinWidget,
        notifyDragStart, notifyDragEnd,
        registerDragHandlers, unregisterDragHandlers,
        pinnedIds, zMap,
        registerElement,
    };

    const isDragging = draggingId !== null;

    return (
        <DockAreaContext.Provider value={ctx}>
            <div
                ref={areaRef}
                className={styles.area}
                onPointerMove={handleAreaPointerMove}
                onPointerUp={handleAreaPointerUp}
            >
                {/* Floating widgets render as position:absolute children of .area */}
                {children}

                {/* Pinned layout — shrinks away from the hovered drop-zone edge */}
                {layoutRoot && (
                    <div
                        className={styles.pinnedRoot}
                        style={pinnedRootStyle(isDragging, hoveredSide)}
                    >
                        <LayoutRenderer
                            node={layoutRoot}
                            widgetMap={elementMapRef.current}
                            flexShares={flexShares}
                            onFlexChange={handleFlexChange}
                        />
                    </div>
                )}

                {/* Drop zones — roll in from each edge when any widget is being dragged */}
                {(['left', 'right', 'top', 'bottom'] as DockSide[]).map(s => (
                    <DropZone key={s} side={s} visible={isDragging} isOver={hoveredSide === s} />
                ))}
            </div>
        </DockAreaContext.Provider>
    );
}
