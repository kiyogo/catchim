"use client";

import { useEffect, useState } from "react";
import { TransitionTopIcon } from "@hugeicons/core-free-icons";
import { HugeiconsIcon } from "@hugeicons/react";
import {
  Dialog,
  DialogContent,
  DialogTrigger,
  DialogTitle,
} from "@/components/ui/dialog";
import { Button } from "@/components/ui/button";
import { Progress } from "@/components/ui/progress";
import { cn } from "@/utils/ui";
import {
  getExportMimeType,
  getExportFileExtension,
  downloadBuffer,
  resolveExportCanvasSize,
} from "@/export";
import {
  BookmarkPlus,
  Check,
  Copy,
  Download,
  RotateCcw,
  Trash2,
} from "lucide-react";
import {
  EXPORT_FORMAT_VALUES,
  EXPORT_QUALITY_VALUES,
  EXPORT_RESOLUTION_VALUES,
  type ExportFormat,
  type ExportQuality,
  type ExportResolution,
} from "@/export";
import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from "@/components/ui/select";
import { useEditor } from "@/editor/use-editor";
import { DEFAULT_EXPORT_OPTIONS } from "@/export/defaults";
import { useTranslation, type TranslationKey } from "@/i18n";
import { Input } from "@/components/ui/input";
import { floatToFrameRate } from "@/fps/utils";
import {
  deleteExportPreset,
  loadSavedExportPresets,
  saveExportPreset,
  type ExportFpsPreset,
  type SavedExportPreset,
} from "@/export/presets";
import { PLATFORM_EXPORT_PRESETS, findPlatformExportPreset } from "@/export/platform-presets";
import { validateExportRangeInput } from "@/export/range-input";
import { resolveExportRange } from "@/export/range";
import { addMediaTime, mediaTime, mediaTimeFromSeconds, mediaTimeToSeconds, type MediaTime } from "@/wasm";

const EXPORT_DIALOG_MESSAGES = {
  vi: {
    output: "Loại đầu ra", video: "Video", audioOnly: "Chỉ âm thanh (WAV)",
    range: "Phạm vi", full: "Toàn bộ timeline", selection: "Khoảng clip đã chọn", custom: "Điểm vào / ra",
    inPoint: "Điểm vào (giây)", outPoint: "Điểm ra (giây)", setIn: "Lấy điểm vào từ playhead", setOut: "Lấy điểm ra từ playhead",
    selectionHelp: "Xuất tất cả nội dung nằm giữa điểm đầu và cuối của các clip đã chọn.",
    rangeRequired: "Nhập đủ điểm vào và điểm ra.", rangeInvalid: "Điểm vào và điểm ra phải là số giây hợp lệ.",
    rangeOrder: "Điểm ra phải nằm sau điểm vào.", rangeOutside: "Phạm vi phải nằm trong thời lượng timeline.",
    selectionRequired: "Chọn ít nhất một clip trên timeline.", emptyTimeline: "Thêm nội dung vào timeline trước khi xuất.",
    keepAspect: "Preset chỉ đổi độ phân giải, chất lượng và fps; giữ tỷ lệ khung hình dự án.",
    duration: "Thời lượng xuất", seconds: "giây", audioOutput: "WAV · PCM 16-bit · âm thanh đã trộn",
    timelineDuration: "Thời lượng timeline",
  },
  en: {
    output: "Output", video: "Video", audioOnly: "Audio only (WAV)",
    range: "Range", full: "Entire timeline", selection: "Selected clips span", custom: "In / out points",
    inPoint: "In point (seconds)", outPoint: "Out point (seconds)", setIn: "Set in from playhead", setOut: "Set out from playhead",
    selectionHelp: "Exports all timeline content between the first and last selected clips.",
    rangeRequired: "Enter both in and out points.", rangeInvalid: "In and out points must be valid seconds.",
    rangeOrder: "The out point must be after the in point.", rangeOutside: "The range must fit within the timeline duration.",
    selectionRequired: "Select at least one timeline clip.", emptyTimeline: "Add timeline content before exporting.",
    keepAspect: "Presets change resolution, quality and fps only; the project aspect ratio is retained.",
    duration: "Export duration", seconds: "seconds", audioOutput: "WAV · 16-bit PCM · mixed audio",
    timelineDuration: "Timeline duration",
  },
} as const;

