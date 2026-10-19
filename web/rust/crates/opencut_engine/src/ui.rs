use crate::canvas::{ColorRgba, RectF};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum UiNodeType {
    Container = 0,
    Button = 1,
    Text = 2,
    Slider = 3,
    Icon = 4,
    Splitter = 5,
    Input = 6,
    Canvas = 7,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FlexDirection {
    Row = 0,
    Column = 1,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum AlignItems {
    Start = 0,
    Center = 1,
    End = 2,
    Stretch = 3,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum JustifyContent {
    Start = 0,
    Center = 1,
    End = 2,
    SpaceBetween = 3,
}

#[derive(Debug, Clone)]
pub struct UiStyle {
    pub background: ColorRgba,
    pub border_color: ColorRgba,
    pub border_width: f32,
    pub border_radius: f32,
    pub opacity: f32,
    pub padding: [f32; 4], // [top, right, bottom, left]
    pub margin: [f32; 4],
    pub flex_direction: FlexDirection,
    pub align_items: AlignItems,
    pub justify_content: JustifyContent,
    pub gap: f32,
    pub z_index: i32,
}

impl Default for UiStyle {
    fn default() -> Self {
        Self {
            background: ColorRgba { r: 0.0, g: 0.0, b: 0.0, a: 0.0 },
            border_color: ColorRgba { r: 0.0, g: 0.0, b: 0.0, a: 0.0 },
            border_width: 0.0,
            border_radius: 0.0,
            opacity: 1.0,
            padding: [0.0; 4],
            margin: [0.0; 4],
            flex_direction: FlexDirection::Row,
            align_items: AlignItems::Start,
            justify_content: JustifyContent::Start,
            gap: 0.0,
            z_index: 0,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct UiInteractionState {
    pub is_hovered: bool,
    pub is_pressed: bool,
    pub is_focused: bool,
    pub is_disabled: bool,
}

impl Default for UiInteractionState {
    fn default() -> Self {
        Self {
            is_hovered: false,
            is_pressed: false,
            is_focused: false,
            is_disabled: false,
        }
    }
}

#[derive(Debug, Clone)]
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

impl UiNode {
    pub fn new(id: u32, node_type: UiNodeType, rect: RectF) -> Self {
        Self {
            id,
            parent_id: None,
            node_type,
            rect,
            computed_rect: rect,
            style: UiStyle::default(),
            interaction: UiInteractionState::default(),
            visible: true,
            dirty: true,
            text_content: None,
            icon_name: None,
            children: Vec::new(),
        }
    }

    pub fn contains_point(&self, px: f32, py: f32) -> bool {
        if !self.visible {
            return false;
        }
        let r = &self.computed_rect;
        px >= r.x && px <= r.x + r.w && py >= r.y && py <= r.y + r.h
    }
}

#[derive(Debug, Clone)]
pub enum UiDrawCommand {
    DrawRect {
        rect: RectF,
        color: ColorRgba,
        radius: f32,
        border_color: ColorRgba,
        border_width: f32,
    },
    DrawText {
        text: String,
        x: f32,
        y: f32,
        font_size: f32,
        color: ColorRgba,
    },
    DrawLine {
        x1: f32,
        y1: f32,
        x2: f32,
        y2: f32,
        color: ColorRgba,
        width: f32,
    },
    DrawIcon {
        name: String,
        rect: RectF,
        color: ColorRgba,
    },
    PushClip {
        rect: RectF,
    },
    PopClip,
}

pub struct UiScene {
    nodes: Vec<UiNode>,
    next_id: u32,
    hovered_id: Option<u32>,
    focused_id: Option<u32>,
    root_id: Option<u32>,
}

impl Default for UiScene {
    fn default() -> Self {
        Self::new()
    }
}

impl UiScene {
    pub fn new() -> Self {
        Self {
            nodes: Vec::with_capacity(64),
            next_id: 1,
            hovered_id: None,
            focused_id: None,
            root_id: None,
        }
    }

    pub fn create_node(&mut self, node_type: UiNodeType, rect: RectF) -> u32 {
        let id = self.next_id;
        self.next_id += 1;
        let node = UiNode::new(id, node_type, rect);
        self.nodes.push(node);
        if self.root_id.is_none() {
            self.root_id = Some(id);
        }
        id
    }

    pub fn add_child(&mut self, parent_id: u32, child_id: u32) -> bool {
        let mut found_parent = false;
        for node in &mut self.nodes {
            if node.id == parent_id {
                if !node.children.contains(&child_id) {
                    node.children.push(child_id);
                    found_parent = true;
                }
                break;
            }
        }
        if found_parent {
            for node in &mut self.nodes {
                if node.id == child_id {
                    node.parent_id = Some(parent_id);
                    node.dirty = true;
                    return true;
                }
            }
        }
        false
    }

    pub fn get_node(&self, id: u32) -> Option<&UiNode> {
        self.nodes.iter().find(|n| n.id == id)
    }

    pub fn get_node_mut(&mut self, id: u32) -> Option<&mut UiNode> {
        self.nodes.iter_mut().find(|n| n.id == id)
    }

    pub fn hit_test(&self, px: f32, py: f32) -> Option<u32> {
        // Iterate backwards from top z-index and top children
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

    pub fn set_hover(&mut self, px: f32, py: f32) -> Option<u32> {
        let hit = self.hit_test(px, py);
        if hit != self.hovered_id {
            if let Some(old_id) = self.hovered_id {
                if let Some(node) = self.get_node_mut(old_id) {
                    node.interaction.is_hovered = false;
                    node.dirty = true;
                }
            }
            if let Some(new_id) = hit {
                if let Some(node) = self.get_node_mut(new_id) {
                    node.interaction.is_hovered = true;
                    node.dirty = true;
                }
            }
            self.hovered_id = hit;
        }
        hit
    }

    pub fn set_focus(&mut self, id: Option<u32>) {
        if id != self.focused_id {
            if let Some(old_id) = self.focused_id {
                if let Some(node) = self.get_node_mut(old_id) {
                    node.interaction.is_focused = false;
                    node.dirty = true;
                }
            }
            if let Some(new_id) = id {
                if let Some(node) = self.get_node_mut(new_id) {
                    node.interaction.is_focused = true;
                    node.dirty = true;
                }
            }
            self.focused_id = id;
        }
    }

    pub fn compute_layout(&mut self, root_w: f32, root_h: f32) {
        if let Some(root_id) = self.root_id {
            if let Some(root) = self.get_node_mut(root_id) {
                root.computed_rect.x = 0.0;
                root.computed_rect.y = 0.0;
                root.computed_rect.w = root_w;
                root.computed_rect.h = root_h;
            }
        }

        // Layout tree pass
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

    pub fn generate_draw_commands(&self) -> Vec<UiDrawCommand> {
        let mut commands = Vec::with_capacity(self.nodes.len() * 2);

        for node in &self.nodes {
            if !node.visible {
                continue;
            }

            // Draw node background / borders
            if node.style.background.a > 0.0 || node.style.border_width > 0.0 {
                let mut bg = node.style.background;
                if node.interaction.is_hovered {
                    bg.r = (bg.r * 1.15).min(1.0);
                    bg.g = (bg.g * 1.15).min(1.0);
                    bg.b = (bg.b * 1.15).min(1.0);
                }
                commands.push(UiDrawCommand::DrawRect {
                    rect: node.computed_rect,
                    color: bg,
                    radius: node.style.border_radius,
                    border_color: node.style.border_color,
                    border_width: node.style.border_width,
                });
            }

            // Draw text
            if let Some(ref text) = node.text_content {
                commands.push(UiDrawCommand::DrawText {
                    text: text.clone(),
                    x: node.computed_rect.x + node.style.padding[3],
                    y: node.computed_rect.y + node.style.padding[0] + 12.0,
                    font_size: 13.0,
                    color: ColorRgba { r: 1.0, g: 1.0, b: 1.0, a: 1.0 },
                });
            }

            // Draw icon
            if let Some(ref icon) = node.icon_name {
                commands.push(UiDrawCommand::DrawIcon {
                    name: icon.clone(),
                    rect: RectF {
                        x: node.computed_rect.x + node.style.padding[3],
                        y: node.computed_rect.y + node.style.padding[0],
                        w: 16.0,
                        h: 16.0,
                    },
                    color: ColorRgba { r: 0.9, g: 0.9, b: 0.9, a: 1.0 },
                });
            }
        }

        commands
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_ui_scene_hit_test() {
        let mut scene = UiScene::new();
        let root = scene.create_node(UiNodeType::Container, RectF { x: 0.0, y: 0.0, w: 800.0, h: 600.0 });
        let btn = scene.create_node(UiNodeType::Button, RectF { x: 10.0, y: 10.0, w: 100.0, h: 40.0 });
        scene.add_child(root, btn);
        scene.compute_layout(800.0, 600.0);

        let hit = scene.hit_test(20.0, 20.0);
        assert_eq!(hit, Some(btn));

        let miss = scene.hit_test(900.0, 900.0);
        assert_eq!(miss, None);
    }
}
