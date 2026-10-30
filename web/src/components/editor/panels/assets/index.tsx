"use client";

import { Separator } from "@/components/ui/separator";
import {
	type Tab,
	useAssetsPanelStore,
} from "@/components/editor/panels/assets/assets-panel-store";
import { TabBar } from "./tabbar";
import { Captions } from "@/subtitles/components/assets-view";
import { MediaView } from "./views/assets";
import { SettingsView } from "./views/settings";
import { SoundsView } from "@/sounds/components/assets-view";
import { StickersView } from "@/stickers/components/assets-view";
import { TextView } from "@/text/components/assets-view";
import { EffectsView } from "@/effects/components/assets-view";
import { AdjustmentView } from "@/effects/components/adjustment-view";
import { TransitionsView } from "@/transitions/transitions-view";
import { cn } from "@/utils/ui";
import dynamic from "next/dynamic";

const McpWorkspace = dynamic(
	() => import("@/mcp/workspace").then((module) => module.McpWorkspace),
	{ ssr: false },
);

export function AssetsPanel({
	showTabBar = true,
	className,
}: {
	showTabBar?: boolean;
	className?: string;
}) {
	const { activeTab } = useAssetsPanelStore();

	const viewMap: Record<Tab, React.ReactNode> = {
		mcp: <McpWorkspace />,
		media: <MediaView />,
		sounds: <SoundsView />,
		text: <TextView />,
		stickers: <StickersView />,
		effects: <EffectsView />,
		transitions: <TransitionsView />,
		captions: <Captions />,
		adjustment: <AdjustmentView />,
		settings: <SettingsView />,
	};

	return (
		<div
			className={cn(
				"panel bg-background flex h-full overflow-hidden rounded-sm border",
				className,
			)}
		>
			{showTabBar && (
				<>
					<TabBar />
					<Separator orientation="vertical" />
				</>
			)}
			<div className="flex-1 overflow-hidden">{viewMap[activeTab]}</div>
		</div>
	);
}
