import { effectsRegistry } from "../registry";
import { blurEffectDefinition } from "./blur";
import { colorGradingEffectDefinition } from "./color-grading";
import { chromaKeyEffectDefinition } from "./chroma-key";
import { vignetteEffectDefinition } from "./vignette";

const defaultEffects = [
	blurEffectDefinition,
	colorGradingEffectDefinition,
	vignetteEffectDefinition,
	chromaKeyEffectDefinition,
];

export function registerDefaultEffects(): void {
	for (const definition of defaultEffects) {
		if (effectsRegistry.has(definition.type)) {
			continue;
		}
		effectsRegistry.register({
			key: definition.type,
			definition,
		});
	}
}
