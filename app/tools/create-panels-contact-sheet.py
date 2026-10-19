"""Create a QA contact sheet from actual native Qt captures (no mock UI)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageOps

root = Path(__file__).resolve().parents[1]
captures = root / "qa" / "panels-parity" / "dpi-1"
font = ImageFont.truetype(str(root / "resources" / "fonts" / "Inter.ttf"), 17)
tiles = [
    ("Cài đặt", captures / "tab-8.png", (0, 0, 476, 489)),
    ("Xuất dự án", captures / "export-light.png", None),
    ("Menu thu phóng", captures / "previewZoomCombo-popup.png", None),
    ("Văn bản / Giọng đọc", captures / "text-tts.png", (47, 235, 476, 489)),
    ("Phụ đề / Ngôn ngữ", captures / "captionLanguage-context.png", (0, 0, 476, 489)),
    ("Timeline / Tooltip phía trên", root / "qa" / "editor-parity" / "tooltip-above.png", (0, 420, 520, 800)),
]
cell_w, cell_h, gap = 460, 500, 12
sheet = Image.new("RGB", (3 * cell_w + 4 * gap, 2 * cell_h + 3 * gap), "#eef0f2")
draw = ImageDraw.Draw(sheet)
for i, (title, path, crop) in enumerate(tiles):
    x = gap + (i % 3) * (cell_w + gap)
    y = gap + (i // 3) * (cell_h + gap)
    draw.rounded_rectangle((x, y, x + cell_w, y + cell_h), radius=8, fill="white")
    draw.text((x + 12, y + 10), title, font=font, fill="#212121")
    image = Image.open(path).convert("RGBA")
    if crop:
        image = image.crop(crop)
    image = ImageOps.contain(image, (cell_w - 16, cell_h - 48), Image.Resampling.LANCZOS)
    sheet.paste(image, (x + (cell_w - image.width) // 2, y + 40), image)
output = root / "qa" / "panels-parity" / "six-panels.png"
sheet.save(output)
print(output)
