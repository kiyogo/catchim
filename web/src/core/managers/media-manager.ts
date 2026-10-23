import type { EditorCore } from "@/core";
import { toast } from "sonner";
import type { MediaAsset } from "@/media/types";
import type {
  MediaBin,
  MediaOrganizationAsset,
  MediaOrganizationPatch,
  MediaOrganizationSnapshot,
} from "@/media/types";
import { storageService } from "@/services/storage/service";
import { generateUUID } from "@/utils/id";
import { videoCache } from "@/services/video-cache/service";
import { waveformCache } from "@/services/waveform-cache/service";
import {
  BatchCommand,
  RemoveMediaAssetCommand,
  RenameMediaAssetCommand,
  RelinkMediaAssetCommand,
  ReplaceMediaAssetCommand,
  UpdateMediaOrganizationCommand,
} from "@/commands";
import { buildWaveformSourceKey } from "@/media/waveform-summary";
import { TICKS_PER_SECOND } from "@/wasm";

export class MediaManager {
  private assets: MediaAsset[] = [];
  private bins: MediaBin[] = [];
  private loadedProjectId: string | null = null;
  private isLoading = false;
  private listeners = new Set<() => void>();

  constructor(private editor: EditorCore) {}

  async addMediaAsset({
    projectId,
    asset,
  }: {
    projectId: string;
    asset: Omit<MediaAsset, "id">;
  }): Promise<MediaAsset | null> {
    const newAsset: MediaAsset = {
      ...asset,
      id: generateUUID(),
      tags: asset.tags ?? [],
      favorite: asset.favorite ?? false,
    };

    this.assets = [...this.assets, newAsset];
    this.notify();

    try {
      await storageService.saveMediaAsset({ projectId, mediaAsset: newAsset });
      this.editor.project.ratchetFpsForImportedMedia({
        importedAssets: [newAsset],
      });
      return newAsset;
    } catch (error) {
      console.error("Failed to save media asset:", error);
      this.assets = this.assets.filter((asset) => asset.id !== newAsset.id);
      this.notify();

      if (storageService.isQuotaExceededError({ error })) {
        toast.error("Not enough browser storage", {
          description: error instanceof Error ? error.message : undefined,
        });
      }

      return null;
    }
  }

  removeMediaAsset({ projectId, id }: { projectId: string; id: string }): void {
    this.removeMediaAssets({ projectId, ids: [id] });
  }

  removeMediaAssets({
    projectId,
    ids,
  }: {
    projectId: string;
    ids: string[];
  }): void {
    const uniqueIds = [...new Set(ids)];
    if (uniqueIds.length === 0) {
      return;
    }

    const command =
      uniqueIds.length === 1
        ? new RemoveMediaAssetCommand({
            projectId,
            assetId: uniqueIds[0],
          })
        : new BatchCommand(
            uniqueIds.map(
              (id) =>
                new RemoveMediaAssetCommand({
                  projectId,
                  assetId: id,
                }),
            ),
          );

    this.editor.command.execute({ command });
  }

  async renameMediaAsset({
    projectId,
    id,
    name,
  }: {
    projectId: string;
    id: string;
    name: string;
  }): Promise<void> {
    const asset = this.assets.find((item) => item.id === id);
    const nextName = name.trim();
    if (!asset) throw new Error("Media asset not found");
    if (!nextName) throw new Error("Media name cannot be empty");
    if (asset.name === nextName) return;

    await storageService.renameMediaAsset({ projectId, id, name: nextName });
    this.assets = this.assets.map((item) =>
      item.id === id ? { ...item, name: nextName } : item,
    );
    this.notify();
    this.editor.command.push({
      command: new RenameMediaAssetCommand({
        projectId,
        assetId: id,
        previousName: asset.name,
        nextName,
      }),
    });
  }

  async applyMediaAssetNameSnapshot({
    projectId,
    assetId,
    name,
  }: {
    projectId: string;
    assetId: string;
    name: string;
  }): Promise<void> {
    try {
      await storageService.renameMediaAsset({
        projectId,
        id: assetId,
        name,
      });
      this.assets = this.assets.map((asset) =>
        asset.id === assetId ? { ...asset, name } : asset,
      );
      this.notify();
    } catch (error) {
      console.error("Failed to restore media name:", error);
      toast.error("Could not update media name");
    }
  }

