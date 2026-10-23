"use client";

import type {
	ParamDefinition,
	NumberParamDefinition,
	ParamValue,
} from "@/params";
import {
	formatNumberForDisplay,
	getFractionDigitsForStep,
	snapToStep,
} from "@/utils/math";
import { SectionField } from "@/components/section";
import { NumberField } from "@/components/ui/number-field";
import { Switch } from "@/components/ui/switch";
import { ColorPicker } from "@/components/ui/color-picker";
import {
	Select,
	SelectContent,
	SelectItem,
	SelectTrigger,
	SelectValue,
} from "@/components/ui/select";
import { usePropertyDraft } from "../hooks/use-property-draft";
import { KeyframeToggle } from "./keyframe-toggle";
import { Textarea } from "@/components/ui/textarea";
import { useTranslation, type TranslationKey } from "@/i18n";

const PARAM_LABEL_KEYS: Record<string, TranslationKey> = {
	"background.color": "properties.backgroundColor",
	"background.cornerRadius": "properties.backgroundRadius",
	"background.enabled": "properties.backgroundEnabled",
	"background.offsetX": "properties.backgroundOffsetX",
	"background.offsetY": "properties.backgroundOffsetY",
	"background.paddingX": "properties.backgroundPaddingX",
	"background.paddingY": "properties.backgroundPaddingY",
	"transform.positionX": "properties.positionX",
	"transform.positionY": "properties.positionY",
	"transform.rotate": "properties.rotate",
	"transform.scaleX": "properties.scaleX",
	"transform.scaleY": "properties.scaleY",
	blendMode: "properties.blendMode",
	color: "properties.color",
	content: "properties.content",
	fontFamily: "properties.fontFamily",
	amount: "properties.effectAmount",
	softness: "properties.effectSoftness",
	roundness: "properties.effectRoundness",
	keyColor: "properties.effectKeyColor",
	similarity: "properties.effectSimilarity",
	smoothness: "properties.effectSmoothness",
	spill: "properties.effectSpill",
	fontSize: "properties.fontSize",
	fontStyle: "properties.fontStyle",
	fontWeight: "properties.fontWeight",
	letterSpacing: "properties.letterSpacing",
	lineHeight: "properties.lineHeight",
	muted: "properties.muted",
	opacity: "properties.opacity",
	textAlign: "properties.textAlign",
	textDecoration: "properties.textDecoration",
	volume: "properties.volume",
};

const SELECT_OPTION_LABEL_KEYS: Record<string, TranslationKey> = {
	"blendMode:color": "properties.blendModeColor",
	"blendMode:color-burn": "properties.blendModeColorBurn",
	"blendMode:color-dodge": "properties.blendModeColorDodge",
	"blendMode:darken": "properties.blendModeDarken",
	"blendMode:difference": "properties.blendModeDifference",
	"blendMode:exclusion": "properties.blendModeExclusion",
	"blendMode:hard-light": "properties.blendModeHardLight",
	"blendMode:hue": "properties.blendModeHue",
	"blendMode:lighten": "properties.blendModeLighten",
	"blendMode:luminosity": "properties.blendModeLuminosity",
	"blendMode:multiply": "properties.blendModeMultiply",
	"blendMode:normal": "properties.blendModeNormal",
	"blendMode:overlay": "properties.blendModeOverlay",
	"blendMode:plus-lighter": "properties.blendModePlusLighter",
	"blendMode:saturation": "properties.blendModeSaturation",
	"blendMode:screen": "properties.blendModeScreen",
	"blendMode:soft-light": "properties.blendModeSoftLight",
	"fontStyle:italic": "properties.fontStyleItalic",
	"fontStyle:normal": "properties.blendModeNormal",
	"fontWeight:bold": "properties.fontWeightBold",
	"fontWeight:normal": "properties.blendModeNormal",
	"textAlign:center": "properties.textAlignCenter",
	"textAlign:left": "properties.textAlignLeft",
	"textAlign:right": "properties.textAlignRight",
	"textDecoration:line-through": "properties.textDecorationLineThrough",
	"textDecoration:none": "properties.textDecorationNone",
	"textDecoration:underline": "properties.textDecorationUnderline",
};

