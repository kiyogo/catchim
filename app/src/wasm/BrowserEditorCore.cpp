// Browser presentation stays in HTML/Canvas. Project validation and committed
// history use the same C++ editor as the desktop build, outside the paint loop.
#include "editor/EditorEngine.h"
#include "editor/project/ProjectSerializer.h"
#include <emscripten/emscripten.h>
#include <nlohmann/json.hpp>

namespace {
using Json = nlohmann::json;
catchim::editor::EditorEngine engine;
Json document;

class BrowserEdit final : public catchim::editor::EditorCommand {
    Json before_, after_;
    catchim::editor::Project oldProject_, newProject_;
public:
    BrowserEdit(Json next, catchim::editor::Project project)
        : before_(document), after_(std::move(next)), oldProject_(engine.project()), newProject_(std::move(project)) {}
    bool execute() override { document = after_; engine.project() = newProject_; return true; }
    bool undo() override { document = before_; engine.project() = oldProject_; return true; }
    std::string name() const override { return "Browser edit"; }
};
}

extern "C" EMSCRIPTEN_KEEPALIVE const char* catchim_browser_request(const char* input) {
    static std::string response;
    try {
        const auto request = Json::parse(input);
        const auto op = request.value("op", "state");
        if (op == "load" || op == "commit") {
            const auto& next = request.at("document");
            auto project = catchim::editor::ProjectSerializer::fromJson(next.at("project"));
            if (!project.ok()) throw std::runtime_error(project.message());
            if (op == "load") {
                engine.history().clear();
                document = next;
                engine.project() = project.unwrap();
            } else {
                engine.history().execute(std::make_unique<BrowserEdit>(next, project.unwrap()));
            }
        } else if (op == "undo") engine.undo();
        else if (op == "redo") engine.redo();
        else if (op != "state") throw std::runtime_error("Unknown browser editor operation");
        response = Json{{"ok", true}, {"document", document}, {"canUndo", engine.canUndo()}, {"canRedo", engine.canRedo()}}.dump();
    } catch (const std::exception& error) {
        response = Json{{"ok", false}, {"error", error.what()}}.dump();
    }
    return response.c_str();
}
