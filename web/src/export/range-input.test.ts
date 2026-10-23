import { describe, expect, it } from "vitest";
import { validateExportRangeInput } from "./range-input";

describe("export range form validation", () => {
  it("accepts a fractional section and an out point at the timeline end", () => {
    expect(validateExportRangeInput({ start: "1.125", end: "3.5", duration: 3.5 })).toBeNull();
  });
  it.each([
    ["", "3", "required"], ["0", "  ", "required"], ["NaN", "3", "invalid"],
    ["0", "Infinity", "invalid"], ["3", "3", "order"], ["3", "2", "order"],
    ["-1", "2", "outside"], ["0", "3.001", "outside"],
  ] as const)("rejects %s → %s with %s", (start, end, expected) => {
    expect(validateExportRangeInput({ start, end, duration: 3 })).toBe(expected);
  });
});
