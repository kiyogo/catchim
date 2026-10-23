/** Convert subtitle markup to editor plain text without creating DOM nodes. */
export function decodeSubtitleText(input: string): string {
	return input.replace(/<\/?(?:b|i|u|font|c|v|lang|ruby|rt)(?:[ \t.][^>]*)?>|<(?:\d{2,}:)?\d{2}:\d{2}\.\d{3}>/gi, "").replace(/&(#x[\da-f]+|#\d+|amp|lt|gt|nbsp|lrm|rlm);/gi, (entity, value: string) => {
		const named: Record<string, string> = { amp: "&", lt: "<", gt: ">", nbsp: "\u00a0", lrm: "\u200e", rlm: "\u200f" };
		if (value[0] !== "#") return named[value.toLowerCase()] ?? entity;
		const point = value[1]?.toLowerCase() === "x" ? parseInt(value.slice(2), 16) : parseInt(value.slice(1), 10);
		return point > 0 && point <= 0x10ffff && !(point >= 0xd800 && point <= 0xdfff) ? String.fromCodePoint(point) : "\ufffd";
	});
}