  async replaceMediaAsset({
    projectId,
    id,
    replacement,
  }: {
    projectId: string;
    id: string;
    replacement: Omit<MediaAsset, "id">;
  }): Promise<void> {
    const previousAsset = this.assets.find((asset) => asset.id === id);
    if (!previousAsset) throw new Error("Media asset not found");
    if (previousAsset.type !== replacement.type) {
      throw new Error("Replacement media must have the same type");
    }
    const durationTolerance = 1 / TICKS_PER_SECOND;
    if (
      previousAsset.duration !== undefined &&
      (replacement.duration === undefined ||
        replacement.duration + durationTolerance < previousAsset.duration)
    ) {
      throw new Error("Replacement media is shorter than the original source");
    }

    const replacementAsset: MediaAsset = {
      ...replacement,
      id,
      name: previousAsset.name,
      binId: previousAsset.binId,
      tags: previousAsset.tags ?? [],
      favorite: previousAsset.favorite ?? false,
    };
    await storageService.replaceMediaAsset({
      projectId,
      mediaAsset: replacementAsset,
    });
    this.installReplacedAsset({ asset: replacementAsset });
    this.editor.command.push({
      command: new ReplaceMediaAssetCommand({
        projectId,
        previousAsset,
        replacementAsset,
      }),
    });
  }

  async relinkMediaAsset({
    projectId,
    id,
    replacement,
  }: {
    projectId: string;
    id: string;
    replacement: Omit<MediaAsset, "id">;
  }): Promise<void> {
    const previousAsset = this.assets.find((asset) => asset.id === id);
    if (!previousAsset?.missing) {
      throw new Error("Media asset is not missing");
    }
    if (previousAsset.type !== replacement.type) {
      throw new Error("Replacement media must have the same type");
    }
    const durationTolerance = 1 / TICKS_PER_SECOND;
    if (
      previousAsset.duration !== undefined &&
      (replacement.duration === undefined ||
        replacement.duration + durationTolerance < previousAsset.duration)
    ) {
      throw new Error("Replacement media is shorter than the original source");
    }

    const replacementAsset: MediaAsset = {
      ...replacement,
      id,
      name: previousAsset.name,
      binId: previousAsset.binId,
      tags: previousAsset.tags ?? [],
      favorite: previousAsset.favorite ?? false,
    };
    await storageService.relinkMediaAsset({
      projectId,
      mediaAsset: replacementAsset,
    });
    this.installReplacedAsset({ asset: replacementAsset });
    this.editor.command.push({
      command: new RelinkMediaAssetCommand({
        projectId,
        previousAsset,
        replacementAsset,
      }),
    });
  }

  async applyRelinkMediaAssetSnapshot({
    projectId,
    asset,
  }: {
    projectId: string;
    asset: MediaAsset;
  }): Promise<void> {
    try {
      if (asset.missing) {
        await storageService.saveMissingMediaAsset({
          projectId,
          mediaAsset: asset,
        });
        this.installMissingAsset({ asset });
        return;
      }
      await storageService.relinkMediaAsset({ projectId, mediaAsset: asset });
      this.installReplacedAsset({ asset });
    } catch (error) {
      console.error("Failed to restore relinked media:", error);
      toast.error("Could not restore the media source");
    }
  }

  async applyMediaAssetSnapshot({
    projectId,
    asset,
  }: {
    projectId: string;
    asset: MediaAsset;
  }): Promise<void> {
    try {
      await storageService.replaceMediaAsset({ projectId, mediaAsset: asset });
      this.installReplacedAsset({ asset });
    } catch (error) {
      console.error("Failed to restore replaced media:", error);
      toast.error("Could not restore the media file");
    }
  }

  private installReplacedAsset({ asset }: { asset: MediaAsset }): void {
    const previousAsset = this.assets.find((item) => item.id === asset.id);
    if (!previousAsset) return;

    if (previousAsset.url) URL.revokeObjectURL(previousAsset.url);
    if (
      previousAsset.thumbnailUrl?.startsWith("blob:") &&
      previousAsset.thumbnailUrl !== asset.thumbnailUrl
    ) {
      URL.revokeObjectURL(previousAsset.thumbnailUrl);
    }
    if (asset.url?.startsWith("blob:")) URL.revokeObjectURL(asset.url);

    const restoredAsset: MediaAsset = {
      ...asset,
      url: URL.createObjectURL(asset.file),
      missing: false,
      storedSize: undefined,
      storedLastModified: undefined,
    };
    videoCache.clearVideo({ mediaId: asset.id });
    waveformCache.clearSource({
      sourceKey: buildWaveformSourceKey({ kind: "media", id: asset.id }),
    });
    this.assets = this.assets.map((item) =>
      item.id === asset.id ? restoredAsset : item,
    );
    this.notify();
  }

