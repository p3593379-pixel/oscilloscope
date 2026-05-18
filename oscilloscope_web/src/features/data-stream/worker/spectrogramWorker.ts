/**
 * OffscreenCanvas worker — spectrogram with frequency on X, time on Y.
 * Each incoming frame paints one horizontal row.
 * Rows scroll upward: newest row at the bottom, oldest at the top.
 *
 * Internal storage: row-major ring buffer per channel.
 * Row r occupies bytes [r * W * 4 .. (r+1) * W * 4).
 */

let canvas:   OffscreenCanvas | null = null;
let ctx:      OffscreenCanvasRenderingContext2D | null = null;
let selectedChannel: number = 0;

// Row-major RGBA ring buffers, one per channel panel
let waterfalls: Uint8ClampedArray[] = [];
let channelCount = 1;
let writeRows: number[] = [];
let dbMin:          number    = -120;
let dbMax:          number    = 0;

// const TARGET_FPS     = 30;
// const FRAME_INTERVAL = 1000 / TARGET_FPS;
let lastFrameTime    = 0;
let rafHandle        = 0;
let dirty            = false;

// Display settings forwarded from SpectrogramCanvas
let workerFftSize:  number = 512;
let workerFps:      number = 30;


function colormapRgba(t: number): [number, number, number] {
    const stops: [number, [number, number, number]][] = [
        [0.00, [  0,   0,   0]],
        [0.15, [ 20,   0,  80]],
        [0.35, [120,   0, 100]],
        [0.55, [200,  40,   0]],
        [0.75, [240, 140,   0]],
        [0.90, [255, 220,  80]],
        [1.00, [255, 255, 255]],
    ];
    let lo = stops[0], hi = stops[stops.length - 1];
    for (let i = 0; i < stops.length - 1; i++) {
        if (t >= stops[i][0] && t <= stops[i + 1][0]) {
            lo = stops[i]; hi = stops[i + 1]; break;
        }
    }
    const f = (t - lo[0]) / (hi[0] - lo[0] || 1);
    return [
        Math.round(lo[1][0] + f * (hi[1][0] - lo[1][0])),
        Math.round(lo[1][1] + f * (hi[1][1] - lo[1][1])),
        Math.round(lo[1][2] + f * (hi[1][2] - lo[1][2])),
    ];
}

function paint(now: number) {
    rafHandle = requestAnimationFrame(paint);
    if (!dirty) return;
    if (now - lastFrameTime < 1000 / Math.max(1, workerFps)) return;
    lastFrameTime = now;
    dirty = false;

    if (!canvas || !ctx) return;
    const W = canvas.width;
    const H = canvas.height;
    if (W === 0 || H === 0) return;

    const ch = Math.min(selectedChannel, channelCount - 1);
    const wf = waterfalls[ch];
    // chH for storage is still floor(H/channelCount) — but we draw it stretched to H
    const chH = Math.max(1, Math.floor(H / channelCount));
    if (!wf || wf.length !== chH * W * 4) return;

    const img = ctx.createImageData(W, H);
    const dst = img.data;

    for (let srcRow = 0; srcRow < chH; srcRow++) {
        const age  = (writeRows[ch] - 1 - srcRow + chH) % chH;
        const dstY = chH - 1 - age;

        // Stretch: map chH rows → H rows
        const dstYStart = Math.round(dstY       / chH * H);
        const dstYEnd   = Math.round((dstY + 1) / chH * H);

        const srcOff = srcRow * W * 4;
        for (let y = dstYStart; y < dstYEnd; y++) {
            dst.set(wf.subarray(srcOff, srcOff + W * 4), y * W * 4);
        }
    }

    ctx.putImageData(img, 0, 0);
}

function initWaterfalls() {
    if (!canvas) return;
    const W   = canvas.width;
    const H   = canvas.height;
    if (W === 0 || H === 0) return;
    const chH = Math.max(1, Math.floor(H / channelCount));
    waterfalls = Array.from({ length: channelCount },
        () => new Uint8ClampedArray(chH * W * 4));
    writeRows = new Array(channelCount).fill(0);
}

