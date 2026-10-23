import { afterEach, beforeEach, describe, expect, test, vi } from "vitest";
import { toast } from "sonner";
import { storageService } from "@/services/storage/service";
import { readVideoFile } from "./mediabunny";
import { processMediaAssets } from "./processing";

vi.mock("sonner", () => ({ toast: { error: vi.fn() } }));
vi.mock("@/services/storage/service", () => ({
	storageService: { canStoreFile: vi.fn() },
}));
vi.mock("./mediabunny", () => ({ readVideoFile: vi.fn() }));

const videoFile = {
	name: "source.mp4",
	size: 1024,
	type: "video/mp4",
} as File;

describe("processMediaAssets", () => {
	beforeEach(() => {
		vi.clearAllMocks();
		vi.mocked(storageService.canStoreFile).mockResolvedValue({
			canStore: true,
			reason: "enough-space",
			availableBytes: 10_000,
		});
		vi.stubGlobal("URL", {
			createObjectURL: vi.fn(() => "blob:source"),
			revokeObjectURL: vi.fn(),
		});
	});

	afterEach(() => {
		vi.unstubAllGlobals();
	});

	test("rejects an unreadable video and completes import progress", async () => {
		vi.mocked(readVideoFile).mockRejectedValue(new Error("No video track found"));
		const onProgress = vi.fn();

		const assets = await processMediaAssets({
			files: [videoFile],
			onProgress,
		});

		expect(assets).toEqual([]);
		expect(URL.revokeObjectURL).toHaveBeenCalledWith("blob:source");
		expect(onProgress).toHaveBeenLastCalledWith({ progress: 100 });
		expect(toast.error).toHaveBeenCalledWith("Couldn't process source.mp4", {
			description: "No video track found",
		});
	});

	test("rejects video with invalid metadata instead of creating a default clip", async () => {
		vi.mocked(readVideoFile).mockResolvedValue({
			duration: Number.NaN,
			width: 1920,
			height: 1080,
			fps: 30,
			hasAudio: true,
			codec: "avc",
			canDecode: true,
			thumbnailUrl: null,
		});

		const assets = await processMediaAssets({ files: [videoFile] });

		expect(assets).toEqual([]);
		expect(URL.revokeObjectURL).toHaveBeenCalledWith("blob:source");
		expect(toast.error).toHaveBeenCalledWith("Couldn't process source.mp4", {
			description: "Video metadata is incomplete or invalid",
		});
	});

	test("preserves a valid video's measured metadata and object URL", async () => {
		vi.mocked(readVideoFile).mockResolvedValue({
			duration: 219,
			width: 1920,
			height: 1080,
			fps: 29.97,
			hasAudio: true,
			codec: "avc",
			canDecode: true,
			thumbnailUrl: "data:image/jpeg;base64,thumbnail",
		});

		const assets = await processMediaAssets({ files: [videoFile] });

		expect(assets).toHaveLength(1);
		expect(assets[0]).toMatchObject({
			name: "source.mp4",
			type: "video",
			duration: 219,
			width: 1920,
			height: 1080,
			fps: 29.97,
			hasAudio: true,
			url: "blob:source",
		});
		expect(URL.revokeObjectURL).not.toHaveBeenCalled();
	});
});
