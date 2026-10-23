import type {
	TimelineTrack,
	VideoTrack,
	AudioTrack,
	GraphicTrack,
	TextTrack,
	EffectTrack,
	SceneTracks,
} from "@/timeline";

export function canTrackHaveAudio(
	track: TimelineTrack,
): track is VideoTrack | AudioTrack {
	return track.type === "audio" || track.type === "video";
}

export function canTrackBeHidden(
	track: TimelineTrack,
): track is VideoTrack | TextTrack | GraphicTrack | EffectTrack {
	return track.type !== "audio";
}

export function isTrackLocked(track: TimelineTrack): boolean {
	return track.locked === true;
}

export function preserveLockedTrackContents({
	current,
	next,
}: {
	current: SceneTracks;
	next: SceneTracks;
}): SceneTracks {
	const currentById = new Map<string, TimelineTrack>([
		current.main,
		...current.overlay,
		...current.audio,
	].map((track) => [track.id, track]));
	const preserve = <TTrack extends TimelineTrack>(track: TTrack): TTrack => {
		const previous = currentById.get(track.id);
		return previous && isTrackLocked(previous) && track.locked !== false
			? ({ ...track, locked: true, elements: previous.elements } as TTrack)
			: track;
	};
	return {
		main: preserve(next.main),
		overlay: next.overlay.map(preserve),
		audio: next.audio.map(preserve),
	};
}
