use std::ffi::CStr;
use std::os::raw::c_char;
use std::slice;
use crate::time::{
    format_timecode, floor_to_frame, last_frame_time, parse_timecode, round_to_frame,
    snapped_seek_time, FrameRate, TimecodeFormat, TICKS_PER_SECOND,
};
use crate::animation::{evaluate_bezier_1d, solve_bezier_t_for_x, ScalarChannel, ScalarKeyframe, SegmentType, ExtrapolationMode};
use crate::timeline::{
    snap_points_linear as rust_snap_points_linear,
    snap_points_sorted as rust_snap_points_sorted,
    Clip, Timeline, TrackType,
};
use crate::audio::{
    apply_gain_ramp, clamp_buffer_samples, compute_buffer_peak, compute_peak_buckets,
    compute_rms_buckets, detect_audio_beats, downmix_stereo, mix_buffers, mix_channel_retime,
    resample_linear,
};
use crate::effects::{
    apply_chroma_key_rgba, apply_color_grading_rgba, apply_gaussian_blur_rgba, apply_vignette_rgba,
};
use crate::masks::{apply_mask_to_buffer, evaluate_mask_alpha, MaskType};
use crate::compositor::{clear_buffer_rgba, composite_layer_rgba, BlendMode, QuadTransform};

use std::alloc::{alloc, dealloc, Layout};

#[no_mangle]
pub extern "C" fn oc_malloc(size: usize) -> *mut u8 {
    let layout = match Layout::from_size_align(size + 8, 8) {
        Ok(l) => l,
        Err(_) => return std::ptr::null_mut(),
    };
    unsafe {
        let ptr = alloc(layout);
        if ptr.is_null() {
            return std::ptr::null_mut();
        }
        *(ptr as *mut usize) = size;
        ptr.add(8)
    }
}

#[no_mangle]
pub extern "C" fn oc_free(ptr: *mut u8) {
    if ptr.is_null() {
        return;
    }
    unsafe {
        let actual_ptr = ptr.sub(8);
        let size = *(actual_ptr as *const usize);
        if let Ok(layout) = Layout::from_size_align(size + 8, 8) {
            dealloc(actual_ptr, layout);
        }
    }
}

unsafe fn c_str_to_str<'a>(ptr: *const c_char) -> &'a str {
    if ptr.is_null() {
        ""
    } else {
        CStr::from_ptr(ptr).to_str().unwrap_or("")
    }
}

// Time APIs
#[no_mangle]
pub extern "C" fn oc_time_ticks_per_second() -> f64 {
    TICKS_PER_SECOND as f64
}

#[no_mangle]
pub extern "C" fn ocw_time_from_seconds(seconds: f64) -> f64 {
    (seconds * TICKS_PER_SECOND as f64).round()
}

#[no_mangle]
pub extern "C" fn ocw_time_to_seconds(ticks: f64) -> f64 {
    ticks / TICKS_PER_SECOND as f64
}

#[no_mangle]
pub extern "C" fn ocw_time_round_to_frame(ticks: f64, num: u32, den: u32) -> f64 {
    let rate = FrameRate { numerator: num, denominator: den };
    round_to_frame(ticks as i64, rate).unwrap_or(ticks as i64) as f64
}

#[no_mangle]
pub extern "C" fn ocw_time_floor_to_frame(ticks: f64, num: u32, den: u32) -> f64 {
    let rate = FrameRate { numerator: num, denominator: den };
    floor_to_frame(ticks as i64, rate).unwrap_or(ticks as i64) as f64
}

#[no_mangle]
pub extern "C" fn ocw_time_last_frame(duration: f64, num: u32, den: u32) -> f64 {
    let rate = FrameRate { numerator: num, denominator: den };
    last_frame_time(duration as i64, rate).unwrap_or(0) as f64
}

#[no_mangle]
pub extern "C" fn ocw_time_snapped_seek(time: f64, duration: f64, num: u32, den: u32) -> f64 {
    let rate = FrameRate { numerator: num, denominator: den };
    snapped_seek_time(time as i64, duration as i64, rate).unwrap_or(time as i64) as f64
}

