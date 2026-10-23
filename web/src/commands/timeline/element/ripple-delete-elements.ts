import { DeleteElementsCommand } from "./delete-elements";

export class RippleDeleteElementsCommand extends DeleteElementsCommand {
	override readonly forcesRipple = true;
}
