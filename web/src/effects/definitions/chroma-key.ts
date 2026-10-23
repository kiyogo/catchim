import type { EffectDefinition } from "@/effects/types";

function readNumber({
	params,
	key,
	defaultValue,
}: {
	params: Record<string, unknown>;
	key: string;
	defaultValue: number;
}): number {
	const value = params[key];
	return typeof value === "number" && Number.isFinite(value)
		? value
		: defaultValue;
}

function readColorChannel({
	color,
	index,
}: {
	color: unknown;
	index: number;
}): number {
	if (typeof color !== "string") return index === 1 ? 1 : 0;
	const hex = color.replace(/^#/, "");
	const expanded = hex.length === 3 ? [...hex].map((part) => part + part).join("") : hex;
	if (!/^[0-9a-fA-F]{6}$/.test(expanded)) return index === 1 ? 1 : 0;
	return Number.parseInt(expanded.slice(index * 2, index * 2 + 2), 16) / 255;
}

export const chromaKeyEffectDefinition: EffectDefinition = {
	type: "chroma-key",
	name: "Chroma key",
	keywords: ["chroma", "key", "green screen", "background", "transparent"],
	category: "keying",
	params: [
		{ key: "keyColor", label: "Key color", type: "color", default: "#00FF00" },
		{ key: "similarity", label: "Similarity", type: "number", default: 0.4, min: 0.01, max: 1, step: 0.01 },
		{ key: "smoothness", label: "Smoothness", type: "number", default: 0.1, min: 0.001, max: 1, step: 0.01 },
		{ key: "spill", label: "Spill suppression", type: "number", default: 0.1, min: 0, max: 1, step: 0.01 },
	],
	renderer: {
		passes: [
			{
				shader: "chroma_key",
				uniforms: ({ effectParams }) => ({
					keyR: readColorChannel({ color: effectParams.keyColor, index: 0 }),
					keyG: readColorChannel({ color: effectParams.keyColor, index: 1 }),
					keyB: readColorChannel({ color: effectParams.keyColor, index: 2 }),
					similarity: readNumber({ params: effectParams, key: "similarity", defaultValue: 0.4 }),
					smoothness: readNumber({ params: effectParams, key: "smoothness", defaultValue: 0.1 }),
					spill: readNumber({ params: effectParams, key: "spill", defaultValue: 0.1 }),
				}),
			},
		],
	},
};