#[no_mangle]
pub extern "C" fn ocw_time_format_timecode(
    ticks: f64,
    format_idx: u32,
    num: u32,
    den: u32,
    out_buf: *mut c_char,
    out_len: usize,
) -> i32 {
    if out_buf.is_null() || out_len == 0 {
        return 0;
    }
    let fmt = TimecodeFormat::from_u32(format_idx);
    let rate = FrameRate { numerator: num, denominator: den };
    let s = format_timecode(ticks as i64, fmt, rate);
    let bytes = s.as_bytes();
    let copy_len = bytes.len().min(out_len - 1);
    unsafe {
        let dest = slice::from_raw_parts_mut(out_buf as *mut u8, out_len);
        dest[..copy_len].copy_from_slice(&bytes[..copy_len]);
        dest[copy_len] = 0;
    }
    1
}

#[no_mangle]
pub extern "C" fn ocw_time_parse_timecode(
    s_ptr: *const c_char,
    format_idx: u32,
    num: u32,
    den: u32,
) -> f64 {
    let s = unsafe { c_str_to_str(s_ptr) };
    let fmt = TimecodeFormat::from_u32(format_idx);
    let rate = FrameRate { numerator: num, denominator: den };
    parse_timecode(s, fmt, rate).map(|t| t as f64).unwrap_or(-1.0)
}

// Timeline APIs
#[no_mangle]
pub extern "C" fn oc_timeline_create() -> *mut Timeline {
    let timeline = Box::new(Timeline::new());
    Box::into_raw(timeline)
}

#[no_mangle]
pub extern "C" fn oc_timeline_destroy(ptr: *mut Timeline) {
    if !ptr.is_null() {
        unsafe {
            let _ = Box::from_raw(ptr);
        }
    }
}

#[no_mangle]
pub extern "C" fn oc_timeline_add_track(timeline: *mut Timeline, track_id_ptr: *const c_char) -> i32 {
    if timeline.is_null() {
        return -1;
    }
    let tl = unsafe { &mut *timeline };
    let track_id = unsafe { c_str_to_str(track_id_ptr) };
    tl.add_track(track_id.to_string(), TrackType::Main);
    0
}

#[no_mangle]
pub extern "C" fn ocw_timeline_insert_clip(
    timeline: *mut Timeline,
    track_id_ptr: *const c_char,
    clip_id_ptr: *const c_char,
    start: f64,
    duration: f64,
) -> i32 {
    if timeline.is_null() {
        return -1;
    }
    let tl = unsafe { &mut *timeline };
    let track_id = unsafe { c_str_to_str(track_id_ptr) };
    let clip_id = unsafe { c_str_to_str(clip_id_ptr) };
    let ok = tl.insert_clip(
        track_id,
        Clip {
            id: clip_id.to_string(),
            start_time: start as i64,
            duration: duration as i64,
            trim_start: 0,
            trim_end: 0,
        },
    );
    if ok { 0 } else { -1 }
}

#[no_mangle]
pub extern "C" fn ocw_timeline_can_place(
    timeline: *mut Timeline,
    track_id_ptr: *const c_char,
    start: f64,
    duration: f64,
    exclude_id_ptr: *const c_char,
) -> i32 {
    if timeline.is_null() {
        return 0;
    }
    let tl = unsafe { &*timeline };
    let track_id = unsafe { c_str_to_str(track_id_ptr) };
    let exclude_id = unsafe { c_str_to_str(exclude_id_ptr) };
    if tl.can_place(track_id, start as i64, duration as i64, exclude_id) { 1 } else { 0 }
}

#[no_mangle]
pub extern "C" fn ocw_timeline_find_available_gap(
    timeline: *mut Timeline,
    track_id_ptr: *const c_char,
    duration: f64,
    min_start: f64,
) -> f64 {
    if timeline.is_null() {
        return min_start;
    }
    let tl = unsafe { &*timeline };
    let track_id = unsafe { c_str_to_str(track_id_ptr) };
    if let Some(track) = tl.tracks.get(track_id) {
        track.find_gap(duration as i64, min_start as i64) as f64
    } else {
        min_start
    }
}

