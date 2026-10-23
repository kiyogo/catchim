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

export const vignetteEffectDefinition: EffectDefinition = {
	type: "vignette",
	name: "Vignette",
	keywords: ["vignette", "edge", "focus", "darken"],
	category: "stylize",
	params: [
		{ key: "amount", label: "Amount", type: "number", default: 0.45, min: 0, max: 1, step: 0.01 },
		{ key: "softness", label: "Softness", type: "number", default: 0.55, min: 0.01, max: 1, step: 0.01 },
		{ key: "roundness", label: "Roundness", type: "number", default: 1, min: 0, max: 1, step: 0.01 },
	],
	renderer: {
		passes: [
			{
				shader: "vignette",
				uniforms: ({ effectParams }) => ({
					amount: readNumber({ params: effectParams, key: "amount", defaultValue: 0.45 }),
					softness: readNumber({ params: effectParams, key: "softness", defaultValue: 0.55 }),
					roundness: readNumber({ params: effectParams, key: "roundness", defaultValue: 1 }),
				}),
			},
		],
	},
};
