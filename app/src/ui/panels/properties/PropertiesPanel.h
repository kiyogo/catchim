#pragma once
#if defined(HAVE_QT6)
#include <QWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QLabel>
#include <QVBoxLayout>
#include <functional>
#include <map>
#include <vector>
#include "editor/EditorEngine.h"
#include "ui/icons/UiIcons.h"

namespace catchim::ui {
class PropertiesPanel : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesPanel(editor::EditorEngine& engine, QWidget* parent = nullptr);
    void refresh();
    QSize sizeHint() const override { return QSize(320, 600); }
    QSize minimumSizeHint() const override { return QSize(280, 300); }
private:
    void setupUi();
    QVBoxLayout* addTab(const QString& key, const QString& title, UiIcon icon);
    void selectTab(const QString& key);
    void patch(const nlohmann::json& values, std::optional<bool> muted = std::nullopt);
    editor::EditorEngine& engine_;
    QWidget* emptyView_{nullptr};
    QLabel* multiSelection_{nullptr};
    QWidget* inspectorView_{nullptr};
    QWidget* rail_{nullptr};
    QScrollArea* scrollArea_{nullptr};
    struct Tab { QPushButton* button; QWidget* page; };
    std::map<QString, Tab> tabs_;
    std::map<editor::ClipType, QString> rememberedTabs_;
    std::vector<std::function<void(const editor::Clip&)>> readers_;
    QString activeTab_;
    QString themeSignature_;
    QString visibleTabsSignature_;
    bool isUpdatingUi_{false};
};
}
#endif