#[no_mangle]
pub extern "C" fn ocw_timeline_apply_ripple(
    timeline: *mut Timeline,
    track_id_ptr: *const c_char,
    after_time: f64,
    delta_ticks: f64,
) -> i32 {
    if timeline.is_null() {
        return -1;
    }
    let tl = unsafe { &mut *timeline };
    let track_id = unsafe { c_str_to_str(track_id_ptr) };
    if let Some(track) = tl.tracks.get_mut(track_id) {
        track.apply_ripple(after_time as i64, delta_ticks as i64);
        0
    } else {
        -1
    }
}

// Snapping APIs
#[no_mangle]
pub extern "C" fn ocw_snap_points_sorted(
    target: f64,
    points_ptr: *const f64,
    count: usize,
    threshold: f64,
    out_snapped: *mut f64,
    out_matched_index: *mut i32,
) -> i32 {
    if points_ptr.is_null() || count == 0 {
        return 0;
    }
    let points = unsafe { slice::from_raw_parts(points_ptr, count) };
    let points_i64: Vec<i64> = points.iter().map(|&p| p as i64).collect();
    let res = rust_snap_points_sorted(target as i64, &points_i64, threshold);

    if !out_snapped.is_null() {
        unsafe { *out_snapped = res.snapped_time as f64; }
    }
    if !out_matched_index.is_null() {
        unsafe { *out_matched_index = res.matched_index; }
    }
    if res.matched_index >= 0 { 1 } else { 0 }
}

#[no_mangle]
pub extern "C" fn ocw_snap_points_linear(
    target: f64,
    points_ptr: *const f64,
    count: usize,
    threshold: f64,
    out_snapped: *mut f64,
    out_matched_index: *mut i32,
) -> i32 {
    if points_ptr.is_null() || count == 0 {
        return 0;
    }
    let points = unsafe { slice::from_raw_parts(points_ptr, count) };
    let points_i64: Vec<i64> = points.iter().map(|&p| p as i64).collect();
    let res = rust_snap_points_linear(target as i64, &points_i64, threshold as i64);

    if !out_snapped.is_null() {
        unsafe { *out_snapped = res.snapped_time as f64; }
    }
    if !out_matched_index.is_null() {
        unsafe { *out_matched_index = res.matched_index; }
    }
    if res.matched_index >= 0 { 1 } else { 0 }
}

// Animation APIs
#[no_mangle]
pub extern "C" fn ocw_animation_solve_bezier(time: f64, t0: f64, t1: f64, t2: f64, t3: f64) -> f64 {
    solve_bezier_t_for_x(time, t0, t1, t2, t3)
}

#[no_mangle]
pub extern "C" fn ocw_animation_evaluate_bezier_point(progress: f64, p0: f64, p1: f64, p2: f64, p3: f64) -> f64 {
    evaluate_bezier_1d(progress, p0, p1, p2, p3)
}

#[no_mangle]
pub extern "C" fn ocw_animation_evaluate_channel(
    key_times: *const f64,
    key_values: *const f64,
    key_interp: *const u32,
    num_keys: usize,
    eval_time: f64,
) -> f64 {
    if key_times.is_null() || key_values.is_null() || num_keys == 0 {
        return 0.0;
    }
    let times = unsafe { slice::from_raw_parts(key_times, num_keys) };
    let values = unsafe { slice::from_raw_parts(key_values, num_keys) };
    let interps = if key_interp.is_null() {
        None
    } else {
        Some(unsafe { slice::from_raw_parts(key_interp, num_keys) })
    };

    let mut keys = Vec::with_capacity(num_keys);
    for i in 0..num_keys {
        let seg = match interps.and_then(|it| it.get(i).copied()) {
            Some(1) => SegmentType::Step,
            Some(2) => SegmentType::Bezier,
            _ => SegmentType::Linear,
        };
        keys.push(ScalarKeyframe {
            time: times[i],
            value: values[i],
            segment_to_next: seg,
            left_handle: None,
            right_handle: None,
        });
    }

    let channel = ScalarChannel {
        keys,
        extrapolation_before: ExtrapolationMode::Hold,
        extrapolation_after: ExtrapolationMode::Hold,
    };
    channel.evaluate(eval_time, 0.0)
}

// Audio APIs
#[no_mangle]
pub extern "C" fn ocw_audio_apply_gain_ramp(samples: *mut f32, count: usize, start_gain: f32, end_gain: f32) {
    if samples.is_null() || count == 0 { return; }
    let s = unsafe { slice::from_raw_parts_mut(samples, count) };
    apply_gain_ramp(s, start_gain, end_gain);
}

