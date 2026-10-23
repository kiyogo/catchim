"use client";

import Image from "next/image";
import { forwardRef, useMemo, useRef, useState } from "react";
import { PanelView } from "@/components/editor/panels/assets/views/base-panel";
import { MediaDragOverlay } from "@/components/editor/panels/assets/drag-overlay";
import { DraggableItem } from "@/components/editor/panels/assets/draggable-item";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import {
  Dialog,
  DialogBody,
  DialogContent,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog";
import { toast } from "sonner";
import {
  ContextMenu,
  ContextMenuContent,
  ContextMenuItem,
  ContextMenuTrigger,
} from "@/components/ui/context-menu";
import {
  DropdownMenu,
  DropdownMenuContent,
  DropdownMenuItem,
  DropdownMenuTrigger,
} from "@/components/ui/dropdown-menu";
import {
  Check,
  CircleAlert,
  Download,
  Folder,
  FolderPlus,
  Filter,
  Pencil,
  Replace,
  Star,
  Tag,
} from "lucide-react";
import {
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/tooltip";
import { DEFAULT_NEW_ELEMENT_DURATION } from "@/timeline/creation";
import { mediaTimeFromSeconds, type MediaTime } from "@/wasm";
import { useEditor } from "@/editor/use-editor";
import { useFileUpload } from "@/media/use-file-upload";
import { invokeAction } from "@/actions";
import { processMediaAssets } from "@/media/processing";
import { showMediaUploadToast } from "@/media/upload-toast";
import {
  SelectableItem,
  SelectableSurface,
  useSelection,
  useSelectionScope,
} from "@/selection";
import { buildElementFromMedia } from "@/timeline/element-utils";
import {
  type MediaSortKey,
  type MediaSortOrder,
  type MediaViewMode,
  useAssetsPanelStore,
} from "@/components/editor/panels/assets/assets-panel-store";
import { MASKABLE_ELEMENT_TYPES } from "@/timeline";
import type { MediaAsset, MediaBin, MediaType } from "@/media/types";
import { cn } from "@/utils/ui";
import { downloadBlob } from "@/utils/browser";
import {
  CloudUploadIcon,
  GridViewIcon,
  LeftToRightListDashIcon,
  SortingOneNineIcon,
  Image02Icon,
  MusicNote03Icon,
  Video01Icon,
} from "@hugeicons/core-free-icons";
import { HugeiconsIcon, type IconSvgElement } from "@hugeicons/react";
import { useTranslation, type TranslationKey } from "@/i18n";

const MEDIA_SORT_LABEL_KEYS: Record<MediaSortKey, TranslationKey> = {
  name: "assets.name",
  type: "assets.type",
  duration: "assets.duration",
  size: "assets.fileSize",
};

type MediaTypeFilter = "all" | MediaType;
type MediaCollection = "all" | "favorites" | "missing" | "unfiled" | `bin:${string}` | `tag:${string}`;

const MEDIA_FILTER_LABEL_KEYS: Record<MediaTypeFilter, TranslationKey> = {
  all: "assets.all",
  video: "assets.video",
  audio: "properties.audio",
  image: "assets.image",
};

export function MediaView() {
  const { t } = useTranslation();
  const editor = useEditor();
  const mediaFiles = useEditor((e) => e.media.getAssets());
  const activeProject = useEditor((e) => e.project.getActive());

  const {
    mediaViewMode,
    setMediaViewMode,
    highlightMediaId,
    clearHighlight,
    mediaSortBy,
    mediaSortOrder,
    setMediaSort,
  } = useAssetsPanelStore();

  const [isProcessing, setIsProcessing] = useState(false);
  const [progress, setProgress] = useState(0);
  const [searchQuery, setSearchQuery] = useState("");
  const [mediaTypeFilter, setMediaTypeFilter] =
    useState<MediaTypeFilter>("all");
  const [collection, setCollection] = useState<MediaCollection>("all");
  const [tagDraft, setTagDraft] = useState("");
  const [binDialog, setBinDialog] = useState<"create" | MediaBin | null>(null);
  const [binDraft, setBinDraft] = useState("");
  const bins = useEditor((e) => e.media.getBins());
  const mediaTags = useMemo(
    () => [...new Set(mediaFiles.flatMap((item) => item.tags ?? []))].sort((a, b) => a.localeCompare(b)),
    [mediaFiles],
  );

  const processFiles = async ({ files }: { files: File[] }) => {
    if (!files || files.length === 0) return;
    if (!activeProject) {
      toast.error(t("assets.noActiveProject"));
      return;
    }

    setIsProcessing(true);
    setProgress(0);
    try {
      await showMediaUploadToast({
        filesCount: files.length,
        promise: async () => {
          const processedAssets = await processMediaAssets({
            files,
            onProgress: (progress: { progress: number }) =>
              setProgress(progress.progress),
          });
          for (const asset of processedAssets) {
            await editor.media.addMediaAsset({
              projectId: activeProject.metadata.id,
              asset,
            });
          }
          return {
            uploadedCount: processedAssets.length,
            assetNames: processedAssets.map((asset) => asset.name),
          };
        },
      });
    } catch (error) {
      console.error("Error processing files:", error);
    } finally {
      setIsProcessing(false);
      setProgress(0);
    }
  };

  const { isDragOver, dragProps, openFilePicker, fileInputProps } =
    useFileUpload({
      accept: "image/*,video/*,audio/*",
      multiple: true,
      onFilesSelected: (files) => processFiles({ files }),
    });

  const handleRemove = ({
    event,
    ids,
  }: {
    event: React.MouseEvent;
    ids: string[];
  }) => {
    event.stopPropagation();

    invokeAction("remove-media-assets", {
      projectId: activeProject.metadata.id,
      assetIds: ids,
    });
  };

  const handleSort = ({ key }: { key: MediaSortKey }) => {
    if (mediaSortBy === key) {
      setMediaSort({
        key,
        order: mediaSortOrder === "asc" ? "desc" : "asc",
      });
    } else {
      setMediaSort({ key, order: "asc" });
    }
  };

  const handleRename = async ({
    item,
    name,
  }: {
    item: MediaAsset;
    name: string;
  }): Promise<boolean> => {
    if (!activeProject) return false;
    try {
      await editor.media.renameMediaAsset({
        projectId: activeProject.metadata.id,
        id: item.id,
        name,
      });
      return true;
    } catch (error) {
      toast.error(t("assets.renameFailed"), {
        description: error instanceof Error ? error.message : undefined,
      });
      return false;
    }
  };

  const handleReplace = async ({
    item,
    file,
  }: {
    item: MediaAsset;
    file: File;
  }) => {
    if (!activeProject) return;
    setIsProcessing(true);
    try {
      const [replacement] = await processMediaAssets({ files: [file] });
      if (!replacement) return;
      if (item.missing) {
        await editor.media.relinkMediaAsset({
          projectId: activeProject.metadata.id,
          id: item.id,
          replacement,
        });
        toast.success(t("assets.mediaRelinked"));
      } else {
        await editor.media.replaceMediaAsset({
          projectId: activeProject.metadata.id,
          id: item.id,
          replacement,
        });
        toast.success(t("assets.mediaReplaced"));
      }
    } catch (error) {
      const message = error instanceof Error ? error.message : "";
      const description = message.includes("same type")
        ? t("assets.replacementTypeMismatch")
        : message.includes("shorter")
          ? t("assets.replacementTooShort")
          : undefined;
      toast.error(
        item.missing ? t("assets.relinkFailed") : t("assets.replaceFailed"),
        {
          description,
        },
      );
    } finally {
      setIsProcessing(false);
    }
  };

  const handleDownload = ({ item }: { item: MediaAsset }) => {
    if (item.missing) return;
    const filename = item.name
      .replace(/[<>:"/\\|?*\u0000-\u001f]/g, "_")
      .trim();
    downloadBlob({
      blob: item.file,
      filename: filename || item.file.name || "media",
    });
  };

  const handleCreateOrRenameBin = async () => {
    if (!activeProject) return;
    try {
      if (binDialog === "create") {
        const created = await editor.media.createMediaBin({
          projectId: activeProject.metadata.id,
          name: binDraft,
        });
        setCollection(`bin:${created.id}`);
      } else if (binDialog) {
        await editor.media.renameMediaBin({
          projectId: activeProject.metadata.id,
          id: binDialog.id,
          name: binDraft,
        });
      }
      setBinDialog(null);
      setBinDraft("");
    } catch (error) {
      toast.error(error instanceof Error ? error.message : t("assets.error"));
    }
  };

  const handleDeleteBin = async (binId: string) => {
    if (!activeProject) return;
    try {
      await editor.media.deleteMediaBin({
        projectId: activeProject.metadata.id,
        id: binId,
      });
      if (collection === `bin:${binId}`) setCollection("all");
    } catch (error) {
      toast.error(error instanceof Error ? error.message : t("assets.error"));
    }
  };

  const filteredMediaItems = useMemo(() => {
    const query = searchQuery.trim().toLocaleLowerCase();
    const filtered = mediaFiles.filter((item) => {
      if (item.ephemeral) return false;
      if (collection === "favorites" && !item.favorite) return false;
      if (collection === "missing" && !item.missing) return false;
      if (collection === "unfiled" && item.binId) return false;
      if (collection.startsWith("bin:") && item.binId !== collection.slice(4)) return false;
      if (
        collection.startsWith("tag:") &&
        !(item.tags ?? []).includes(collection.slice(4))
      ) return false;
      if (mediaTypeFilter !== "all" && item.type !== mediaTypeFilter) {
        return false;
      }
      if (
        query &&
        !item.name.toLocaleLowerCase().includes(query) &&
        !item.file.name.toLocaleLowerCase().includes(query)
      ) {
        return false;
      }
      return true;
    });

    filtered.sort((a, b) => {
      let valueA: string | number;
      let valueB: string | number;

      switch (mediaSortBy) {
        case "name":
          valueA = a.name.toLowerCase();
          valueB = b.name.toLowerCase();
          break;
        case "type":
          valueA = a.type;
          valueB = b.type;
          break;
        case "duration":
          valueA = a.duration || 0;
          valueB = b.duration || 0;
          break;
        case "size":
          valueA = a.storedSize ?? a.file.size;
          valueB = b.storedSize ?? b.file.size;
          break;
        default:
          return 0;
      }

      if (valueA < valueB) return mediaSortOrder === "asc" ? -1 : 1;
      if (valueA > valueB) return mediaSortOrder === "asc" ? 1 : -1;
      return 0;
    });

    return filtered;
  }, [collection, mediaFiles, mediaSortBy, mediaSortOrder, mediaTypeFilter, searchQuery]);
  const orderedMediaIds = useMemo(() => {
    return filteredMediaItems.map((item) => item.id);
  }, [filteredMediaItems]);

  return (
    <>
      <input {...fileInputProps} />

      <PanelView
        title={t("assets.media")}
        actions={
          <MediaActions
            mediaViewMode={mediaViewMode}
            setMediaViewMode={setMediaViewMode}
            isProcessing={isProcessing}
            sortBy={mediaSortBy}
            sortOrder={mediaSortOrder}
            mediaTypeFilter={mediaTypeFilter}
            setMediaTypeFilter={setMediaTypeFilter}
            onSort={handleSort}
            onImport={openFilePicker}
          />
        }
        className={cn(isDragOver && "bg-accent/30")}
        contentClassName="h-full"
        {...dragProps}
      >
        {isDragOver ? (
          <MediaDragOverlay
            isVisible={true}
            isProcessing={isProcessing}
            progress={progress}
            onClick={openFilePicker}
          />
        ) : (
          <div className="flex h-full flex-col">
            <div className="mb-2">
              <Input
                type="search"
                size="sm"
                value={searchQuery}
                onChange={(event) => setSearchQuery(event.target.value)}
                placeholder={t("assets.searchMedia")}
                aria-label={t("assets.searchMedia")}
              />
            </div>
            <div className="mb-2 flex flex-wrap gap-1 border-b pb-2">
              <LibraryFilterButton
                active={collection === "all"}
                label={t("assets.allMedia")}
                count={mediaFiles.filter((item) => !item.ephemeral).length}
                onClick={() => setCollection("all")}
              />
              <LibraryFilterButton
                active={collection === "favorites"}
                label={t("assets.favorites")}
                count={mediaFiles.filter((item) => !item.ephemeral && item.favorite).length}
                icon={<Star className="size-3" />}
                onClick={() => setCollection("favorites")}
              />
              <LibraryFilterButton
                active={collection === "unfiled"}
                label={t("assets.unfiled")}
                count={mediaFiles.filter((item) => !item.ephemeral && !item.binId).length}
                onClick={() => setCollection("unfiled")}
              />
              <LibraryFilterButton
                active={collection === "missing"}
                label={t("assets.missingSources")}
                count={mediaFiles.filter((item) => !item.ephemeral && item.missing).length}
                onClick={() => setCollection("missing")}
              />
              {bins.map((bin) => (
                <ContextMenu key={bin.id}>
                  <ContextMenuTrigger asChild>
                    <LibraryFilterButton
                      active={collection === `bin:${bin.id}`}
                      label={bin.name}
                      count={mediaFiles.filter((item) => !item.ephemeral && item.binId === bin.id).length}
                      icon={<Folder className="size-3" />}
                      onClick={() => setCollection(`bin:${bin.id}`)}
                    />
                  </ContextMenuTrigger>
                  <ContextMenuContent>
                    <ContextMenuItem
                      icon={<Pencil className="size-4" />}
                      onClick={() => {
                        setBinDraft(bin.name);
                        setBinDialog(bin);
                      }}
                    >
                      {t("assets.renameBin")}
                    </ContextMenuItem>
                    <ContextMenuItem
                      variant="destructive"
                      onClick={() => void handleDeleteBin(bin.id)}
                    >
                      {t("assets.deleteBin")}
                    </ContextMenuItem>
                  </ContextMenuContent>
                </ContextMenu>
              ))}
              <Button
                size="sm"
                variant="ghost"
                className="h-7 gap-1 px-2 text-xs"
                onClick={() => {
                  setBinDraft("");
                  setBinDialog("create");
                }}
              >
                <FolderPlus className="size-3.5" />
                {t("assets.addBin")}
              </Button>
              {mediaTags.map((tag) => (
                <LibraryFilterButton
                  key={tag}
                  active={collection === `tag:${tag}`}
                  label={tag}
                  count={mediaFiles.filter((item) => !item.ephemeral && (item.tags ?? []).includes(tag)).length}
                  icon={<Tag className="size-3" />}
                  onClick={() => setCollection(`tag:${tag}`)}
                />
              ))}
            </div>
            {mediaFiles.every((item) => item.ephemeral) ? (
              <MediaDragOverlay
                isVisible={true}
                isProcessing={isProcessing}
                progress={progress}
                onClick={openFilePicker}
              />
            ) : filteredMediaItems.length === 0 ? (
              <div className="text-muted-foreground flex flex-1 flex-col items-center justify-center gap-2 text-center text-sm">
                <span>{t("assets.noMediaMatches")}</span>
                <Button
                  variant="ghost"
                  size="sm"
                  onClick={() => {
                    setSearchQuery("");
                    setMediaTypeFilter("all");
                    setCollection("all");
                  }}
                >
                  {t("assets.clearMediaFilters")}
                </Button>
              </div>
            ) : (
              <SelectableSurface
                ariaLabel={t("assets.media")}
                orderedIds={orderedMediaIds}
                revealId={highlightMediaId}
                onRevealComplete={clearHighlight}
              >
                <MediaScopeRegistrar />
                {activeProject && (
                  <MediaBulkActions
                    projectId={activeProject.metadata.id}
                    bins={bins}
                    selectedTag={collection.startsWith("tag:") ? collection.slice(4) : null}
                    tagDraft={tagDraft}
                    setTagDraft={setTagDraft}
                  />
                )}
                <MediaItemList
                  items={filteredMediaItems}
                  mode={mediaViewMode}
                  onRemove={handleRemove}
                  onRename={handleRename}
                  onReplace={handleReplace}
                  onDownload={handleDownload}
                />
              </SelectableSurface>
            )}
          </div>
        )}
      </PanelView>
      <Dialog
        open={binDialog !== null}
        onOpenChange={(open) => {
          if (!open) setBinDialog(null);
        }}
      >
        <DialogContent>
          <DialogHeader>
            <DialogTitle>
              {binDialog === "create" ? t("assets.createBin") : t("assets.renameBin")}
            </DialogTitle>
          </DialogHeader>
          <DialogBody>
            <Label htmlFor="media-bin-name">{t("assets.binName")}</Label>
            <Input
              id="media-bin-name"
              value={binDraft}
              onChange={(event) => setBinDraft(event.target.value)}
              onKeyDown={(event) => {
                if (event.key === "Enter") {
                  event.preventDefault();
                  void handleCreateOrRenameBin();
                }
              }}
            />
          </DialogBody>
          <DialogFooter>
            <Button variant="outline" onClick={() => setBinDialog(null)}>
              {t("common.cancel")}
            </Button>
            <Button disabled={!binDraft.trim()} onClick={() => void handleCreateOrRenameBin()}>
              {t("common.save")}
            </Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>
    </>
  );
}

type LibraryFilterButtonProps = React.ComponentPropsWithoutRef<typeof Button> & {
  active: boolean;
  label: string;
  count: number;
  icon?: React.ReactNode;
};

const LibraryFilterButton = forwardRef<HTMLButtonElement, LibraryFilterButtonProps>(function LibraryFilterButton({
  active,
  label,
  count,
  icon,
  className,
  ...props
}, ref) {
  return (
    <Button
      {...props}
      ref={ref}
      size="sm"
      variant={active ? "secondary" : "ghost"}
      className={cn("h-7 max-w-full gap-1 px-2 text-xs", className)}
      title={`${label} (${count})`}
    >
      {icon}
      <span className="max-w-28 truncate">{label}</span>
      <span className="text-muted-foreground">{count}</span>
    </Button>
  );
});

function MediaBulkActions({
  projectId,
  bins,
  selectedTag,
  tagDraft,
  setTagDraft,
}: {
  projectId: string;
  bins: MediaBin[];
  selectedTag: string | null;
  tagDraft: string;
  setTagDraft: (value: string) => void;
}) {
  const { t } = useTranslation();
  const editor = useEditor();
  const { selectedIds, clearSelection } = useSelection();
  const selectedAssets = selectedIds
    .map((id) => editor.media.getAssets().find((asset) => asset.id === id))
    .filter((asset): asset is MediaAsset => Boolean(asset));
  if (selectedAssets.length === 0) return null;

  const update = (patch: { binId?: string; tags?: string[]; favorite?: boolean }) =>
    editor.media.updateMediaOrganization({
      projectId,
      ids: selectedAssets.map((asset) => asset.id),
      patch,
    }).catch((error) => {
      toast.error(error instanceof Error ? error.message : t("assets.error"));
    });

  const applyTag = () => {
    const nextTag = tagDraft.trim();
    if (!nextTag) return;
    void editor.media.updateMediaOrganizationBatch({
      projectId,
      updates: selectedAssets.map((asset) => ({
        id: asset.id,
        patch: { tags: [...new Set([...(asset.tags ?? []), nextTag])] },
      })),
    }).then(() => setTagDraft("")).catch((error) => {
      toast.error(error instanceof Error ? error.message : t("assets.error"));
    });
  };

  const allFavorite = selectedAssets.every((asset) => asset.favorite);
  return (
    <div
      className="mb-2 flex flex-wrap items-center gap-1.5 rounded-md border bg-muted/40 p-1.5"
      // Toolbar controls and their portaled menus must not clear the selection
      // or start marquee selection in the surrounding SelectableSurface.
      onMouseDown={(event) => event.stopPropagation()}
      onClick={(event) => event.stopPropagation()}
    >
      <span className="text-muted-foreground px-1 text-xs">
        {t("assets.selectedMediaCount", { count: selectedAssets.length })}
      </span>
      <DropdownMenu>
        <DropdownMenuTrigger asChild>
          <Button size="sm" variant="outline" className="h-7 gap-1 px-2 text-xs">
            <Folder className="size-3.5" />
            {t("assets.moveToBin")}
          </Button>
        </DropdownMenuTrigger>
        <DropdownMenuContent align="start">
          <DropdownMenuItem onClick={() => void update({ binId: undefined })}>
            {t("assets.unfiled")}
          </DropdownMenuItem>
          {bins.map((bin) => (
            <DropdownMenuItem key={bin.id} onClick={() => void update({ binId: bin.id })}>
              {bin.name}
            </DropdownMenuItem>
          ))}
        </DropdownMenuContent>
      </DropdownMenu>
      <div className="flex items-center gap-1">
        <Input
          size="sm"
          value={tagDraft}
          onChange={(event) => setTagDraft(event.target.value)}
          onKeyDown={(event) => {
            if (event.key === "Enter") {
              event.preventDefault();
              applyTag();
            }
          }}
          placeholder={t("assets.tagName")}
          aria-label={t("assets.tagName")}
          className="h-7 w-28"
        />
        <Button size="sm" variant="outline" className="h-7 px-2 text-xs" disabled={!tagDraft.trim()} onClick={applyTag}>
          <Tag className="mr-1 size-3.5" />
          {t("assets.addTag")}
        </Button>
      </div>
      {selectedTag && (
        <Button
          size="sm"
          variant="ghost"
          className="h-7 px-2 text-xs"
          onClick={() => {
            void editor.media.updateMediaOrganizationBatch({
              projectId,
              updates: selectedAssets.map((asset) => ({
                id: asset.id,
                patch: { tags: (asset.tags ?? []).filter((tag) => tag !== selectedTag) },
              })),
            }).catch((error) => {
              toast.error(error instanceof Error ? error.message : t("assets.error"));
            });
          }}
        >
          {t("assets.removeTag")}
        </Button>
      )}
      <Button
        size="sm"
        variant="ghost"
        className="h-7 gap-1 px-2 text-xs"
        onClick={() => void update({ favorite: !allFavorite })}
        title={allFavorite ? t("assets.unfavoriteMedia") : t("assets.favoriteMedia")}
      >
        <Star className="size-3.5" />
        {allFavorite ? t("assets.unfavoriteMedia") : t("assets.favoriteMedia")}
      </Button>
      <Button size="sm" variant="ghost" className="ml-auto h-7 px-2 text-xs" onClick={clearSelection}>
        {t("common.cancel")}
      </Button>
    </div>
  );
}

function MediaScopeRegistrar() {
  useSelectionScope();
  return null;
}

function MediaAssetDraggable({
  item,
  preview,
  variant,
  isRounded,
}: {
  item: MediaAsset;
  preview: React.ReactNode;
  variant: "card" | "compact";
  isRounded?: boolean;
}) {
  const editor = useEditor();

  const addElementAtTime = ({
    asset,
    startTime,
  }: {
    asset: MediaAsset;
    startTime: MediaTime;
  }) => {
    const duration =
      asset.duration != null
        ? mediaTimeFromSeconds({ seconds: asset.duration })
        : DEFAULT_NEW_ELEMENT_DURATION;
    const element = buildElementFromMedia({
      mediaId: asset.id,
      mediaType: asset.type,
      name: asset.name,
      duration,
      startTime,
    });
    editor.timeline.insertElement({
      element,
      placement: { mode: "auto" },
    });
  };

  const taxonomyBadge = item.favorite || (item.tags?.length ?? 0) > 0;

  return (
    <DraggableItem
      name={item.name}
      preview={
        taxonomyBadge ? (
          <div className="relative size-full">
            {preview}
            <div className="absolute top-1 left-1 flex max-w-[calc(100%-0.5rem)] items-center gap-1 rounded bg-black/70 px-1.5 py-0.5 text-[10px] text-white">
              {item.favorite && <Star className="size-3 fill-amber-300 text-amber-300" />}
              {item.tags?.[0] && <span className="truncate">{item.tags[0]}</span>}
              {item.tags && item.tags.length > 1 && <span>+{item.tags.length - 1}</span>}
            </div>
          </div>
        ) : preview
      }
      dragData={{
        id: item.id,
        type: "media",
        mediaType: item.type,
        name: item.name,
        ...(item.type !== "audio" && {
          targetElementTypes: [...MASKABLE_ELEMENT_TYPES],
        }),
      }}
      shouldShowPlusOnDrag={false}
      onAddToTimeline={
        item.missing
          ? undefined
          : ({ currentTime }) =>
              addElementAtTime({ asset: item, startTime: currentTime })
      }
      isDraggable={!item.missing}
      variant={variant}
      isRounded={isRounded}
    />
  );
}

function MediaItemWithContextMenu({
  item,
  children,
  onRemove,
  onRename,
  onReplace,
  onDownload,
}: {
  item: MediaAsset;
  children: React.ReactNode;
  onRemove: ({
    event,
    ids,
  }: {
    event: React.MouseEvent;
    ids: string[];
  }) => void;
  onRename: (args: { item: MediaAsset; name: string }) => Promise<boolean>;
  onReplace: (args: { item: MediaAsset; file: File }) => Promise<void>;
  onDownload: (args: { item: MediaAsset }) => void;
}) {
  const { t } = useTranslation();
  const editor = useEditor();
  const activeProject = useEditor((currentEditor) => currentEditor.project.getActiveOrNull());
  const { isSelected, selectedIds } = useSelection();
  const [isRenameOpen, setIsRenameOpen] = useState(false);
  const [isSavingRename, setIsSavingRename] = useState(false);
  const [draftName, setDraftName] = useState(item.name);
  const replacementInputRef = useRef<HTMLInputElement>(null);
  const idsToDelete = isSelected(item.id) ? selectedIds : [item.id];
  const deleteLabel =
    idsToDelete.length > 1
      ? t("timeline.deleteElements", { count: idsToDelete.length })
      : t("timeline.deleteElement");
  const saveRename = async () => {
    if (isSavingRename) return;
    setIsSavingRename(true);
    try {
      if (await onRename({ item, name: draftName })) setIsRenameOpen(false);
    } finally {
      setIsSavingRename(false);
    }
  };
  const toggleFavorite = () => {
    if (!activeProject) return;
    void editor.media.updateMediaOrganization({
      projectId: activeProject.metadata.id,
      ids: [item.id],
      patch: { favorite: !item.favorite },
    }).catch((error) => {
      toast.error(error instanceof Error ? error.message : t("assets.error"));
    });
  };

  return (
    <>
      <input
        ref={replacementInputRef}
        type="file"
        accept={`${item.type}/*`}
        className="hidden"
        aria-label={t("assets.replaceFile")}
        onChange={(event) => {
          const file = event.target.files?.[0];
          if (file) void onReplace({ item, file });
          event.currentTarget.value = "";
        }}
      />
      <ContextMenu>
        <ContextMenuTrigger asChild>{children}</ContextMenuTrigger>
        <ContextMenuContent>
          <ContextMenuItem
            icon={<Star className={cn("size-4", item.favorite && "fill-amber-300 text-amber-300")} />}
            onClick={toggleFavorite}
          >
            {item.favorite ? t("assets.unfavoriteMedia") : t("assets.favoriteMedia")}
          </ContextMenuItem>
          <ContextMenuItem
            icon={<Pencil className="size-4" />}
            onClick={() => {
              setDraftName(item.name);
              setIsRenameOpen(true);
            }}
          >
            {t("assets.renameMedia")}
          </ContextMenuItem>
          <ContextMenuItem
            icon={<Replace className="size-4" />}
            onClick={() => replacementInputRef.current?.click()}
          >
            {item.missing ? t("assets.relinkFile") : t("assets.replaceFile")}
          </ContextMenuItem>
          {!item.missing && (
            <ContextMenuItem
              icon={<Download className="size-4" />}
              onClick={() => onDownload({ item })}
            >
              {t("assets.downloadSource")}
            </ContextMenuItem>
          )}
          <ContextMenuItem
            variant="destructive"
            onClick={(event: React.MouseEvent<HTMLDivElement>) =>
              onRemove({ event, ids: idsToDelete })
            }
          >
            {deleteLabel}
          </ContextMenuItem>
        </ContextMenuContent>
      </ContextMenu>
      <Dialog open={isRenameOpen} onOpenChange={setIsRenameOpen}>
        <DialogContent>
          <DialogHeader>
            <DialogTitle>{t("assets.renameMedia")}</DialogTitle>
          </DialogHeader>
          <DialogBody>
            <Label htmlFor={`media-name-${item.id}`}>{t("assets.name")}</Label>
            <Input
              id={`media-name-${item.id}`}
              value={draftName}
              onChange={(event) => setDraftName(event.target.value)}
              onKeyDown={(event) => {
                if (event.key === "Enter") {
                  event.preventDefault();
                  void saveRename();
                }
              }}
            />
          </DialogBody>
          <DialogFooter>
            <Button variant="outline" onClick={() => setIsRenameOpen(false)}>
              {t("common.cancel")}
            </Button>
            <Button
              disabled={!draftName.trim() || isSavingRename}
              onClick={() => void saveRename()}
            >
              {t("common.save")}
            </Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>
    </>
  );
}

function MediaItemList({
  items,
  mode,
  onRemove,
  onRename,
  onReplace,
  onDownload,
}: {
  items: MediaAsset[];
  mode: MediaViewMode;
  onRemove: ({
    event,
    ids,
  }: {
    event: React.MouseEvent;
    ids: string[];
  }) => void;
  onRename: (args: { item: MediaAsset; name: string }) => Promise<boolean>;
  onReplace: (args: { item: MediaAsset; file: File }) => Promise<void>;
  onDownload: (args: { item: MediaAsset }) => void;
}) {
  const isGrid = mode === "grid";

  return (
    <div
      className={cn(isGrid ? "grid gap-4" : "flex flex-col gap-1.5")}
      style={
        isGrid ? { gridTemplateColumns: "repeat(auto-fill, 7rem)" } : undefined
      }
    >
      {items.map((item) => (
        <MediaItemWithContextMenu
          item={item}
          onRemove={onRemove}
          onRename={onRename}
          onReplace={onReplace}
          onDownload={onDownload}
          key={item.id}
        >
          <SelectableItem className={cn(!isGrid && "w-full")} id={item.id}>
            <MediaAssetDraggable
              item={item}
              preview={
                <MediaPreview
                  item={item}
                  variant={isGrid ? "grid" : "compact"}
                />
              }
              variant={isGrid ? "card" : "compact"}
              isRounded={isGrid ? false : undefined}
            />
          </SelectableItem>
        </MediaItemWithContextMenu>
      ))}
    </div>
  );
}

function formatDuration({ duration }: { duration: number }) {
  const min = Math.floor(duration / 60);
  const sec = Math.floor(duration % 60);
  return `${min}:${sec.toString().padStart(2, "0")}`;
}

function MediaDurationBadge({ duration }: { duration?: number }) {
  if (!duration) return null;

  return (
    <div className="absolute right-1 bottom-1 rounded bg-black/70 px-1 text-xs text-white">
      {formatDuration({ duration })}
    </div>
  );
}

function MediaDurationLabel({ duration }: { duration?: number }) {
  if (!duration) return null;

  return (
    <span className="text-xs opacity-70">{formatDuration({ duration })}</span>
  );
}

function MediaTypePlaceholder({
  icon,
  label,
  duration,
  variant,
}: {
  icon: IconSvgElement;
  label: string;
  duration?: number;
  variant: "muted" | "bordered";
}) {
  const iconClassName = cn("size-6", variant === "bordered" && "mb-1");

  return (
    <div
      className={cn(
        "text-muted-foreground flex size-full flex-col items-center justify-center rounded",
        variant === "muted" ? "bg-muted/30" : "border",
      )}
    >
      <HugeiconsIcon icon={icon} className={iconClassName} />
      <span className="text-xs">{label}</span>
      <MediaDurationLabel duration={duration} />
    </div>
  );
}

function MediaPreview({
  item,
  variant = "grid",
}: {
  item: MediaAsset;
  variant?: "grid" | "compact";
}) {
  const { t } = useTranslation();
  const shouldShowDurationBadge = variant === "grid";
  if (item.missing) {
    return (
      <div className="text-destructive flex size-full flex-col items-center justify-center gap-1 rounded bg-destructive/10 p-2">
        <CircleAlert className="size-5" />
        <span className="text-center text-xs">{t("assets.missingMedia")}</span>
        <MediaDurationLabel duration={item.duration} />
      </div>
    );
  }

  if (item.type === "image") {
    return (
      <div className="relative flex size-full items-center justify-center bg-muted">
        <Image
          src={item.url ?? ""}
          alt={item.name}
          fill
          sizes="100vw"
          className="object-cover"
          loading="lazy"
          unoptimized
        />
      </div>
    );
  }

  if (item.type === "video") {
    if (item.thumbnailUrl) {
      return (
        <div className="relative size-full">
          <Image
            src={item.thumbnailUrl}
            alt={item.name}
            fill
            sizes="100vw"
            className="rounded object-cover"
            loading="lazy"
            unoptimized
          />
          {shouldShowDurationBadge ? (
            <MediaDurationBadge duration={item.duration} />
          ) : null}
        </div>
      );
    }

    return (
      <MediaTypePlaceholder
        icon={Video01Icon}
        label={t("assets.video")}
        duration={item.duration}
        variant="muted"
      />
    );
  }

  if (item.type === "audio") {
    return (
      <MediaTypePlaceholder
        icon={MusicNote03Icon}
        label={t("properties.audio")}
        duration={item.duration}
        variant="bordered"
      />
    );
  }

  return (
    <MediaTypePlaceholder
      icon={Image02Icon}
      label={t("assets.unknown")}
      variant="muted"
    />
  );
}

function MediaActions({
  mediaViewMode,
  setMediaViewMode,
  isProcessing,
  sortBy,
  sortOrder,
  mediaTypeFilter,
  setMediaTypeFilter,
  onSort,
  onImport,
}: {
  mediaViewMode: MediaViewMode;
  setMediaViewMode: (mode: MediaViewMode) => void;
  isProcessing: boolean;
  sortBy: MediaSortKey;
  sortOrder: MediaSortOrder;
  mediaTypeFilter: MediaTypeFilter;
  setMediaTypeFilter: (filter: MediaTypeFilter) => void;
  onSort: ({ key }: { key: MediaSortKey }) => void;
  onImport: () => void;
}) {
  const { t } = useTranslation();

  return (
    <div className="flex gap-1.5">
      <TooltipProvider>
        <DropdownMenu>
          <Tooltip>
            <TooltipTrigger asChild>
              <DropdownMenuTrigger asChild>
                <Button
                  size="icon"
                  variant="ghost"
                  disabled={isProcessing}
                  className="items-center justify-center"
                  aria-label={t(MEDIA_FILTER_LABEL_KEYS[mediaTypeFilter])}
                >
                  <Filter className="size-4" />
                </Button>
              </DropdownMenuTrigger>
            </TooltipTrigger>
            <TooltipContent>
              <p>{t(MEDIA_FILTER_LABEL_KEYS[mediaTypeFilter])}</p>
            </TooltipContent>
          </Tooltip>
          <DropdownMenuContent align="end">
            {(Object.keys(MEDIA_FILTER_LABEL_KEYS) as MediaTypeFilter[]).map(
              (filter) => (
                <DropdownMenuItem
                  key={filter}
                  onClick={() => setMediaTypeFilter(filter)}
                >
                  <span className="flex-1">
                    {t(MEDIA_FILTER_LABEL_KEYS[filter])}
                  </span>
                  {mediaTypeFilter === filter && <Check className="size-4" />}
                </DropdownMenuItem>
              ),
            )}
          </DropdownMenuContent>
        </DropdownMenu>
        <Tooltip>
          <TooltipTrigger asChild>
            <Button
              size="icon"
              variant="ghost"
              onClick={() =>
                setMediaViewMode(mediaViewMode === "grid" ? "list" : "grid")
              }
              disabled={isProcessing}
              className="items-center justify-center"
            >
              {mediaViewMode === "grid" ? (
                <HugeiconsIcon icon={LeftToRightListDashIcon} />
              ) : (
                <HugeiconsIcon icon={GridViewIcon} />
              )}
            </Button>
          </TooltipTrigger>
          <TooltipContent>
            <p>
              {mediaViewMode === "grid"
                ? t("assets.listView")
                : t("assets.gridView")}
            </p>
          </TooltipContent>
        </Tooltip>
        <Tooltip>
          <DropdownMenu>
            <TooltipTrigger asChild>
              <DropdownMenuTrigger asChild>
                <Button
                  size="icon"
                  variant="ghost"
                  disabled={isProcessing}
                  className="items-center justify-center"
                >
                  <HugeiconsIcon icon={SortingOneNineIcon} />
                </Button>
              </DropdownMenuTrigger>
            </TooltipTrigger>
            <DropdownMenuContent align="end">
              <SortMenuItem
                label={t(MEDIA_SORT_LABEL_KEYS.name)}
                sortKey="name"
                currentSortBy={sortBy}
                currentSortOrder={sortOrder}
                onSort={onSort}
              />
              <SortMenuItem
                label={t(MEDIA_SORT_LABEL_KEYS.type)}
                sortKey="type"
                currentSortBy={sortBy}
                currentSortOrder={sortOrder}
                onSort={onSort}
              />
              <SortMenuItem
                label={t(MEDIA_SORT_LABEL_KEYS.duration)}
                sortKey="duration"
                currentSortBy={sortBy}
                currentSortOrder={sortOrder}
                onSort={onSort}
              />
              <SortMenuItem
                label={t(MEDIA_SORT_LABEL_KEYS.size)}
                sortKey="size"
                currentSortBy={sortBy}
                currentSortOrder={sortOrder}
                onSort={onSort}
              />
            </DropdownMenuContent>
          </DropdownMenu>
          <TooltipContent>
            <p>
              {t("assets.sortBy", {
                label: t(MEDIA_SORT_LABEL_KEYS[sortBy]),
                direction:
                  sortOrder === "asc"
                    ? t("timeline.ascending")
                    : t("timeline.descending"),
              })}
            </p>
          </TooltipContent>
        </Tooltip>
      </TooltipProvider>
      <Button
        variant="outline"
        onClick={onImport}
        disabled={isProcessing}
        size="sm"
        className="items-center justify-center gap-1.5"
      >
        <HugeiconsIcon icon={CloudUploadIcon} />
        {t("assets.import")}
      </Button>
    </div>
  );
}

function SortMenuItem({
  label,
  sortKey,
  currentSortBy,
  currentSortOrder,
  onSort,
}: {
  label: string;
  sortKey: MediaSortKey;
  currentSortBy: MediaSortKey;
  currentSortOrder: MediaSortOrder;
  onSort: ({ key }: { key: MediaSortKey }) => void;
}) {
  const isActive = currentSortBy === sortKey;
  const arrow = isActive ? (currentSortOrder === "asc" ? "↑" : "↓") : "";

  return (
    <DropdownMenuItem onClick={() => onSort({ key: sortKey })}>
      {label} {arrow}
    </DropdownMenuItem>
  );
}
