/** Keep only the newest pointer position until the next display frame. */
export class LatestFrameTask<T> {
  private pending: { value: T } | null = null;
  private frame: number | null = null;
  constructor(private readonly apply: (value: T) => void) {}
  schedule = (value: T) => {
    this.pending = { value };
    if (this.frame !== null) return;
    this.frame = requestAnimationFrame(() => { this.frame = null; this.flush(); });
  };
  flush = () => {
    if (this.frame !== null) cancelAnimationFrame(this.frame);
    this.frame = null;
    const pending = this.pending; this.pending = null;
    if (pending) this.apply(pending.value);
  };
  cancel = () => {
    if (this.frame !== null) cancelAnimationFrame(this.frame);
    this.frame = null; this.pending = null;
  };
}
