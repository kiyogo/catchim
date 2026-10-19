pub const COMPOSITOR_WGSL: &str = r#"
struct VertexInput {
    @location(0) position: vec2<f32>,
    @location(1) uv: vec2<f32>,
};

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

struct LayerUniforms {
    transform: mat3x3<f32>,
    opacity: f32,
    blend_mode: u32,
    flip_x: u32,
    flip_y: u32,
};

@group(0) @binding(0) var<uniform> layer: LayerUniforms;
@group(0) @binding(1) var layer_texture: texture_2d<f32>;
@group(0) @binding(2) var layer_sampler: sampler;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let transformed = layer.transform * vec3<f32>(in.position, 1.0);
    out.position = vec4<f32>(transformed.xy, 0.0, 1.0);
    
    var uv = in.uv;
    if (layer.flip_x == 1u) { uv.x = 1.0 - uv.x; }
    if (layer.flip_y == 1u) { uv.y = 1.0 - uv.y; }
    out.uv = uv;
    return out;
}

fn blend_channel(base: f32, blend: f32, mode: u32) -> f32 {
    switch (mode) {
        case 1u: { return base * blend; } // Multiply
        case 2u: { return 1.0 - (1.0 - base) * (1.0 - blend); } // Screen
        case 3u: { // Overlay
            if (base < 0.5) { return 2.0 * base * blend; }
            else { return 1.0 - 2.0 * (1.0 - base) * (1.0 - blend); }
        }
        case 4u: { return min(base, blend); } // Darken
        case 5u: { return max(base, blend); } // Lighten
        case 6u: { // ColorDodge
            if (blend >= 1.0) { return 1.0; }
            return min(1.0, base / (1.0 - blend));
        }
        case 7u: { // ColorBurn
            if (blend <= 0.0) { return 0.0; }
            return max(0.0, 1.0 - (1.0 - base) / blend);
        }
        case 8u: { // HardLight
            if (blend < 0.5) { return 2.0 * base * blend; }
            else { return 1.0 - 2.0 * (1.0 - base) * (1.0 - blend); }
        }
        case 9u: { // SoftLight
            if (blend < 0.5) {
                return base - (1.0 - 2.0 * blend) * base * (1.0 - base);
            } else {
                let d = select(sqrt(base), ((16.0 * base - 12.0) * base + 4.0) * base, base <= 0.25);
                return base + (2.0 * blend - 1.0) * (d - base);
            }
        }
        case 10u: { return abs(base - blend); } // Difference
        case 11u: { return base + blend - 2.0 * base * blend; } // Exclusion
        default: { return blend; } // Normal
    }
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    var tex_color = textureSample(layer_texture, layer_sampler, in.uv);
    tex_color.a = tex_color.a * layer.opacity;
    return tex_color;
}
"#;

pub const BLUR_COMPUTE_WGSL: &str = r#"
struct BlurParams {
    direction: vec2<f32>,
    sigma: f32,
    radius: u32,
    width: u32,
    height: u32,
};

@group(0) @binding(0) var<uniform> params: BlurParams;
@group(0) @binding(1) var input_tex: texture_2d<f32>;
@group(0) @binding(2) var output_tex: texture_storage_2d<rgba8unorm, write>;

@compute @workgroup_size(16, 16)
fn cs_blur(@builtin(global_invocation_id) id: vec3<u32>) {
    if (id.x >= params.width || id.y >= params.height) { return; }

    let r = i32(params.radius);
    let s = params.sigma;
    var weight_sum = 0.0;
    var color_sum = vec4<f32>(0.0);

    for (var i = -r; i <= r; i = i + 1) {
        let offset = vec2<f32>(f32(i) * params.direction.x, f32(i) * params.direction.y);
        let sample_pos = vec2<i32>(
            clamp(i32(id.x) + i32(offset.x), 0, i32(params.width) - 1),
            clamp(i32(id.y) + i32(offset.y), 0, i32(params.height) - 1)
        );
        let weight = exp(-0.5 * (f32(i) * f32(i)) / (s * s));
        let c = textureLoad(input_tex, sample_pos, 0);
        color_sum = color_sum + c * weight;
        weight_sum = weight_sum + weight;
    }

    textureStore(output_tex, vec2<i32>(id.xy), color_sum / weight_sum);
}
"#;

pub const COLOR_GRADE_COMPUTE_WGSL: &str = r#"
struct ColorParams {
    brightness: f32,
    contrast: f32,
    saturation: f32,
    exposure: f32,
    temperature: f32,
    tint: f32,
    hue: f32,
    gamma: f32,
    width: u32,
    height: u32,
};

@group(0) @binding(0) var<uniform> params: ColorParams;
@group(0) @binding(1) var input_tex: texture_2d<f32>;
@group(0) @binding(2) var output_tex: texture_storage_2d<rgba8unorm, write>;

