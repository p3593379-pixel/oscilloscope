import { createContext, useContext, type ReactElement } from 'react';

export type DockSide = 'left' | 'right' | 'top' | 'bottom';

export interface DockAreaContextValue {
    registerFloating:       (id: string) => void;
    unregisterFloating:     (id: string) => void;
    bringToFront:           (id: string) => void;
    pinWidget:              (id: string, side: DockSide, savedW: number, savedH: number) => void;
    unpinWidget:            (id: string) => void;
    notifyDragStart:        (id: string) => void;
    notifyDragEnd:          (id: string) => void;
    registerDragHandlers:   (id: string, onMove: (mx: number, my: number) => void, onUp: () => void) => void;
    unregisterDragHandlers: (id: string) => void;
    pinnedIds:              ReadonlySet<string>;
    zMap:                   Record<string, number>;
    registerElement:        (id: string, el: ReactElement) => void;
}

export const DockAreaContext = createContext<DockAreaContextValue | null>(null);

export function useDockArea(): DockAreaContextValue {
    const ctx = useContext(DockAreaContext);
    if (!ctx) throw new Error('useDockArea must be used inside <DockWidgetArea>');
    return ctx;
}
