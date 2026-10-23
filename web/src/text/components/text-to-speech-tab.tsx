"use client";

import { useState, useEffect, useMemo, useTransition } from "react";
import type { TextElement } from "@/timeline";
import { useEditor } from "@/editor/use-editor";
import { TTS_VOICES } from "@/services/tts/voices";
import { TtsService } from "@/services/tts/tts-service";
import type { TtsVoice, VoiceCategory } from "@/services/tts/types";
import { insertGeneratedSpeech } from "@/services/tts/insert-generated-speech";
import { Button } from "@/components/ui/button";
import { Label } from "@/components/ui/label";
import { Slider } from "@/components/ui/slider";
import { Textarea } from "@/components/ui/textarea";
import { toast } from "sonner";
import { cn } from "@/utils/ui";
import { useTranslation } from "@/i18n";
import { HugeiconsIcon } from "@hugeicons/react";
import {
	PlayIcon,
	PauseIcon,
	Tick02Icon,
	Loading03Icon,
} from "@hugeicons/core-free-icons";

const CATEGORIES: { id: VoiceCategory; labelKey: string; icon: string }[] = [
	{ id: "all", labelKey: "tts.catAll", icon: "🌐" },
	{ id: "trending", labelKey: "tts.catTrending", icon: "🔥" },
	{ id: "story", labelKey: "tts.catStory", icon: "📖" },
];