self.onmessage = (e: MessageEvent) => {
    const msg = e.data as {
        type:             string;
        canvas?:          OffscreenCanvas;
        channelCount?:    number;
        selectedChannel?: number;      // ← add this
        channel?:         number;
        data?:            Float32Array;
        width?:           number;
        height?:          number;
        dbMin?:           number;
        dbMax?:           number;
        channelVisible?:  boolean[];
        spectrogramFftSize?: number;
        spectrogramFps?:     number;
    };

    switch (msg.type) {
        case 'init': {
            canvas          = msg.canvas!;
            ctx             = canvas.getContext('2d')!;
            channelCount    = msg.channelCount ?? 1;
            selectedChannel = msg.selectedChannel ?? 0;
            initWaterfalls();
            rafHandle = requestAnimationFrame(paint);
            break;
        }
        case 'selectChannel': {
            if (msg.channel !== undefined) {
                selectedChannel = Math.max(0, Math.min(msg.channel, channelCount - 1));
                dirty = true;
            }
            break;
        }

        case 'channelCount': {
            if (msg.channelCount !== undefined && msg.channelCount !== channelCount) {
                channelCount = msg.channelCount;
                // Remove: channelVisible = new Array(channelCount).fill(true);
                initWaterfalls();
            }
            break;
        }

        case 'spectrum': {
            const ch   = msg.channel ?? 0;
            const bins = msg.data!;
            if (!canvas || canvas.width === 0 || canvas.height === 0) break;

            const W   = canvas.width;
            const H   = canvas.height;
            const chH = Math.max(1, Math.floor(canvas.height / channelCount));
            const expectedLen = chH * W * 4;

            console.log(`[spectrum] ch=${ch} bins=${bins.length} W=${W} H=${H} chH=${chH} channelCount=${channelCount} writeRow=${writeRows[ch]} wfLen=${waterfalls[ch]?.length} expected=${expectedLen}`);

            if (!waterfalls[ch] || waterfalls[ch].length !== expectedLen) {
                initWaterfalls();
                if (!waterfalls[ch] || waterfalls[ch].length !== expectedLen) break;
            }

            const wf     = waterfalls[ch];
            const rowOff = writeRows[ch] * W * 4;

            for (let x = 0; x < W; x++) {
                const binIdx = Math.floor(x / W * bins.length);
                const t      = Math.max(0, Math.min(1,
                    (bins[binIdx] - dbMin) / (dbMax - dbMin)));
                const [r, g, b] = colormapRgba(t);
                const off = rowOff + x * 4;
                wf[off]     = r;
                wf[off + 1] = g;
                wf[off + 2] = b;
                wf[off + 3] = 255;
            }

            // Each channel advances its own row pointer independently
            writeRows[ch] = (writeRows[ch] + 1) % chH;
            dirty = true;
            break;
        }

        case 'resize': {
            if (canvas) {
                canvas.width  = msg.width!;
                canvas.height = msg.height!;
                initWaterfalls();
            }
            break;
        }

        // case 'settings': {
        //     if (msg.dbMin          !== undefined) dbMin          = msg.dbMin;
        //     if (msg.dbMax          !== undefined) dbMax          = msg.dbMax;
        //     if (msg.channelVisible !== undefined) channelVisible = msg.channelVisible;
        //     break;
        // }


        case 'displaySettings': {
            if (msg.spectrogramFftSize !== undefined) {
                workerFftSize = msg.spectrogramFftSize;
                // Reinitialise waterfalls when expected bin count changes
                if (canvas && canvas.width > 0) initWaterfalls();
            }
            if (msg.spectrogramFps !== undefined) {
                workerFps = msg.spectrogramFps;
                // Clamp the render throttle to the stream FPS so we don't
                // paint empty rows faster than data arrives
            }
            dirty = true;
            console.log(workerFftSize);
            break;
        }

        case 'stop': {
            cancelAnimationFrame(rafHandle);
            canvas = null;
            ctx    = null;
            break;
        }
    }
};
