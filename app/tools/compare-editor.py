"""Compare same-size editor captures; a low pixel error is not a parity claim."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageChops, ImageStat, ImageDraw

parser = argparse.ArgumentParser()
parser.add_argument("reference", type=Path)
parser.add_argument("candidate", type=Path)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
reference = Image.open(args.reference).convert("RGB")
candidate = Image.open(args.candidate).convert("RGB")
if reference.size != candidate.size:
    raise SystemExit(f"Capture sizes differ: {reference.size} != {candidate.size}")
args.output.mkdir(parents=True, exist_ok=True)
diff = ImageChops.difference(reference, candidate)
diff.point(lambda p: min(255, p * 4)).save(args.output / "difference-x4.png")
Image.blend(reference, candidate, .5).save(args.output / "overlay.png")

def canvas_bounds(image):
    # The blank project's canvas is the only large solid-black rectangle.
    rows = [(y, sum(image.getpixel((x, y)) == (0, 0, 0) for x in range(image.width)))
            for y in range(50, min(490, image.height))]
    y = max(rows, key=lambda row: row[1])[0]
    xs = [x for x in range(image.width) if image.getpixel((x, y)) == (0, 0, 0)]
    x = xs[len(xs) // 2]
    ys = [y for y in range(50, min(490, image.height)) if image.getpixel((x, y)) == (0, 0, 0)]
    return [min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1]

report = {
    "reference": str(args.reference), "candidate": str(args.candidate),
    "size": reference.size,
    "mean_absolute_channel_error_0_255": ImageStat.Stat(diff).mean,
    "reference_canvas": canvas_bounds(reference),
    "candidate_canvas": canvas_bounds(candidate),
    "limits": "Includes font rasterization, tooltip and dev badge differences. Does not test behavior or other screens.",
}
(args.output / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(report, ensure_ascii=False, indent=2))