export function TextToSpeechTab({
	element,
	trackId,
}: {
	element: TextElement;
	trackId: string;
}) {
	const { t } = useTranslation();
	const editor = useEditor();
	const [, startTransition] = useTransition();

	const [text, setText] = useState<string>(String(element.params.content ?? ""));
	const [selectedCategory, setSelectedCategory] = useState<VoiceCategory>("all");
	const [selectedVoiceId, setSelectedVoiceId] = useState<string>("vi-female-sweet");
	const [previewVoiceId, setPreviewVoiceId] = useState<string | null>(null);
	const [isGenerating, setIsGenerating] = useState(false);
	const [speed, setSpeed] = useState<number>(1.0);
	const [pitch, setPitch] = useState<number>(0);

	// Sync text when element content changes externally
	useEffect(() => {
		setText(String(element.params.content ?? ""));
	}, [element.params.content]);

	// Cleanup preview audio on unmount
	useEffect(() => {
		return () => {
			TtsService.stopPreview();
		};
	}, []);

	const categoryCounts = useMemo(() => {
		const counts: Record<VoiceCategory, number> = {
			all: TTS_VOICES.length,
			vietnamese: 0,
			english: 0,
			trending: 0,
			story: 0,
			fun: 0,
		};
		for (const v of TTS_VOICES) {
			for (const c of v.categories) {
				if (c in counts) {
					counts[c]++;
				}
			}
		}
		return counts;
	}, []);

	const filteredVoices = useMemo(() => {
		if (selectedCategory === "all") return TTS_VOICES;
		return TTS_VOICES.filter((v) => v.categories.includes(selectedCategory));
	}, [selectedCategory]);

	const selectedVoice = useMemo(() => {
		return TTS_VOICES.find((v) => v.id === selectedVoiceId) || TTS_VOICES[0];
	}, [selectedVoiceId]);

	const handlePreviewToggle = (e: React.MouseEvent, voice: TtsVoice) => {
		e.stopPropagation();

		if (previewVoiceId === voice.id) {
			TtsService.stopPreview();
			setPreviewVoiceId(null);
			return;
		}

		setPreviewVoiceId(voice.id);
		TtsService.previewVoice(
			voice,
			text.trim().length > 0 ? text : undefined,
			() => setPreviewVoiceId(null),
			(err) => {
				setPreviewVoiceId(null);
				const message = err instanceof Error ? err.message : "Không thể phát giọng đọc mẫu.";
				toast.error(message);
				console.error("[TTS Preview Error]:", err);
			},
		);
	};

	const handleApplyToTimeline = async () => {
		const cleanText = text.trim();
		if (!cleanText) {
			toast.error(t("tts.emptyTextError") || "Nội dung văn bản không được để trống.");
			return;
		}
		const project = editor.project.getActiveOrNull();
		const scene = editor.scenes.getActiveSceneOrNull();
		if (!project || !scene) {
			toast.error("Không có dự án hoặc cảnh đang mở để thêm giọng đọc.");
			return;
		}

		TtsService.stopPreview();
		setPreviewVoiceId(null);
		setIsGenerating(true);

		try {
			const result = await TtsService.synthesizeSpeechAudio({
				text: cleanText,
				voice: selectedVoice,
				speed,
				pitch,
			});
			await insertGeneratedSpeech({
				editor,
				projectId: project.metadata.id,
				sceneId: scene.id,
				result,
				name: `[TTS] ${selectedVoice.name} - ${cleanText.slice(0, 16)}`,
				startTime: element.startTime,
				beforeInsert: () => {
					if (cleanText !== String(element.params.content ?? "")) {
						editor.timeline.updateElements({
							updates: [
								{
									trackId,
									elementId: element.id,
									patch: {
										params: {
											...element.params,
											content: cleanText,
										},
									},
								},
							],
						});
					}
				},
			});

			toast.success(
				t("tts.successToast") || "Đã tạo giọng nói thành công và thêm vào dòng thời gian!",
			);
		} catch (error) {
			console.error("[TTS Generation Error]:", error);
			toast.error(
				error instanceof Error
					? error.message
					: "Có lỗi xảy ra khi tạo giọng nói. Vui lòng thử lại.",
			);
		} finally {
			setIsGenerating(false);
		}
	};

	return (
		<div className="flex flex-col gap-4 p-4 pb-20">
			{/* Text Preview / Edit Box */}
			<div className="flex flex-col gap-1.5">
				<div className="flex items-center justify-between text-xs">
					<Label className="text-muted-foreground font-medium">
						{t("tts.textToRead") || "Văn bản cần đọc"}
					</Label>
					<span className="text-muted-foreground text-[11px]">
						{text.length} {t("tts.characters") || "ký tự"}
					</span>
				</div>
				<Textarea
					value={text}
					onChange={(e) => setText(e.target.value)}
					placeholder={t("tts.enterTextPlaceholder") || "Nhập văn bản cần chuyển thành giọng nói..."}
					className="min-h-[72px] resize-none text-xs leading-relaxed bg-muted/40 focus:bg-background"
					rows={3}
				/>
			</div>

			{/* Category Filter Pills (CapCut style) */}
			<div className="flex flex-col gap-1.5 pt-1">
				<div className="flex items-center justify-between text-xs">
					<Label className="text-muted-foreground font-medium">
						{t("tts.selectVoice") || "Chọn giọng đọc"}
					</Label>
					<span className="text-muted-foreground text-[11px] font-medium">
						{filteredVoices.length} / {TTS_VOICES.length} {t("tts.voices") || "giọng"}
					</span>
				</div>
				<div className="flex gap-1.5 overflow-x-auto pb-1 scrollbar-hidden">
					{CATEGORIES.map((cat) => {
						const count = categoryCounts[cat.id] ?? 0;
						return (
							<button
								key={cat.id}
								type="button"
								onClick={() => startTransition(() => setSelectedCategory(cat.id))}
								className={cn(
									"shrink-0 rounded-full px-2.5 py-1 text-xs font-medium transition-all cursor-pointer flex items-center gap-1",
									selectedCategory === cat.id
										? "bg-primary text-primary-foreground shadow-xs"
										: "bg-muted/60 text-muted-foreground hover:bg-muted hover:text-foreground",
								)}
							>
								<span>{cat.icon}</span>
								<span>{t(cat.labelKey as any) || cat.id}</span>
								<span
									className={cn(
										"text-[10px] ml-0.5 font-semibold",
										selectedCategory === cat.id
											? "text-primary-foreground/90"
											: "text-muted-foreground/80",
									)}
								>
									({count})
								</span>
							</button>
						);
					})}
				</div>
			</div>

			{/* Voice Cards Grid (CapCut style 2-columns with clean spacing) */}
			<div className="grid grid-cols-2 gap-2.5">
				{filteredVoices.map((voice) => {
					const isSelected = selectedVoiceId === voice.id;
					const isPreviewing = previewVoiceId === voice.id;

					return (
						<div
							key={voice.id}
							onClick={() => setSelectedVoiceId(voice.id)}
							className={cn(
								"group relative flex flex-col justify-between rounded-xl p-2.5 text-left transition-all cursor-pointer border",
								isSelected
									? "border-primary bg-primary/10 shadow-xs ring-1 ring-primary/40"
									: "border-border/50 bg-card/60 hover:border-border hover:bg-accent/40",
							)}
						>
							{/* Selected Checkmark Badge */}
							{isSelected && (
								<div className="absolute top-2 right-2 flex h-4 w-4 items-center justify-center rounded-full bg-primary text-primary-foreground shadow-xs">
									<HugeiconsIcon icon={Tick02Icon} size={10} strokeWidth={3} />
								</div>
							)}

							{/* Top Row: Avatar & Play Button */}
							<div className="flex items-center justify-between">
								<div
									className={cn(
										"flex h-9 w-9 shrink-0 items-center justify-center rounded-full bg-gradient-to-tr text-base shadow-xs",
										voice.avatarGradient,
									)}
								>
									{voice.avatarIcon || "🎙️"}
								</div>

								<Button
									type="button"
									variant="ghost"
									size="icon"
									onClick={(e) => handlePreviewToggle(e, voice)}
									className={cn(
										"h-7 w-7 rounded-full transition-all shrink-0",
										isPreviewing
											? "bg-primary text-primary-foreground hover:bg-primary/90 shadow-xs scale-105"
											: "bg-muted/80 text-muted-foreground hover:bg-muted hover:text-foreground",
									)}
									title={isPreviewing ? "Dừng nghe thử" : "Nghe thử giọng"}
								>
									<HugeiconsIcon
										icon={isPreviewing ? PauseIcon : PlayIcon}
										size={13}
										fill="currentColor"
									/>
								</Button>
							</div>

							{/* Voice Info */}
							<div className="mt-2 flex flex-col">
								<span className="font-semibold text-xs text-foreground line-clamp-1 leading-snug">
									{voice.name}
								</span>
								<span className="text-[10px] text-muted-foreground line-clamp-1 mt-0.5">
									{voice.description}
								</span>
							</div>

							{/* Wave Animation Indicator during preview */}
							{isPreviewing && (
								<div className="mt-2 flex items-center gap-0.5">
									<div className="h-2 w-0.5 animate-pulse bg-primary rounded-full" />
									<div className="h-3.5 w-0.5 animate-pulse bg-primary rounded-full delay-75" />
									<div className="h-2.5 w-0.5 animate-pulse bg-primary rounded-full delay-150" />
									<div className="h-4 w-0.5 animate-pulse bg-primary rounded-full delay-200" />
									<span className="text-[10px] font-medium text-primary ml-1">Đang nghe thử...</span>
								</div>
							)}
						</div>
					);
				})}
			</div>

			{/* Voice Adjustments (Speed & Pitch Sliders) */}
			<div className="flex flex-col gap-3 rounded-xl border border-border/50 bg-muted/20 p-3 mt-1">
				<div className="flex flex-col gap-2">
					<div className="flex items-center justify-between text-xs">
						<Label className="text-muted-foreground font-medium">
							{t("tts.speed") || "Tốc độ đọc"}
						</Label>
						<span className="font-mono text-xs font-semibold text-foreground">
							{speed.toFixed(1)}x
						</span>
					</div>
					<Slider
						value={[speed]}
						min={0.5}
						max={2.0}
						step={0.1}
						onValueChange={([val]) => setSpeed(val)}
					/>
				</div>

				<div className="flex flex-col gap-2">
					<div className="flex items-center justify-between text-xs">
						<Label className="text-muted-foreground font-medium">
							{t("tts.pitch") || "Cao độ (Trầm / Bổng)"}
						</Label>
						<span className="font-mono text-xs font-semibold text-foreground">
							{pitch > 0 ? `+${pitch}` : pitch}%
						</span>
					</div>
					<Slider
						value={[pitch]}
						min={-40}
						max={40}
						step={5}
						onValueChange={([val]) => setPitch(val)}
					/>
				</div>
			</div>

			{/* Sticky / Full-width Action Button at Bottom */}
			<div className="sticky bottom-0 pt-2 bg-gradient-to-t from-background via-background to-transparent">
				<Button
					type="button"
					size="lg"
					disabled={isGenerating || text.trim().length === 0}
					onClick={handleApplyToTimeline}
					className="w-full gap-2 font-semibold shadow-md cursor-pointer"
				>
					{isGenerating ? (
						<>
							<HugeiconsIcon icon={Loading03Icon} className="animate-spin" size={18} />
							<span>{t("tts.generating") || "Đang tạo giọng nói..."}</span>
						</>
					) : (
						<>
							<span>🔊</span>
							<span>{t("tts.applyToTimeline") || "Áp dụng vào dòng thời gian"}</span>
						</>
					)}
				</Button>
			</div>
		</div>
	);
}