const EXPORT_FPS_PRESETS: ExportFpsPreset[] = [
  "project",
  "24",
  "25",
  "30",
  "50",
  "60",
];

function isExportFpsPreset(value: string): value is ExportFpsPreset {
  return EXPORT_FPS_PRESETS.some((preset) => preset === value);
}

function isExportFormat(value: string): value is ExportFormat {
  return EXPORT_FORMAT_VALUES.some((formatValue) => formatValue === value);
}

function isExportQuality(value: string): value is ExportQuality {
  return EXPORT_QUALITY_VALUES.some((qualityValue) => qualityValue === value);
}

function isExportResolution(value: string): value is ExportResolution {
  return EXPORT_RESOLUTION_VALUES.some(
    (resolutionValue) => resolutionValue === value,
  );
}

const EXPORT_RESOLUTION_LABEL_KEYS: Record<ExportResolution, TranslationKey> = {
  source: "export.resolutionSource",
  "480p": "export.resolution480p",
  "720p": "export.resolution720p",
  "1080p": "export.resolution1080p",
  "1440p": "export.resolution1440p",
  "2160p": "export.resolution2160p",
  "4320p": "export.resolution4320p",
};

const EXPORT_FORMAT_LABEL_KEYS: Record<ExportFormat, TranslationKey> = {
  mp4: "export.formatMp4",
  webm: "export.formatWebm",
};

const EXPORT_QUALITY_LABEL_KEYS: Record<ExportQuality, TranslationKey> = {
  low: "export.qualityLow",
  medium: "export.qualityMedium",
  high: "export.qualityHigh",
  very_high: "export.qualityVeryHigh",
};

export function ExportButton() {
  const { t } = useTranslation();
  const [isExportDialogOpen, setIsExportDialogOpen] = useState(false);
  const editor = useEditor();
  const activeProject = useEditor((e) => e.project.getActiveOrNull());
  const hasProject = !!activeProject;

  const handleDialogOpenChange = ({ open }: { open: boolean }) => {
    if (!open) {
      editor.project.cancelExport();
      editor.project.clearExportState();
    }
    setIsExportDialogOpen(open);
  };

  return (
    <Dialog
      open={isExportDialogOpen}
      onOpenChange={(open) => handleDialogOpenChange({ open })}
    >
      <DialogTrigger asChild>
        <button
          type="button"
          className={cn(
            "flex items-center gap-1.5 rounded-md bg-[#38BDF8] px-[0.12rem] py-[0.12rem] text-white",
            hasProject ? "cursor-pointer" : "cursor-not-allowed opacity-50",
          )}
          onClick={hasProject ? () => setIsExportDialogOpen(true) : undefined}
          disabled={!hasProject}
          onKeyDown={(event) => {
            if (hasProject && (event.key === "Enter" || event.key === " ")) {
              event.preventDefault();
              setIsExportDialogOpen(true);
            }
          }}
        >
          <div className="relative flex items-center gap-1.5 rounded-[0.6rem] bg-linear-270 from-[#2567EC] to-[#37B6F7] px-4 py-1 shadow-[0_1px_3px_0px_rgba(0,0,0,0.65)]">
            <HugeiconsIcon icon={TransitionTopIcon} className="z-50 size-3.5" />
            <span className="z-50 text-[0.875rem]">{t("export.button")}</span>
            <div className="absolute top-0 left-0 z-10 flex size-full items-center justify-center rounded-[0.6rem] bg-linear-to-t from-white/0 to-white/50">
              <div className="absolute top-[0.08rem] z-50 h-[calc(100%-2px)] w-[calc(100%-2px)] rounded-[0.6rem] bg-linear-270 from-[#2567EC] to-[#37B6F7]"></div>
            </div>
          </div>
        </button>
      </DialogTrigger>
      {hasProject && <ExportDialog onOpenChange={setIsExportDialogOpen} />}
    </Dialog>
  );
}

