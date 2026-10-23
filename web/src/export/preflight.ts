import type { MediaAsset } from "@/media/types";
import type { SceneTracks } from "@/timeline";
import { canTrackHaveAudio } from "@/timeline/track-capabilities";
import { canElementHaveAudio, hasMediaId } from "@/timeline/element-utils";
import { isElementMuted } from "@/timeline/audio-state";
import type { ExportRange } from "./range";

export function getMissingExportSources({
	tracks,
	mediaAssets,
	range,
	includeAudio,
	audioOnly,
}: {
	tracks: SceneTracks;
	mediaAssets: MediaAsset[];
	range: ExportRange;
	includeAudio: boolean;
	audioOnly: boolean;
}): string[] {
	const assets = new Map(mediaAssets.map((asset) => [asset.id, asset]));
	const visualTracks = [...tracks.overlay, tracks.main];
	const allTracks = [...visualTracks, ...tracks.audio];
	const visualSolo = visualTracks.some((track) => track.solo === true);
	const audioSolo = allTracks.some(
		(track) => canTrackHaveAudio(track) && track.solo === true,
	);
	const missing = new Set<string>();
	for (const track of allTracks) {
		const visual =
			!audioOnly &&
			track.type !== "audio" &&
			!("hidden" in track && track.hidden) &&
			(!visualSolo || track.solo === true);
		const audio =
			includeAudio &&
			canTrackHaveAudio(track) &&
			!track.muted &&
			(!audioSolo || track.solo === true);
		for (const element of track.elements) {
			if (
				!hasMediaId(element) ||
				element.startTime >= range.end ||
				element.startTime + element.duration <= range.start
			)
				continue;
			const asset = assets.get(element.mediaId);
			if (asset && !asset.missing) continue;
			const needsPicture =
				visual &&
				(element.type === "video" || element.type === "image") &&
				!element.hidden;
			const needsAudio =
				audio &&
				canElementHaveAudio(element) &&
				!isElementMuted({ element }) &&
				(element.type === "audio" ||
					(element.isSourceAudioEnabled !== false &&
						asset?.hasAudio !== false));
			if (needsPicture || needsAudio) missing.add(asset?.name ?? element.name);
		}
	}
	return [...missing];
}
