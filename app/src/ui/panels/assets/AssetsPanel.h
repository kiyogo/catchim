#pragma once

#if defined(HAVE_QT6)
#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QHash>
#include <QImage>
#include <QSet>
#include <QThreadPool>
#include "editor/EditorEngine.h"
#include "media/MediaLibrary.h"

namespace catchim::ui {

class SidebarTooltipWidget;

class AssetsPanel : public QWidget {
    Q_OBJECT
public:
    AssetsPanel(editor::EditorEngine& engine, media::MediaLibrary& mediaLibrary, QWidget* parent = nullptr);
    ~AssetsPanel() override;

    void refresh();
    void revealMedia(const core::MediaId& mediaId);
    void showTabTooltip(int row);

    QSize sizeHint() const override { return QSize(340, 600); }
    QSize minimumSizeHint() const override { return QSize(220, 180); }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void onImportClicked();
    void onMediaItemDoubleClicked(QListWidgetItem* item);
    void onAddMediaToTimeline(const core::MediaId& id);
    void onAddMediaAsNewLayer(const core::MediaId& id);
    void onAddAudioSfx(const QString& name, double durationSec);
    void onAddTextPreset(const QString& title, const QString& fontStyle);
    void onAddGraphicPreset(const QString& name, const QString& shapeType);
    void onApplyEffectPreset(const QString& effectName);
    void onApplyTransitionPreset(const QString& name, double durationSec);
    void onApplyAdjustment(double brightness, double contrast, double saturation, double temperature, double tint);
    void onAddAdjustmentLayer();
    void onImportSrtClicked();
    void onCanvasAspectChanged(int width, int height);
    void onCanvasBgColorChanged(const QColor& color);

private:
    void setupUi();
    QWidget* createMediaView();
    QWidget* createAudioView();
    QWidget* createTextView();
    QWidget* createStickersView();
    QWidget* createEffectsView();
    QWidget* createTransitionsView();
    QWidget* createSubtitlesView();
    QWidget* createAdjustmentView();
    QWidget* createSettingsView();

    editor::EditorEngine& engine_;
    media::MediaLibrary& mediaLibrary_;

    enum class MediaSortKey { Name, Type, Duration, Size };
    enum class MediaSortOrder { Asc, Desc };

    MediaSortKey mediaSortKey_{MediaSortKey::Name};
    MediaSortOrder mediaSortOrder_{MediaSortOrder::Asc};
    QPushButton* sortBtn_{nullptr};
    QMenu* sortMenu_{nullptr};

    void updateSortMenu();
    void applySort();
    void requestThumbnail(const std::shared_ptr<media::MediaAsset>& asset);
    QHash<QString,QImage> mediaThumbnails_;
    QSet<QString> pendingThumbnails_;
    QThreadPool thumbnailPool_;

    QListWidget* tabList_{nullptr};
    QStackedWidget* viewsStack_{nullptr};
    QListWidget* mediaListWidget_{nullptr};
    QWidget* mediaEmptyOverlay_{nullptr};
    QLineEdit* mediaSearchInput_{nullptr};
    SidebarTooltipWidget* sidebarTooltip_{nullptr};
};

} // namespace catchim::ui
#endif
