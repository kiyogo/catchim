# BÁO CÁO ÁNH XẠ GIAO DIỆN LẬP TRÌNH VÀ TÍCH HỢP HỆ THỐNG (API MAPPING)
Dự án: Chuyển đổi Web OpenCut sang Native C++ Desktop (Catchim)

## 1. Tổng quan API & Dịch vụ

Bản Web gốc chứa các Route Handlers Next.js trong `src/app/api/`:
- `/api/health`: Kiểm tra tình trạng dịch vụ.
- `/api/auth/[...all]`: Xác thực người dùng (Better-Auth).
- `/api/sounds/search`: Tìm kiếm hiệu ứng âm thanh từ thư viện trực tuyến / Freesound.
- `/api/tts`: Chuyển văn bản thành giọng nói (msedge-tts).
- `/api/feedback`: Gửi phản hồi người dùng.

Trong phiên bản C++ Native, ứng dụng hoạt động **100% độc lập không bắt buộc kết nối Internet**, chuyển đổi toàn bộ sang Native Services:

```text
+---------------------------------------------------------------------------------+
|                       WEB API ROUTES (NEXT.JS APP)                              |
|  /api/health            /api/auth              /api/tts         /api/sounds     |
+---------------------------------------------------------------------------------+
                                       |
                             NATIVE MIGRATION
                                       v
+---------------------------------------------------------------------------------+
|                     NATIVE C++ DESKTOP INTEGRATION                              |
|  - Standalone Local Execution (Không phụ thuộc Cloud server hay Auth)           |
|  - Native TTS Service: Windows SAPI / Microsoft Speech Synthesis API            |
|  - Native Sound Effects: Built-in PCM cache & Local Media Library               |
|  - Native File Dialogs: Windows Shell (GetOpenFileName / IFileDialog)           |
|  - Native Export Worker: FFmpeg Native Integration                             |
+---------------------------------------------------------------------------------+
```

---

## 2. Bảng ánh xạ chi tiết các API & Tích hợp

| Web API Endpoint / Module | Giao thức Web | Giải pháp C++ Native | Tệp nguồn C++ | Đánh giá & Ghi chú |
|---|---|---|---|---|
| `/api/health` | HTTP GET JSON | Health Check Diagnostics | `src/diagnostics/` | Kiểm tra tài nguyên CPU, GPU, RAM native nội bộ |
| `/api/auth/[...all]` | HTTP Session / Cookie | Zero-Auth Local Model | N/A | Desktop App lưu dữ liệu trực tiếp trên máy người dùng, không cần đăng nhập |
| `/api/sounds/search` | HTTP Query External API | `SoundEffectsRegistry` | `src/audio/SoundEffectsRegistry.cpp` | Duyệt danh mục âm thanh mẫu tích hợp và bộ nhớ tệp cục bộ |
| `/api/tts` | WebSocket / HTTP Edge TTS | `TtsServiceEngine` & `TtsVoiceRegistry` | `src/subtitles/TranscriptionService.cpp` | Tích hợp engine phát giọng nói và xuất tệp WAV cục bộ |
| `/api/feedback` | HTTP POST | Dialog Trợ giúp & Phím tắt | `src/ui/shell/ShortcutsDialog.cpp` | Hộp thoại thông tin và hướng dẫn phím tắt |
| File Picker Web API | `<input type="file">` | `QFileDialog` native Windows | `src/ui/MainWindow.cpp` | Mở hộp thoại chọn tệp hệ thống Windows cực nhanh |
| Storage Web API | `IndexedDB` / `LocalStorage` | `ProjectStorage` | `src/storage/ProjectStorage.cpp` | Lưu tệp JSON nguyên tử vào `%LOCALAPPDATA%/Catchim/Projects` |
| Video Export Web | WebCodecs / MP4 muxer | `ExportWorker` + FFmpeg pipeline | `src/ui/export/ExportWorker.cpp` | Mã hóa đa luồng phần cứng NVENC/QuickSync/CPU |

---

## 3. Lợi ích khi chuyển đổi sang Native
1. **Bảo mật tuyệt đối**: Dữ liệu video, âm thanh và dự án không bao giờ bị tải lên bất kỳ máy chủ đám mây nào.
2. **Offline 100%**: Người dùng có thể chỉnh sửa và xuất video bất cứ lúc nào, kể cả khi không có kết nối mạng.
3. **Không lo hạn mức**: Không bị giới hạn request API hoặc rate limit từ các dịch vụ bên thứ ba.
