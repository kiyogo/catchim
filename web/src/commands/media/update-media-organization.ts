import { EditorCore } from "@/core";
import { Command } from "@/commands/base-command";
import type { MediaOrganizationSnapshot } from "@/media/types";

export class UpdateMediaOrganizationCommand extends Command {
	private readonly projectId: string;
	private readonly previous: MediaOrganizationSnapshot;
	private readonly next: MediaOrganizationSnapshot;

	constructor({
		projectId,
		previous,
		next,
	}: {
		projectId: string;
		previous: MediaOrganizationSnapshot;
		next: MediaOrganizationSnapshot;
	}) {
		super();
		this.projectId = projectId;
		this.previous = structuredClone(previous);
		this.next = structuredClone(next);
	}

	execute(): undefined {
		void EditorCore.getInstance().media.applyMediaOrganizationSnapshot({
			projectId: this.projectId,
			snapshot: this.next,
		}).catch(() => undefined);
		return undefined;
	}

	undo(): void {
		void EditorCore.getInstance().media.applyMediaOrganizationSnapshot({
			projectId: this.projectId,
			snapshot: this.previous,
		}).catch(() => undefined);
	}
}
