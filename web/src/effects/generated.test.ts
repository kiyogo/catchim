import { beforeAll, describe, expect, it } from "vitest";
import { createCanvas } from "@napi-rs/canvas";
import { compileGeneratedEffect, getEffectDefinition } from "./generated";
import { generatedEffectSchema, type GeneratedEffect } from "./generated-schema";
import { initRustEngine } from "@/native/rust-engine";
import { effectGraphOrder } from "@/native/effect-graph";
import { warmNativeTimelineCore } from "@/native/opencut-core";
import { applyEffectPasses } from "@/native/opencut-wasm-compat";
import { effectsRegistry, resolveEffectPasses } from "./index";

export function recipe(): GeneratedEffect {
	return { version: 1, id: "soft-contrast", name: "Soft Contrast", description: "A user-defined grade and vignette graph", createdBy: "user", parameters: [{ key: "brightness", label: "Brightness", type: "number", default: 0.2, min: -1, max: 1, step: 0.01 }, { key: "edges", label: "Edges", type: "number", default: 0.4, min: 0, max: 1, step: 0.01 }], nodes: [{ id: "edges", input: "grade", effectType: "vignette", bindings: { amount: { parameter: "edges" } } }, { id: "grade", input: "source", effectType: "color-grading", bindings: { brightness: { parameter: "brightness" }, contrast: { value: 0.1 } } }], output: "edges" };
}
beforeAll(async () => { await initRustEngine(); await warmNativeTimelineCore(); });

describe("editable generated effects with real Rust graph validation", () => {
	it("orders nodes through the WASM ABI and creates the same passes as built-in renderers", () => {
		expect(effectGraphOrder([1, -1], 0)).toEqual([1, 0]);
		const definition = compileGeneratedEffect(recipe());
		const passes = resolveEffectPasses({ definition, effectParams: { brightness: 0.5, edges: 0.25 }, width: 32, height: 32 });
		expect(passes.map((p) => p.shader)).toEqual(["color_grading", "vignette"]);
		expect(passes[0].uniforms.brightness).toBe(0.5);
		expect(passes[1].uniforms.amount).toBe(0.25);
	});
	it("rejects cycles, missing references, unused nodes and oversized graphs", () => {
		for (const [inputs, output] of [[[1, 0], 0], [[-2], 0], [[-1, -1], 0], [[-1], 2]] as Array<[number[], number]>) expect(() => effectGraphOrder(inputs, output)).toThrow();
		expect(() => effectGraphOrder(Array(9).fill(-1), 0)).toThrow();
		const cyclic = recipe(); cyclic.nodes[1].input = "edges";
		expect(() => compileGeneratedEffect(cyclic)).toThrow("cycle");
	});
	it("rejects incompatible bindings, duplicate IDs, unimplemented shaders and excessive pass budgets", () => {
		const incompatible = recipe(); incompatible.nodes[0].bindings.unknown = { value: 1 };
		expect(() => compileGeneratedEffect(incompatible)).toThrow("Unknown node parameter");
		const range = recipe(); range.parameters[0] = { key: "brightness", label: "Brightness", type: "number", default: 0, min: -10, max: 10, step: 1 };
		expect(() => compileGeneratedEffect(range)).toThrow("range");
		const duplicate = recipe(); duplicate.nodes[1].id = duplicate.nodes[0].id;
		expect(generatedEffectSchema.safeParse(duplicate).success).toBe(false);
		const shader = recipe(); (shader.nodes[0] as { effectType: string }).effectType = "untrusted-wgsl";
		expect(generatedEffectSchema.safeParse(shader).success).toBe(false);
		const huge = recipe(); huge.parameters = []; huge.nodes = Array.from({ length: 8 }, (_, i) => ({ id: `blur${i}`, input: i ? `blur${i - 1}` : "source", effectType: "blur" as const, bindings: { intensity: { value: 100 } } })); huge.output = "blur7";
		expect(() => compileGeneratedEffect(huge)).toThrow("32 render passes");
	});
	it("preserves unknown definitions and reconstructs a recipe after JSON reopening", () => {
		const effect = { id: "effect", type: "generated:soft-contrast", enabled: true, params: { brightness: 0.2, edges: 0.4 }, generated: recipe() };
		expect(getEffectDefinition(JSON.parse(JSON.stringify(effect)))?.name).toBe("Soft Contrast");
		const future = JSON.parse(JSON.stringify(effect)); future.generated.version = 2;
		expect(getEffectDefinition(future)).toBeNull(); expect(future.generated.version).toBe(2);
		expect(getEffectDefinition({ ...effect, generated: undefined, type: "missing-plugin" })).toBeNull();
	});
	it("renders actual pixels through the existing CPU/WASM path, with zero difference from explicit passes", () => {
		const makeSource = () => { const canvas = createCanvas(32, 32); const ctx = canvas.getContext("2d"); ctx.fillStyle = "#567890"; ctx.fillRect(0, 0, 32, 32); return canvas; };
		const generated = makeSource(), explicit = makeSource(), before = Buffer.from(generated.getContext("2d").getImageData(0, 0, 32, 32).data);
		const effectParams = { brightness: 0.2, edges: 0.4 };
		const passes = resolveEffectPasses({ definition: compileGeneratedEffect(recipe()), effectParams, width: 32, height: 32 });
		applyEffectPasses({ source: generated as unknown as OffscreenCanvas, width: 32, height: 32, passes });
		const manual = [
			...resolveEffectPasses({ definition: effectsRegistry.get("color-grading"), effectParams: { brightness: 0.2, contrast: 0.1 }, width: 32, height: 32 }),
			...resolveEffectPasses({ definition: effectsRegistry.get("vignette"), effectParams: { amount: 0.4 }, width: 32, height: 32 }),
		];
		applyEffectPasses({ source: explicit as unknown as OffscreenCanvas, width: 32, height: 32, passes: manual });
		const actual = Buffer.from(generated.getContext("2d").getImageData(0, 0, 32, 32).data);
		expect(actual.equals(before)).toBe(false);
		expect(actual.equals(Buffer.from(explicit.getContext("2d").getImageData(0, 0, 32, 32).data))).toBe(true);
	});
});