  private installMissingAsset({ asset }: { asset: MediaAsset }): void {
    const previousAsset = this.assets.find((item) => item.id === asset.id);
    if (previousAsset?.url?.startsWith("blob:")) {
      URL.revokeObjectURL(previousAsset.url);
    }
    if (previousAsset?.thumbnailUrl?.startsWith("blob:")) {
      URL.revokeObjectURL(previousAsset.thumbnailUrl);
    }
    videoCache.clearVideo({ mediaId: asset.id });
    waveformCache.clearSource({
      sourceKey: buildWaveformSourceKey({ kind: "media", id: asset.id }),
    });
    this.assets = this.assets.map((item) =>
      item.id === asset.id
        ? {
            ...asset,
            file: new File([], asset.name, {
              lastModified: asset.storedLastModified ?? 0,
            }),
            url: undefined,
            thumbnailUrl: undefined,
            missing: true,
          }
        : item,
    );
    this.notify();
  }

  async loadProjectMedia({ projectId }: { projectId: string }): Promise<void> {
    this.isLoading = true;
    this.notify();

    try {
      const [mediaAssets, library] = await Promise.all([
        storageService.loadAllMediaAssets({ projectId }),
        storageService.loadMediaLibrary({ projectId }).catch((error) => {
          console.error("Failed to load media library:", error);
          return { bins: [] };
        }),
      ]);
      this.loadedProjectId = projectId;
      this.assets = mediaAssets;
      this.bins = library.bins;
      this.notify();
    } catch (error) {
      console.error("Failed to load media assets:", error);
    } finally {
      this.isLoading = false;
      this.notify();
    }
  }

  async clearProjectMedia({ projectId }: { projectId: string }): Promise<void> {
    waveformCache.clearAll();

    this.assets.forEach((asset) => {
      if (asset.url) {
        URL.revokeObjectURL(asset.url);
      }
      if (asset.thumbnailUrl) {
        URL.revokeObjectURL(asset.thumbnailUrl);
      }
    });

    const mediaIds = this.assets.map((asset) => asset.id);
    this.assets = [];
    this.notify();

    try {
      await Promise.all(
        mediaIds.map((id) =>
          storageService.deleteMediaAsset({ projectId, id }),
        ),
      );
    } catch (error) {
      console.error("Failed to clear media assets from storage:", error);
    }
  }

  clearAllAssets(): void {
    videoCache.clearAll();
    waveformCache.clearAll();

    this.assets.forEach((asset) => {
      if (asset.url) {
        URL.revokeObjectURL(asset.url);
      }
      if (asset.thumbnailUrl) {
        URL.revokeObjectURL(asset.thumbnailUrl);
      }
    });

    this.assets = [];
    this.bins = [];
    this.loadedProjectId = null;
    this.notify();
  }

  getAssets(): MediaAsset[] {
    return this.assets;
  }

  getBins(): MediaBin[] {
    return this.bins;
  }

  async updateMediaOrganization({
    projectId,
    ids,
    patch,
  }: {
    projectId: string;
    ids: string[];
    patch: MediaOrganizationPatch;
  }): Promise<void> {
    this.assertLoadedProject({ projectId });
    const uniqueIds = [...new Set(ids)];
    await this.updateMediaOrganizationBatch({
      projectId,
      updates: uniqueIds.map((id) => ({ id, patch })),
    });
  }

  async updateMediaOrganizationBatch({
    projectId,
    updates,
  }: {
    projectId: string;
    updates: Array<{ id: string; patch: MediaOrganizationPatch }>;
  }): Promise<void> {
    this.assertLoadedProject({ projectId });
    const updatesById = new Map(updates.map((update) => [update.id, update.patch]));
    const selected = this.assets.filter((asset) => updatesById.has(asset.id));
    if (selected.length === 0) return;

    const previousAssets = selected.map((asset) => this.toOrganizationAsset({ asset }));
    const nextAssets = previousAssets.map((asset) => {
      const patch = updatesById.get(asset.id) ?? {};
      return {
        ...asset,
        ...(Object.hasOwn(patch, "binId") && { binId: patch.binId }),
        ...(patch.tags !== undefined && { tags: [...patch.tags] }),
        ...(patch.favorite !== undefined && { favorite: patch.favorite }),
      };
    });
    await this.commitMediaOrganizationChange({
      projectId,
      previous: {
        bins: this.bins.map((bin) => ({ ...bin })),
        assets: previousAssets,
      },
      next: {
        bins: this.bins.map((bin) => ({ ...bin })),
        assets: nextAssets,
      },
    });
  }

