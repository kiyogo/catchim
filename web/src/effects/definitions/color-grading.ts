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

export const colorGradingEffectDefinition: EffectDefinition = {
	type: "color-grading",
	name: "Color grading",
	keywords: ["color", "grade", "brightness", "contrast", "exposure", "tone"],
	category: "color",
	params: [
		{ key: "brightness", label: "Brightness", type: "number", default: 0, min: -1, max: 1, step: 0.01 },
		{ key: "contrast", label: "Contrast", type: "number", default: 0, min: -1, max: 1, step: 0.01 },
		{ key: "saturation", label: "Saturation", type: "number", default: 0, min: -1, max: 2, step: 0.01 },
		{ key: "exposure", label: "Exposure", type: "number", default: 0, min: -4, max: 4, step: 0.1 },
		{ key: "temperature", label: "Temperature", type: "number", default: 0, min: -1, max: 1, step: 0.01 },
		{ key: "tint", label: "Tint", type: "number", default: 0, min: -1, max: 1, step: 0.01 },
		{ key: "hue", label: "Hue", type: "number", default: 0, min: -180, max: 180, step: 1 },
		{ key: "gamma", label: "Gamma", type: "number", default: 1, min: 0.1, max: 3, step: 0.01 },
	],
	renderer: {
		passes: [
			{
				shader: "color_grading",
				uniforms: ({ effectParams }) => ({
					brightness: readNumber({ params: effectParams, key: "brightness", defaultValue: 0 }),
					contrast: readNumber({ params: effectParams, key: "contrast", defaultValue: 0 }),
					saturation: readNumber({ params: effectParams, key: "saturation", defaultValue: 0 }),
					exposure: readNumber({ params: effectParams, key: "exposure", defaultValue: 0 }),
					temperature: readNumber({ params: effectParams, key: "temperature", defaultValue: 0 }),
					tint: readNumber({ params: effectParams, key: "tint", defaultValue: 0 }),
					hue: readNumber({ params: effectParams, key: "hue", defaultValue: 0 }),
					gamma: readNumber({ params: effectParams, key: "gamma", defaultValue: 1 }),
				}),
			},
		],
	},
};
