import type {
  AudioElement,
  VideoElement,
  LibraryAudioElement,
  RetimeConfig,
  SceneTracks,
  TimelineTrack,
} from "@/timeline";
import { shouldMaintainPitch } from "@/retime/rate";
import type { MediaAsset } from "@/media/types";
import { applyAudioMasteringToBuffer } from "@/media/audio-mastering";
import type { AudioCapableElement } from "@/timeline/audio-state";
import {
  hasAnimatedVolume,
  isElementMuted,
  resolveEffectiveAudioGain,
} from "@/timeline/audio-state";
import { doesElementHaveEnabledAudio } from "@/timeline/audio-separation";
import { canElementHaveAudio, hasMediaId } from "@/timeline/element-utils";
import { canTrackHaveAudio } from "@/timeline";
import { mediaSupportsAudio } from "@/media/media-utils";
import { getSourceTimeAtClipTime, renderRetimedBuffer } from "@/retime";
import { Input, ALL_FORMATS, BlobSource, AudioBufferSink } from "mediabunny";
import { TICKS_PER_SECOND } from "@/wasm";
import { computeRmsBuckets, type SampleBucket } from "@/media/waveform-summary";
import { getNativeTimelineBindings } from "@/native/opencut-core";
import {
  AUDIO_WORK_CHUNK_SAMPLES,
  isAudioAbortError,
  runAudioTask,
  throwIfAudioAborted,
  yieldAudioWork,
} from "./audio-cancellation";

const MAX_AUDIO_CHANNELS = 2;
const EXPORT_SAMPLE_RATE = 44100;

export interface CollectedAudioElement {
  timelineElement: AudioCapableElement;
  buffer: AudioBuffer;
  startTime: number;
  duration: number;
  trimStart: number;
  trimEnd: number;
  volume: number;
  muted: boolean;
  retime?: RetimeConfig;
}

export function createAudioContext({
  sampleRate,
}: {
  sampleRate?: number;
} = {}): AudioContext {
  const AudioContextConstructor =
    window.AudioContext ||
    (window as typeof window & { webkitAudioContext?: typeof AudioContext })
      .webkitAudioContext;

  return new AudioContextConstructor(sampleRate ? { sampleRate } : undefined);
}

export interface DecodedAudio {
  samples: Float32Array;
  sampleRate: number;
}

export async function decodeAudioToFloat32({
  audioBlob,
  sampleRate,
}: {
  audioBlob: Blob;
  sampleRate?: number;
}): Promise<DecodedAudio> {
  const audioContext = createAudioContext({ sampleRate });
  const arrayBuffer = await audioBlob.arrayBuffer();
  const audioBuffer = await audioContext.decodeAudioData(arrayBuffer);

  // mix down to mono
  const numChannels = audioBuffer.numberOfChannels;
  const length = audioBuffer.length;
  const samples = new Float32Array(length);

  if (numChannels === 1) {
    samples.set(audioBuffer.getChannelData(0));
    return { samples, sampleRate: audioBuffer.sampleRate };
  }

  const bindings = getNativeTimelineBindings();
  if (bindings && numChannels === 2 && length > 0) {
    const byteSize = length * 4;
    const leftPtr = bindings.malloc(byteSize);
    const rightPtr = bindings.malloc(byteSize);
    const outPtr = bindings.malloc(byteSize);
    if (leftPtr !== 0 && rightPtr !== 0 && outPtr !== 0) {
      try {
        bindings.HEAPF32.set(audioBuffer.getChannelData(0), leftPtr >> 2);
        bindings.HEAPF32.set(audioBuffer.getChannelData(1), rightPtr >> 2);
        bindings.downmixStereo(leftPtr, rightPtr, outPtr, length);
        samples.set(
          bindings.HEAPF32.subarray(outPtr >> 2, (outPtr >> 2) + length),
        );
        return { samples, sampleRate: audioBuffer.sampleRate };
      } finally {
        bindings.free(leftPtr);
        bindings.free(rightPtr);
        bindings.free(outPtr);
      }
    }
  }

  for (let i = 0; i < length; i++) {
    let sum = 0;
    for (let channel = 0; channel < numChannels; channel++) {
      sum += audioBuffer.getChannelData(channel)[i];
    }
    samples[i] = sum / numChannels;
  }

  return { samples, sampleRate: audioBuffer.sampleRate };
}