#[no_mangle]
pub extern "C" fn ocw_audio_mix_buffers(dest: *mut f32, src: *const f32, count: usize, volume: f32) {
    if dest.is_null() || src.is_null() || count == 0 { return; }
    let d = unsafe { slice::from_raw_parts_mut(dest, count) };
    let s = unsafe { slice::from_raw_parts(src, count) };
    mix_buffers(d, s, volume);
}

#[no_mangle]
pub extern "C" fn ocw_audio_downmix_stereo(left: *const f32, right: *const f32, out: *mut f32, count: usize) {
    if left.is_null() || right.is_null() || out.is_null() || count == 0 { return; }
    let l = unsafe { slice::from_raw_parts(left, count) };
    let r = unsafe { slice::from_raw_parts(right, count) };
    let o = unsafe { slice::from_raw_parts_mut(out, count) };
    downmix_stereo(l, r, o);
}

#[no_mangle]
pub extern "C" fn ocw_audio_resample_linear(src: *const f32, src_len: usize, dst: *mut f32, dst_len: usize) {
    if src.is_null() || dst.is_null() || src_len == 0 || dst_len == 0 { return; }
    let s = unsafe { slice::from_raw_parts(src, src_len) };
    let d = unsafe { slice::from_raw_parts_mut(dst, dst_len) };
    resample_linear(s, d);
}

#[no_mangle]
pub extern "C" fn ocw_audio_compute_peak_buckets(
    samples_ptr: *const f32,
    starts_ptr: *const u32,
    ends_ptr: *const u32,
    num_buckets: usize,
    out_peaks_ptr: *mut f32,
) {
    if samples_ptr.is_null() || starts_ptr.is_null() || ends_ptr.is_null() || out_peaks_ptr.is_null() || num_buckets == 0 {
        return;
    }
    let starts = unsafe { slice::from_raw_parts(starts_ptr, num_buckets) };
    let ends = unsafe { slice::from_raw_parts(ends_ptr, num_buckets) };
    let out = unsafe { slice::from_raw_parts_mut(out_peaks_ptr, num_buckets) };
    let max_sample = ends.iter().copied().max().unwrap_or(0) as usize;
    let samples = unsafe { slice::from_raw_parts(samples_ptr, max_sample) };
    compute_peak_buckets(samples, starts, ends, out);
}

#[no_mangle]
pub extern "C" fn ocw_audio_compute_rms_buckets(
    samples_ptr: *const f32,
    max_window: usize,
    starts_ptr: *const u32,
    ends_ptr: *const u32,
    num_buckets: usize,
    out_rms_ptr: *mut f32,
) {
    if samples_ptr.is_null() || starts_ptr.is_null() || ends_ptr.is_null() || out_rms_ptr.is_null() || num_buckets == 0 {
        return;
    }
    let starts = unsafe { slice::from_raw_parts(starts_ptr, num_buckets) };
    let ends = unsafe { slice::from_raw_parts(ends_ptr, num_buckets) };
    let out = unsafe { slice::from_raw_parts_mut(out_rms_ptr, num_buckets) };
    let max_sample = ends.iter().copied().max().unwrap_or(0) as usize;
    let samples = unsafe { slice::from_raw_parts(samples_ptr, max_sample) };
    compute_rms_buckets(samples, max_window, starts, ends, out);
}

#[no_mangle]
pub extern "C" fn ocw_audio_detect_beats(
    samples_ptr: *const f32,
    samples_per_channel: usize,
    channel_count: usize,
    sample_rate: f64,
    start_seconds: f64,
    end_seconds: f64,
    max_beats: usize,
    out_times_ptr: *mut f64,
) -> usize {
    if samples_ptr.is_null() || out_times_ptr.is_null() || channel_count == 0 || max_beats == 0 {
        return 0;
    }
    let Some(total_samples) = samples_per_channel.checked_mul(channel_count) else {
        return 0;
    };
    if total_samples > isize::MAX as usize / std::mem::size_of::<f32>()
        || max_beats > isize::MAX as usize / std::mem::size_of::<f64>()
    {
        return 0;
    }
    let samples = unsafe { slice::from_raw_parts(samples_ptr, total_samples) };
    let beats = detect_audio_beats(
        samples,
        samples_per_channel,
        channel_count,
        sample_rate,
        start_seconds,
        end_seconds,
        max_beats,
    );
    let beat_count = beats.len().min(max_beats);
    unsafe {
        slice::from_raw_parts_mut(out_times_ptr, beat_count).copy_from_slice(&beats[..beat_count]);
    }
    beat_count
}

