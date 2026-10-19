# PIXEL-PERFECT BASELINE SPECIFICATION

## 1. Baseline Environment Configuration

To guarantee 100% visual and geometric parity down to the individual pixel between the legacy DOM editor and the Rust-first Canvas/WebGPU engine, the following baseline parameters are strictly locked:

- **Browser Engines Tested**: Chromium 130+ (Google Chrome, Microsoft Edge, Brave) with WebGPU enabled.
- **Operating Systems**: Windows 11 (build 22631+), macOS 14+ (Metal backend), Linux X11/Wayland (Vulkan).
- **Target Viewports**:
  - Full HD: `1920 × 1080` (devicePixelRatio: 1.0)
  - QHD: `2560 × 1440` (devicePixelRatio: 1.0 & 1.25)
  - 4K / High-DPI: `3840 × 2160` (devicePixelRatio: 1.5 & 2.0)
  - Laptop / Compact: `1366 × 768` and `1280 × 720`
- **Color Space**: Rec. 709 / sRGB (linear gamma compositing with sRGB output transfer function).
- **Font Stack**: Inter, -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", sans-serif. Monospace font: ui-monospace, SFMono-Regular, "Roboto Mono", Consolas.

---

## 2. Geometric Tolerance Thresholds

| UI Region | Position Tolerance | Sizing Tolerance | Color Vector Delta (ΔE*ab) | Anti-Aliasing Allowance |
|---|---|---|---|---|
| **Header & Nav Bars** | 0.0 px (Strict) | 0.0 px | < 0.5 (Imperceptible) | Subpixel text rendering identical |
| **Dock Panels (Left/Right)** | 0.0 px | 0.0 px | < 0.5 | Border alignment exact |
| **Preview Container** | 0.0 px | 0.0 px | 0.0 (Bit-identical video output) | Nearest/Bilinear texture filter |
| **Timeline Viewport** | 0.0 px | 0.0 px | < 0.5 | Track boundaries exact |
| **Timeline Clips** | 0.0 px | 0.0 px | < 0.8 (Alpha blend parity) | Corner radius: 6.0 px |
| **Playhead Needle & Line** | 0.0 px | 0.0 px | 0.0 (`#ef4444`) | Centered 1.0 px vertical line |
| **Ruler Ticks & Labels** | 0.0 px | 0.0 px | < 0.5 | Major ticks 14px, Minor ticks 7px |

---

## 3. Pixel Difference & Diff Heatmap Protocol

The verification suite executes image comparison on headless Chromium instances capturing:

1. **Baseline Screenshot** ($I_{\text{baseline}}$): Rendered by the original CSS/DOM layout.
2. **Engine Screenshot** ($I_{\text{engine}}$): Rendered by the Rust scene graph / Canvas / WebGPU compositor.
3. **Difference Heatmap** ($D(x, y)$):
   $$D(x, y) = \sqrt{\Delta R^2 + \Delta G^2 + \Delta B^2 + \Delta A^2}$$
4. **Mismatch Percentage** ($M$):
   $$M = \frac{1}{W \cdot H} \sum_{x,y} \mathbb{I}[D(x, y) > \epsilon_{\text{threshold}}]$$
   where $\epsilon_{\text{threshold}} = 0.01$ (allowing for hardware rasterization differences in font hinting).

### Acceptance Criteria:
- **Layout and Boundary Regions**: $M = 0.00\%$. Any element displacement $> 0.5\text{px}$ is classified as a critical defect.
- **Text Rasterization**: $M < 0.05\%$ localized strictly to glyph edges due to GPU texture atlas anti-aliasing versus DirectWrite/CoreText hinting.
- **Overlay Scrollbars**: Zero layout shift ($0.0\text{px}$ displacement of content when scrollbars appear/fade).

---

## 4. Multi-DPI & Scale Parity Validation

For devicePixelRatio $\in \{1.0, 1.25, 1.5, 2.0\}$:
- Canvas backing store dimensions are dynamically multiplied by `dpr`:
  $$\text{canvas.width} = \lfloor \text{cssWidth} \cdot \text{dpr} \rfloor, \quad \text{canvas.height} = \lfloor \text{cssHeight} \cdot \text{dpr} \rfloor$$
- Context scale transformation applied at render loop start:
  $$\text{ctx.setTransform}(\text{dpr}, 0, 0, \text{dpr}, 0, 0)$$
- Eliminates any blurriness or fuzzy borders on Retina/High-DPI displays.
