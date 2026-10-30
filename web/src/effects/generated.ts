import { generatedEffectSchema, type GeneratedEffect } from "./generated-schema";
import { effectsRegistry } from "./registry";
import { registerDefaultEffects } from "./definitions";
import type { Effect, EffectDefinition, EffectPass } from "./types";
import { buildDefaultParamValues } from "@/params/registry";
import { coerceParamValue, type ParamDefinition, type ParamValue, type ParamValues } from "@/params";
import { effectGraphOrder } from "@/native/effect-graph";

const compiled = new WeakMap<object, { source: string; definition: EffectDefinition }>();
const MAX_PASSES = 32;

function validateValue(param: ParamDefinition, value: ParamValue) {
	if (coerceParamValue({ param, value }) === null) throw new Error(`Invalid value: ${param.key}`);
	if (param.type === "number" && (typeof value !== "number" || value < param.min || (param.max !== undefined && value > param.max))) throw new Error(`Value outside effect range: ${param.key}`);
	if (param.type === "color" && (typeof value !== "string" || !/^#[\da-f]{6}$/i.test(value))) throw new Error(`Invalid effect color: ${param.key}`);
}

/** No generated JS or shader execution. A recipe composes only implemented render passes. */
export function compileGeneratedEffect(input: GeneratedEffect): EffectDefinition {
	const cached = compiled.get(input);
	const source = JSON.stringify(input);
	if (cached?.source === source) return cached.definition;
	const recipe = generatedEffectSchema.parse(input);
	registerDefaultEffects();
	const index = new Map(recipe.nodes.map((node, i) => [node.id, i]));
	const order = effectGraphOrder(recipe.nodes.map((node) => node.input === "source" ? -1 : index.get(node.input) ?? -2), index.get(recipe.output) ?? -1);
	for (const node of recipe.nodes) {
		const definition = effectsRegistry.get(node.effectType);
		for (const [key, binding] of Object.entries(node.bindings)) {
			const param = definition.params.find((p) => p.key === key);
			if (!param) throw new Error(`Unknown node parameter: ${node.id}.${key}`);
			if ("value" in binding) validateValue(param, binding.value);
			else {
				const exposed = recipe.parameters.find((p) => p.key === binding.parameter);
				if (!exposed || exposed.type !== param.type) throw new Error(`Incompatible parameter binding: ${node.id}.${key}`);
				validateValue(param, exposed.default);
				if (exposed.type === "number" && param.type === "number" && (exposed.min < param.min || (param.max !== undefined && exposed.max > param.max))) throw new Error(`Exposed range exceeds node range: ${binding.parameter}`);
			}
		}
	}
	for (const p of recipe.parameters) {
		if (!recipe.nodes.some((node) => Object.values(node.bindings).some((b) => "parameter" in b && b.parameter === p.key))) throw new Error(`Unbound effect parameter: ${p.key}`);
	}
	const definition: EffectDefinition = {
		type: `generated:${recipe.id}`, name: recipe.name, category: "stylize", keywords: [recipe.name], params: recipe.parameters,
		renderer: { passes: [], buildPasses: ({ effectParams, width, height }) => {
			if (!Number.isInteger(width) || !Number.isInteger(height) || width < 1 || height < 1 || width > 8192 || height > 8192 || width * height > 16_777_216) throw new Error("Generated effect resolution exceeds its resource budget");
			for (const param of recipe.parameters) validateValue(param, effectParams[param.key] ?? param.default);
			const passes: EffectPass[] = [];
			for (const i of order) {
				const node = recipe.nodes[i], base = effectsRegistry.get(node.effectType);
				const values = buildDefaultParamValues(base.params);
				for (const [key, binding] of Object.entries(node.bindings)) {
					values[key] = "value" in binding ? binding.value : effectParams[binding.parameter] ?? recipe.parameters.find((p) => p.key === binding.parameter)!.default;
				}
				const next = base.renderer.buildPasses ? base.renderer.buildPasses({ effectParams: values, width, height }) : base.renderer.passes.map((p) => ({ shader: p.shader, uniforms: p.uniforms({ effectParams: values, width, height }) }));
				passes.push(...next);
				if (passes.length > MAX_PASSES) throw new Error("Generated effect exceeds 32 render passes");
			}
			return passes;
		} },
	};
	// Check worst-case parameter values against the pass budget before accepting the recipe.
	const worst: ParamValues = Object.fromEntries(recipe.parameters.map((p) => [p.key, p.type === "number" ? p.max : p.default]));
	definition.renderer.buildPasses!({ effectParams: worst, width: 4096, height: 4096 });
	compiled.set(input, { source, definition });
	return definition;
}

/** Preserve unavailable definitions; callers show an unavailable state instead of deleting data. */
export function getEffectDefinition(effect: Effect): EffectDefinition | null {
	if (effect.generated) {
		try {
			const definition = compileGeneratedEffect(effect.generated);
			return definition.type === effect.type ? definition : null;
		} catch { return null; }
	}
	registerDefaultEffects();
	return effectsRegistry.has(effect.type) ? effectsRegistry.get(effect.type) : null;
}
