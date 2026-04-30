/**
 * OffscreenCanvas worker — receives decoded DataChunk samples from the main
 * thread via postMessage and paints the waveform directly onto an OffscreenCanvas.
 *
 * Message protocol (main → worker):
 *   { type: 'init',     canvas: OffscreenCanvas, channelCount: number }
 *   { type: 'samples',  channel: number, data: Float32Array }
 *   { type: 'resize',   width: number, height: number }
 *   { type: 'settings', xStart: number, xShow: number, yPeakToPeak: number,
 *                        verticalOffset: number[], channelVisible: boolean[] }
 *   { type: 'stop' }
 *
 *  xStart >= 0  → show samples starting xStart positions from the live end
 *  xStart = -1  → live (always show the latest xShow samples)
 */

import { RingBuffer } from '../../../shared/lib/ringBuffer';

const BUFFER_CAPACITY = 1 << 17; // 131 072 samples per channel

let canvas:   OffscreenCanvas | null = null;
let ctx:      OffscreenCanvasRenderingContext2D | null = null;
let channels: RingBuffer[] = [];
let rafHandle = 0;

let xStart:         number    = -1;
let xShow:          number    = 8192;
let yPeakToPeak:    number    = 8.0;
let verticalOffset: number[]  = [0.0, 0.0];
let channelVisible: boolean[] = [true, true];

const H_DIVS = 10;
const V_DIVS = 8;
const CHANNEL_COLORS = ['#87cefa', '#f08080', '#48dbfb', '#ff6b81'];

function paint() {
  if (!canvas || !ctx) return;
  const W = canvas.width;
  const H = canvas.height;

  ctx.fillStyle = '#0d1117';
  ctx.fillRect(0, 0, W, H);

  // Minor grid
  ctx.strokeStyle = 'rgba(255,255,255,0.07)';
  ctx.lineWidth = 1;
  for (let i = 0; i <= H_DIVS; i++) {
    const x = (i / H_DIVS) * W;
    ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, H); ctx.stroke();
  }
  for (let i = 0; i <= V_DIVS; i++) {
    const y = (i / V_DIVS) * H;
    ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke();
  }
  // Centre axes
  ctx.strokeStyle = 'rgba(255,255,255,0.16)';
  ctx.beginPath(); ctx.moveTo(W / 2, 0); ctx.lineTo(W / 2, H); ctx.stroke();
  ctx.beginPath(); ctx.moveTo(0, H / 2); ctx.lineTo(W, H / 2); ctx.stroke();

  const yScale = H / yPeakToPeak;

  for (let ch = 0; ch < channels.length; ch++) {
    if (channelVisible[ch] === false) continue;
    const buf = channels[ch];
    if (buf.length < 2) continue;

    const samples = xStart < 0
        ? buf.readLast(xShow)
        : buf.readFromEnd(xStart, xShow);

    if (samples.length < 2) continue;

    const voff = verticalOffset[ch] ?? 0.0;
    ctx.strokeStyle = CHANNEL_COLORS[ch % CHANNEL_COLORS.length];
    ctx.lineWidth   = 1.5;
    ctx.beginPath();

    for (let i = 0; i < samples.length; i++) {
      const x = (i / (samples.length - 1)) * W;
      const y = H / 2 - (samples[i] - voff) * yScale;
      i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
    }
    ctx.stroke();
  }

  rafHandle = requestAnimationFrame(paint);
}

self.onmessage = (e: MessageEvent) => {
  const msg = e.data as {
    type:            string;
    canvas?:         OffscreenCanvas;
    channelCount?:   number;
    channel?:        number;
    data?:           Float32Array;
    width?:          number;
    height?:         number;
    xStart?:         number;
    xShow?:          number;
    yPeakToPeak?:    number;
    verticalOffset?: number[];
    channelVisible?: boolean[];
  };

  switch (msg.type) {
    case 'init': {
      canvas   = msg.canvas!;
      ctx      = canvas.getContext('2d')!;
      channels = Array.from(
          { length: msg.channelCount ?? 2 },
          () => new RingBuffer(BUFFER_CAPACITY)
      );
      rafHandle = requestAnimationFrame(paint);
      break;
    }
    case 'samples': {
      const ch = msg.channel ?? 0;
      if (!channels[ch]) channels[ch] = new RingBuffer(BUFFER_CAPACITY);
      channels[ch].push(msg.data!);
      break;
    }
    case 'resize': {
      if (canvas) { canvas.width = msg.width!; canvas.height = msg.height!; }
      break;
    }
    case 'settings': {
      if (msg.xStart         !== undefined) xStart         = msg.xStart;
      if (msg.xShow          !== undefined) xShow          = msg.xShow;
      if (msg.yPeakToPeak    !== undefined) yPeakToPeak    = msg.yPeakToPeak;
      if (msg.verticalOffset !== undefined) verticalOffset = msg.verticalOffset;
      if (msg.channelVisible !== undefined) channelVisible = msg.channelVisible;
      break;
    }
    case 'stop': {
      cancelAnimationFrame(rafHandle);
      break;
    }
  }
};