export interface AudibleElementCandidate {
  element: AudioElement | VideoElement;
  mediaAsset: MediaAsset | null;
}

function hasSoloedAudioTrack({ tracks }: { tracks: SceneTracks }): boolean {
  return [...tracks.overlay, tracks.main, ...tracks.audio].some(
    (track) => canTrackHaveAudio(track) && track.solo === true,
  );
}

function isExcludedByAudioSolo({
  track,
  hasSolo,
}: {
  track: TimelineTrack;
  hasSolo: boolean;
}): boolean {
  return hasSolo && canTrackHaveAudio(track) && track.solo !== true;
}

export function collectAudibleCandidates({
  tracks,
  mediaAssets,
}: {
  tracks: SceneTracks;
  mediaAssets: MediaAsset[];
}): AudibleElementCandidate[] {
  const allTracks = [...tracks.overlay, tracks.main, ...tracks.audio];
  const hasSolo = hasSoloedAudioTrack({ tracks });
  const mediaMap = new Map(mediaAssets.map((a) => [a.id, a]));
  const candidates: AudibleElementCandidate[] = [];

  for (const track of allTracks) {
    if (
      (canTrackHaveAudio(track) && track.muted) ||
      isExcludedByAudioSolo({ track, hasSolo })
    )
      continue;

    for (const element of track.elements) {
      if (!canElementHaveAudio(element)) continue;
      if (element.duration <= 0) continue;

      const mediaAsset = hasMediaId(element)
        ? (mediaMap.get(element.mediaId) ?? null)
        : null;
      if (mediaAsset?.missing) continue;
      if (!doesElementHaveEnabledAudio({ element, mediaAsset })) continue;

      candidates.push({ element, mediaAsset });
    }
  }

  return candidates;
}

export function timelineHasAudio({
  tracks,
  mediaAssets,
}: {
  tracks: SceneTracks;
  mediaAssets: MediaAsset[];
}): boolean {
  return collectAudibleCandidates({ tracks, mediaAssets }).some(
    ({ element }) => !isElementMuted({ element }),
  );
}

export async function collectAudioElements({
  tracks,
  mediaAssets,
  audioContext,
  signal,
  range,
}: {
  tracks: SceneTracks;
  mediaAssets: MediaAsset[];
  audioContext: AudioContext;
  signal?: AbortSignal;
  range?: { start: number; end: number };
}): Promise<CollectedAudioElement[]> {
  throwIfAudioAborted(signal);
  const candidates = collectAudibleCandidates({ tracks, mediaAssets });
  const mediaMap = new Map<string, MediaAsset>(
    mediaAssets.map((media) => [media.id, media]),
  );
  const pendingElements: Array<Promise<CollectedAudioElement | null>> = [];

  for (const { element, mediaAsset } of candidates) {
    throwIfAudioAborted(signal);
    if (range && (element.startTime >= range.end || element.startTime + element.duration <= range.start)) continue;
    if (element.type === "audio") {
      pendingElements.push(
        resolveAudioBufferForElement({
          element,
          mediaMap,
          audioContext,
          signal,
        }).then((audioBuffer) => {
          if (!audioBuffer) return null;
          return {
            timelineElement: element,
            buffer: audioBuffer,
            startTime: element.startTime / TICKS_PER_SECOND,
            duration: element.duration / TICKS_PER_SECOND,
            trimStart: element.trimStart / TICKS_PER_SECOND,
            trimEnd: element.trimEnd / TICKS_PER_SECOND,
            volume: resolveEffectiveAudioGain({
              element,
              trackMuted: false,
              localTime: 0,
            }),
            muted: isElementMuted({ element }),
            retime: element.retime,
          };
        }),
      );
      continue;
    }

    if (element.type === "video") {
      if (!mediaAsset || !mediaSupportsAudio({ media: mediaAsset })) continue;

      pendingElements.push(
        resolveAudioBufferForAsset({
          asset: mediaAsset,
          audioContext,
          signal,
        }).then((audioBuffer) => {
          if (!audioBuffer) return null;
          return {
            timelineElement: element,
            buffer: audioBuffer,
            startTime: element.startTime / TICKS_PER_SECOND,
            duration: element.duration / TICKS_PER_SECOND,
            trimStart: element.trimStart / TICKS_PER_SECOND,
            trimEnd: element.trimEnd / TICKS_PER_SECOND,
            volume: resolveEffectiveAudioGain({
              element,
              trackMuted: false,
              localTime: 0,
            }),
            muted: isElementMuted({ element }),
            retime: element.retime,
          };
        }),
      );
    }
  }

  const resolvedElements = await runAudioTask(
    () => Promise.all(pendingElements),
    signal,
  );
  const audioElements: CollectedAudioElement[] = [];
  for (const element of resolvedElements) {
    if (element) audioElements.push(element);
  }
  return audioElements;
}

