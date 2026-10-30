import { z } from "zod";
const name = z.string().regex(/^[a-zA-Z][\w-]{0,63}$/).refine((s) => !["source", "__proto__", "constructor", "prototype"].includes(s), "Reserved identifier");
const value = z.union([z.number().finite(), z.string().max(100), z.boolean()]);
const numberParam = z.object({ key: name, label: z.string().min(1).max(80), type: z.literal("number"), default: z.number().finite(), min: z.number().finite(), max: z.number().finite(), step: z.number().positive().finite() }).strict().refine((p) => p.min <= p.default && p.default <= p.max && p.min < p.max, "Invalid parameter range");
const colorParam = z.object({ key: name, label: z.string().min(1).max(80), type: z.literal("color"), default: z.string().regex(/^#[\da-fA-F]{6}$/) }).strict();
const booleanParam = z.object({ key: name, label: z.string().min(1).max(80), type: z.literal("boolean"), default: z.boolean() }).strict();
export const generatedEffectSchema = z.object({
	version: z.literal(1),
	id: name,
	name: z.string().min(1).max(100),
	description: z.string().max(2000),
	createdBy: z.enum(["ai", "user"]),
	parameters: z.array(z.union([numberParam, colorParam, booleanParam])).max(32),
	nodes: z.array(z.object({
		id: name,
		input: z.string().min(1).max(64),
		effectType: z.enum(["blur", "color-grading", "vignette", "chroma-key"]),
		bindings: z.record(name, z.union([z.object({ parameter: name }).strict(), z.object({ value }).strict()])).refine((v) => Object.keys(v).length <= 32, "Too many bindings"),
	}).strict()).min(1).max(8),
	output: name,
}).strict().superRefine((definition, ctx) => {
	for (const [path, ids] of [["parameters", definition.parameters.map((p) => p.key)], ["nodes", definition.nodes.map((n) => n.id)]] as const) {
		if (new Set(ids).size !== ids.length) ctx.addIssue({ code: "custom", path: [path], message: "Identifiers must be unique" });
	}
});
export type GeneratedEffect = z.infer<typeof generatedEffectSchema>;