function ExportDialog({
  onOpenChange,
}: {
  onOpenChange: (open: boolean) => void;
}) {
  const { t, language } = useTranslation();
  const messages = EXPORT_DIALOG_MESSAGES[language];
  const editor = useEditor();
  const activeProject = useEditor((e) => e.project.getActive());
  const exportState = useEditor((e) => e.project.getExportState());
  const { isExporting, progress, result: exportResult } = exportState;
  const [format, setFormat] = useState<ExportFormat>(
    DEFAULT_EXPORT_OPTIONS.format,
  );
  const [quality, setQuality] = useState<ExportQuality>(
    DEFAULT_EXPORT_OPTIONS.quality,
  );
  const [resolution, setResolution] = useState<ExportResolution>(
    DEFAULT_EXPORT_OPTIONS.resolution,
  );
  const [shouldIncludeAudio, setShouldIncludeAudio] = useState<boolean>(
    DEFAULT_EXPORT_OPTIONS.includeAudio ?? true,
  );
  const [fpsPreset, setFpsPreset] = useState<ExportFpsPreset>("project");
  const [audioOnly, setAudioOnly] = useState(false);
  const totalDuration = useEditor((e) => e.timeline.getTotalDuration());
  const selectedSpan = useEditor((e): [MediaTime, MediaTime] | null => {
    const selected = e.timeline.getElementsWithTracks({ elements: e.selection.getSelectedElements() });
    if (selected.length === 0) return null;
    return [
      mediaTime({ ticks: Math.min(...selected.map(({ element }) => element.startTime)) }),
      mediaTime({ ticks: Math.max(...selected.map(({ element }) => addMediaTime({ a: element.startTime, b: element.duration }))) }),
    ];
  });
  const durationSeconds = mediaTimeToSeconds({ time: totalDuration });
  const [rangeMode, setRangeMode] = useState("full");
  const [rangeStart, setRangeStart] = useState("0");
  const [rangeEnd, setRangeEnd] = useState(() => String(durationSeconds));
  const [savedPresets, setSavedPresets] = useState<SavedExportPreset[]>([]);
  const [selectedPresetId, setSelectedPresetId] = useState("custom");
  const [isSavingPreset, setIsSavingPreset] = useState(false);
  const [presetName, setPresetName] = useState("");
  const [presetMessage, setPresetMessage] = useState("");

  useEffect(() => {
    setSavedPresets(loadSavedExportPresets());
  }, []);

  useEffect(() => {
    if (rangeMode !== "custom") setRangeEnd(String(durationSeconds));
  }, [durationSeconds, rangeMode]);

  const applyPreset = (preset: SavedExportPreset) => {
    setFormat(preset.format);
    setQuality(preset.quality);
    setResolution(preset.resolution);
    setShouldIncludeAudio(preset.includeAudio);
    setFpsPreset(preset.fps);
    setAudioOnly(preset.audioOnly ?? false);
    setSelectedPresetId(preset.id);
    setPresetMessage("");
  };

  const handleSavePreset = () => {
    const name = presetName.trim();
    if (!name) {
      setPresetMessage(t("export.presetNameRequired"));
      return;
    }
    try {
      const preset: SavedExportPreset = {
        id:
          globalThis.crypto?.randomUUID?.() ??
          `export-${Date.now().toString(36)}`,
        name: name.slice(0, 40),
        format,
        quality,
        resolution,
        includeAudio: shouldIncludeAudio,
        fps: fpsPreset,
        audioOnly,
      };
      setSavedPresets(saveExportPreset({ preset }));
      setSelectedPresetId(preset.id);
      setPresetName("");
      setIsSavingPreset(false);
      setPresetMessage(t("export.presetSaved"));
    } catch {
      setPresetMessage(t("export.presetSaveFailed"));
    }
  };

  const handleDeletePreset = () => {
    try {
      setSavedPresets(deleteExportPreset({ id: selectedPresetId }));
      setSelectedPresetId("custom");
      setPresetMessage(t("export.presetDeleted"));
    } catch {
      setPresetMessage(t("export.presetSaveFailed"));
    }
  };

  const markCustomSettings = () => setSelectedPresetId("custom");

  const outputCanvasSize = resolveExportCanvasSize({
    sourceSize: activeProject.settings.canvasSize,
    resolution,
  });

  let rangeError = "";
  let exportRange: { start: MediaTime; end: MediaTime } | undefined;
  let outputDuration = totalDuration;
  if (totalDuration <= 0) {
    rangeError = messages.emptyTimeline;
  } else if (rangeMode === "selection") {
    if (!selectedSpan) rangeError = messages.selectionRequired;
    else exportRange = { start: selectedSpan[0], end: selectedSpan[1] };
  } else if (rangeMode === "custom") {
    const error = validateExportRangeInput({ start: rangeStart, end: rangeEnd, duration: durationSeconds });
    if (error) {
      rangeError = { required: messages.rangeRequired, invalid: messages.rangeInvalid, order: messages.rangeOrder, outside: messages.rangeOutside }[error];
    } else {
      exportRange = { start: mediaTimeFromSeconds({ seconds: Number(rangeStart) }), end: mediaTimeFromSeconds({ seconds: Number(rangeEnd) }) };
    }
  }
  if (!rangeError) {
    try {
      outputDuration = resolveExportRange({ duration: totalDuration, range: exportRange }).duration;
    } catch {
      rangeError = messages.rangeOutside;
    }
  }

  const handleExport = async () => {
    if (!activeProject || rangeError) return;

    const result = await editor.project.export({
      options: {
        format,
        quality,
        resolution,
        fps:
          fpsPreset === "project"
            ? activeProject.settings.fps
            : floatToFrameRate(Number(fpsPreset)),
        includeAudio: audioOnly || shouldIncludeAudio,
        audioOnly,
        range: exportRange,
      },
    });

    if (result.cancelled) {
      editor.project.clearExportState();
      return;
    }

    if (result.success && result.buffer) {
      downloadBuffer({
        buffer: result.buffer,
        filename: `${activeProject.metadata.name}${audioOnly ? ".wav" : getExportFileExtension({ format })}`,
        mimeType: audioOnly ? "audio/wav" : getExportMimeType({ format }),
      });

      editor.project.clearExportState();
      onOpenChange(false);
    }
  };

  const handleCancel = () => {
    editor.project.cancelExport();
  };

  return (
    <DialogContent
      className="bg-background flex max-h-[calc(100dvh-3rem)] max-w-[26rem] flex-col overflow-hidden p-0"
      closeClassName="top-3.5 right-3.5"
    >
      {exportResult && !exportResult.success ? (
        <ExportError
          error={exportResult.error || t("export.errorFallback")}
          onRetry={handleExport}
        />
      ) : (
        <>
          <div className="flex items-center justify-between p-3 pr-14 border-b">
            <DialogTitle className="font-medium text-sm">
              {isExporting ? t("export.exportingProject") : t("export.title")}
            </DialogTitle>
          </div>

          <div className="flex min-h-0 flex-col gap-4 overflow-y-auto">
            {!isExporting && (
              <>
                <div className="flex flex-col divide-y">
                  <div className="flex items-center gap-2 px-3 py-2.5">
                    <span className="min-w-20 text-sm font-medium text-muted-foreground">
                      {t("export.presets")}
                    </span>
                    <Select
                      value={selectedPresetId}
                      onValueChange={(value) => {
                        setPresetMessage("");
                        const platformPreset = findPlatformExportPreset(value);
                        if (platformPreset) {
                          setResolution(platformPreset.resolution);
                          setQuality(platformPreset.quality);
                          setFpsPreset(platformPreset.fps);
                          setSelectedPresetId(platformPreset.id);
                          return;
                        }
                        const preset = savedPresets.find(
                          (item) => item.id === value,
                        );
                        if (preset) applyPreset(preset);
                        else if (value === "custom") setSelectedPresetId(value);
                      }}
                    >
                      <SelectTrigger className="h-8 min-w-0 flex-1 justify-between bg-accent">
                        <SelectValue />
                      </SelectTrigger>
                      <SelectContent className="min-w-56">
                        <SelectItem value="custom">
                          {t("export.customSettings")}
                        </SelectItem>
                        {!audioOnly && PLATFORM_EXPORT_PRESETS.map((preset) => (
                          <SelectItem key={preset.id} value={preset.id}>
                            {preset.name}
                          </SelectItem>
                        ))}
                        {savedPresets.map((preset) => (
                          <SelectItem key={preset.id} value={preset.id}>
                            {preset.name}
                          </SelectItem>
                        ))}
                      </SelectContent>
                    </Select>
                    <Button
                      variant="text"
                      size="icon"
                      className="size-8 shrink-0"
                      aria-label={t("export.savePreset")}
                      title={t("export.savePreset")}
                      disabled={savedPresets.length >= 20}
                      onClick={() => {
                        setIsSavingPreset((current) => !current);
                        setPresetMessage("");
                      }}
                    >
                      <BookmarkPlus className="size-4" />
                    </Button>
                    {savedPresets.some(
                      (preset) => preset.id === selectedPresetId,
                    ) && (
                      <Button
                        variant="text"
                        size="icon"
                        className="text-muted-foreground hover:text-destructive size-8 shrink-0"
                        aria-label={t("export.deletePreset")}
                        title={t("export.deletePreset")}
                        onClick={handleDeletePreset}
                      >
                        <Trash2 className="size-4" />
                      </Button>
                    )}
                  </div>
                  {isSavingPreset && (
                    <div className="flex items-center gap-2 px-3 pb-2.5">
                      <Input
                        autoFocus
                        size="sm"
                        value={presetName}
                        maxLength={40}
                        placeholder={t("export.presetNamePlaceholder")}
                        onChange={(event) =>
                          setPresetName(event.currentTarget.value)
                        }
                        onKeyDown={(event) => {
                          if (event.key === "Enter") handleSavePreset();
                          if (event.key === "Escape") setIsSavingPreset(false);
                        }}
                      />
                      <Button size="sm" onClick={handleSavePreset}>
                        {t("common.save")}
                      </Button>
                      <Button
                        variant="outline"
                        size="sm"
                        onClick={() => setIsSavingPreset(false)}
                      >
                        {t("common.cancel")}
                      </Button>
                    </div>
                  )}
                  {presetMessage && (
                    <div
                      className="px-3 pb-2 text-xs text-muted-foreground"
                      role="status"
                    >
                      {presetMessage}
                    </div>
                  )}
                  {findPlatformExportPreset(selectedPresetId) && (
                    <p className="px-3 pb-2 text-xs text-muted-foreground">{messages.keepAspect}</p>
                  )}
                  <ExportSelectRow
                    label={messages.output}
                    value={audioOnly ? "audio" : "video"}
                    onValueChange={(value) => { setAudioOnly(value === "audio"); markCustomSettings(); }}
                    items={[{ value: "video", label: messages.video }, { value: "audio", label: messages.audioOnly }]}
                  />
                  <ExportSelectRow
                    label={messages.range}
                    value={rangeMode}
                    onValueChange={setRangeMode}
                    items={[
                      { value: "full", label: messages.full },
                      { value: "selection", label: messages.selection },
                      { value: "custom", label: messages.custom },
                    ]}
                  />
                  {rangeMode === "selection" && (
                    <div className="space-y-1 px-3 py-2 text-xs text-muted-foreground">
                      <p>{messages.selectionHelp}</p>
                      {selectedSpan && <p>{mediaTimeToSeconds({ time: selectedSpan[0] }).toFixed(3)} → {mediaTimeToSeconds({ time: selectedSpan[1] }).toFixed(3)} {messages.seconds}</p>}
                    </div>
                  )}
                  {rangeMode === "custom" && (
                    <div className="space-y-2 px-3 py-2.5">
                      <p className="text-xs text-muted-foreground">{messages.timelineDuration}: {durationSeconds.toFixed(3)} {messages.seconds}</p>
                      <div className="grid grid-cols-2 gap-2">
                        <label className="space-y-1 text-xs text-muted-foreground">
                          <span>{messages.inPoint}</span>
                          <Input type="number" min="0" max={durationSeconds} step="0.001" size="sm" value={rangeStart} onChange={(event) => setRangeStart(event.currentTarget.value)} />
                        </label>
                        <label className="space-y-1 text-xs text-muted-foreground">
                          <span>{messages.outPoint}</span>
                          <Input type="number" min="0" max={durationSeconds} step="0.001" size="sm" value={rangeEnd} onChange={(event) => setRangeEnd(event.currentTarget.value)} />
                        </label>
                        <Button variant="outline" size="sm" onClick={() => setRangeStart(String(mediaTimeToSeconds({ time: editor.playback.getCurrentTime() })))}>{messages.setIn}</Button>
                        <Button variant="outline" size="sm" onClick={() => setRangeEnd(String(mediaTimeToSeconds({ time: editor.playback.getCurrentTime() })))}>{messages.setOut}</Button>
                      </div>
                    </div>
                  )}
                  {!audioOnly && <>
                  <ExportSelectRow
                    label={t("export.resolution")}
                    value={resolution}
                    onValueChange={(value) => {
                      if (isExportResolution(value)) {
                        setResolution(value);
                        markCustomSettings();
                      }
                    }}
                    items={EXPORT_RESOLUTION_VALUES.map((value) => ({
                      value,
                      label: t(EXPORT_RESOLUTION_LABEL_KEYS[value]),
                    }))}
                  />
                  <ExportSelectRow
                    label={t("export.format")}
                    value={format}
                    onValueChange={(value) => {
                      if (isExportFormat(value)) {
                        setFormat(value);
                        markCustomSettings();
                      }
                    }}
                    items={EXPORT_FORMAT_VALUES.map((value) => ({
                      value,
                      label: t(EXPORT_FORMAT_LABEL_KEYS[value]),
                    }))}
                  />
                  <ExportSelectRow
                    label={t("export.quality")}
                    value={quality}
                    onValueChange={(value) => {
                      if (isExportQuality(value)) {
                        setQuality(value);
                        markCustomSettings();
                      }
                    }}
                    items={EXPORT_QUALITY_VALUES.map((value) => ({
                      value,
                      label: t(EXPORT_QUALITY_LABEL_KEYS[value]),
                    }))}
                  />
                  <ExportSelectRow
                    label={t("export.audio")}
                    value={shouldIncludeAudio ? "include" : "mute"}
                    onValueChange={(value) => {
                      setShouldIncludeAudio(value === "include");
                      markCustomSettings();
                    }}
                    items={[
                      { value: "include", label: t("export.audioInclude") },
                      { value: "mute", label: t("export.audioMute") },
                    ]}
                  />
                  <ExportSelectRow
                    label={t("export.frameRate")}
                    value={fpsPreset}
                    onValueChange={(value) => {
                      if (isExportFpsPreset(value)) {
                        setFpsPreset(value);
                        markCustomSettings();
                      }
                    }}
                    items={EXPORT_FPS_PRESETS.map((value) => ({
                      value,
                      label:
                        value === "project"
                          ? t("export.frameRateProject", {
                              fps:
                                activeProject.settings.fps.numerator /
                                activeProject.settings.fps.denominator,
                            })
                          : `${value} fps`,
                    }))}
                  />
                  <div className="px-3 py-2 text-xs text-muted-foreground">
                    {t("export.outputSize", {
                      width: outputCanvasSize.width,
                      height: outputCanvasSize.height,
                    })}
                  </div>
                  </>}
                  {audioOnly && <p className="px-3 py-2 text-xs text-muted-foreground">{messages.audioOutput}</p>}
                  {!rangeError && (
                    <p className="px-3 py-2 text-xs text-muted-foreground">{messages.duration}: {mediaTimeToSeconds({ time: outputDuration }).toFixed(3)} {messages.seconds}</p>
                  )}
                  {rangeError && <p className="px-3 py-2 text-xs text-destructive" role="alert">{rangeError}</p>}
                </div>

                <div className="p-3 pt-0">
                  <Button onClick={handleExport} disabled={Boolean(rangeError)} className="w-full gap-2">
                    <Download className="size-4" />
                    {t("export.button")}
                  </Button>
                </div>
              </>
            )}

            {isExporting && (
              <div className="space-y-4 p-3">
                <div className="flex flex-col gap-2">
                  <div className="flex items-center justify-between text-center">
                    <p className="text-muted-foreground text-sm">
                      {Math.round(progress * 100)}%
                    </p>
                    <p className="text-muted-foreground text-sm">100%</p>
                  </div>
                  <Progress value={progress * 100} className="w-full" />
                </div>

                <Button
                  variant="outline"
                  className="w-full rounded-md"
                  onClick={handleCancel}
                >
                  {t("export.cancel")}
                </Button>
              </div>
            )}
          </div>
        </>
      )}
    </DialogContent>
  );
}