async function resolveAudioBufferForElement({
  element,
  mediaMap,
  audioContext,
  signal,
}: {
  element: AudioElement;
  mediaMap: Map<string, MediaAsset>;
  audioContext: AudioContext;
  signal?: AbortSignal;
}): Promise<AudioBuffer | null> {
  try {
    throwIfAudioAborted(signal);
    if (element.sourceType === "upload") {
      const asset = mediaMap.get(element.mediaId);
      if (!asset) return null;
      return await resolveAudioBufferForAsset({ asset, audioContext, signal });
    }

    if (element.buffer) return element.buffer;

    const response = await runAudioTask(
      () => fetch(element.sourceUrl, { signal }),
      signal,
    );
    if (!response.ok) {
      throw new Error(`Library audio fetch failed: ${response.status}`);
    }

    const arrayBuffer = await runAudioTask(
      () => response.arrayBuffer(),
      signal,
    );
    return await runAudioTask(
      () => audioContext.decodeAudioData(arrayBuffer.slice(0)),
      signal,
    );
  } catch (error) {
    if (signal?.aborted || isAudioAbortError(error)) throw error;
    console.warn("Failed to decode audio:", error);
    return null;
  }
}

async function resolveAudioBufferForAsset({
  asset,
  audioContext,
  signal,
}: {
  asset: MediaAsset;
  audioContext: AudioContext;
  signal?: AbortSignal;
}): Promise<AudioBuffer | null> {
  throwIfAudioAborted(signal);
  if (asset.type === "audio") {
    try {
      const arrayBuffer = await runAudioTask(
        () => asset.file.arrayBuffer(),
        signal,
      );
      return await runAudioTask(
        () => audioContext.decodeAudioData(arrayBuffer.slice(0)),
        signal,
      );
    } catch (error) {
      if (signal?.aborted || isAudioAbortError(error)) throw error;
      console.warn("Failed to decode audio asset:", error);
      return null;
    }
  }

  const input = new Input({
    source: new BlobSource(asset.file),
    formats: ALL_FORMATS,
  });
  const disposeInput = () => input.dispose();
  signal?.addEventListener("abort", disposeInput, { once: true });

  try {
    const audioTrack = await runAudioTask(
      () => input.getPrimaryAudioTrack(),
      signal,
    );
    if (!audioTrack) return null;

    const sink = new AudioBufferSink(audioTrack);
    const targetSampleRate = audioContext.sampleRate;

    const chunks: AudioBuffer[] = [];
    let totalSamples = 0;

    const iterator = sink.buffers(0);
    try {
      while (true) {
        const decoded = await runAudioTask(() => iterator.next(), signal);
        if (decoded.done) break;
        chunks.push(decoded.value.buffer);
        totalSamples += decoded.value.buffer.length;
        if (signal) await yieldAudioWork(signal);
      }
    } finally {
      void iterator.return().catch(() => {});
    }

    if (chunks.length === 0) return null;

    const nativeSampleRate = chunks[0].sampleRate;
    const numChannels = Math.min(
      MAX_AUDIO_CHANNELS,
      chunks[0].numberOfChannels,
    );

    const nativeChannels = Array.from(
      { length: numChannels },
      () => new Float32Array(totalSamples),
    );
    let offset = 0;
    for (const chunk of chunks) {
      throwIfAudioAborted(signal);
      for (let channel = 0; channel < numChannels; channel++) {
        const sourceData = chunk.getChannelData(
          Math.min(channel, chunk.numberOfChannels - 1),
        );
        nativeChannels[channel].set(sourceData, offset);
      }
      offset += chunk.length;
      if (signal) await yieldAudioWork(signal);
    }

    // use OfflineAudioContext for high-quality resampling to target rate
    const outputSamples = Math.ceil(
      totalSamples * (targetSampleRate / nativeSampleRate),
    );
    const offlineContext = new OfflineAudioContext(
      numChannels,
      outputSamples,
      targetSampleRate,
    );

    const nativeBuffer = audioContext.createBuffer(
      numChannels,
      totalSamples,
      nativeSampleRate,
    );
    for (let ch = 0; ch < numChannels; ch++) {
      nativeBuffer.copyToChannel(nativeChannels[ch], ch);
    }

    const sourceNode = offlineContext.createBufferSource();
    sourceNode.buffer = nativeBuffer;
    sourceNode.connect(offlineContext.destination);
    sourceNode.start(0);

    try {
      return await runAudioTask(
        () => offlineContext.startRendering(),
        signal,
        () => {
          sourceNode.stop();
        },
      );
    } finally {
      sourceNode.disconnect();
    }
  } catch (error) {
    if (signal?.aborted || isAudioAbortError(error)) throw error;
    console.warn("Failed to decode asset audio:", error);
    return null;
  } finally {
    signal?.removeEventListener("abort", disposeInput);
    input.dispose();
  }
}

