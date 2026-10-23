import { EditorCore } from "@/core";
import { Command } from "@/commands/base-command";

export class RenameMediaAssetCommand extends Command {
	private readonly projectId: string;
	private readonly assetId: string;
	private readonly previousName: string;
	private readonly nextName: string;

	constructor({
		projectId,
		assetId,
		previousName,
		nextName,
	}: {
		projectId: string;
		assetId: string;
		previousName: string;
		nextName: string;
	}) {
		super();
		this.projectId = projectId;
		this.assetId = assetId;
		this.previousName = previousName;
		this.nextName = nextName;
	}

	execute(): undefined {
		void EditorCore.getInstance().media.applyMediaAssetNameSnapshot({
			projectId: this.projectId,
			assetId: this.assetId,
			name: this.nextName,
		});
		return undefined;
	}

	undo(): void {
		void EditorCore.getInstance().media.applyMediaAssetNameSnapshot({
			projectId: this.projectId,
			assetId: this.assetId,
			name: this.previousName,
		});
	}
}
