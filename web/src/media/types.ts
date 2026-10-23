import type { MediaAssetData } from "@/services/storage/types";

export type MediaType = "image" | "video" | "audio";

export interface MediaAsset extends Omit<
  MediaAssetData,
  "size" | "lastModified"
> {
  file: File;
  url?: string;
  missing?: boolean;
  storedSize?: number;
  storedLastModified?: number;
}

export interface MediaBin {
  id: string;
  name: string;
}

export interface MediaLibraryState {
  bins: MediaBin[];
}

export interface MediaOrganizationAsset {
  id: string;
  binId?: string;
  tags: string[];
  favorite: boolean;
}

export interface MediaOrganizationSnapshot {
  bins: MediaBin[];
  assets: MediaOrganizationAsset[];
}

export type MediaOrganizationPatch = Partial<
  Pick<MediaOrganizationAsset, "binId" | "tags" | "favorite">
>;
