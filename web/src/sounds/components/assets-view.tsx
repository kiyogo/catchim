"use client";

import { useEffect, useRef, useState } from "react";
import { Button } from "@/components/ui/button";
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogFooter,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from "@/components/ui/dialog";
import {
  DropdownMenu,
  DropdownMenuCheckboxItem,
  DropdownMenuContent,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu";
import { Input } from "@/components/ui/input";
import { ScrollArea } from "@/components/ui/scroll-area";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { useInfiniteScroll } from "@/hooks/use-infinite-scroll";
import { useSoundSearch } from "@/sounds/use-sound-search";
import { useSoundsStore } from "@/sounds/sounds-store";
import type { SavedSound, SoundEffect } from "@/sounds/types";
import { cn } from "@/utils/ui";
import { useEditor } from "@/editor/use-editor";
import { insertVoiceoverRecording } from "@/services/voiceover/insert-recording";
import {
  FavouriteIcon,
  FilterMailIcon,
  PauseIcon,
  PlayIcon,
  PlusSignIcon,
} from "@hugeicons/core-free-icons";
import { HugeiconsIcon } from "@hugeicons/react";
import { useTranslation } from "@/i18n";
import { Mic, Mic2, Square } from "lucide-react";

type VoiceoverStatus = "idle" | "requesting" | "recording" | "saving";

export function SoundsView() {
  const { t } = useTranslation();
  const [activeTab, setActiveTab] = useState("sound-effects");

  return (
    <div className="flex h-full flex-col py-2">
      <Tabs
        value={activeTab}
        onValueChange={setActiveTab}
        variant="underline"
        className="flex min-h-0 flex-1 flex-col"
      >
        {activeTab !== "voiceover" && <SoundSearchControls />}
        <TabsList aria-label={t("assets.sounds")}>
          <TabsTrigger value="sound-effects">
            {t("assets.soundEffects")}
          </TabsTrigger>
          <TabsTrigger value="saved">{t("assets.saved")}</TabsTrigger>
          <TabsTrigger value="voiceover">{t("assets.voiceover")}</TabsTrigger>
        </TabsList>
        <TabsContent
          value="sound-effects"
          className="mt-0 flex min-h-0 flex-1 flex-col pt-4"
        >
          <SoundEffectsView />
        </TabsContent>
        <TabsContent
          value="saved"
          className="mt-0 flex min-h-0 flex-1 flex-col pt-4"
        >
          <SavedSoundsView />
        </TabsContent>
        <TabsContent
          value="voiceover"
					forceMount
					className="data-[state=inactive]:hidden mt-0 flex min-h-0 flex-1 flex-col pt-4"
        >
          <VoiceoverView />
        </TabsContent>
      </Tabs>
    </div>
  );
}

function VoiceoverView() {
  const { t } = useTranslation();
  const editor = useEditor();
  const project = editor.project.getActiveOrNull();
  const scene = editor.scenes.getActiveSceneOrNull();
  const [status, setStatus] = useState<VoiceoverStatus>("idle");
  const [devices, setDevices] = useState<MediaDeviceInfo[]>([]);
  const [selectedDeviceId, setSelectedDeviceId] = useState("");
  const [elapsedMs, setElapsedMs] = useState(0);
  const [message, setMessage] = useState("");
  const recorderRef = useRef<MediaRecorder | null>(null);
  const streamRef = useRef<MediaStream | null>(null);
  const chunksRef = useRef<Blob[]>([]);
  const startedAtRef = useRef(0);
  const cancelledRef = useRef(false);
  const recordingContextRef = useRef<{
    projectId: string;
    sceneId: string;
    startTime: ReturnType<typeof editor.playback.getCurrentTime>;
  } | null>(null);

  const refreshDevices = async () => {
    if (!navigator.mediaDevices?.enumerateDevices) return;
    try {
      const available = await navigator.mediaDevices.enumerateDevices();
      const audioInputs = available.filter(
        (device) => device.kind === "audioinput",
      );
      setDevices(audioInputs);
      setSelectedDeviceId((current) =>
        current && audioInputs.some((device) => device.deviceId === current)
          ? current
          : (audioInputs[0]?.deviceId ?? ""),
      );
    } catch (error) {
      console.warn("Could not list microphone inputs:", error);
    }
  };

  useEffect(() => {
    void refreshDevices();
    return () => {
      cancelledRef.current = true;
      const recorder = recorderRef.current;
      if (recorder && recorder.state !== "inactive") recorder.stop();
      streamRef.current?.getTracks().forEach((track) => track.stop());
    };
  }, []);

  useEffect(() => {
    if (status !== "recording") return;
    const timer = window.setInterval(() => {
      setElapsedMs(performance.now() - startedAtRef.current);
    }, 200);
    return () => window.clearInterval(timer);
  }, [status]);

  const startRecording = async () => {
    setMessage("");
    if (!project?.metadata.id || !scene?.id) {
      setMessage(t("assets.voiceoverNoProject"));
      return;
    }
    if (
      !navigator.mediaDevices?.getUserMedia ||
      typeof MediaRecorder === "undefined"
    ) {
      setMessage(t("assets.voiceoverUnsupported"));
      return;
    }

    setStatus("requesting");
    cancelledRef.current = false;
    editor.playback.pause();
    const startTime = editor.playback.getCurrentTime();
    const projectId = project.metadata.id;
    const sceneId = scene.id;
    let stream: MediaStream | null = null;

    try {
      stream = await navigator.mediaDevices.getUserMedia({
        audio: selectedDeviceId
          ? {
              deviceId: { exact: selectedDeviceId },
              echoCancellation: true,
              noiseSuppression: true,
            }
          : { echoCancellation: true, noiseSuppression: true },
      });
      if (cancelledRef.current) {
        stream.getTracks().forEach((track) => track.stop());
        return;
      }
      streamRef.current = stream;
      await refreshDevices();
      const mimeType = [
        "audio/webm;codecs=opus",
        "audio/webm",
        "audio/mp4",
        "audio/ogg;codecs=opus",
      ].find((candidate) => MediaRecorder.isTypeSupported(candidate));
      const recorder = mimeType
        ? new MediaRecorder(stream, { mimeType })
        : new MediaRecorder(stream);
      chunksRef.current = [];
      recordingContextRef.current = { projectId, sceneId, startTime };
      startedAtRef.current = performance.now();
      setElapsedMs(0);
      recorder.addEventListener("dataavailable", (event) => {
        if (event.data.size > 0) chunksRef.current.push(event.data);
      });
      recorder.addEventListener("stop", () => {
        stream?.getTracks().forEach((track) => track.stop());
        if (streamRef.current === stream) streamRef.current = null;
        if (cancelledRef.current) return;
        void saveRecording({ recorder });
      });
      recorderRef.current = recorder;
      recorder.start(250);
      setStatus("recording");
    } catch (error) {
      stream?.getTracks().forEach((track) => track.stop());
      streamRef.current = null;
      setStatus("idle");
      const errorName = error instanceof DOMException ? error.name : "";
      setMessage(
        errorName === "NotAllowedError" || errorName === "SecurityError"
          ? t("assets.voiceoverPermissionDenied")
          : errorName === "NotFoundError"
            ? t("assets.voiceoverNoMicrophone")
            : error instanceof Error
              ? error.message
              : t("assets.voiceoverError"),
      );
    }
  };

  const saveRecording = async ({ recorder }: { recorder: MediaRecorder }) => {
    const context = recordingContextRef.current;
    const durationSeconds = (performance.now() - startedAtRef.current) / 1000;
    const blob = new Blob(chunksRef.current, {
      type: recorder.mimeType || "audio/webm",
    });
    chunksRef.current = [];
    if (!context || blob.size === 0 || durationSeconds < 0.1) {
      setStatus("idle");
      setMessage(t("assets.voiceoverEmpty"));
      return;
    }

    setStatus("saving");
    try {
      const stamp = new Date().toISOString().replace(/[:.]/g, "-");
      await insertVoiceoverRecording({
        editor,
        ...context,
        blob,
        durationSeconds,
        name: `[Voiceover] ${stamp}`,
      });
      setMessage(t("assets.voiceoverSaved"));
    } catch (error) {
      setMessage(
        error instanceof Error ? error.message : t("assets.voiceoverError"),
      );
    } finally {
      setStatus("idle");
      recorderRef.current = null;
    }
  };

  const stopRecording = () => {
    if (recorderRef.current?.state === "recording") {
      setStatus("saving");
      recorderRef.current.stop();
    }
  };

  const statusLabel =
    status === "requesting"
      ? t("assets.voiceoverRequesting")
      : status === "recording"
        ? t("assets.voiceoverRecording")
        : status === "saving"
          ? t("assets.voiceoverSaving")
          : t("assets.voiceoverHint");
  const elapsedLabel = `${String(Math.floor(elapsedMs / 60_000)).padStart(2, "0")}:${String(Math.floor((elapsedMs % 60_000) / 1000)).padStart(2, "0")}`;

  return (
    <div className="flex flex-1 flex-col gap-5 overflow-y-auto px-2 pb-4">
      <div className="flex flex-col items-center gap-4 rounded-xl border border-border bg-card p-5 text-center">
        <div className="flex size-14 items-center justify-center rounded-full bg-primary/10 text-primary">
          <Mic2 className="size-7" aria-hidden="true" />
        </div>
        <div className="flex flex-col gap-1">
          <h3 className="text-base font-semibold">
            {t("assets.voiceoverTitle")}
          </h3>
          <p className="text-muted-foreground text-sm">{statusLabel}</p>
        </div>
        <div
          className={cn(
            "font-mono text-2xl tabular-nums",
            status === "recording" && "text-destructive",
          )}
          aria-live="polite"
        >
          {elapsedLabel}
        </div>
        <label className="flex w-full flex-col gap-2 text-left text-sm">
          <span className="text-muted-foreground">
            {t("assets.voiceoverMicrophone")}
          </span>
          <select
            className="border-input bg-background h-9 w-full rounded-md border px-3 text-sm"
            value={selectedDeviceId}
            disabled={status !== "idle" || devices.length === 0}
            onChange={(event) => setSelectedDeviceId(event.currentTarget.value)}
          >
            {devices.length === 0 && (
              <option value="">{t("assets.voiceoverDefaultMicrophone")}</option>
            )}
            {devices.map((device, index) => (
              <option key={device.deviceId || index} value={device.deviceId}>
                {device.label ||
                  `${t("assets.voiceoverMicrophone")} ${index + 1}`}
              </option>
            ))}
          </select>
        </label>
        {status === "recording" ? (
          <Button
            variant="destructive"
            className="w-full"
            onClick={stopRecording}
          >
            <Square className="size-4 fill-current" aria-hidden="true" />
            {t("assets.voiceoverStopAndInsert")}
          </Button>
        ) : (
          <Button
            className="w-full"
            onClick={startRecording}
            disabled={status !== "idle"}
          >
            <Mic className="size-4" aria-hidden="true" />
            {status === "saving" || status === "requesting"
              ? statusLabel
              : t("assets.voiceoverStart")}
          </Button>
        )}
        {message && (
          <p className="text-muted-foreground text-sm" role="status">
            {message}
          </p>
        )}
      </div>
      <p className="text-muted-foreground text-xs leading-relaxed">
        {t("assets.voiceoverPrivacyHint")}
      </p>
    </div>
  );
}

function SoundSearchControls() {
  const { t } = useTranslation();
  const {
    searchQuery,
    setSearchQuery,
    showCommercialOnly,
    toggleCommercialFilter,
  } = useSoundsStore();

  return (
    <div className="px-2">
      <div className="flex items-center gap-2">
        <Input
          size="sm"
          variant="default"
          placeholder={t("assets.search")}
          className="w-full"
          containerClassName="w-full"
          value={searchQuery}
          onChange={({ currentTarget }) =>
            setSearchQuery({ query: currentTarget.value })
          }
          showClearIcon
          onClear={() => setSearchQuery({ query: "" })}
        />
        <DropdownMenu>
          <DropdownMenuTrigger asChild>
            <Button
              variant="text"
              size="icon"
              className={cn(showCommercialOnly && "text-primary")}
            >
              <HugeiconsIcon icon={FilterMailIcon} />
            </Button>
          </DropdownMenuTrigger>
          <DropdownMenuContent align="end" className="w-56">
            <DropdownMenuCheckboxItem
              checked={showCommercialOnly}
              onCheckedChange={() => toggleCommercialFilter()}
            >
              {t("assets.showCommercialSoundsOnly")}
            </DropdownMenuCheckboxItem>
            <div className="text-muted-foreground px-2 py-1.5 text-xs">
              {showCommercialOnly
                ? t("assets.showingCommercialSoundsOnly")
                : t("assets.showAllSoundsLicense")}
            </div>
          </DropdownMenuContent>
        </DropdownMenu>
      </div>
    </div>
  );
}

function SoundEffectsView() {
  const { t } = useTranslation();
  const {
    topSoundEffects,
    isLoading,
    searchQuery,
    scrollPosition,
    setScrollPosition,
    loadSavedSounds,
    showCommercialOnly,
    hasLoaded,
    setTopSoundEffects,
    setLoading,
    setError,
    setHasLoaded,
    setCurrentPage,
    setHasNextPage,
    setTotalCount,
  } = useSoundsStore();
  const {
    results: searchResults,
    isLoading: isSearching,
    loadMore,
    hasNextPage,
    isLoadingMore,
  } = useSoundSearch({
    query: searchQuery,
    commercialOnly: showCommercialOnly,
  });

  const [playingId, setPlayingId] = useState<number | null>(null);
  const [audioElement, setAudioElement] = useState<HTMLAudioElement | null>(
    null,
  );

  const { scrollAreaRef, handleScroll } = useInfiniteScroll({
    onLoadMore: loadMore,
    hasMore: hasNextPage,
    isLoading: isLoadingMore || isSearching,
  });

  useEffect(() => {
    loadSavedSounds();
  }, [loadSavedSounds]);

  useEffect(() => {
    if (hasLoaded) {
      return;
    }

    let shouldIgnore = false;

    const fetchTopSounds = async () => {
      try {
        if (!shouldIgnore) {
          setLoading({ loading: true });
          setError({ error: null });
        }

        const response = await fetch(
          "/api/sounds/search?page_size=50&sort=downloads",
        );

        if (!shouldIgnore) {
          if (!response.ok) {
            const data = await response.json().catch(() => null);
            setError({
              error:
                data?.error ||
                data?.message ||
                `Failed to load sounds (${response.status})`,
            });
            setHasLoaded({ loaded: true });
            return;
          }

          const data = await response.json();
          setTopSoundEffects({ sounds: data.results });
          setHasLoaded({ loaded: true });

          setCurrentPage({ page: 1 });
          setHasNextPage({ hasNext: !!data.next });
          setTotalCount({ count: data.count });
        }
      } catch (error) {
        if (!shouldIgnore) {
          setError({
            error:
              error instanceof Error ? error.message : "Failed to load sounds",
          });
          setHasLoaded({ loaded: true });
        }
      } finally {
        if (!shouldIgnore) {
          setLoading({ loading: false });
        }
      }
    };

    const timeoutId = setTimeout(fetchTopSounds, 100, {});

    return () => {
      shouldIgnore = true;
      clearTimeout(timeoutId);
    };
  }, [
    hasLoaded,
    setTopSoundEffects,
    setLoading,
    setError,
    setHasLoaded,
    setCurrentPage,
    setHasNextPage,
    setTotalCount,
  ]);

  useEffect(() => {
    if (!scrollAreaRef.current || scrollPosition <= 0) {
      return;
    }

    const restoreScrollPosition = () => {
      scrollAreaRef.current?.scrollTo({ top: scrollPosition });
    };

    const timeoutId = setTimeout(restoreScrollPosition, 100, {});

    return () => clearTimeout(timeoutId);
  }, [scrollPosition, scrollAreaRef]);

  const handleScrollWithPosition = (event: React.UIEvent<HTMLDivElement>) => {
    const { currentTarget } = event;
    const { scrollTop } = currentTarget;
    setScrollPosition({ position: scrollTop });
    handleScroll(event);
  };

  const displayedSounds = searchQuery ? searchResults : topSoundEffects;

  const playSound = ({ sound }: { sound: SoundEffect }) => {
    if (playingId === sound.id) {
      audioElement?.pause();
      setPlayingId(null);
      return;
    }

    audioElement?.pause();

    if (sound.previewUrl) {
      const audio = new Audio(sound.previewUrl);
      audio.addEventListener("ended", () => {
        setPlayingId(null);
      });
      audio.addEventListener("error", () => {
        setPlayingId(null);
      });
      audio.play().catch((error) => {
        console.error("Failed to play sound preview:", error);
        setPlayingId(null);
      });

      setAudioElement(audio);
      setPlayingId(sound.id);
    }
  };

  return (
    <div className="flex h-full flex-col">
      <div className="relative h-full overflow-hidden">
        <ScrollArea
          className="h-full flex-1"
          ref={scrollAreaRef}
          onScrollCapture={handleScrollWithPosition}
        >
          <div className="flex flex-col gap-4">
            {isLoading && !searchQuery && (
              <div className="text-muted-foreground text-sm">
                {t("assets.loadingSounds")}
              </div>
            )}
            {isSearching && searchQuery && (
              <div className="text-muted-foreground text-sm">
                {t("assets.searching")}
              </div>
            )}
            {displayedSounds.map((sound) => (
              <AudioItem
                key={sound.id}
                sound={sound}
                isPlaying={playingId === sound.id}
                onPlay={playSound}
              />
            ))}
            {!isLoading && !isSearching && displayedSounds.length === 0 && (
              <div className="text-muted-foreground text-sm">
                {searchQuery
                  ? t("assets.noSoundsFound")
                  : t("assets.noSoundsAvailable")}
              </div>
            )}
            {isLoadingMore && (
              <div className="text-muted-foreground py-4 text-center text-sm">
                {t("assets.loadingMoreSounds")}
              </div>
            )}
          </div>
        </ScrollArea>
      </div>
    </div>
  );
}

function SavedSoundsView() {
  const { t } = useTranslation();
  const {
    savedSounds,
    isLoadingSavedSounds,
    savedSoundsError,
    loadSavedSounds,
    clearSavedSounds,
  } = useSoundsStore();

  const [playingId, setPlayingId] = useState<number | null>(null);
  const [audioElement, setAudioElement] = useState<HTMLAudioElement | null>(
    null,
  );

  const [showClearDialog, setShowClearDialog] = useState(false);

  useEffect(() => {
    loadSavedSounds();
  }, [loadSavedSounds]);

  const playSound = ({ sound }: { sound: SoundEffect }) => {
    if (playingId === sound.id) {
      audioElement?.pause();
      setPlayingId(null);
      return;
    }

    audioElement?.pause();

    if (sound.previewUrl) {
      const audio = new Audio(sound.previewUrl);
      audio.addEventListener("ended", () => {
        setPlayingId(null);
      });
      audio.addEventListener("error", () => {
        setPlayingId(null);
      });
      audio.play().catch((error) => {
        console.error("Failed to play sound preview:", error);
        setPlayingId(null);
      });

      setAudioElement(audio);
      setPlayingId(sound.id);
    }
  };

  const convertToSoundEffect = ({
    savedSound,
  }: {
    savedSound: SavedSound;
  }): SoundEffect => ({
    id: savedSound.id,
    name: savedSound.name,
    description: "",
    url: "",
    previewUrl: savedSound.previewUrl,
    downloadUrl: savedSound.downloadUrl,
    duration: savedSound.duration,
    filesize: 0,
    type: "audio",
    channels: 0,
    bitrate: 0,
    bitdepth: 0,
    samplerate: 0,
    username: savedSound.username,
    tags: savedSound.tags,
    license: savedSound.license,
    created: savedSound.savedAt,
    downloads: 0,
    rating: 0,
    ratingCount: 0,
  });

  if (isLoadingSavedSounds) {
    return (
      <div className="flex h-full items-center justify-center">
        <div className="text-muted-foreground text-sm">
          {t("assets.loadingSavedSounds")}
        </div>
      </div>
    );
  }

  if (savedSoundsError) {
    return (
      <div className="flex h-full items-center justify-center">
        <div className="text-destructive text-sm">
          {t("assets.error")}: {savedSoundsError}
        </div>
      </div>
    );
  }

  if (savedSounds.length === 0) {
    return (
      <div className="bg-background flex h-full flex-col items-center justify-center gap-3 p-4">
        <HugeiconsIcon
          icon={FavouriteIcon}
          className="text-muted-foreground size-10"
        />
        <div className="flex flex-col gap-2 text-center">
          <p className="text-lg font-medium">{t("assets.noSavedSounds")}</p>
          <p className="text-muted-foreground text-sm text-balance">
            {t("assets.saveSoundHint")}
          </p>
        </div>
      </div>
    );
  }

  return (
    <div className="mt-1 flex h-full flex-col gap-5">
      <div className="flex items-center justify-between">
        <p className="text-muted-foreground text-sm">
          {t("assets.savedSoundCount", {
            count: savedSounds.length,
            soundLabel: savedSounds.length === 1 ? "sound" : "sounds",
          })}
        </p>
        <Dialog open={showClearDialog} onOpenChange={setShowClearDialog}>
          <DialogTrigger asChild>
            <Button
              variant="text"
              size="sm"
              className="text-muted-foreground hover:text-destructive h-auto !opacity-100"
            >
              {t("assets.clearAll")}
            </Button>
          </DialogTrigger>
          <DialogContent>
            <DialogHeader>
              <DialogTitle>{t("assets.clearAllSavedSounds")}</DialogTitle>
              <DialogDescription>
                {t("assets.clearAllSavedSoundsDescription", {
                  count: savedSounds.length,
                })}
              </DialogDescription>
            </DialogHeader>
            <DialogFooter>
              <Button variant="text" onClick={() => setShowClearDialog(false)}>
                {t("common.cancel")}
              </Button>
              <Button
                variant="destructive"
                onClick={async ({
                  stopPropagation,
                }: React.MouseEvent<HTMLButtonElement>) => {
                  stopPropagation();
                  await clearSavedSounds();
                  setShowClearDialog(false);
                }}
              >
                {t("assets.clearAll")}
              </Button>
            </DialogFooter>
          </DialogContent>
        </Dialog>
      </div>

      <div className="relative h-full overflow-hidden">
        <ScrollArea className="h-full flex-1">
          <div className="flex flex-col gap-4">
            {savedSounds.map((sound) => (
              <AudioItem
                key={sound.id}
                sound={convertToSoundEffect({ savedSound: sound })}
                isPlaying={playingId === sound.id}
                onPlay={playSound}
              />
            ))}
          </div>
        </ScrollArea>
      </div>
    </div>
  );
}

interface AudioItemProps {
  sound: SoundEffect;
  isPlaying: boolean;
  onPlay: ({ sound }: { sound: SoundEffect }) => void;
}

function AudioItem({ sound, isPlaying, onPlay }: AudioItemProps) {
  const { t } = useTranslation();
  const { addSoundToTimeline, isSoundSaved, toggleSavedSound } =
    useSoundsStore();
  const isSaved = isSoundSaved({ soundId: sound.id });

  const handleClick = () => {
    onPlay({ sound });
  };

  const handleSaveClick = ({
    stopPropagation,
  }: React.MouseEvent<HTMLButtonElement>) => {
    stopPropagation();
    toggleSavedSound({ soundEffect: sound });
  };

  const handleAddToTimeline = async ({
    stopPropagation,
  }: React.MouseEvent<HTMLButtonElement>) => {
    stopPropagation();
    await addSoundToTimeline({ sound });
  };

  return (
    <div className="group flex items-center gap-3 opacity-100 hover:opacity-75">
      <button
        type="button"
        className="flex min-w-0 flex-1 items-center gap-3 text-left"
        onClick={handleClick}
      >
        <div className="bg-accent relative flex size-12 shrink-0 items-center justify-center overflow-hidden rounded-md">
          <div className="from-primary/20 absolute inset-0 bg-gradient-to-br to-transparent" />
          {isPlaying ? (
            <HugeiconsIcon icon={PauseIcon} className="size-5" />
          ) : (
            <HugeiconsIcon icon={PlayIcon} className="size-5" />
          )}
        </div>

        <div className="min-w-0 flex-1 overflow-hidden">
          <p className="truncate text-sm font-medium">{sound.name}</p>
          <span className="text-muted-foreground block truncate text-xs">
            {sound.username}
          </span>
        </div>
      </button>

      <div className="flex items-center gap-3 pr-2">
        <Button
          variant="text"
          size="icon"
          className="text-muted-foreground hover:text-foreground w-auto !opacity-100"
          onClick={handleAddToTimeline}
          title={t("assets.addToTimeline")}
        >
          <HugeiconsIcon icon={PlusSignIcon} />
        </Button>
        <Button
          variant="text"
          size="icon"
          className={`hover:text-foreground w-auto !opacity-100 ${
            isSaved
              ? "text-red-500 hover:text-red-600"
              : "text-muted-foreground"
          }`}
          onClick={handleSaveClick}
          title={isSaved ? t("assets.removeFromSaved") : t("assets.saveSound")}
        >
          <HugeiconsIcon
            icon={FavouriteIcon}
            className={`${isSaved ? "fill-current" : ""}`}
          />
        </Button>
      </div>
    </div>
  );
}