#[no_mangle]
pub extern "C" fn ocw_audio_compute_buffer_peak(samples: *const f32, count: usize) -> f32 {
    if samples.is_null() || count == 0 { return 0.0; }
    let s = unsafe { slice::from_raw_parts(samples, count) };
    compute_buffer_peak(s)
}

#[no_mangle]
pub extern "C" fn ocw_audio_clamp_buffer_samples(samples: *mut f32, count: usize, max_peak: f32) {
    if samples.is_null() || count == 0 { return; }
    let s = unsafe { slice::from_raw_parts_mut(samples, count) };
    clamp_buffer_samples(s, max_peak);
}

#[no_mangle]
pub extern "C" fn ocw_audio_mix_channel_retime(
    out_ptr: *mut f32,
    out_start: usize,
    rendered_len: usize,
    out_len: usize,
    sample_rate: f64,
    source_ptr: *const f32,
    source_len: usize,
    source_sample_rate: f64,
    trim_start: f64,
    retime_rate: f64,
    gain: f32,
) {
    if out_ptr.is_null() || source_ptr.is_null() || out_len == 0 || source_len == 0 { return; }
    let out = unsafe { slice::from_raw_parts_mut(out_ptr, out_len) };
    let source = unsafe { slice::from_raw_parts(source_ptr, source_len) };
    mix_channel_retime(out, out_start, rendered_len, sample_rate, source, source_sample_rate, trim_start, retime_rate, gain);
}

// Effects & Compositing APIs
#[no_mangle]
pub extern "C" fn ocw_compositor_clear_buffer(
    buf: *mut u8,
    width: usize,
    height: usize,
    r: f32,
    g: f32,
    b: f32,
    a: f32,
) {
    if buf.is_null() { return; }
    let s = unsafe { slice::from_raw_parts_mut(buf, width * height * 4) };
    clear_buffer_rgba(s, width, height, r, g, b, a);
}

#[no_mangle]
pub extern "C" fn ocw_compositor_composite_layer(
    dest: *mut u8,
    dest_w: usize,
    dest_h: usize,
    src: *const u8,
    src_w: usize,
    src_h: usize,
    cx: f32,
    cy: f32,
    w: f32,
    h: f32,
    rot: f32,
    flip_x: u32,
    flip_y: u32,
    opacity: f32,
    blend_mode: u32,
    mask_alpha: *const f32,
) {
    if dest.is_null() || src.is_null() { return; }
    let d = unsafe { slice::from_raw_parts_mut(dest, dest_w * dest_h * 4) };
    let s = unsafe { slice::from_raw_parts(src, src_w * src_h * 4) };
    let mask = if mask_alpha.is_null() {
        None
    } else {
        Some(unsafe { slice::from_raw_parts(mask_alpha, dest_w * dest_h) })
    };
    let t = QuadTransform {
        center_x: cx,
        center_y: cy,
        width: w,
        height: h,
        rotation_degrees: rot,
        flip_x: flip_x != 0,
        flip_y: flip_y != 0,
    };
    composite_layer_rgba(d, dest_w, dest_h, s, src_w, src_h, t, opacity, BlendMode::from_u32(blend_mode), mask);
}

#[no_mangle]
pub extern "C" fn ocw_effects_apply_color_grading(
    pixels: *mut u8,
    width: usize,
    height: usize,
    brightness: f32,
    contrast: f32,
    saturation: f32,
    exposure: f32,
    temperature: f32,
    tint: f32,
    hue: f32,
    gamma: f32,
) {
    if pixels.is_null() { return; }
    let s = unsafe { slice::from_raw_parts_mut(pixels, width * height * 4) };
    apply_color_grading_rgba(s, width, height, brightness, contrast, saturation, exposure, temperature, tint, hue, gamma);
}

