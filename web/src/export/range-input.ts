export type ExportRangeInputError = "required" | "invalid" | "order" | "outside";

/** Browser form validation; the export planner validates the actual media times. */
export function validateExportRangeInput({
  start,
  end,
  duration,
}: {
  start: string;
  end: string;
  duration: number;
}): ExportRangeInputError | null {
  if (!start.trim() || !end.trim()) return "required";
  const startSeconds = Number(start);
  const endSeconds = Number(end);
  if (!Number.isFinite(startSeconds) || !Number.isFinite(endSeconds)) return "invalid";
  if (!Number.isFinite(duration) || startSeconds < 0 || endSeconds > duration) return "outside";
  if (startSeconds >= endSeconds) return "order";
  return null;
}
