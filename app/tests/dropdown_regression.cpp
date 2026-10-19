#include <QApplication>
#include <QDir>
#include <QScrollBar>
#include <iostream>
#include "ui/widgets/WebComboBox.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    catchim::ui::Theme::instance().setTheme(catchim::ui::ThemeMode::Light);
    QWidget window;
    window.resize(640, 600);
    window.move(20, 20);
    catchim::ui::WebComboBox combo(&window);
    combo.addItems({"Vừa khung", "25%", "50%", "75%", "100%", "150%", "200%"});
    combo.setGeometry(40, 40, 106, 28);
    window.show();
    app.processEvents();
    bool passed = true;
    auto check = [&](const char* name, bool ok) {
        std::cout << (ok ? "PASS " : "FAIL ") << name << std::endl;
        passed &= ok;
    };
    const QString output = QDir::currentPath() + "/qa/dropdown";
    QDir().mkpath(output);
    auto open = [&] {
        combo.showPopup();
        app.processEvents();
        return combo.popupWidget()->findChild<QListView*>("webSelectList");
    };
    auto* list = open();
    const auto last = list->model()->index(combo.count()-1, 0);
    std::cout << "viewport=" << list->viewport()->height()
              << " last-bottom=" << list->visualRect(last).bottom()
              << " row-hint=" << list->sizeHintForRow(0)
              << " row-pitch=" << list->visualRect(list->model()->index(1,0)).top()
                                    -list->visualRect(list->model()->index(0,0)).top()
              << " scroll-range=" << list->verticalScrollBar()->maximum() << std::endl;
    check("all-seven-options-fully-visible", list->viewport()->rect().contains(list->visualRect(last)));
    check("no-scrolling-when-options-fit", list->verticalScrollBar()->maximum()==0);
    combo.popupWidget()->grab().save(output + "/full-menu.png");
    combo.hidePopup();

    combo.setCurrentIndex(6);
    list = open();
    check("last-selected-does-not-hide-first-option",
          list->viewport()->rect().contains(list->visualRect(list->model()->index(0,0))));
    combo.hidePopup();

    combo.move(40, 400);
    list = open();
    const QRect appBounds(window.mapToGlobal(QPoint()), window.size());
    check("overflow-stops-at-app-bottom", combo.popupWidget()->geometry().bottom()<=appBounds.bottom());
    check("overflow-stays-below-control",
          combo.popupWidget()->y()>=combo.mapToGlobal(QPoint(0,combo.height())).y());
    check("overflow-can-scroll", list->verticalScrollBar()->maximum()>0);
    list->verticalScrollBar()->setValue(list->verticalScrollBar()->maximum());
    app.processEvents();
    check("overflow-last-option-fully-visible", list->viewport()->rect().contains(list->visualRect(last)));
    check("no-white-strip-after-last-option", list->viewport()->height()-list->visualRect(last).bottom()-1<=2);
    combo.popupWidget()->grab().save(output + "/overflow-menu.png");
    combo.hidePopup();
    return passed ? 0 : 1;
}
