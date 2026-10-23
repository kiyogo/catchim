import type { Language } from "@/i18n";

const vi = {
	createTab: "Tạo phụ đề",
	editTab: "Biên tập phụ đề",
	track: "Track chữ / phụ đề",
	chooseTrack: "Chọn track để chỉnh sửa",
	empty: "Tạo, nhập phụ đề hoặc thêm track chữ để bắt đầu biên tập.",
	locked: "Track đang khóa. Mở khóa trên timeline để chỉnh sửa.",
	search: "Tìm nội dung",
	replace: "Thay bằng",
	replaceAll: "Thay tất cả",
	matchCase: "Phân biệt hoa thường",
	start: "Bắt đầu (giây)",
	end: "Kết thúc (giây)",
	save: "Lưu câu",
	content: "Nội dung phụ đề",
	seek: "Đi tới câu",
	split: "Tách tại playhead",
	merge: "Gộp câu tiếp",
	splitHint: "Đặt playhead trong câu và con trỏ giữa nội dung để tách.",
	mergeHint:
		"Gộp giữ kiểu và chuyển động của câu đầu, bao gồm cả khoảng trống giữa hai câu.",
	offset: "Dịch thời gian (giây)",
	applyOffset: "Dịch cả track",
	offsetHint:
		"Giữ nguyên khoảng cách. Hủy thao tác nếu có câu bị dịch trước 0.",
	style: "Kiểu cho cả track",
	applyStyle: "Áp dụng kiểu",
	classic: "Trắng đậm",
	yellow: "Vàng nổi bật",
	box: "Nền tối",
	minimal: "Tối giản",
	count: "câu",
	noResults: "Không tìm thấy câu phù hợp.",
	saved: "Đã cập nhật. Có thể hoàn tác trên thanh công cụ.",
	noChange: "Không có thay đổi.",
	invalidTiming:
		"Thời gian phải hợp lệ: bắt đầu từ 0 trở lên và kết thúc sau thời điểm bắt đầu.",
	invalidOffset:
		"Không thể dịch thời gian: có câu vượt giới hạn thời gian hoặc bị dịch trước 0.",
	invalidSplit:
		"Đặt playhead trong câu và con trỏ giữa hai phần nội dung không rỗng để tách.",
	mergeUnavailable: "Không có câu tiếp theo để gộp.",
	emptyText: "Nội dung câu không được để trống.",
	stale: "Track đã thay đổi. Chọn lại câu và thử lại.",
	failed: "Không thể cập nhật phụ đề.",
	unsaved: "Có nội dung chưa lưu",
	selectCue: "Chọn câu",
	exportSrt: "Xuất SRT",
	exportVtt: "Xuất VTT",
};

const en: Record<keyof typeof vi, string> = {
	createTab: "Create captions",
	editTab: "Edit captions",
	track: "Text / caption track",
	chooseTrack: "Choose a track to edit",
	empty: "Generate or import captions, or add a text track to start editing.",
	locked: "This track is locked. Unlock it on the timeline to edit.",
	search: "Find text",
	replace: "Replace with",
	replaceAll: "Replace all",
	matchCase: "Match case",
	start: "Start (seconds)",
	end: "End (seconds)",
	save: "Save cue",
	content: "Caption text",
	seek: "Go to cue",
	split: "Split at playhead",
	merge: "Merge next cue",
	splitHint:
		"Place the playhead inside the cue and the text cursor between words to split.",
	mergeHint:
		"Merging keeps the first cue's style and animation, including any gap between the two cues.",
	offset: "Timing offset (seconds)",
	applyOffset: "Shift entire track",
	offsetHint:
		"Spacing is preserved. The operation is cancelled if any cue would move before 0.",
	style: "Style entire track",
	applyStyle: "Apply style",
	classic: "Bold white",
	yellow: "Highlight yellow",
	box: "Dark background",
	minimal: "Minimal",
	count: "cues",
	noResults: "No matching cues.",
	saved: "Updated. Use the toolbar to undo.",
	noChange: "No changes.",
	invalidTiming:
		"Use valid times: start must be 0 or later, and end must be after start.",
	invalidOffset:
		"Cannot shift timing: a cue would move before 0 or exceed the supported range.",
	invalidSplit:
		"Place the playhead inside the cue and the text cursor between two nonempty text parts to split.",
	mergeUnavailable: "There is no next cue to merge.",
	emptyText: "Cue text cannot be empty.",
	stale: "The track changed. Select the cue again and retry.",
	failed: "Unable to update captions.",
	unsaved: "Unsaved edits",
	selectCue: "Select cue",
	exportSrt: "Export SRT",
	exportVtt: "Export VTT",
};

export type CaptionMessage = keyof typeof vi;
export const captionMessages = (
	language: Language,
): Record<CaptionMessage, string> => (language === "vi" ? vi : en);
