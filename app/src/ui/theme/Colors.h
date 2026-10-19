#pragma once

#if defined(HAVE_QT6)
#include <QColor>
#endif

namespace catchim::ui {

#if defined(HAVE_QT6)
struct Palette {
    QColor background;
    QColor panelBackground;
    QColor textPrimary;
    QColor textSecondary;
    QColor primaryAccent;
    QColor secondary;
    QColor border;
    QColor panelBorder;
    QColor hover;
    QColor selected;
    QColor destructive;

    // Popover / Dropdown Menu colors (matching web dropdown-menu & select)
    QColor popoverBackground;
    QColor popoverForeground;
    QColor popoverBorder;
    QColor popoverHover;
    QColor popoverSeparator;

    // Timeline clip colors
    QColor clipVideo;
    QColor clipAudio;
    QColor clipText;
    QColor clipGraphic;
    QColor clipEffect;
    QColor clipBookmark;
    QColor waveformColor;
};

inline Palette getDarkPalette() {
    Palette p;
    p.background = QColor("#0d0d0d");       // hsl(0, 0%, 5%)
    p.panelBackground = QColor("#1a1a1a");  // hsl(0, 0%, 10%)
    p.textPrimary = QColor("#dedede");      // hsl(0, 0%, 87%)
    p.textSecondary = QColor("#808080");    // hsl(0, 0%, 50%)
    p.primaryAccent = QColor("#38bdf8");    // sky-400
    p.secondary = QColor("#1a1a1a");        // panel background
    p.border = QColor("#2e2e2e");           // hsl(0, 0%, 18%)
    p.panelBorder = QColor("#2e2e2e");      // hsl(0, 0%, 18%)
    p.hover = QColor("#242424");            // hsl(0, 0%, 14%)
    p.selected = QColor("#0284c7");         // sky-600
    p.destructive = QColor("#ef4444");      // red-500

    p.popoverBackground = QColor("#0d0d0d"); // web .dark --popover hsl(0, 0%, 5%)
    p.popoverForeground = QColor("#dedede"); // web .dark --popover-foreground hsl(0, 0%, 95%)
    p.popoverBorder = QColor("#2e2e2e");     // web .dark --border hsl(0, 0%, 16%)
    p.popoverHover = QColor("#212121");      // web .dark --popover-hover hsl(0, 0%, 13%)
    p.popoverSeparator = QColor("#2e2e2e");  // web .dark --muted hsl(0, 0%, 20%)

    // Clip colors matching web timeline exactly
    p.clipVideo = QColor("#1e293b");        // Video (slate-800)
    p.clipAudio = QColor("#8F5DBA");        // Audio (web #8F5DBA purple)
    p.clipText = QColor("#5DBAA0");         // Text (web #5DBAA0 teal)
    p.clipGraphic = QColor("#BA5D7A");      // Graphic (web #BA5D7A rose)
    p.clipEffect = QColor("#5D93BA");       // Effect (web #5D93BA soft blue)
    p.clipBookmark = QColor("#009DFF");     // Bookmark (web #009DFF sky)
    p.waveformColor = QColor(255, 255, 255, 180);
    return p;
}

inline Palette getLightPalette() {
    Palette p;
    p.background = QColor("#ffffff");       // white window container background
    p.panelBackground = QColor("#F9FAFB");  // #F9FAFB panel surface
    p.textPrimary = QColor("#212121");      // .panel --foreground: hsl(0 0% 13%)
    p.textSecondary = QColor("#7a7a7a");    // .panel --muted-foreground: 48%
    p.primaryAccent = QColor("#17aaF3");    // --primary: hsl(200 90% 52%)
    p.secondary = QColor("#EEEFF0");        // #EEEFF0 drop card surface
    p.border = QColor("#DEDEDE");           // #DEDEDE web border
    p.panelBorder = QColor("#DEDEDE");      // #DEDEDE panel border
    p.hover = QColor("#ededed");            // .panel --accent: hsl(0 0% 93%)
    p.selected = QColor("#e0f2fe");         // sky-100
    p.destructive = QColor("#ef4444");      // red-500

    p.popoverBackground = QColor("#ffffff"); // web :root --popover hsl(0, 0%, 100%)
    p.popoverForeground = QColor("#09090b"); // web :root --popover-foreground hsl(0, 0%, 2%)
    p.popoverBorder = QColor("#dedede");     // web :root --border hsl(0, 0%, 91%)
    p.popoverHover = QColor("#f4f4f5");      // web :root --popover-hover hsl(0, 0%, 96%)
    p.popoverSeparator = QColor("#e4e4e7");  // web :root --muted hsl(0, 0%, 85.1%)

    p.clipVideo = QColor("#2563eb");        // blue-600
    p.clipAudio = QColor("#8F5DBA");
    p.clipText = QColor("#5DBAA0");
    p.clipGraphic = QColor("#BA5D7A");
    p.clipEffect = QColor("#5D93BA");
    p.clipBookmark = QColor("#0284c7");     // sky-600
    p.waveformColor = QColor(255, 255, 255, 220);
    return p;
}
#endif

} // namespace catchim::ui