#[no_mangle]
pub extern "C" fn ocw_effects_apply_gaussian_blur(
    pixels: *mut u8,
    width: usize,
    height: usize,
    radius: usize,
    sigma: f32,
) {
    if pixels.is_null() { return; }
    let s = unsafe { slice::from_raw_parts_mut(pixels, width * height * 4) };
    apply_gaussian_blur_rgba(s, width, height, radius, sigma);
}

#[no_mangle]
pub extern "C" fn ocw_effects_apply_vignette(
    pixels: *mut u8,
    width: usize,
    height: usize,
    amount: f32,
    softness: f32,
    roundness: f32,
) {
    if pixels.is_null() { return; }
    let s = unsafe { slice::from_raw_parts_mut(pixels, width * height * 4) };
    apply_vignette_rgba(s, width, height, amount, softness, roundness);
}

#[no_mangle]
pub extern "C" fn ocw_effects_apply_chroma_key(
    pixels: *mut u8,
    width: usize,
    height: usize,
    key_r: f32,
    key_g: f32,
    key_b: f32,
    similarity: f32,
    smoothness: f32,
    spill: f32,
) {
    if pixels.is_null() { return; }
    let s = unsafe { slice::from_raw_parts_mut(pixels, width * height * 4) };
    apply_chroma_key_rgba(s, width, height, key_r, key_g, key_b, similarity, smoothness, spill);
}

#[no_mangle]
pub extern "C" fn ocw_mask_evaluate_alpha(
    px: f32,
    py: f32,
    mask_type: u32,
    cx: f32,
    cy: f32,
    sx: f32,
    sy: f32,
    rot: f32,
    feather: f32,
    inverted: u32,
) -> f32 {
    evaluate_mask_alpha(px, py, MaskType::from_u32(mask_type), cx, cy, sx, sy, rot, feather, inverted != 0)
}

#[no_mangle]
pub extern "C" fn ocw_mask_apply_to_buffer(
    pixels: *mut u8,
    width: usize,
    height: usize,
    mask_type: u32,
    cx: f32,
    cy: f32,
    sx: f32,
    sy: f32,
    rot: f32,
    feather: f32,
    inverted: u32,
) {
    if pixels.is_null() { return; }
    let s = unsafe { slice::from_raw_parts_mut(pixels, width * height * 4) };
    apply_mask_to_buffer(s, width, height, MaskType::from_u32(mask_type), cx, cy, sx, sy, rot, feather, inverted != 0);
}

#[no_mangle]
pub extern "C" fn ocw_audio_evaluate_fade(offset: f64, duration: f64, fade_in: f64, fade_out: f64) -> f64 {
    if duration <= 0.0 { return 1.0; }
    let mut gain = 1.0;
    if fade_in > 0.0 && offset < fade_in {
        gain *= (offset / fade_in).clamp(0.0, 1.0);
    }
    let end_offset = duration - offset;
    if fade_out > 0.0 && end_offset < fade_out {
        gain *= (end_offset / fade_out).clamp(0.0, 1.0);
    }
    gain
}

#[no_mangle]
pub extern "C" fn ocw_speed_map_timeline_to_source(
    timeline_offset: f64,
    _timeline_duration: f64,
    _ratios_ptr: *const f64,
    _mults_ptr: *const f64,
    _num_points: usize,
    constant_speed: f64,
) -> f64 {
    timeline_offset * constant_speed.max(0.001)
}

#[no_mangle]
pub extern "C" fn oc_scene_create() -> *mut () {
    Box::into_raw(Box::new(()))
}

#[no_mangle]
pub extern "C" fn oc_scene_destroy(ptr: *mut ()) {
    if !ptr.is_null() {
        unsafe { let _ = Box::from_raw(ptr); }
    }
}

#[no_mangle]
pub extern "C" fn ocw_scene_add_item(
    _scene: *mut (),
    _id: *const c_char,
    _type_val: u32,
    _z_index: i32,
    _opacity: f32,
    _blend_mode: u32,
    _cx: f32,
    _cy: f32,
    _w: f32,
    _h: f32,
    _rot: f32,
    _flip_x: u32,
    _flip_y: u32,
    _asset_id: *const c_char,
) {}

