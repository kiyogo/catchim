"use client";

import { useEffect, useRef, useState } from "react";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Textarea } from "@/components/ui/textarea";
import {
	Select,
	SelectContent,
	SelectItem,
	SelectTrigger,
	SelectValue,
} from "@/components/ui/select";
import { useEditor } from "@/editor/use-editor";
import { useTranslation } from "@/i18n";
import { executeCaptionTrackEdit } from "@/commands/timeline/element/edit-caption-track";
import type { TextElement } from "@/timeline/types";
import { mediaTimeFromSeconds, TICKS_PER_SECOND, addMediaTime } from "@/wasm";
import { captionMessages } from "../messages";
import {
	CaptionEditError,
	orderedCaptionCues,
	type CaptionEditAction,
	type CaptionStylePreset,
} from "../edit";
import { serializeSubtitleTrack, type SubtitleExportFormat } from "../export";

function seconds(value: number): string {
	return (value / TICKS_PER_SECOND).toFixed(3);
}

export function CaptionWorkbench() {
	const editor = useEditor();
	const { language } = useTranslation();
	const m = captionMessages(language);
	const tracks = useEditor((e) =>
		e.scenes
			.getActiveScene()
			.tracks.overlay.filter((track) => track.type === "text"),
	);
	const selection = useEditor((e) => e.selection.getSelectedElements());
	const [trackId, setTrackId] = useState("");
	const [cueId, setCueId] = useState("");
	const [find, setFind] = useState("");
	const [replacement, setReplacement] = useState("");
	const [matchCase, setMatchCase] = useState(false);
	const [offset, setOffset] = useState("0");
	const [preset, setPreset] = useState<CaptionStylePreset>("classic");
	const [status, setStatus] = useState<{ error: boolean; text: string } | null>(
		null,
	);

	useEffect(() => {
		const selected = selection.find((reference) =>
			tracks.some((track) => track.id === reference.trackId),
		);
		if (selected) {
			setTrackId(selected.trackId);
			setCueId(selected.elementId);
		}
	}, [selection, tracks]);

	const track = tracks.find((current) => current.id === trackId) ?? tracks[0];
	const cues = track ? orderedCaptionCues(track) : [];
	const visible = cues.filter((element) => {
		const content = String(element.params.content ?? "");
		return matchCase
			? content.includes(find)
			: content.toLocaleLowerCase().includes(find.toLocaleLowerCase());
	});
	const selected = cues.find((element) => element.id === cueId) ?? cues[0];
	const locked = track?.locked === true;

	const run = (action: CaptionEditAction) => {
		if (!track) return;
		try {
			const changed = executeCaptionTrackEdit({
				editor,
				trackId: track.id,
				action,
			});
			setStatus({ error: false, text: changed ? m.saved : m.noChange });
		} catch (error) {
			setStatus({
				error: true,
				text:
					error instanceof CaptionEditError
						? m[error.code]
						: error instanceof Error
							? error.message
							: m.failed,
			});
		}
	};

	const selectCue = (element: TextElement) => {
		if (!track) return;
		setCueId(element.id);
		editor.selection.setSelectedElements({
			elements: [{ trackId: track.id, elementId: element.id }],
		});
		editor.playback.seek({ time: element.startTime });
	};

	const download = (format: SubtitleExportFormat) => {
		if (!track) return;
		try {
			const output = serializeSubtitleTrack({ track, format });
			const url = URL.createObjectURL(
				new Blob([output], {
					type:
						format === "vtt"
							? "text/vtt;charset=utf-8"
							: "application/x-subrip;charset=utf-8",
				}),
			);
			const anchor = document.createElement("a");
			anchor.href = url;
			anchor.download = `${track.name.replace(/[<>:"/\\|?*]/g, "_") || "captions"}.${format}`;
			anchor.click();
			setTimeout(() => URL.revokeObjectURL(url), 1000);
		} catch (error) {
			setStatus({
				error: true,
				text: error instanceof Error ? error.message : m.failed,
			});
		}
	};

	return (
		<div className="flex min-h-0 flex-col gap-3 px-3 pb-3">
			<label className="space-y-1 text-xs">
				<span className="text-muted-foreground">{m.track}</span>
				<Select
					value={track?.id ?? ""}
					onValueChange={(value) => {
						setTrackId(value);
						setCueId("");
						setStatus(null);
						const next = tracks.find((item) => item.id === value);
						if (next)
							editor.selection.setSelectedElements({
								elements: next.elements[0]
									? [{ trackId: next.id, elementId: next.elements[0].id }]
									: [],
							});
					}}
					disabled={tracks.length === 0}
				>
					<SelectTrigger>
						<SelectValue placeholder={m.chooseTrack} />
					</SelectTrigger>
					<SelectContent>
						{tracks.map((item) => (
							<SelectItem key={item.id} value={item.id}>
								{item.name} · {item.elements.length} {m.count}
							</SelectItem>
						))}
					</SelectContent>
				</Select>
			</label>
			{!track ? (
				<p className="text-muted-foreground py-5 text-sm">{m.empty}</p>
			) : (
				<>
					{locked && (
						<p
							role="status"
							className="text-muted-foreground rounded-md border p-2 text-xs"
						>
							{m.locked}
						</p>
					)}
					<Input
						aria-label={m.search}
						placeholder={m.search}
						value={find}
						onChange={(event) => setFind(event.target.value)}
						size="sm"
					/>
					<details className="rounded-md border p-2 text-xs">
						<summary className="cursor-pointer">{m.replaceAll}</summary>
						<div className="mt-2 flex flex-col gap-2">
							<Input
								aria-label={m.replace}
								placeholder={m.replace}
								value={replacement}
								onChange={(event) => setReplacement(event.target.value)}
								size="sm"
							/>
							<label className="flex items-center gap-2">
								<input
									type="checkbox"
									checked={matchCase}
									onChange={(event) => setMatchCase(event.target.checked)}
								/>
								{m.matchCase}
							</label>
							<Button
								type="button"
								size="sm"
								variant="outline"
								disabled={locked || !find || visible.length === 0}
								onClick={() =>
									run({ type: "replace", find, replacement, matchCase })
								}
							>
								{m.replaceAll}
							</Button>
						</div>
					</details>
					<div
						role="list"
						aria-label={m.editTab}
						className="max-h-56 space-y-1 overflow-y-auto"
					>
						{visible.length === 0 && (
							<p className="text-muted-foreground py-2 text-xs">
								{m.noResults}
							</p>
						)}
						{visible.map((element) => (
							<button
								type="button"
								role="listitem"
								key={element.id}
								onClick={() => selectCue(element)}
								aria-current={selected?.id === element.id ? "true" : undefined}
								className={`w-full rounded-md border p-2 text-left text-xs ${selected?.id === element.id ? "border-primary bg-primary/5" : "border-transparent hover:bg-accent"}`}
							>
								<span className="text-muted-foreground block font-mono">
									{seconds(element.startTime)} →{" "}
									{seconds(element.startTime + element.duration)}
								</span>
								<span className="line-clamp-2 whitespace-pre-wrap">
									{String(element.params.content ?? "")}
								</span>
							</button>
						))}
					</div>
					{selected && (
						<CaptionCueEditor
							key={`${track.id}:${selected.id}`}
							element={selected}
							locked={locked}
							hasNext={cues.indexOf(selected) < cues.length - 1}
							onRun={run}
							onSeek={() => selectCue(selected)}
						/>
					)}
					<details className="rounded-md border p-2 text-xs">
						<summary className="cursor-pointer">{m.offset}</summary>
						<div className="mt-2 flex flex-col gap-2">
							<Input
								aria-label={m.offset}
								type="number"
								step="0.1"
								value={offset}
								onChange={(event) => setOffset(event.target.value)}
								size="sm"
							/>
							<p className="text-muted-foreground">{m.offsetHint}</p>
							<Button
								type="button"
								variant="outline"
								size="sm"
								disabled={
									locked ||
									cues.length === 0 ||
									!offset ||
									!Number.isFinite(Number(offset)) ||
									Number(offset) === 0
								}
								onClick={() =>
									run({
										type: "offset",
										offset: mediaTimeFromSeconds({ seconds: Number(offset) }),
									})
								}
							>
								{m.applyOffset}
							</Button>
						</div>
					</details>
					<details className="rounded-md border p-2 text-xs">
						<summary className="cursor-pointer">{m.style}</summary>
						<div className="mt-2 flex flex-col gap-2">
							<Select
								value={preset}
								onValueChange={(value) =>
									setPreset(value as CaptionStylePreset)
								}
							>
								<SelectTrigger>
									<SelectValue />
								</SelectTrigger>
								<SelectContent>
									{(["classic", "yellow", "box", "minimal"] as const).map(
										(style) => (
											<SelectItem key={style} value={style}>
												{m[style]}
											</SelectItem>
										),
									)}
								</SelectContent>
							</Select>
							<Button
								type="button"
								variant="outline"
								size="sm"
								disabled={locked || cues.length === 0}
								onClick={() => run({ type: "style", preset })}
							>
								{m.applyStyle}
							</Button>
						</div>
					</details>
					<div className="flex gap-2">
						<Button
							type="button"
							variant="outline"
							size="sm"
							disabled={cues.length === 0}
							onClick={() => download("srt")}
						>
							{m.exportSrt}
						</Button>
						<Button
							type="button"
							variant="outline"
							size="sm"
							disabled={cues.length === 0}
							onClick={() => download("vtt")}
						>
							{m.exportVtt}
						</Button>
					</div>
				</>
			)}
			{status && (
				<p
					role={status.error ? "alert" : "status"}
					className={`rounded-md border p-2 text-xs ${status.error ? "text-destructive" : "text-muted-foreground"}`}
				>
					{status.text}
				</p>
			)}
		</div>
	);
}

