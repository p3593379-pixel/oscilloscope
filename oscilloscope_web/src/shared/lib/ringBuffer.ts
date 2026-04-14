/**
 * Fixed-capacity circular buffer for waveform samples.
 * Written to from the OffscreenCanvas worker; read from the render loop.
 */
export class RingBuffer {
  private buf: Float32Array;
  private writeIdx = 0;
  public length = 0;

  constructor(public readonly capacity: number) {
    this.buf = new Float32Array(capacity);
  }

  push(samples: Float32Array): void {
    for (let i = 0; i < samples.length; i++) {
      this.buf[this.writeIdx] = samples[i];
      this.writeIdx = (this.writeIdx + 1) % this.capacity;
    }
    this.length = Math.min(this.capacity, this.length + samples.length);
  }

  /** Read the most recent `count` samples in chronological order. */
  readLast(count: number): Float32Array {
    const n = Math.min(count, this.length);
    const out = new Float32Array(n);
    const start = (this.writeIdx - n + this.capacity) % this.capacity;
    for (let i = 0; i < n; i++) {
      out[i] = this.buf[(start + i) % this.capacity];
    }
    return out;
  }

  clear(): void {
    this.writeIdx = 0;
    this.length = 0;
  }
}