interface AudioMixSource {
  timelineElement: AudioCapableElement;
  file: File;
  startTime: number;
  duration: number;
  trimStart: number;
  trimEnd: number;
  volume: number;
  retime?: RetimeConfig;
}

export interface AudioClipSource {
  timelineElement: AudioCapableElement;
  id: string;
  sourceKey: string;
  file: File;
  startTime: number;
  duration: number;
  trimStart: number;
  trimEnd: number;
  volume: number;
  muted: boolean;
  retime?: RetimeConfig;
}

async function fetchLibraryAudioSource({
  element,
  volume,
}: {
  element: LibraryAudioElement;
  volume: number;
}): Promise<AudioMixSource | null> {
  try {
    const response = await fetch(element.sourceUrl);
    if (!response.ok) {
      throw new Error(`Library audio fetch failed: ${response.status}`);
    }

    const blob = await response.blob();
    const file = new File([blob], `${element.name}.mp3`, {
      type: "audio/mpeg",
    });

    return {
      timelineElement: element,
      file,
      startTime: element.startTime / TICKS_PER_SECOND,
      duration: element.duration / TICKS_PER_SECOND,
      trimStart: element.trimStart / TICKS_PER_SECOND,
      trimEnd: element.trimEnd / TICKS_PER_SECOND,
      volume,
      retime: element.retime,
    };
  } catch (error) {
    console.warn("Failed to fetch library audio:", error);
    return null;
  }
}

async function fetchLibraryAudioClip({
  element,
  muted,
  volume,
}: {
  element: LibraryAudioElement;
  muted: boolean;
  volume: number;
}): Promise<AudioClipSource | null> {
  try {
    const response = await fetch(element.sourceUrl);
    if (!response.ok) {
      throw new Error(`Library audio fetch failed: ${response.status}`);
    }

    const blob = await response.blob();
    const file = new File([blob], `${element.name}.mp3`, {
      type: "audio/mpeg",
    });

    return {
      timelineElement: element,
      id: element.id,
      sourceKey: element.id,
      file,
      startTime: element.startTime,
      duration: element.duration,
      trimStart: element.trimStart,
      trimEnd: element.trimEnd,
      volume,
      muted,
      retime: element.retime,
    };
  } catch (error) {
    console.warn("Failed to fetch library audio:", error);
    return null;
  }
}

function collectMediaAudioSource({
  element,
  mediaAsset,
  volume,
}: {
  element: AudioCapableElement;
  mediaAsset: MediaAsset;
  volume: number;
}): AudioMixSource {
  return {
    timelineElement: element,
    file: mediaAsset.file,
    startTime: element.startTime / TICKS_PER_SECOND,
    duration: element.duration / TICKS_PER_SECOND,
    trimStart: element.trimStart / TICKS_PER_SECOND,
    trimEnd: element.trimEnd / TICKS_PER_SECOND,
    volume,
    retime: element.retime,
  };
}

