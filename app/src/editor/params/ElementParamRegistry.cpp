#include "editor/params/ElementParamRegistry.h"

namespace catchim::editor {

const std::vector<BlendModeOption>& ElementParamRegistry::getBlendModeOptions() noexcept {
    static const std::vector<BlendModeOption> s_modes = {
        {"normal",        "Normal"},
        {"darken",        "Darken"},
        {"multiply",      "Multiply"},
        {"color-burn",    "Color Burn"},
        {"lighten",       "Lighten"},
        {"screen",        "Screen"},
        {"plus-lighter",  "Plus Lighter"},
        {"color-dodge",   "Color Dodge"},
        {"overlay",       "Overlay"},
        {"soft-light",    "Soft Light"},
        {"hard-light",    "Hard Light"},
        {"difference",    "Difference"},
        {"exclusion",     "Exclusion"},
        {"hue",           "Hue"},
        {"saturation",    "Saturation"},
        {"color",         "Color"},
        {"luminosity",    "Luminosity"}
    };
    return s_modes;
}

std::vector<ElementParamDefinition> ElementParamRegistry::getBuiltInElementParams(ClipType type) {
    std::vector<ElementParamDefinition> params;

    // Visual params
    bool isVisual = (type == ClipType::Video || type == ClipType::Image ||
                     type == ClipType::Text || type == ClipType::Sticker ||
                     type == ClipType::Graphic || type == ClipType::Effect);

    if (isVisual) {
        params.push_back({"transform.positionX", "Position X", "number", 0.0, -100000.0, 100000.0, 1.0});
        params.push_back({"transform.positionY", "Position Y", "number", 0.0, -100000.0, 100000.0, 1.0});
        params.push_back({"transform.scaleX",    "Scale X",    "number", 1.0, 0.01, 100.0, 0.01});
        params.push_back({"transform.scaleY",    "Scale Y",    "number", 1.0, 0.01, 100.0, 0.01});
        params.push_back({"transform.rotate",    "Rotate",     "number", 0.0, -360.0, 360.0, 1.0});
        params.push_back({"opacity",             "Opacity",    "number", 1.0, 0.0, 1.0, 0.01});
        params.push_back({"blendMode",           "Blend Mode", "select", "normal", std::nullopt, std::nullopt, std::nullopt});
    }

    // Audio params
    bool hasAudio = (type == ClipType::Audio || type == ClipType::Video);
    if (hasAudio) {
        params.push_back({"volume", "Volume", "number", 0.0, -60.0, 12.0, 0.1});
        params.push_back({"pan",    "Pan",    "number", 0.0, -1.0, 1.0, 0.05});
    }

    // Text params
    if (type == ClipType::Text) {
        params.push_back({"content",                "Content",          "string",  "Title",   std::nullopt, std::nullopt, std::nullopt});
        params.push_back({"fontSize",               "Font Size",        "number",  40.0,      1.0, 500.0, 1.0});
        params.push_back({"fontFamily",             "Font Family",      "string",  "Inter",   std::nullopt, std::nullopt, std::nullopt});
        params.push_back({"fontWeight",             "Font Weight",      "select",  "normal",  std::nullopt, std::nullopt, std::nullopt});
        params.push_back({"fontStyle",              "Font Style",       "select",  "normal",  std::nullopt, std::nullopt, std::nullopt});
        params.push_back({"textAlign",              "Text Align",       "select",  "center",  std::nullopt, std::nullopt, std::nullopt});
        params.push_back({"textDecoration",         "Text Decoration",  "select",  "none",    std::nullopt, std::nullopt, std::nullopt});
        params.push_back({"letterSpacing",          "Letter Spacing",   "number",  0.0,       -50.0, 100.0, 0.5});
        params.push_back({"lineHeight",             "Line Height",      "number",  1.2,       0.5, 3.0, 0.1});
        params.push_back({"color",                  "Text Color",       "color",   "#ffffff", std::nullopt, std::nullopt, std::nullopt});
        params.push_back({"background.enabled",     "Enable Background","boolean", false,     std::nullopt, std::nullopt, std::nullopt});
        params.push_back({"background.color",       "Background Color", "color",   "#000000", std::nullopt, std::nullopt, std::nullopt});
        params.push_back({"background.cornerRadius","Corner Radius",    "number",  0.0,       0.0, 100.0, 1.0});
        params.push_back({"background.paddingX",    "Padding X",        "number",  30.0,      0.0, std::nullopt, 1.0});
        params.push_back({"background.paddingY",    "Padding Y",        "number",  42.0,      0.0, std::nullopt, 1.0});
        params.push_back({"background.offsetX",     "Offset X",         "number",  0.0,       -100000.0, std::nullopt, 1.0});
        params.push_back({"background.offsetY",     "Offset Y",         "number",  0.0,       -100000.0, std::nullopt, 1.0});
    }

    return params;
}

nlohmann::json ElementParamRegistry::buildDefaultParamValues(ClipType type) {
    nlohmann::json result = nlohmann::json::object();
    for (const auto& param : getBuiltInElementParams(type)) {
        result[param.key] = param.defaultValue;
    }
    return result;
}

} // namespace catchim::editor