@compute @workgroup_size(16, 16)
fn cs_color_grade(@builtin(global_invocation_id) id: vec3<u32>) {
    if (id.x >= params.width || id.y >= params.height) { return; }

    var c = textureLoad(input_tex, vec2<i32>(id.xy), 0);
    let exp_mult = pow(2.0, params.exposure);
    var r = (c.r * exp_mult) + params.brightness;
    var g = (c.g * exp_mult) + params.brightness;
    var b = (c.b * exp_mult) + params.brightness;

    // Contrast
    r = (r - 0.5) * (1.0 + params.contrast) + 0.5;
    g = (g - 0.5) * (1.0 + params.contrast) + 0.5;
    b = (b - 0.5) * (1.0 + params.contrast) + 0.5;

    // Temperature & Tint
    r = r * (1.0 + params.temperature * 0.1);
    g = g * (1.0 + params.tint * 0.1);
    b = b * (1.0 - params.temperature * 0.1);

    // Saturation
    let luma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    let sat_factor = 1.0 + params.saturation;
    r = luma + (r - luma) * sat_factor;
    g = luma + (g - luma) * sat_factor;
    b = luma + (b - luma) * sat_factor;

    // Gamma
    if (abs(params.gamma - 1.0) > 0.001 && params.gamma > 0.0) {
        let inv_gamma = 1.0 / params.gamma;
        r = pow(max(r, 0.0), inv_gamma);
        g = pow(max(g, 0.0), inv_gamma);
        b = pow(max(b, 0.0), inv_gamma);
    }

    textureStore(output_tex, vec2<i32>(id.xy), vec4<f32>(clamp(r, 0.0, 1.0), clamp(g, 0.0, 1.0), clamp(b, 0.0, 1.0), c.a));
}
"#;

pub const CHROMA_KEY_COMPUTE_WGSL: &str = r#"
struct ChromaParams {
    key_color: vec3<f32>,
    similarity: f32,
    smoothness: f32,
    spill: f32,
    width: u32,
    height: u32,
};

@group(0) @binding(0) var<uniform> params: ChromaParams;
@group(0) @binding(1) var input_tex: texture_2d<f32>;
@group(0) @binding(2) var output_tex: texture_storage_2d<rgba8unorm, write>;

@compute @workgroup_size(16, 16)
fn cs_chroma_key(@builtin(global_invocation_id) id: vec3<u32>) {
    if (id.x >= params.width || id.y >= params.height) { return; }

    var c = textureLoad(input_tex, vec2<i32>(id.xy), 0);
    let diff = c.rgb - params.key_color;
    let dist = length(diff);
    let sim = max(params.similarity, 0.01);
    let smooth = max(params.smoothness, 0.001);
    var alpha = clamp((dist - sim) / smooth, 0.0, 1.0);

    // Spill suppression
    var r = c.r;
    var g = c.g;
    var b = c.b;
    if (params.spill > 0.0 && alpha < 1.0) {
        let max_non_key = max(r, b);
        let excess = max(0.0, g - max_non_key);
        g = g - excess * params.spill * (1.0 - alpha);
    }

    textureStore(output_tex, vec2<i32>(id.xy), vec4<f32>(r, g, b, c.a * alpha));
}
"#;

pub const WAVEFORM_COMPUTE_WGSL: &str = r#"
struct WaveformParams {
    total_samples: u32,
    num_buckets: u32,
    samples_per_bucket: u32,
};

@group(0) @binding(0) var<uniform> params: WaveformParams;
@group(0) @binding(1) var<storage, read> samples: array<f32>;
@group(0) @binding(2) var<storage, read_write> peaks: array<f32>;

@compute @workgroup_size(256)
fn cs_waveform(@builtin(global_invocation_id) id: vec3<u32>) {
    let bucket = id.x;
    if (bucket >= params.num_buckets) { return; }

    let start = bucket * params.samples_per_bucket;
    let end = min(params.total_samples, start + params.samples_per_bucket);

    var max_val = 0.0f;
    for (var i = start; i < end; i = i + 1u) {
        let s = abs(samples[i]);
        if (s > max_val) {
            max_val = s;
        }
    }

    peaks[bucket] = max_val;
}
"#;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_shaders_non_empty() {
        assert!(!COMPOSITOR_WGSL.is_empty());
        assert!(!BLUR_COMPUTE_WGSL.is_empty());
        assert!(!COLOR_GRADE_COMPUTE_WGSL.is_empty());
        assert!(!CHROMA_KEY_COMPUTE_WGSL.is_empty());
        assert!(!WAVEFORM_COMPUTE_WGSL.is_empty());
    }
}