function collectMediaAudioClip({
  element,
  mediaAsset,
  muted,
  volume,
}: {
  element: AudioCapableElement;
  mediaAsset: MediaAsset;
  muted: boolean;
  volume: number;
}): AudioClipSource {
  return {
    timelineElement: element,
    id: element.id,
    sourceKey: mediaAsset.id,
    file: mediaAsset.file,
    startTime: element.startTime / TICKS_PER_SECOND,
    duration: element.duration / TICKS_PER_SECOND,
    trimStart: element.trimStart / TICKS_PER_SECOND,
    trimEnd: element.trimEnd / TICKS_PER_SECOND,
    volume,
    muted,
    retime: element.retime,
  };
}

export async function collectAudioMixSources({
  tracks,
  mediaAssets,
}: {
  tracks: SceneTracks;
  mediaAssets: MediaAsset[];
}): Promise<AudioMixSource[]> {
  const orderedTracks = [...tracks.overlay, tracks.main, ...tracks.audio];
  const hasSolo = hasSoloedAudioTrack({ tracks });
  const audioMixSources: AudioMixSource[] = [];
  const mediaMap = new Map<string, MediaAsset>(
    mediaAssets.map((asset) => [asset.id, asset]),
  );
  const pendingLibrarySources: Array<Promise<AudioMixSource | null>> = [];

  for (const track of orderedTracks) {
    if (
      (canTrackHaveAudio(track) && track.muted) ||
      isExcludedByAudioSolo({ track, hasSolo })
    )
      continue;

    for (const element of track.elements) {
      if (!canElementHaveAudio(element)) continue;
      if (isElementMuted({ element })) continue;
      const mediaAsset = hasMediaId(element)
        ? (mediaMap.get(element.mediaId) ?? null)
        : null;
      if (!doesElementHaveEnabledAudio({ element, mediaAsset })) continue;
      const volume = resolveEffectiveAudioGain({
        element,
        localTime: 0,
      });

      if (element.type === "audio") {
        if (element.sourceType === "upload") {
          const mediaAsset = mediaMap.get(element.mediaId);
          if (!mediaAsset) continue;

          audioMixSources.push(
            collectMediaAudioSource({ element, mediaAsset, volume }),
          );
        } else {
          pendingLibrarySources.push(
            fetchLibraryAudioSource({ element, volume }),
          );
        }
        continue;
      }

      if (element.type === "video") {
        if (mediaAsset && mediaSupportsAudio({ media: mediaAsset })) {
          audioMixSources.push(
            collectMediaAudioSource({ element, mediaAsset, volume }),
          );
        }
      }
    }
  }

  const resolvedLibrarySources = await Promise.all(pendingLibrarySources);
  for (const source of resolvedLibrarySources) {
    if (source) audioMixSources.push(source);
  }

  return audioMixSources;
}

export async function collectAudioClips({
  tracks,
  mediaAssets,
}: {
  tracks: SceneTracks;
  mediaAssets: MediaAsset[];
}): Promise<AudioClipSource[]> {
  const orderedTracks = [...tracks.overlay, tracks.main, ...tracks.audio];
  const hasSolo = hasSoloedAudioTrack({ tracks });
  const clips: AudioClipSource[] = [];
  const mediaMap = new Map<string, MediaAsset>(
    mediaAssets.map((asset) => [asset.id, asset]),
  );
  const pendingLibraryClips: Array<Promise<AudioClipSource | null>> = [];

  for (const track of orderedTracks) {
    const isTrackMuted =
      (canTrackHaveAudio(track) && track.muted) ||
      isExcludedByAudioSolo({ track, hasSolo });

    for (const element of track.elements) {
      if (!canElementHaveAudio(element)) continue;

      const mediaAsset = hasMediaId(element)
        ? (mediaMap.get(element.mediaId) ?? null)
        : null;
      if (mediaAsset?.missing) continue;
      if (!doesElementHaveEnabledAudio({ element, mediaAsset })) continue;

      const muted = isTrackMuted || isElementMuted({ element });
      const volume = resolveEffectiveAudioGain({
        element,
        trackMuted: isTrackMuted,
        localTime: 0,
      });

      if (element.type === "audio") {
        if (element.sourceType === "upload") {
          const mediaAsset = mediaMap.get(element.mediaId);
          if (!mediaAsset) continue;

          clips.push(
            collectMediaAudioClip({
              element,
              mediaAsset,
              muted,
              volume,
            }),
          );
        } else {
          pendingLibraryClips.push(
            fetchLibraryAudioClip({ element, muted, volume }),
          );
        }
        continue;
      }

      if (element.type === "video") {
        if (mediaAsset && mediaSupportsAudio({ media: mediaAsset })) {
          clips.push(
            collectMediaAudioClip({
              element,
              mediaAsset,
              muted,
              volume,
            }),
          );
        }
      }
    }
  }

  const resolvedLibraryClips = await Promise.all(pendingLibraryClips);
  for (const clip of resolvedLibraryClips) {
    if (clip) clips.push(clip);
  }

  return clips;
}