function ExportSelectRow({
  label,
  value,
  onValueChange,
  items,
}: {
  label: string;
  value: string;
  onValueChange: (value: string) => void;
  items: Array<{ value: string; label: string }>;
}) {
  return (
    <div className="flex items-center justify-between gap-3 px-3 py-2.5">
      <span className="text-sm font-medium text-muted-foreground">{label}</span>
      <Select value={value} onValueChange={onValueChange}>
        <SelectTrigger className="h-8 min-w-44 justify-between bg-accent">
          <SelectValue />
        </SelectTrigger>
        <SelectContent className="min-w-56">
          {items.map((item) => (
            <SelectItem key={item.value} value={item.value}>
              {item.label}
            </SelectItem>
          ))}
        </SelectContent>
      </Select>
    </div>
  );
}

function ExportError({
  error,
  onRetry,
}: {
  error: string;
  onRetry: () => void;
}) {
  const { t } = useTranslation();
  const [copied, setCopied] = useState(false);

  const handleCopy = async () => {
    await navigator.clipboard.writeText(error);
    setCopied(true);
    setTimeout(() => setCopied(false), 1000);
  };

  return (
    <div className="space-y-4 p-3">
      <div className="flex flex-col gap-1.5">
        <p className="text-destructive text-sm font-medium">
          {t("export.exportFailed")}
        </p>
        <p className="text-muted-foreground text-xs">{error}</p>
      </div>

      <div className="flex gap-2">
        <Button
          variant="outline"
          size="sm"
          className="h-8 flex-1 text-xs"
          onClick={handleCopy}
        >
          {copied ? <Check className="text-constructive" /> : <Copy />}
          {t("export.copyError")}
        </Button>
        <Button
          variant="outline"
          size="sm"
          className="h-8 flex-1 text-xs"
          onClick={onRetry}
        >
          <RotateCcw />
          {t("common.retry")}
        </Button>
      </div>
    </div>
  );
}
