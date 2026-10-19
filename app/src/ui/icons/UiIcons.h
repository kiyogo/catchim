#pragma once

#if defined(HAVE_QT6)
#include <QIcon>
#include <QPixmap>
#include <QColor>
#include <QSize>

namespace catchim::ui {

enum class UiIcon {
    Media,
    CloudUpload,
    Audio,
    Text,
    Stickers,
    Effects,
    Transitions,
    Captions,
    Settings,
    Split,
    Duplicate,
    Delete,
    Magnet,
    Play,
    Pause,
    StepBack,
    StepForward,
    SkipBack,
    SkipForward,
    ZoomIn,
    ZoomOut,
    Undo,
    Redo,
    Export,
    Plus,
    Eye,
    EyeOff,
    Search,
    SplitLeft,
    SplitRight,
    Unlink,
    Link,
    Adjustment,
    Fullscreen,
    Target,
    Chart,
    ShapeRect,
    ShapeCircle,
    ShapeStar,
    ShapeArrow,
    Sun,
    Contrast,
    Sliders,
    Volume,
    VolumeMute,
    Grid,
    Keyboard,
    Check,
    List,
    Clock,
    Folder,
    Edit,
    MoreHorizontal,
    ArrowDown,
    ArrowUp,
    Info,
    Logo,
    Upload,
    Ripple,
    Bookmark,
    Video,
    Layers,
    Snow,
    SortOneNine,
    SlidersBox,
    Music,
    Transform,
    Font,
    Drop,
    Speed
};

class UiIcons {
public:
    static QIcon get(UiIcon icon, const QColor& color = QColor("#f4f4f5"), int size = 20);
    static QPixmap getPixmap(UiIcon icon, const QColor& color = QColor("#f4f4f5"), int size = 20);
    static const char* getSvgContent(UiIcon icon);
};

} // namespace catchim::ui
#endif
