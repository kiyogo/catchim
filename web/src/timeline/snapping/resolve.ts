import type { SnapPoint, SnapResult } from "./types";
import type { MediaTime } from "@/wasm";
import { getRustEngineBindings } from "@/native/rust-engine";

function isSortedByTime(snapPoints: SnapPoint[]): boolean {
	for (let index = 1; index < snapPoints.length; index += 1) {
		if (snapPoints[index - 1].time > snapPoints[index].time) {
			return false;
		}
	}

	return true;
}

function findLowerBound({
	snapPoints,
	targetTime,
}: {
	snapPoints: SnapPoint[];
	targetTime: number;
}): number {
	let low = 0;
	let high = snapPoints.length;

	while (low < high) {
		const mid = low + Math.floor((high - low) / 2);
		if (snapPoints[mid].time < targetTime) {
			low = mid + 1;
		} else {
			high = mid;
		}
	}

	return low;
}

function resolveTimelineSnapLinear({
	targetTime,
	snapPoints,
	maxSnapDistance,
}: {
	targetTime: MediaTime;
	snapPoints: SnapPoint[];
	maxSnapDistance: number;
}): SnapResult {
	let closestSnapPoint: SnapPoint | null = null;
	let closestDistance = Infinity;

	for (const snapPoint of snapPoints) {
		const distance = Math.abs(targetTime - snapPoint.time);
		if (distance <= maxSnapDistance && distance < closestDistance) {
			closestDistance = distance;
			closestSnapPoint = snapPoint;
		}
	}

	return {
		snappedTime: closestSnapPoint ? closestSnapPoint.time : targetTime,
		snapPoint: closestSnapPoint,
		snapDistance: closestDistance,
	};
}

export function resolveSortedTimelineSnap({
	targetTime,
	snapPoints,
	maxSnapDistance,
}: {
	targetTime: MediaTime;
	snapPoints: SnapPoint[];
	maxSnapDistance: number;
}): SnapResult {
	let closestSnapPoint: SnapPoint | null = null;
	let closestDistance = Infinity;

	const firstCandidateIndex = findLowerBound({
		snapPoints,
		targetTime: targetTime - maxSnapDistance,
	});
	for (
		let index = firstCandidateIndex;
		index < snapPoints.length &&
		snapPoints[index].time <= targetTime + maxSnapDistance;
		index += 1
	) {
		const snapPoint = snapPoints[index];
		const distance = Math.abs(targetTime - snapPoint.time);
		if (distance <= maxSnapDistance && distance < closestDistance) {
			closestDistance = distance;
			closestSnapPoint = snapPoint;
		}
	}

	return {
		snappedTime: closestSnapPoint ? closestSnapPoint.time : targetTime,
		snapPoint: closestSnapPoint,
		snapDistance: closestDistance,
	};
}

function resolveSortedTimelineSnapNative({
	targetTime,
	snapPoints,
	maxSnapDistance,
}: {
	targetTime: MediaTime;
	snapPoints: SnapPoint[];
	maxSnapDistance: number;
}): SnapResult | null {
	const bindings = getRustEngineBindings();
	if (
		!bindings ||
		snapPoints.length === 0 ||
		!Number.isSafeInteger(targetTime) ||
		!snapPoints.every((snapPoint) => Number.isSafeInteger(snapPoint.time))
	) {
		return null;
	}

	let pointsPtr = 0;
	let snappedTimePtr = 0;
	let matchedIndexPtr = 0;
	try {
		pointsPtr = bindings.malloc(snapPoints.length * Float64Array.BYTES_PER_ELEMENT);
		snappedTimePtr = bindings.malloc(Float64Array.BYTES_PER_ELEMENT);
		matchedIndexPtr = bindings.malloc(Int32Array.BYTES_PER_ELEMENT);
		if (!pointsPtr || !snappedTimePtr || !matchedIndexPtr) {
			return null;
		}

		const points = new Float64Array(
			bindings.HEAPF64.buffer,
			pointsPtr,
			snapPoints.length,
		);
		for (let index = 0; index < snapPoints.length; index += 1) {
			points[index] = snapPoints[index].time;
		}

		const matched = bindings.snapPointsSorted(
			targetTime,
			pointsPtr,
			snapPoints.length,
			maxSnapDistance,
			snappedTimePtr,
			matchedIndexPtr,
		);
		if (matched === 0) {
			return {
				snappedTime: targetTime,
				snapPoint: null,
				snapDistance: Infinity,
			};
		}

		const matchedIndex = new Int32Array(
			bindings.HEAPU32.buffer,
			matchedIndexPtr,
			1,
		)[0];
		const snapPoint = snapPoints[matchedIndex];
		if (!snapPoint) {
			return null;
		}
		const snappedTime = new Float64Array(
			bindings.HEAPF64.buffer,
			snappedTimePtr,
			1,
		)[0];
		if (snappedTime !== snapPoint.time) {
			return null;
		}

		return {
			snappedTime: snappedTime as MediaTime,
			snapPoint,
			snapDistance: Math.abs(targetTime - snapPoint.time),
		};
	} catch {
		return null;
	} finally {
		if (matchedIndexPtr) bindings.free(matchedIndexPtr);
		if (snappedTimePtr) bindings.free(snappedTimePtr);
		if (pointsPtr) bindings.free(pointsPtr);
	}
}

export function resolveTimelineSnap({
	targetTime,
	snapPoints,
	maxSnapDistance,
}: {
	targetTime: MediaTime;
	snapPoints: SnapPoint[];
	maxSnapDistance: number;
}): SnapResult {
	if (!isSortedByTime(snapPoints)) {
		return resolveTimelineSnapLinear({
			targetTime,
			snapPoints,
			maxSnapDistance,
		});
	}

	const nativeResult = resolveSortedTimelineSnapNative({
		targetTime,
		snapPoints,
		maxSnapDistance,
	});
	if (nativeResult) {
		return nativeResult;
	}

	return resolveSortedTimelineSnap({
		targetTime,
		snapPoints,
		maxSnapDistance,
	});
}