function CaptionCueEditor({
	element,
	locked,
	hasNext,
	onRun,
	onSeek,
}: {
	element: TextElement;
	locked: boolean;
	hasNext: boolean;
	onRun: (action: CaptionEditAction) => void;
	onSeek: () => void;
}) {
	const editor = useEditor();
	const { language } = useTranslation();
	const m = captionMessages(language);
	const [content, setContent] = useState(String(element.params.content ?? ""));
	const [start, setStart] = useState(seconds(element.startTime));
	const [end, setEnd] = useState(seconds(element.startTime + element.duration));
	const [caret, setCaret] = useState(0);
	const textarea = useRef<HTMLTextAreaElement>(null);
	useEffect(() => {
		setContent(String(element.params.content ?? ""));
		setStart(seconds(element.startTime));
		setEnd(seconds(element.startTime + element.duration));
		setCaret(0);
	}, [element.params.content, element.startTime, element.duration]);
	const startChanged = start !== seconds(element.startTime);
	const endChanged = end !== seconds(element.startTime + element.duration);
	const timingChanged = startChanged || endChanged;
	const dirty =
		content !== String(element.params.content ?? "") || timingChanged;
	const validInput =
		start.trim() !== "" &&
		end.trim() !== "" &&
		Number.isFinite(Number(start)) &&
		Number.isFinite(Number(end)) &&
		Number(start) >= 0 &&
		Number(end) > Number(start);
	return (
		<div className="space-y-2 rounded-md border p-2">
			<Textarea
				ref={textarea}
				aria-label={m.content}
				value={content}
				disabled={locked}
				onChange={(event) => setContent(event.target.value)}
				onSelect={(event) => setCaret(event.currentTarget.selectionStart)}
				className="min-h-24 text-sm"
			/>
			<div className="grid grid-cols-2 gap-2">
				<label className="text-xs text-muted-foreground">
					{m.start}
					<Input
						type="number"
						min="0"
						step="0.001"
						size="sm"
						aria-label={m.start}
						value={start}
						disabled={locked}
						onChange={(event) => setStart(event.target.value)}
					/>
				</label>
				<label className="text-xs text-muted-foreground">
					{m.end}
					<Input
						type="number"
						min="0"
						step="0.001"
						size="sm"
						aria-label={m.end}
						value={end}
						disabled={locked}
						onChange={(event) => setEnd(event.target.value)}
					/>
				</label>
			</div>
			{dirty && <p className="text-muted-foreground text-xs">{m.unsaved}</p>}
			<div className="flex flex-wrap gap-1.5">
				<Button
					type="button"
					size="sm"
					disabled={locked || !dirty || !content.trim() || !validInput}
					onClick={() =>
						onRun({
							type: "update",
							elementId: element.id,
							content,
							startTime: startChanged
								? mediaTimeFromSeconds({ seconds: Number(start) })
								: element.startTime,
							endTime: endChanged
								? mediaTimeFromSeconds({ seconds: Number(end) })
								: addMediaTime({ a: element.startTime, b: element.duration }),
						})
					}
				>
					{m.save}
				</Button>
				<Button type="button" size="sm" variant="outline" onClick={onSeek}>
					{m.seek}
				</Button>
				<Button
					type="button"
					size="sm"
					variant="outline"
					title={m.splitHint}
					disabled={locked || dirty || caret <= 0 || caret >= content.length}
					onClick={() =>
						onRun({
							type: "split",
							elementId: element.id,
							time: editor.playback.getCurrentTime(),
							textOffset: textarea.current?.selectionStart ?? caret,
						})
					}
				>
					{m.split}
				</Button>
				<Button
					type="button"
					size="sm"
					variant="outline"
					title={m.mergeHint}
					disabled={locked || dirty || !hasNext}
					onClick={() => onRun({ type: "merge", elementId: element.id })}
				>
					{m.merge}
				</Button>
			</div>
			<p className="text-muted-foreground text-xs">{m.splitHint}</p>
		</div>
	);
}
