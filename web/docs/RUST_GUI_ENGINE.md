# RUST CUSTOM GUI ENGINE ARCHITECTURE

## 1. Executive Summary

The Rust Custom GUI Engine (`opencut_engine::ui`) is a retained-mode user interface layout and scene graph system. It replaces DOM-based UI rendering with a high-performance scene graph managed in WebAssembly, emitting batched drawing commands directly to Canvas 2D or WebGPU render passes.

---

## 2. Retained-Mode Scene Graph (`src/ui.rs`)

### 2.1. Node Hierarchy & State
Every UI element (containers, toolbars, buttons, sliders, text labels, splitters) is represented as a `UiNode` with a stable integer identifier:

```rust
pub struct UiNode {
    pub id: u32,
    pub parent_id: Option<u32>,
    pub node_type: UiNodeType,
    pub rect: RectF,
    pub computed_rect: RectF,
    pub style: UiStyle,
    pub interaction: UiInteractionState,
    pub visible: bool,
    pub dirty: bool,
    pub text_content: Option<String>,
    pub icon_name: Option<String>,
    pub children: Vec<u32>,
}
```

### 2.2. Dirty Region & Partial Damage Tracking
- Instead of re-traversing the full scene graph or redrawing the entire screen on minor hover/focus changes:
  - When a node is hovered or pressed, only that node and its computed bounding box are flagged `dirty = true`.
  - The render scheduler uses the computed bounds to execute localized damage rect updates (`ctx.clearRect(damageRect)`).
  - During idle states, GPU and CPU utilization drop to **0%**.

---

## 3. Rust Flexbox Layout Computation

The layout engine computes geometric bounding boxes in a single deterministic top-down pass:

```rust
pub fn compute_layout(&mut self, root_w: f32, root_h: f32) {
    if let Some(root_id) = self.root_id {
        if let Some(root) = self.get_node_mut(root_id) {
            root.computed_rect = RectF { x: 0.0, y: 0.0, w: root_w, h: root_h };
        }
    }

    for i in 0..self.nodes.len() {
        let (parent_rect, children_ids, dir, gap, pad) = {
            let node = &self.nodes[i];
            (
                node.computed_rect,
                node.children.clone(),
                node.style.flex_direction,
                node.style.gap,
                node.style.padding,
            )
        };

        let mut offset_x = parent_rect.x + pad[3];
        let mut offset_y = parent_rect.y + pad[0];

        for child_id in children_ids {
            if let Some(child) = self.get_node_mut(child_id) {
                child.computed_rect.x = offset_x;
                child.computed_rect.y = offset_y;
                match dir {
                    FlexDirection::Row => {
                        offset_x += child.computed_rect.w + gap;
                    }
                    FlexDirection::Column => {
                        offset_y += child.computed_rect.h + gap;
                    }
                }
            }
        }
    }
}
```

---

## 4. Hit-Testing & Pointer Event Resolution

Hit testing traverses the node tree in reverse depth order (respecting `z_index` and child ordering):

```rust
pub fn hit_test(&self, px: f32, py: f32) -> Option<u32> {
    let mut hit = None;
    let mut best_z = i32::MIN;

    for node in self.nodes.iter().rev() {
        if node.contains_point(px, py) {
            if node.style.z_index > best_z {
                best_z = node.style.z_index;
                hit = Some(node.id);
            }
        }
    }
    hit
}
```

---

## 5. Typography & Vietnamese Accent Support

Text layout handles high-precision baseline calculation, character advance widths, and Vietnamese diacritics (Telex / VNI):
- Font loading preserves standard font weights: Inter 400 (Regular), Inter 500 (Medium), Inter 600 (SemiBold).
- Subpixel text rendering respects the `devicePixelRatio` using high-DPI scaling (`scale(dpr, dpr)`).
- Text truncation computes ellipsis (`...`) using font metrics without overflowing parent containers.

---

## 6. Vector Icon Rendering

Icons are rendered using precise GPU vector path rasters or unified sprite atlases:
- Preserves stroke width (1.5 px for Hugeicons standard outline).
- 16×16 px standard icon bounding box aligned to integer device coordinates to prevent anti-aliasing blur.
- Hover colors match CSS tokens (`hsl(var(--foreground))` and `hsl(var(--muted-foreground))`).