#[no_mangle]
pub extern "C" fn ocw_scene_build_display_list(
    _scene: *mut (),
    _viewport_w: f32,
    _viewport_h: f32,
    _culling: u32,
) -> u32 {
    0
}

#[no_mangle]
pub extern "C" fn ocw_subtitles_parse_timestamp(ts_ptr: *const c_char) -> f64 {
    let s = unsafe { c_str_to_str(ts_ptr) };
    // Formats like "00:01:23,456"
    let clean = s.replace(',', ".");
    let parts: Vec<&str> = clean.split(':').collect();
    if parts.len() == 3 {
        let h: f64 = parts[0].parse().unwrap_or(0.0);
        let m: f64 = parts[1].parse().unwrap_or(0.0);
        let sec: f64 = parts[2].parse().unwrap_or(0.0);
        (h * 3600.0 + m * 60.0 + sec) * TICKS_PER_SECOND as f64
    } else {
        0.0
    }
}

#[no_mangle]
pub extern "C" fn ocw_subtitles_format_timestamp(ticks: f64, out_buf: *mut c_char, out_len: usize) -> i32 {
    let sec = (ticks / TICKS_PER_SECOND as f64).max(0.0);
    let total_s = sec.floor() as i64;
    let ms = ((sec - total_s as f64) * 1000.0).round() as i64;
    let h = total_s / 3600;
    let m = (total_s % 3600) / 60;
    let s = total_s % 60;
    let formatted = format!("{:02}:{:02}:{:02},{:03}", h, m, s, ms);
    let bytes = formatted.as_bytes();
    let copy_len = bytes.len().min(out_len - 1);
    unsafe {
        let dest = slice::from_raw_parts_mut(out_buf as *mut u8, out_len);
        dest[..copy_len].copy_from_slice(&bytes[..copy_len]);
        dest[copy_len] = 0;
    }
    1
}

#[no_mangle]
pub extern "C" fn ocw_text_measure_line_height(font_size: f32, ratio: f32) -> f32 {
    font_size * ratio
}

#[no_mangle]
pub extern "C" fn ocw_text_break_lines_count(text_ptr: *const c_char, max_w: f32, avg_char_w: f32) -> u32 {
    let s = unsafe { c_str_to_str(text_ptr) };
    if max_w <= 0.0 || avg_char_w <= 0.0 {
        return 1;
    }
    let chars_per_line = (max_w / avg_char_w).floor() as usize;
    if chars_per_line == 0 {
        return 1;
    }
    let mut count = 0;
    for line in s.split('\n') {
        let line_len = line.chars().count();
        count += (line_len + chars_per_line - 1) / chars_per_line.max(1);
    }
    count.max(1) as u32
}

#[no_mangle]
pub extern "C" fn ocw_geometry_point_in_rotated_rect(
    px: f32,
    py: f32,
    cx: f32,
    cy: f32,
    w: f32,
    h: f32,
    rot: f32,
) -> i32 {
    let rad = -rot * std::f32::consts::PI / 180.0;
    let cos_r = rad.cos();
    let sin_r = rad.sin();
    let dx = px - cx;
    let dy = py - cy;
    let lx = (dx * cos_r - dy * sin_r).abs();
    let ly = (dx * sin_r + dy * cos_r).abs();
    if lx <= w * 0.5 && ly <= h * 0.5 { 1 } else { 0 }
}

#[no_mangle]
pub extern "C" fn ocw_geometry_test_snap(
    source_val: f64,
    target_val: f64,
    threshold: f64,
    out_snapped: *mut f64,
    out_delta: *mut f64,
) -> i32 {
    let delta = target_val - source_val;
    if delta.abs() <= threshold {
        if !out_snapped.is_null() {
            unsafe { *out_snapped = target_val; }
        }
        if !out_delta.is_null() {
            unsafe { *out_delta = delta; }
        }
        1
    } else {
        if !out_snapped.is_null() {
            unsafe { *out_snapped = source_val; }
        }
        if !out_delta.is_null() {
            unsafe { *out_delta = 0.0; }
        }
        0
    }
}

// UI Scene Graph FFI
#[no_mangle]
pub extern "C" fn ocw_ui_scene_create() -> *mut crate::ui::UiScene {
    Box::into_raw(Box::new(crate::ui::UiScene::new()))
}

