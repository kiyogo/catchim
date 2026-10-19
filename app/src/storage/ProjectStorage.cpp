#include "ProjectStorage.h"
#include "editor/project/ProjectSerializer.h"
#include "core/logging/Logger.h"
#include <fstream>
#include <sstream>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace catchim::storage {

namespace {
void persistBrowserFilesystem() {
#if defined(__EMSCRIPTEN__)
    EM_ASM({
        if (Module.catchimPersistNow) Module.catchimPersistNow();
    });
#endif
}
}

std::filesystem::path ProjectStorage::getDefaultProjectsDirectory() {
    std::filesystem::path baseDir;
#if defined(_WIN32)
    const char* appData = std::getenv("LOCALAPPDATA");
    if (appData) {
        baseDir = std::filesystem::path(appData) / "Catchim" / "Projects";
    } else {
        baseDir = std::filesystem::current_path() / "Projects";
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        baseDir = std::filesystem::path(home) / ".catchim" / "projects";
    } else {
        baseDir = std::filesystem::current_path() / "Projects";
    }
#endif
    std::error_code ec;
    std::filesystem::create_directories(baseDir, ec);
    return baseDir;
}

std::vector<std::filesystem::path> ProjectStorage::listProjects() {
    std::vector<std::filesystem::path> list;
    auto dir = getDefaultProjectsDirectory();
    std::error_code ec;
    if (std::filesystem::exists(dir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                list.push_back(entry.path());
            }
        }
    }
    return list;
}

core::Result<editor::Project> ProjectStorage::load(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return core::Result<editor::Project>(core::ErrorCode::FileNotFound, "Project file does not exist");
    }

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return core::Result<editor::Project>(core::ErrorCode::FileCorrupted, "Failed to open project file");
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string jsonStr = ss.str();

    auto res = editor::ProjectSerializer::deserialize(jsonStr);
    if (!res.ok()) {
        return res;
    }

    LOG_INFO("Successfully loaded project from: {}", path.string());
    return res;
}

core::Result<void> ProjectStorage::saveAtomic(const editor::Project& project, const std::filesystem::path& path) {
    std::error_code ec;
    auto parentDir = path.parent_path();
    if (!parentDir.empty()) {
        std::filesystem::create_directories(parentDir, ec);
    }

    std::filesystem::path tmpPath = path;
    tmpPath += ".tmp";

    std::string jsonStr = editor::ProjectSerializer::serialize(project, 2);

    // 1. Write to tmp file
    {
        std::ofstream file(tmpPath, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!file.is_open()) {
            return core::Result<void>(core::ErrorCode::InternalError, "Cannot create temporary project file");
        }
        file.write(jsonStr.data(), jsonStr.size());
        file.flush();
        if (!file.good()) {
            return core::Result<void>(core::ErrorCode::InternalError, "Write failed to temporary project file");
        }
    }

    // 2. Validate written tmp file
    {
        std::ifstream file(tmpPath, std::ios::in | std::ios::binary);
        std::ostringstream ss;
        ss << file.rdbuf();
        auto valRes = editor::ProjectSerializer::deserialize(ss.str());
        if (!valRes.ok()) {
            std::filesystem::remove(tmpPath, ec);
            return core::Result<void>(core::ErrorCode::FileCorrupted, "Validation failed on written project file");
        }
    }

    // 3. Atomic rename/replace
#if defined(_WIN32)
    if (!MoveFileExW(reinterpret_cast<LPCWSTR>(tmpPath.c_str()), reinterpret_cast<LPCWSTR>(path.c_str()), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DWORD err = GetLastError();
        std::filesystem::remove(tmpPath, ec);
        return core::Result<void>(core::ErrorCode::InternalError, "Atomic MoveFileExW failed with error code: " + std::to_string(err));
    }
#else
    std::filesystem::rename(tmpPath, path, ec);
    if (ec) {
        std::filesystem::remove(tmpPath, ec);
        return core::Result<void>(core::ErrorCode::InternalError, "Atomic rename failed: " + ec.message());
    }
#endif

    LOG_INFO("Successfully saved project to: {}", path.string());
    persistBrowserFilesystem();
    return core::Result<void>::success();
}

bool ProjectStorage::deleteProject(const std::filesystem::path& path) {
    std::error_code ec;
    const bool removed = std::filesystem::remove(path, ec);
    if (removed && !ec) persistBrowserFilesystem();
    return removed && !ec;
}

} // namespace catchim::storage