export function PropertyParamField({
	param,
	value,
	onPreview,
	onCommit,
	keyframe,
}: {
	param: ParamDefinition;
	value: ParamValue;
	onPreview: (value: ParamValue) => void;
	onCommit: () => void;
	keyframe?: {
		isActive: boolean;
		isDisabled: boolean;
		onToggle: () => void;
	};
}) {
	const { t } = useTranslation();
	const label = PARAM_LABEL_KEYS[param.key]
		? t(PARAM_LABEL_KEYS[param.key])
		: param.label;

	return (
		<SectionField
			label={label}
			beforeLabel={
				keyframe && param.keyframable !== false ? (
					<KeyframeToggle
						isActive={keyframe.isActive}
						isDisabled={keyframe.isDisabled}
						title={t("properties.keyframeToggle", {
							label: label.toLowerCase(),
						})}
						onToggle={keyframe.onToggle}
					/>
				) : undefined
			}
		>
			<ParamInput
				param={param}
				value={value}
				onPreview={onPreview}
				onCommit={onCommit}
				translateOption={(option) => {
					const key = SELECT_OPTION_LABEL_KEYS[`${param.key}:${option.value}`];
					return key ? t(key) : option.label;
				}}
			/>
		</SectionField>
	);
}

function ParamInput({
	param,
	value,
	onPreview,
	onCommit,
	translateOption,
}: {
	param: ParamDefinition;
	value: ParamValue;
	onPreview: (value: ParamValue) => void;
	onCommit: () => void;
	translateOption: (option: { value: string; label: string }) => string;
}) {
	if (param.type === "number") {
		return (
			<NumberParamField
				param={param}
				value={typeof value === "number" ? value : Number(value)}
				onPreview={onPreview}
				onCommit={onCommit}
			/>
		);
	}

	if (param.type === "boolean") {
		return (
			<Switch
				checked={Boolean(value)}
				onCheckedChange={(checked) => {
					onPreview(checked);
					onCommit();
				}}
			/>
		);
	}

	if (param.type === "select") {
		return (
			<Select
				value={String(value)}
				onValueChange={(selected) => {
					onPreview(selected);
					onCommit();
				}}
			>
				<SelectTrigger className="w-full">
					<SelectValue />
				</SelectTrigger>
				<SelectContent>
					{param.options.map((option) => (
						<SelectItem key={option.value} value={option.value}>
							{translateOption(option)}
						</SelectItem>
					))}
				</SelectContent>
			</Select>
		);
	}

	if (param.type === "color") {
		return (
			<ColorPicker
				value={String(value).replace(/^#/, "").toUpperCase()}
				onChange={(color) => onPreview(`#${color}`)}
				onChangeEnd={(color) => {
					onPreview(`#${color}`);
					onCommit();
				}}
			/>
		);
	}

	if (param.type === "text") {
		return (
			<Textarea
				value={String(value)}
				onChange={(event) => onPreview(event.currentTarget.value)}
				onBlur={onCommit}
			/>
		);
	}

	if (param.type === "font") {
		return (
			<input
				className="border-input bg-accent h-9 w-full rounded-md border px-3 text-sm outline-none"
				value={String(value)}
				onChange={(event) => onPreview(event.currentTarget.value)}
				onBlur={onCommit}
			/>
		);
	}

	return null;
}

function NumberParamField({
	param,
	value,
	onPreview,
	onCommit,
}: {
	param: NumberParamDefinition;
	value: number;
	onPreview: (value: number) => void;
	onCommit: () => void;
}) {
	const { min, max, step, displayMultiplier = 1 } = param;
	const displayValue = value * displayMultiplier;
	const clampDisplayValue = (nextDisplayValue: number) =>
		Math.max(
			min,
			max !== undefined ? Math.min(max, nextDisplayValue) : nextDisplayValue,
		);

	const previewFromDisplay = (displayVal: number) => {
		const clamped = clampDisplayValue(snapToStep({ value: displayVal, step }));
		onPreview(clamped / displayMultiplier);
	};

	const maxFractionDigits = getFractionDigitsForStep({ step });

	const draft = usePropertyDraft({
		displayValue: formatNumberForDisplay({
			value: displayValue,
			maxFractionDigits,
		}),
		parse: (input) => {
			const parsed = parseFloat(input);
			if (Number.isNaN(parsed)) return null;
			return clampDisplayValue(snapToStep({ value: parsed, step }));
		},
		onPreview: previewFromDisplay,
		onCommit,
	});

	const handleReset = () => {
		onPreview(param.default);
		onCommit();
	};

	return (
		<NumberField
			icon={param.shortLabel}
			value={draft.displayValue}
			dragSensitivity="slow"
			isDefault={value === param.default}
			onFocus={draft.onFocus}
			onChange={draft.onChange}
			onBlur={draft.onBlur}
			onScrub={previewFromDisplay}
			onScrubEnd={onCommit}
			onReset={handleReset}
		/>
	);
}