export async function createTimelineAudioBuffer({
  tracks,
  mediaAssets,
  duration,
  startTime = 0,
  sampleRate = EXPORT_SAMPLE_RATE,
  audioContext,
  signal,
}: {
  tracks: SceneTracks;
  mediaAssets: MediaAsset[];
  duration: number;
  startTime?: number;
  sampleRate?: number;
  audioContext?: AudioContext;
  signal?: AbortSignal;
}): Promise<AudioBuffer | null> {
  throwIfAudioAborted(signal);
  if (!Number.isSafeInteger(duration) || duration <= 0 || !Number.isSafeInteger(startTime) || startTime < 0 || !Number.isSafeInteger(startTime + duration)) {
    throw new Error("Invalid audio export window");
  }
  const context = audioContext ?? createAudioContext({ sampleRate });
  try {
    const audioElements = await collectAudioElements({
      tracks,
      mediaAssets,
      audioContext: context,
      signal,
      range: { start: startTime, end: startTime + duration },
    });
    throwIfAudioAborted(signal);
    if (audioElements.length === 0) return null;

    const outputChannels = 2;
    const durationSeconds = duration / TICKS_PER_SECOND;
    const outputLength = Math.ceil(durationSeconds * sampleRate);
    const outputBuffer = context.createBuffer(
      outputChannels,
      outputLength,
      sampleRate,
    );

    for (const element of audioElements) {
      throwIfAudioAborted(signal);
      if (element.muted) continue;
      const windowStartSeconds = startTime / TICKS_PER_SECOND;
      if (element.startTime >= windowStartSeconds + durationSeconds || element.startTime + element.duration <= windowStartSeconds) continue;

      const renderedBuffer = shouldMaintainPitch({
        rate: element.retime?.rate ?? 1,
        maintainPitch: element.retime?.maintainPitch,
      })
        ? await renderRetimedBuffer({
            audioContext: context,
            sourceBuffer: element.buffer,
            trimStart: element.trimStart,
            clipDuration: element.duration,
            retime: element.retime,
            maintainPitch: true,
            signal,
          })
        : undefined;
      throwIfAudioAborted(signal);
      await mixAudioChannels({
        element,
        buffer: renderedBuffer ?? element.buffer,
        trimStart: renderedBuffer ? 0 : element.trimStart,
        retime: renderedBuffer ? undefined : element.retime,
        outputBuffer,
        outputLength,
        sampleRate,
        windowStartSample: Math.floor(windowStartSeconds * sampleRate),
        signal,
      });
    }

    return await applyAudioMasteringToBuffer({
      audioBuffer: outputBuffer,
      signal,
    });
  } finally {
    if (!audioContext) {
      try {
        await context.close();
      } catch (error) {
        console.warn("Failed to close export audio context:", error);
      }
    }
  }
}