#[no_mangle]
pub extern "C" fn ocw_ui_scene_destroy(scene_ptr: *mut crate::ui::UiScene) {
    if !scene_ptr.is_null() {
        unsafe { drop(Box::from_raw(scene_ptr)); }
    }
}

#[no_mangle]
pub extern "C" fn ocw_ui_create_node(
    scene_ptr: *mut crate::ui::UiScene,
    node_type: u32,
    x: f32,
    y: f32,
    w: f32,
    h: f32,
) -> u32 {
    if scene_ptr.is_null() {
        return 0;
    }
    let scene = unsafe { &mut *scene_ptr };
    let ntype = match node_type {
        1 => crate::ui::UiNodeType::Button,
        2 => crate::ui::UiNodeType::Text,
        3 => crate::ui::UiNodeType::Slider,
        4 => crate::ui::UiNodeType::Icon,
        5 => crate::ui::UiNodeType::Splitter,
        6 => crate::ui::UiNodeType::Input,
        7 => crate::ui::UiNodeType::Canvas,
        _ => crate::ui::UiNodeType::Container,
    };
    scene.create_node(ntype, crate::canvas::RectF { x, y, w, h })
}

#[no_mangle]
pub extern "C" fn ocw_ui_add_child(scene_ptr: *mut crate::ui::UiScene, parent_id: u32, child_id: u32) -> i32 {
    if scene_ptr.is_null() {
        return 0;
    }
    let scene = unsafe { &mut *scene_ptr };
    if scene.add_child(parent_id, child_id) { 1 } else { 0 }
}

#[no_mangle]
pub extern "C" fn ocw_ui_compute_layout(scene_ptr: *mut crate::ui::UiScene, root_w: f32, root_h: f32) {
    if !scene_ptr.is_null() {
        let scene = unsafe { &mut *scene_ptr };
        scene.compute_layout(root_w, root_h);
    }
}

#[no_mangle]
pub extern "C" fn ocw_ui_hit_test(scene_ptr: *mut crate::ui::UiScene, px: f32, py: f32) -> i32 {
    if scene_ptr.is_null() {
        return 0;
    }
    let scene = unsafe { &*scene_ptr };
    scene.hit_test(px, py).map(|id| id as i32).unwrap_or(0)
}

#[no_mangle]
pub extern "C" fn ocw_ui_set_hover(scene_ptr: *mut crate::ui::UiScene, px: f32, py: f32) -> i32 {
    if scene_ptr.is_null() {
        return 0;
    }
    let scene = unsafe { &mut *scene_ptr };
    scene.set_hover(px, py).map(|id| id as i32).unwrap_or(0)
}

#[no_mangle]
pub extern "C" fn ocw_timeline_hit_test_clip(
    px: f32,
    py: f32,
    clip_x: f32,
    clip_y: f32,
    clip_w: f32,
    clip_h: f32,
    handle_w: f32,
) -> i32 {
    let engine = crate::canvas::TimelineCanvasLayoutEngine::default();
    let rect = crate::canvas::RectF { x: clip_x, y: clip_y, w: clip_w, h: clip_h };
    engine.hit_test_clip(px, py, rect, handle_w) as i32
}

#[no_mangle]
pub extern "C" fn ocw_canvas_select_rect_intersections(
    rectangles_ptr: *const f64,
    rectangle_count: usize,
    selection_ptr: *const f64,
    out_indices_ptr: *mut i32,
    out_capacity: usize,
) -> usize {
    if rectangles_ptr.is_null()
        || selection_ptr.is_null()
        || out_indices_ptr.is_null()
        || rectangle_count == 0
        || out_capacity < rectangle_count
    {
        return 0;
    }

    let Some(rectangle_values) = rectangle_count.checked_mul(4) else {
        return 0;
    };
    let rectangles = unsafe { slice::from_raw_parts(rectangles_ptr, rectangle_values) };
    let selection = unsafe { slice::from_raw_parts(selection_ptr, 4) };
    let out_indices = unsafe { slice::from_raw_parts_mut(out_indices_ptr, out_capacity) };
    crate::canvas::select_rect_intersections(rectangles, selection, out_indices)
}