  async createMediaBin({
    projectId,
    name,
  }: {
    projectId: string;
    name: string;
  }): Promise<MediaBin> {
    this.assertLoadedProject({ projectId });
    const normalizedName = name.trim();
    if (!normalizedName) throw new Error("Folder name cannot be empty");
    if (this.bins.some((bin) => bin.name.toLocaleLowerCase() === normalizedName.toLocaleLowerCase())) {
      throw new Error("A folder with this name already exists");
    }
    const bin = { id: generateUUID(), name: normalizedName };
    await this.commitMediaOrganizationChange({
      projectId,
      previous: { bins: this.bins.map((item) => ({ ...item })), assets: [] },
      next: { bins: [...this.bins.map((item) => ({ ...item })), bin], assets: [] },
    });
    return bin;
  }

  async renameMediaBin({
    projectId,
    id,
    name,
  }: {
    projectId: string;
    id: string;
    name: string;
  }): Promise<void> {
    this.assertLoadedProject({ projectId });
    const normalizedName = name.trim();
    if (!normalizedName) throw new Error("Folder name cannot be empty");
    if (this.bins.some((bin) => bin.id !== id && bin.name.toLocaleLowerCase() === normalizedName.toLocaleLowerCase())) {
      throw new Error("A folder with this name already exists");
    }
    const previous = this.bins.map((bin) => ({ ...bin }));
    if (!previous.some((bin) => bin.id === id)) throw new Error("Folder not found");
    const next = previous.map((bin) => bin.id === id ? { ...bin, name: normalizedName } : bin);
    await this.commitMediaOrganizationChange({
      projectId,
      previous: { bins: previous, assets: [] },
      next: { bins: next, assets: [] },
    });
  }

  async deleteMediaBin({
    projectId,
    id,
  }: {
    projectId: string;
    id: string;
  }): Promise<void> {
    this.assertLoadedProject({ projectId });
    const previousBins = this.bins.map((bin) => ({ ...bin }));
    if (!previousBins.some((bin) => bin.id === id)) return;
    const assets = this.assets.filter((asset) => asset.binId === id);
    const previousAssets = assets.map((asset) => this.toOrganizationAsset({ asset }));
    const nextAssets = previousAssets.map((asset) => ({ ...asset, binId: undefined }));
    await this.commitMediaOrganizationChange({
      projectId,
      previous: { bins: previousBins, assets: previousAssets },
      next: {
        bins: previousBins.filter((bin) => bin.id !== id),
        assets: nextAssets,
      },
    });
  }

  async applyMediaOrganizationSnapshot({
    projectId,
    snapshot,
  }: {
    projectId: string;
    snapshot: MediaOrganizationSnapshot;
  }): Promise<void> {
    try {
      await storageService.saveMediaLibrarySnapshot({ projectId, snapshot });
      if (this.loadedProjectId !== projectId) return;
      const organizationById = new Map(snapshot.assets.map((asset) => [asset.id, asset]));
      this.assets = this.assets.map((asset) => {
        const organization = organizationById.get(asset.id);
        return organization
          ? {
              ...asset,
              binId: organization.binId,
              tags: [...organization.tags],
              favorite: organization.favorite,
            }
          : asset;
      });
      this.bins = snapshot.bins.map((bin) => ({ ...bin }));
      this.notify();
    } catch (error) {
      console.error("Failed to update media organization:", error);
      throw error;
    }
  }

  private assertLoadedProject({ projectId }: { projectId: string }): void {
    if (this.loadedProjectId !== projectId) {
      throw new Error("The active project's media library is still loading");
    }
  }

  private toOrganizationAsset({ asset }: { asset: MediaAsset }): MediaOrganizationAsset {
    return {
      id: asset.id,
      binId: asset.binId,
      tags: [...(asset.tags ?? [])],
      favorite: asset.favorite ?? false,
    };
  }

  private async commitMediaOrganizationChange({
    projectId,
    previous,
    next,
  }: {
    projectId: string;
    previous: MediaOrganizationSnapshot;
    next: MediaOrganizationSnapshot;
  }): Promise<void> {
    const sameBins = JSON.stringify(previous.bins) === JSON.stringify(next.bins);
    const sameAssets = JSON.stringify(previous.assets) === JSON.stringify(next.assets);
    if (sameBins && sameAssets) return;
    await this.applyMediaOrganizationSnapshot({ projectId, snapshot: next });
    this.editor.command.push({
      command: new UpdateMediaOrganizationCommand({ projectId, previous, next }),
    });
  }

  setAssets({ assets }: { assets: MediaAsset[] }): void {
    this.assets = assets;
    this.notify();
  }

  isLoadingMedia(): boolean {
    return this.isLoading;
  }

  subscribe(listener: () => void): () => void {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  private notify(): void {
    this.listeners.forEach((fn) => {
      fn();
    });
  }
}