function collectPeakRange({
  buffer,
  count,
  startSample,
  endSample,
}: {
  buffer: AudioBuffer;
  count: number;
  startSample: number;
  endSample: number;
}): Float32Array {
  const channels = buffer.numberOfChannels;
  const peaks = new Float32Array(count);

  const bindings = getNativeTimelineBindings();
  if (bindings && count > 0) {
    try {
      const startsPtr = bindings.malloc(count * 4);
      const endsPtr = bindings.malloc(count * 4);
      const outPeaksPtr = bindings.malloc(count * 4);
      const starts = new Uint32Array(bindings.HEAPU32.buffer, startsPtr, count);
      const ends = new Uint32Array(bindings.HEAPU32.buffer, endsPtr, count);
      for (let i = 0; i < count; i++) {
        const { bucketStart, bucketEnd } = getSampleBucketRange({
          startSample,
          endSample,
          bucketIndex: i,
          bucketCount: count,
        });
        starts[i] = bucketStart;
        ends[i] = bucketEnd;
      }

      for (let c = 0; c < channels; c++) {
        const data = buffer.getChannelData(c);
        const chPtr = bindings.malloc(data.length * 4);
        try {
          bindings.HEAPF32.set(data, chPtr >> 2);
          bindings.computePeakBuckets(
            chPtr,
            startsPtr,
            endsPtr,
            count,
            outPeaksPtr,
          );
          const chPeaks = new Float32Array(
            bindings.HEAPF32.buffer,
            outPeaksPtr,
            count,
          );
          for (let i = 0; i < count; i++) {
            if (chPeaks[i] > peaks[i]) {
              peaks[i] = chPeaks[i];
            }
          }
        } finally {
          bindings.free(chPtr);
        }
      }

      bindings.free(startsPtr);
      bindings.free(endsPtr);
      bindings.free(outPeaksPtr);
      return peaks;
    } catch (e) {
      console.warn("[OpenCut] Native collectPeakRange fallback:", e);
    }
  }

  for (let c = 0; c < channels; c++) {
    const data = buffer.getChannelData(c);
    for (let i = 0; i < count; i++) {
      const { bucketStart: start, bucketEnd: end } = getSampleBucketRange({
        startSample,
        endSample,
        bucketIndex: i,
        bucketCount: count,
      });
      for (let j = start; j < end; j++) {
        const abs = Math.abs(data[j]);
        if (abs > peaks[i]) peaks[i] = abs;
      }
    }
  }

  return peaks;
}

export function extractPeakRange({
  buffer,
  count,
  startSample,
  endSample,
}: {
  buffer: AudioBuffer;
  count: number;
  startSample: number;
  endSample: number;
}): number[] {
  return Array.from(
    collectPeakRange({
      buffer,
      count,
      startSample,
      endSample,
    }),
  );
}

export function getSampleBucketRange({
  startSample,
  endSample,
  bucketIndex,
  bucketCount,
}: {
  startSample: number;
  endSample: number;
  bucketIndex: number;
  bucketCount: number;
}): {
  bucketStart: number;
  bucketEnd: number;
} {
  const rangeLength = Math.max(0, endSample - startSample);
  const bucketStart =
    startSample + Math.floor((bucketIndex * rangeLength) / bucketCount);
  const bucketEnd =
    startSample + Math.floor(((bucketIndex + 1) * rangeLength) / bucketCount);
  return {
    bucketStart,
    bucketEnd: Math.max(bucketStart, bucketEnd),
  };
}

export function extractRmsBuckets({
  buffer,
  buckets,
}: {
  buffer: AudioBuffer;
  buckets: SampleBucket[];
}): number[] {
  return computeRmsBuckets({ buffer, buckets });
}

/**
 * Computes per-bucket waveform amplitude using the maximum RMS over a short
 * analysis window inside each bucket.
 *
 * A naive mean-RMS over a whole bucket averages silence together with nearby
 * sound, which smears transitions (e.g. the onset of speech) across the
 * bucket and makes the waveform respond late. Taking the max over fixed
 * short windows (~20 ms) preserves the smooth, non-jittery RMS character
 * while making transitions land where they actually happen in the audio.
 *
 * Channels are combined per-window before taking the max, so the measure
 * reflects total energy regardless of stereo layout.
 */
export function extractRmsRange({
  buffer,
  count,
  startSample,
  endSample,
}: {
  buffer: AudioBuffer;
  count: number;
  startSample: number;
  endSample: number;
}): number[] {
  return extractRmsBuckets({
    buffer,
    buckets: Array.from({ length: count }, (_, bucketIndex) =>
      getSampleBucketRange({
        startSample,
        endSample,
        bucketIndex,
        bucketCount: count,
      }),
    ),
  });
}

