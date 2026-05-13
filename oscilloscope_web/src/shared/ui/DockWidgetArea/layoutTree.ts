/**
 * Pure layout-tree logic — mirrors QML addToLayoutTree / rebuildLayoutTree.
 *
 * Tree nodes:
 *   { kind:'widget', id }                           — leaf: one pinned widget
 *   { kind:'split',  dir:'h'|'v', children: Node[] } — split view, 2+ children
 *
 * Insertion rules (matching QML exactly):
 *   1. Empty tree           → widget becomes the root (full area, no splitter at all)
 *   2. Root is a widget     → wrap both in a new split of the incoming direction
 *   3. Root is a split
 *      a. Same direction    → prepend (left/top) or append (right/bottom)
 *      b. Diff direction    → wrap entire root in a new outer split of incoming direction
 *
 * 'left' / 'top'  → addToStart (prepend)
 * 'right'/ 'bottom' → addToEnd (append)
 *
 * Removal: delete the widget leaf, then collapse any split with exactly one
 * remaining child back into that child (bottom-up, matching QML rebuildLayoutTree).
 */

export type DockSide = 'left' | 'right' | 'top' | 'bottom';

export type WidgetNode = { kind: 'widget'; id: string };
export type SplitNode  = { kind: 'split';  dir: 'h' | 'v'; children: LayoutNode[] };
export type LayoutNode = WidgetNode | SplitNode;

export function sideDir(side: DockSide): 'h' | 'v' {
    return side === 'left' || side === 'right' ? 'h' : 'v';
}
export function sideIsStart(side: DockSide): boolean {
    return side === 'left' || side === 'top';
}

// ── Insert ────────────────────────────────────────────────────────────────────
export function insertWidget(
    root: LayoutNode | null,
    id: string,
    side: DockSide,
): LayoutNode {
    const node: WidgetNode = { kind: 'widget', id };
    const dir     = sideDir(side);
    const toStart = sideIsStart(side);

    // Case 1: empty tree
    if (!root) return node;

    // Case 2: root is a lone widget
    if (root.kind === 'widget') {
        return toStart
            ? { kind: 'split', dir, children: [node, root] }
            : { kind: 'split', dir, children: [root, node] };
    }

    // Case 3a: root split, same direction → insert at start or end
    if (root.dir === dir) {
        const children = toStart
            ? [node, ...root.children]
            : [...root.children, node];
        return { ...root, children };
    }

    // Case 3b: root split, different direction → wrap root in new outer split
    return toStart
        ? { kind: 'split', dir, children: [node, root] }
        : { kind: 'split', dir, children: [root, node] };
}

// ── Remove + simplify ─────────────────────────────────────────────────────────
export function removeWidget(
    root: LayoutNode | null,
    id: string,
): LayoutNode | null {
    if (!root) return null;
    return removeAndSimplify(root, id);
}

function removeAndSimplify(node: LayoutNode, id: string): LayoutNode | null {
    if (node.kind === 'widget') {
        return node.id === id ? null : node;
    }
    const children = node.children
        .map(c => removeAndSimplify(c, id))
        .filter((c): c is LayoutNode => c !== null);

    if (children.length === 0) return null;
    if (children.length === 1) return children[0]; // collapse trivial split
    return { ...node, children };
}
