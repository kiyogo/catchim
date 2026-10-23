import { EditorCore } from "@/core";
import { Command } from "@/commands/base-command";
import type { MediaAsset } from "@/media/types";

export class RelinkMediaAssetCommand extends Command {
  private readonly projectId: string;
  private readonly previousAsset: MediaAsset;
  private readonly replacementAsset: MediaAsset;

  constructor({
    projectId,
    previousAsset,
    replacementAsset,
  }: {
    projectId: string;
    previousAsset: MediaAsset;
    replacementAsset: MediaAsset;
  }) {
    super();
    this.projectId = projectId;
    this.previousAsset = { ...previousAsset, url: undefined };
    this.replacementAsset = { ...replacementAsset, url: undefined };
  }

  execute(): undefined {
    void EditorCore.getInstance().media.applyRelinkMediaAssetSnapshot({
      projectId: this.projectId,
      asset: this.replacementAsset,
    });
    return undefined;
  }

  undo(): void {
    void EditorCore.getInstance().media.applyRelinkMediaAssetSnapshot({
      projectId: this.projectId,
      asset: this.previousAsset,
    });
  }
}