async function mixAudioChannels({
  element,
  buffer,
  trimStart,
  retime,
  outputBuffer,
  outputLength,
  sampleRate,
  windowStartSample = 0,
  signal,
}: {
  element: CollectedAudioElement;
  buffer: AudioBuffer;
  trimStart: number;
  retime?: RetimeConfig;
  outputBuffer: AudioBuffer;
  outputLength: number;
  sampleRate: number;
  windowStartSample?: number;
  signal?: AbortSignal;
}): Promise<void> {
  throwIfAudioAborted(signal);
  const { startTime, duration: elementDuration } = element;
  const outputStartSample = Math.floor(startTime * sampleRate) - windowStartSample;
  const renderedLength = Math.ceil(elementDuration * sampleRate);
  const outputChannels = 2;
  const animatedVolume = hasAnimatedVolume({
    element: element.timelineElement,
  });
  const bindings = getNativeTimelineBindings();
  const rate = getSourceTimeAtClipTime({ clipTime: 1, retime });
  let nativeEnabled = !!bindings && !animatedVolume;
  let outPtr = 0;
  let srcPtr = 0;
  if (renderedLength <= 0 || buffer.length <= 0) return;
  try {
    if (bindings && nativeEnabled) {
      try {
        outPtr = bindings.malloc(
          AUDIO_WORK_CHUNK_SAMPLES * Float32Array.BYTES_PER_ELEMENT,
        );
        srcPtr = bindings.malloc(
          buffer.length * Float32Array.BYTES_PER_ELEMENT,
        );
        if (!outPtr || !srcPtr) throw new Error("Audio mix allocation failed");
      } catch (error) {
        nativeEnabled = false;
        console.warn("[OpenCut] Native mixAudioChannels fallback:", error);
      }
    }
    for (let channel = 0; channel < outputChannels; channel++) {
      throwIfAudioAborted(signal);
      const outputData = outputBuffer.getChannelData(channel);
      const sourceData = buffer.getChannelData(
        Math.min(channel, buffer.numberOfChannels - 1),
      );
      if (bindings && nativeEnabled) {
        for (
          let offset = 0;
          offset < sourceData.length;
          offset += AUDIO_WORK_CHUNK_SAMPLES
        ) {
          throwIfAudioAborted(signal);
          bindings.HEAPF32.set(
            sourceData.subarray(offset, offset + AUDIO_WORK_CHUNK_SAMPLES),
            (srcPtr >> 2) + offset,
          );
          if (signal) await yieldAudioWork(signal);
        }
      }
      for (
        let offset = Math.max(0, -outputStartSample);
        offset < renderedLength;
        offset += AUDIO_WORK_CHUNK_SAMPLES
      ) {
        throwIfAudioAborted(signal);
        const outputIndex = outputStartSample + offset;
        const count = Math.min(
          AUDIO_WORK_CHUNK_SAMPLES,
          renderedLength - offset,
          outputLength - outputIndex,
        );
        if (count <= 0) break;
        let mixedNative = false;
        if (bindings && nativeEnabled && outputIndex >= 0) {
          try {
            bindings.HEAPF32.set(
              outputData.subarray(outputIndex, outputIndex + count),
              outPtr >> 2,
            );
            bindings.mixAudioChannelRetime(
              outPtr,
              0,
              count,
              count,
              sampleRate,
              srcPtr,
              buffer.length,
              buffer.sampleRate,
              trimStart + (offset / sampleRate) * rate,
              rate,
              element.volume,
            );
            outputData.set(
              new Float32Array(bindings.HEAPF32.buffer, outPtr, count),
              outputIndex,
            );
            mixedNative = true;
          } catch (error) {
            nativeEnabled = false;
            console.warn("[OpenCut] Native mixAudioChannels fallback:", error);
          }
        }
        if (!mixedNative) {
          for (let index = offset; index < offset + count; index++) {
            const targetIndex = outputStartSample + index;
            if (targetIndex < 0) continue;
            const clipTime = index / sampleRate;
            const sourceIndex =
              (trimStart + getSourceTimeAtClipTime({ clipTime, retime })) *
              buffer.sampleRate;
            if (sourceIndex >= sourceData.length) break;
            const lowerIndex = Math.floor(sourceIndex);
            const upperIndex = Math.min(sourceData.length - 1, lowerIndex + 1);
            const fraction = sourceIndex - lowerIndex;
            const gain = animatedVolume
              ? resolveEffectiveAudioGain({
                  element: element.timelineElement,
                  localTime: clipTime,
                })
              : element.volume;
            outputData[targetIndex] +=
              (sourceData[lowerIndex] * (1 - fraction) +
                sourceData[upperIndex] * fraction) *
              gain;
          }
        }
        if (signal) await yieldAudioWork(signal);
      }
    }
  } finally {
    if (bindings && srcPtr) bindings.free(srcPtr);
    if (bindings && outPtr) bindings.free(outPtr);
  }
}
