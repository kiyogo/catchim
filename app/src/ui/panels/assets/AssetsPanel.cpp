#include "AssetsPanel.h"

#if defined(HAVE_QT6)
#include "ui/icons/UiIcons.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/WebComboBox.h"
#include "ui/widgets/AssetTile.h"
#include "ui/widgets/FlowLayout.h"
#include "ui/widgets/PresetInsertion.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include "media/probe/MediaProbe.h"
#include "media/decoder/NativeVideoDecoder.h"
#include <QPointer>
#include <QRunnable>
#include "core/time/Timecode.h"
#include "subtitles/SrtParser.h"
#include "subtitles/TranscriptionLanguagesRegistry.h"
#include "audio/TtsEngine.h"
#include "audio/TtsServiceEngine.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QColorDialog>
#include "ui/widgets/ColorPickerDialog.h"
#include "ui/widgets/SidebarTooltipWidget.h"
#include <QLabel>
#include <QScrollArea>
#include <QGroupBox>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QSpinBox>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QTextEdit>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QUuid>
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif
#include <QDrag>
#include <QApplication>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <QToolTip>
#include <fstream>
#include <sstream>
#include <cmath>
#ifdef _WIN32
#include <objbase.h>
#endif

namespace catchim::ui {
static void initializeImportedClip(editor::Clip& clip,const media::MediaAsset& asset) {
    clip.setMediaId(asset.id());clip.setSourceDuration(asset.duration());clip.setParam("hasAudio",asset.hasAudio());
    if(asset.width()>0&&asset.height()>0) {
        clip.setParam("sourceWidth",asset.width());clip.setParam("sourceHeight",asset.height());
        clip.setParam("width",asset.width());clip.setParam("height",asset.height());
    }
    if(asset.fps().toFps()>0)clip.setParam("fps",asset.fps().toFps());
}

class SectionToggle : public QPushButton {
public:
    explicit SectionToggle(const QString& title,QWidget* parent):QPushButton(title,parent){setCheckable(true);setFixedHeight(44);setCursor(Qt::PointingHandCursor);}
protected:
    void paintEvent(QPaintEvent*) override {
        const auto& colors=Theme::instance().palette();QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
        p.setPen(isChecked()?colors.textPrimary:colors.textSecondary);QFont f("Inter");f.setPixelSize(13);f.setWeight(QFont::Medium);p.setFont(f);
        p.drawText(rect().adjusted(14,0,-36,0),Qt::AlignLeft|Qt::AlignVCenter,text());
        p.setPen(QPen(colors.textSecondary,1.2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        const QPointF c(width()-28,height()/2.0);QPainterPath chevron;
        if(isChecked()){chevron.moveTo(c+QPointF(-3,-1.5));chevron.lineTo(c+QPointF(0,1.5));chevron.lineTo(c+QPointF(3,-1.5));}
        else {chevron.moveTo(c+QPointF(-1.5,-3));chevron.lineTo(c+QPointF(1.5,0));chevron.lineTo(c+QPointF(-1.5,3));}
        p.drawPath(chevron);
    }
};

class CanvasPresetButton : public QPushButton {
public:
    CanvasPresetButton(const QString& text,int w,int h,QWidget* parent)
        : QPushButton(text,parent),canvasWidth_(w),canvasHeight_(h) {
        setCheckable(true); setFixedHeight(34); setCursor(Qt::PointingHandCursor);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        const auto& pal=Theme::instance().palette();
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        const bool dark=Theme::instance().mode()==ThemeMode::Dark;
        const QColor ink=isChecked()?QColor(dark?"#38bdf8":"#027dbb"):pal.textSecondary;
        p.setPen(Qt::NoPen);
        if (isChecked()) {p.setBrush(dark?QColor("#15323f"):QColor("#e6f5ff"));p.drawRoundedRect(rect().adjusted(0,0,-1,-1),10,10);p.setPen(QPen(dark?QColor("#204453"):QColor("#d6efff"),1));p.setBrush(Qt::NoBrush);p.drawRoundedRect(rect().adjusted(0,0,-1,-1),10,10);}
        else if (underMouse()) {p.setBrush(pal.hover);p.drawRoundedRect(rect().adjusted(0,0,-1,-1),10,10);}
        p.setPen(QPen(ink,1.1)); p.setBrush(Qt::NoBrush);
        QSizeF shape(canvasWidth_>0?canvasWidth_:1,canvasHeight_>0?canvasHeight_:1);shape.scale(16,16,Qt::KeepAspectRatio);
        QRectF box(10+(20-shape.width())/2,(height()-shape.height())/2,shape.width(),shape.height());
        p.drawRoundedRect(box,1.5,1.5);
        if (canvasWidth_==0) {p.drawLine(QPointF(25,21),QPointF(31,21));p.drawLine(QPointF(28,18),QPointF(28,24));}
        QFont f("Inter");f.setPixelSize(13);f.setWeight(QFont::Medium);p.setFont(f);p.setPen(ink);
        p.drawText(rect().adjusted(40,0,-32,0),Qt::AlignVCenter|Qt::AlignLeft,text());
        if (isChecked()) {QPainterPath tick;tick.moveTo(width()-22,17);tick.lineTo(width()-19,20);tick.lineTo(width()-13,14);p.setPen(QPen(ink,1.2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.drawPath(tick);}
    }
private:
    int canvasWidth_,canvasHeight_;
};

class MediaListWidget : public QListWidget {
public:
    explicit MediaListWidget(QWidget* parent = nullptr) : QListWidget(parent) {}

protected:
    bool viewportEvent(QEvent* event) override {
        if(event->type()==QEvent::ToolTip){QToolTip::hideText();event->accept();return true;}
        return QListWidget::viewportEvent(event);
    }
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            dragStartPos_ = event->pos();
        }
        QListWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (!(event->buttons() & Qt::LeftButton)) {
            QListWidget::mouseMoveEvent(event);
            return;
        }
        if ((event->pos() - dragStartPos_).manhattanLength() < QApplication::startDragDistance()) {
            QListWidget::mouseMoveEvent(event);
            return;
        }
        auto* item = itemAt(dragStartPos_);
        if (!item) {
            item = currentItem();
        }
        if (!item) return;

        QString assetId = item->data(Qt::UserRole).toString();
        QString filePath = item->data(Qt::UserRole + 1).toString();
        if (assetId.isEmpty()) return;

        auto* drag = new QDrag(this);
        auto* mime = new QMimeData();
        mime->setData("application/x-catchim-media-id", assetId.toUtf8());
        mime->setText(assetId);
        if (!filePath.isEmpty()) {
            QList<QUrl> urls;
            urls.append(QUrl::fromLocalFile(filePath));
            mime->setUrls(urls);
        }
        drag->setMimeData(mime);
        const auto ratio=devicePixelRatioF();
        QPixmap ghost(QSize(qRound(84*ratio),qRound(84*ratio)));ghost.setDevicePixelRatio(ratio);ghost.fill(Qt::transparent);
        QPainter painter(&ghost);painter.setRenderHint(QPainter::Antialiasing);painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF card(2,2,80,80);QPainterPath clip;clip.addRoundedRect(card,12,12);painter.setClipPath(clip);
        painter.fillRect(card,Theme::instance().palette().secondary);
        const auto cover=item->icon().pixmap(160,160);
        const int side=std::min(cover.width(),cover.height());
        painter.drawPixmap(card.toRect(),cover,QRect((cover.width()-side)/2,(cover.height()-side)/2,side,side));
        const auto duration=item->data(Qt::UserRole+2).toString();
        const QRectF badge(35,62,43,17);painter.fillRect(badge,QColor(0,0,0,180));painter.setPen(Qt::white);
        QFont font("Inter");font.setPixelSize(12);painter.setFont(font);painter.drawText(badge,Qt::AlignCenter,duration);
        painter.setClipping(false);painter.setPen(QPen(Theme::instance().palette().primaryAccent,2));painter.setBrush(Qt::NoBrush);painter.drawRoundedRect(card,12,12);painter.end();
        drag->setPixmap(ghost);drag->setHotSpot(QPoint(8,8));
        drag->exec(Qt::CopyAction | Qt::MoveAction);
    }

private:
    QPoint dragStartPos_;
};

// Qt stylesheet font sizes round fractional CSS pixels. Keep the media
// caption's 0.72rem size with a fractional painter transform instead.
class MediaDropCaption : public QLabel {
public:
    using QLabel::QLabel;
    QFont displayFont() const {
        auto result = font();
        result.setPixelSize(12);
        result.setHintingPreference(QFont::PreferNoHinting);
        return result;
    }
    QSize sizeHint() const override {
        return {qCeil(QFontMetricsF(displayFont()).horizontalAdvance(text()) * fontScale), 15};
    }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override {
        const QFontMetricsF metrics(displayFont());
        const auto bounds = metrics.boundingRect(QRectF(0, 0, width / fontScale, 1000), Qt::TextWordWrap, text());
        return qCeil(bounds.height() * fontScale);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.scale(fontScale, fontScale);
        painter.setFont(displayFont());
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(QRectF(0, 0, width() / fontScale, height() / fontScale), Qt::AlignCenter | Qt::TextWordWrap, text());
    }
private:
    static constexpr qreal fontScale = 11.52 / 12.0;
};

class DropCardWidget : public QFrame {
public:
    explicit DropCardWidget(QWidget* parent = nullptr) : QFrame(parent) {
        setCursor(Qt::PointingHandCursor);
    }
    QSize sizeHint() const override {
        auto preferred = QFrame::sizeHint();
        if (textLabel) preferred.setWidth(textLabel->sizeHint().width() + 64);
        return preferred;
    }
    QLabel* textLabel{nullptr};
    std::function<void()> onClicked;
protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && onClicked) {
            onClicked();
        }
        QFrame::mousePressEvent(event);
    }
};



// The web rail consists of 32px icon buttons separated by 2px. Painting
// their decorations explicitly avoids QListWidget's platform-dependent padding.
class AssetTabDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return {40, 34}; }
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const auto& palette = Theme::instance().palette();
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRect button(option.rect.x() + 4, option.rect.y(), 32, 32);
        if (option.state & QStyle::State_Selected) {
            painter->setPen(QPen(palette.primaryAccent.lighter(175), 1));
            QColor tint = palette.primaryAccent;
            tint.setAlpha(20);
            painter->setBrush(tint);
            painter->drawRoundedRect(button.adjusted(0, 0, -1, -1), 5.6, 5.6);
        } else if (option.state & QStyle::State_MouseOver) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(palette.hover);
            painter->drawRoundedRect(button, 5.6, 5.6);
        }
        const auto icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        icon.paint(painter, QRect(button.center().x() - 7, button.y() + 8, 16, 16));
        painter->restore();
    }
};

static const std::vector<std::pair<QString, UiIcon>> kTabDefs = {
    {"Tệp media", UiIcon::Media},
    {"Âm thanh", UiIcon::Audio},
    {"Văn bản", UiIcon::Text},
    {"Nhãn dán", UiIcon::Stickers},
    {"Hiệu ứng", UiIcon::Effects},
    {"Chuyển cảnh", UiIcon::Transitions},
    {"Phụ đề", UiIcon::Captions},
    {"Điều chỉnh", UiIcon::Adjustment},
    {"Cài đặt", UiIcon::Settings}
};

AssetsPanel::AssetsPanel(editor::EditorEngine& engine, media::MediaLibrary& mediaLibrary, QWidget* parent)
    : QWidget(parent)
    , engine_(engine)
    , mediaLibrary_(mediaLibrary)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setAcceptDrops(true);
    setupUi();
    refresh();
}

AssetsPanel::~AssetsPanel() {
    thumbnailPool_.clear();thumbnailPool_.waitForDone();
    if (sidebarTooltip_) {
        sidebarTooltip_->hide();
        delete sidebarTooltip_;
        sidebarTooltip_ = nullptr;
    }
}

void AssetsPanel::showTabTooltip(int row) {
    if (row < 0 || row >= static_cast<int>(kTabDefs.size()) || !tabList_) return;
    auto* item = tabList_->item(row);
    if (!item) return;
    QRect rect = tabList_->visualItemRect(item);
    QPoint globalRightCenter = tabList_->viewport()->mapToGlobal(QPoint(tabList_->viewport()->width(), rect.center().y()));
    if (!sidebarTooltip_) {
        sidebarTooltip_ = new SidebarTooltipWidget(this);
    }
    sidebarTooltip_->showTip(kTabDefs[row].first, globalRightCenter);
}

bool AssetsPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == tabList_ || (tabList_ && watched == tabList_->viewport())) {
        if (event->type() == QEvent::ToolTip) {
            // Suppress standard Qt native tooltip so ONLY SidebarTooltipWidget shows!
            event->accept();
            return true;
        }
        if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            QPoint vpPos = (watched == tabList_->viewport()) ? me->pos() : tabList_->viewport()->mapFrom(tabList_, me->pos());
            QListWidgetItem* item = tabList_->itemAt(vpPos);
            if (item) {
                int row = tabList_->row(item);
                if (row >= 0 && row < static_cast<int>(kTabDefs.size())) {
                    showTabTooltip(row);
                    return false;
                }
            }
            if (sidebarTooltip_) {
                sidebarTooltip_->hideTip();
            }
        } else if (event->type() == QEvent::Leave) {
            if (sidebarTooltip_) {
                sidebarTooltip_->hideTip();
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void AssetsPanel::hideEvent(QHideEvent* event) {
    if (sidebarTooltip_) {
        sidebarTooltip_->hideTip();
    }
    QWidget::hideEvent(event);
}

void AssetsPanel::setupUi() {
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(1, 1, 1, 1);
    mainLayout->setSpacing(0);

    // Left vertical TabBar (Media, Text, Stickers, Effects, Transitions, Captions, Adjustment, Settings)
    tabList_ = new QListWidget(this);
    tabList_->setObjectName("tabList");
    tabList_->setFixedWidth(41);
    tabList_->setIconSize(QSize(16, 16));
    tabList_->setItemDelegate(new AssetTabDelegate(tabList_));
    const QString railBorder = Theme::instance().mode() == ThemeMode::Light ? QStringLiteral("#dedede") : QStringLiteral("#2e2e2e");
    tabList_->setStyleSheet("QListWidget#tabList { background: transparent; border: none; border-right: 1px solid " + railBorder + "; padding: 4px 0; } QListWidget#tabList::item { padding: 0; margin: 0; }");
    tabList_->setFocusPolicy(Qt::NoFocus);
    tabList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tabList_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tabList_->setMouseTracking(true);
    tabList_->viewport()->setMouseTracking(true);
    tabList_->installEventFilter(this);
    tabList_->viewport()->installEventFilter(this);

    const auto& pal = Theme::instance().palette();
    for (size_t i = 0; i < kTabDefs.size(); ++i) {
        auto* item = new QListWidgetItem(tabList_);
        item->setIcon(UiIcons::get(kTabDefs[i].second, (i == 0) ? pal.primaryAccent : pal.textSecondary, 18));
        item->setSizeHint(QSize(32, 32));
        item->setTextAlignment(Qt::AlignCenter);
    }
    tabList_->setCurrentRow(0);

    viewsStack_ = new QStackedWidget(this);
    viewsStack_->addWidget(createMediaView());
    viewsStack_->addWidget(createAudioView());
    viewsStack_->addWidget(createTextView());
    viewsStack_->addWidget(createStickersView());
    viewsStack_->addWidget(createEffectsView());
    viewsStack_->addWidget(createTransitionsView());
    viewsStack_->addWidget(createSubtitlesView());
    viewsStack_->addWidget(createAdjustmentView());
    viewsStack_->addWidget(createSettingsView());

    connect(tabList_, &QListWidget::currentRowChanged, [this](int row) {
        viewsStack_->setCurrentIndex(row);
        const auto& curPal = Theme::instance().palette();
        for (int i = 0; i < tabList_->count(); ++i) {
            auto* it = tabList_->item(i);
            if (it && i < static_cast<int>(kTabDefs.size())) {
                QColor col = (i == row) ? curPal.primaryAccent : curPal.textSecondary;
                it->setIcon(UiIcons::get(kTabDefs[i].second, col, 18));
            }
        }
        if (sidebarTooltip_) {
            sidebarTooltip_->hideTip();
        }
    });

    mainLayout->addWidget(tabList_);
    mainLayout->addWidget(viewsStack_, 1);
}

void AssetsPanel::updateSortMenu() {
    if (!sortMenu_) {
        sortMenu_ = new QMenu(this);
        sortMenu_->setAttribute(Qt::WA_TranslucentBackground, true);
        sortMenu_->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    }
    sortMenu_->clear();

    const auto& pal = Theme::instance().palette();
    sortMenu_->setStyleSheet(QString(R"(
        QMenu {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 4px;
        }
        QMenu::item {
            background-color: transparent;
            color: %2;
            padding: 6px 14px;
            min-height: 20px;
            border-radius: 4px;
            margin: 1px 0px;
            font-family: 'Inter', sans-serif;
            font-size: 13px;
            font-weight: 400;
        }
        QMenu::item:selected {
            background-color: %4;
            color: %2;
        }
    )")
    .arg(pal.popoverBackground.name())
    .arg(pal.popoverForeground.name())
    .arg(pal.popoverBorder.name())
    .arg(pal.popoverHover.name()));

    auto makeItem = [this](MediaSortKey key, const QString& title) {
        QString label = title;
        if (mediaSortKey_ == key) {
            label += (mediaSortOrder_ == MediaSortOrder::Asc) ? "  ↑" : "  ↓";
        }
        sortMenu_->addAction(label, [this, key]() {
            if (mediaSortKey_ == key) {
                mediaSortOrder_ = (mediaSortOrder_ == MediaSortOrder::Asc) ? MediaSortOrder::Desc : MediaSortOrder::Asc;
            } else {
                mediaSortKey_ = key;
                mediaSortOrder_ = MediaSortOrder::Asc;
            }
            updateSortMenu();
            applySort();
        });
    };

    makeItem(MediaSortKey::Name, "Name");
    makeItem(MediaSortKey::Type, "Type");
    makeItem(MediaSortKey::Duration, "Duration");
    makeItem(MediaSortKey::Size, "Size");

    if (sortBtn_) {
        QString keyName = (mediaSortKey_ == MediaSortKey::Name) ? "Name" :
                          (mediaSortKey_ == MediaSortKey::Type) ? "Type" :
                          (mediaSortKey_ == MediaSortKey::Duration) ? "Duration" : "Size";
        QString dir = (mediaSortOrder_ == MediaSortOrder::Asc) ? "Ascending" : "Descending";
        sortBtn_->setToolTip(QString("Sort by %1 — %2").arg(keyName, dir));
    }
}

void AssetsPanel::applySort() {
    if (!mediaListWidget_) return;

    std::vector<std::shared_ptr<media::MediaAsset>> sorted = mediaLibrary_.assets();
    std::sort(sorted.begin(), sorted.end(), [this](const auto& a, const auto& b) {
        bool less = false;
        switch (mediaSortKey_) {
            case MediaSortKey::Name:
                less = QString::compare(QString::fromStdString(a->fileName()), QString::fromStdString(b->fileName()), Qt::CaseInsensitive) < 0;
                break;
            case MediaSortKey::Type:
                less = static_cast<int>(a->type()) < static_cast<int>(b->type());
                break;
            case MediaSortKey::Duration:
                less = a->duration().ticks() < b->duration().ticks();
                break;
            case MediaSortKey::Size: {
                uintmax_t sA = 0, sB = 0;
                std::error_code ec;
                sA = std::filesystem::file_size(a->filePath(), ec);
                ec.clear();
                sB = std::filesystem::file_size(b->filePath(), ec);
                less = sA < sB;
                break;
            }
        }
        return (mediaSortOrder_ == MediaSortOrder::Asc) ? less : !less;
    });

    const auto selected=mediaListWidget_->currentItem()?mediaListWidget_->currentItem()->data(Qt::UserRole).toString():QString();
    mediaListWidget_->clear();
    const auto& pal = Theme::instance().palette();
    for (const auto& asset : sorted) {
        auto* item = new QListWidgetItem(mediaListWidget_);
        std::string durStr = core::Timecode::format(asset->duration(), core::TimecodeFormat::MM_SS);
        UiIcon iconType = (asset->type() == media::MediaType::Video) ? UiIcon::Media :
                          (asset->type() == media::MediaType::Audio) ? UiIcon::Audio : UiIcon::Media;
        item->setIcon(UiIcons::get(iconType, pal.primaryAccent, 24));
        const auto assetKey=QString::fromStdString(asset->id().str());
        if(mediaThumbnails_.contains(assetKey)&&!mediaThumbnails_.value(assetKey).isNull())item->setIcon(QIcon(QPixmap::fromImage(mediaThumbnails_.value(assetKey))));
        else if(asset->type()!=media::MediaType::Audio)requestThumbnail(asset);

        item->setText(QString::fromStdString(asset->fileName()));
        item->setData(Qt::UserRole, QString::fromStdString(asset->id().str()));
        item->setData(Qt::UserRole+1,QString::fromStdWString(asset->filePath().wstring()));
        item->setData(Qt::UserRole+2,QString::fromStdString(durStr));
        if(assetKey==selected)mediaListWidget_->setCurrentItem(item);
    }
}

void AssetsPanel::requestThumbnail(const std::shared_ptr<media::MediaAsset>& asset) {
    const auto key=QString::fromStdString(asset->id().str());
    if(mediaThumbnails_.contains(key)||pendingThumbnails_.contains(key))return;
    pendingThumbnails_.insert(key);thumbnailPool_.setMaxThreadCount(1);
    const QPointer<AssetsPanel> target(this);const auto path=asset->filePath();
    thumbnailPool_.start(QRunnable::create([target,path,key]{
#ifdef _WIN32
        const auto com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
#endif
        int width=0,height=0;std::vector<uint8_t> rgba;QImage image;
        auto& decoder=media::NativeVideoDecoder::thumbnailInstance();decoder.setPreviewSize(240,136);
        if(decoder.getFrame(path,0,width,height,rgba,"asset-thumbnail")&&width>0&&height>0)
            image=QImage(rgba.data(),width,height,width*4,QImage::Format_RGBA8888).scaled(240,136,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation).copy();
        QMetaObject::invokeMethod(qApp,[target,key,image]{
            if(!target)return;
            target->pendingThumbnails_.remove(key);target->mediaThumbnails_.insert(key,image);
            if(image.isNull())return;
            for(int i=0;i<target->mediaListWidget_->count();++i){auto* item=target->mediaListWidget_->item(i);if(item->data(Qt::UserRole).toString()==key)item->setIcon(QIcon(QPixmap::fromImage(image)));}
        },Qt::QueuedConnection);
#ifdef _WIN32
        if(SUCCEEDED(com))CoUninitialize();
#endif
    }));
}

static QWidget* createPanelHeaderWidget(QWidget* parent, const QString& title, QWidget* actionWidget = nullptr) {
    const auto& pal = Theme::instance().palette();
    auto* header = new QWidget(parent);
    header->setFixedHeight(44);
    header->setObjectName("panelHeader");
    header->setStyleSheet(QString("border-bottom: 1px solid %1; background: transparent;").arg(Theme::instance().mode() == ThemeMode::Light ? QStringLiteral("#dedede") : QStringLiteral("#2e2e2e")));
    auto* hLayout = new QHBoxLayout(header);
    hLayout->setContentsMargins(12, 0, 8, 0);
    hLayout->setSpacing(6);

    auto* titleLabel = new QLabel(title, header);
    titleLabel->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: 400; border: none;").arg(pal.textSecondary.name()));
    hLayout->addWidget(titleLabel);
    hLayout->addStretch(1);
    if (actionWidget) {
        hLayout->addWidget(actionWidget);
    }
    return header;
}

QWidget* AssetsPanel::createMediaView() {
    auto* view = new QWidget(this);
    auto* layout = new QVBoxLayout(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 1. Header (Matches web/src/components/editor/panels/assets/views/base-panel.tsx)
    const auto& pal = Theme::instance().palette();
    auto* header = new QWidget(view);
    header->setFixedHeight(44);
    header->setObjectName("panelHeader");
    header->setStyleSheet(QString("border-bottom: 1px solid %1; background: transparent;").arg(Theme::instance().mode() == ThemeMode::Light ? QStringLiteral("#dedede") : QStringLiteral("#2e2e2e")));
    auto* hLayout = new QHBoxLayout(header);
    hLayout->setContentsMargins(12, 0, 8, 0);
    hLayout->setSpacing(6);

    auto* titleLabel = new QLabel("Tệp media", header);
    titleLabel->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: 400; border: none;").arg(pal.textSecondary.name()));
    hLayout->addWidget(titleLabel);

    hLayout->addStretch(1);

    // Right actions: ViewMode (Grid/List), Sort dropdown, Import button
    auto* viewModeBtn = new QPushButton(header);
    viewModeBtn->setFixedSize(28, 28);
    viewModeBtn->setIcon(UiIcons::get(UiIcon::List, pal.textSecondary, 16));
    viewModeBtn->setIconSize(QSize(16, 16));
    viewModeBtn->setToolTip("Toggle Grid / List View");
    viewModeBtn->setCursor(Qt::PointingHandCursor);
    viewModeBtn->setStyleSheet(QString("QPushButton { background: transparent; border: none; border-radius: 4px; } QPushButton:hover { background: %1; }").arg(pal.hover.name()));
    connect(viewModeBtn, &QPushButton::clicked, [this, viewModeBtn]() {
        const auto& curPal = Theme::instance().palette();
        if (mediaListWidget_->viewMode() == QListView::IconMode) {
            mediaListWidget_->setViewMode(QListView::ListMode);
            mediaListWidget_->setIconSize(QSize(24, 24));
            viewModeBtn->setIcon(UiIcons::get(UiIcon::Grid, curPal.textSecondary, 16));
        } else {
            mediaListWidget_->setViewMode(QListView::IconMode);
            mediaListWidget_->setIconSize(QSize(64, 48));
            viewModeBtn->setIcon(UiIcons::get(UiIcon::List, curPal.textSecondary, 16));
        }
    });
    hLayout->addWidget(viewModeBtn);

    sortBtn_ = new QPushButton(header);
    sortBtn_->setFixedSize(28, 28);
    sortBtn_->setIcon(UiIcons::get(UiIcon::SortOneNine, pal.textSecondary, 16));
    sortBtn_->setIconSize(QSize(16, 16));
    sortBtn_->setCursor(Qt::PointingHandCursor);
    sortBtn_->setStyleSheet(QString("QPushButton { background: transparent; border: none; border-radius: 4px; } QPushButton:hover { background: %1; }").arg(pal.hover.name()));
    updateSortMenu();
    connect(sortBtn_, &QPushButton::clicked, [this]() {
        if (!sortMenu_) updateSortMenu();
        QPoint p = sortBtn_->mapToGlobal(QPoint(sortBtn_->width() - sortMenu_->sizeHint().width(), sortBtn_->height() + 2));
        sortMenu_->popup(p);
    });
    hLayout->addWidget(sortBtn_);

    auto* importBtn = new QPushButton("Nhập", header);
    importBtn->setToolTip("Nhập tệp media");
    importBtn->setIcon(UiIcons::get(UiIcon::CloudUpload, pal.textPrimary, 16));
    importBtn->setIconSize(QSize(16, 16));
    importBtn->setFixedWidth(76);
    importBtn->setFixedHeight(28);
    importBtn->setCursor(Qt::PointingHandCursor);
    importBtn->setStyleSheet(QString(R"(
        QPushButton {
            background-color: transparent;
            color: %1;
            border: 1px solid %2;
            border-radius: 6px;
            font-size: 12px;
            font-weight: 500;
            padding: 0 10px;
        }
        QPushButton:hover {
            background-color: %3;
            border-color: %4;
        }
    )").arg(pal.textPrimary.name()).arg(pal.border.name()).arg(pal.hover.name()).arg(pal.primaryAccent.name()));
    connect(importBtn, &QPushButton::clicked, this, &AssetsPanel::onImportClicked);
    hLayout->addWidget(importBtn);

    layout->addWidget(header);

    // 2. Content container
    auto* contentArea = new QWidget(view);
    auto* cLayout = new QVBoxLayout(contentArea);
    cLayout->setContentsMargins(8, 8, 8, 8);
    cLayout->setSpacing(0);

    // Empty state: MediaDragOverlay (matches web drag-overlay.tsx)
    mediaEmptyOverlay_ = new QWidget(contentArea);
    auto* emptyLayout = new QVBoxLayout(mediaEmptyOverlay_);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->setSpacing(0);

    auto* dropCard = new DropCardWidget(mediaEmptyOverlay_);
    dropCard->setObjectName("mediaDropCard");
    dropCard->setToolTip("Bấm hoặc kéo video, ảnh và tệp âm thanh vào đây để nhập");
    dropCard->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    dropCard->setMinimumHeight(135);
    dropCard->setStyleSheet(QString(R"(
        #mediaDropCard {
            background-color: %1;
            border: none;
            border-radius: 13px;
        }
        #mediaDropCard:hover {
            background-color: %2;
        }
    )").arg(pal.secondary.name()).arg(pal.hover.name()));
    dropCard->onClicked = [this]() { onImportClicked(); };

    auto* cardInner = new QVBoxLayout(dropCard);
    cardInner->setContentsMargins(32, 32, 32, 32);
    cardInner->setSpacing(16);
    cardInner->setAlignment(Qt::AlignCenter);

    auto* cardIcon = new QLabel(dropCard);
    cardIcon->setAlignment(Qt::AlignCenter);
    cardIcon->setFixedHeight(40);
    cardIcon->setPixmap(UiIcons::getPixmap(UiIcon::Upload, pal.textPrimary, 40));
    cardInner->addWidget(cardIcon);

    auto* cardText = new MediaDropCaption("Kéo thả video, ảnh và tệp âm thanh vào đây", dropCard);
    cardText->setAlignment(Qt::AlignCenter);
    cardText->setWordWrap(true);
    cardText->setStyleSheet(QString("color: %1; font-size: 8.64pt; font-weight: 400;").arg(pal.textSecondary.name()));
    cardText->setMinimumHeight(14);
    cardText->ensurePolished();
    dropCard->textLabel = cardText;
    cardInner->addWidget(cardText);

    emptyLayout->addWidget(dropCard, 0, Qt::AlignLeft | Qt::AlignTop);
    emptyLayout->addStretch(1);

    cLayout->addWidget(mediaEmptyOverlay_);

    // Media list widget (Icon grid mode by default)
    mediaListWidget_ = new MediaListWidget(contentArea);
    mediaListWidget_->setObjectName("mediaListWidget");
    mediaListWidget_->setFrameShape(QFrame::NoFrame);
    mediaListWidget_->setViewMode(QListView::IconMode);
    mediaListWidget_->setIconSize(QSize(64, 48));
    mediaListWidget_->setGridSize(QSize(88, 84));
    mediaListWidget_->setResizeMode(QListView::Adjust);
    mediaListWidget_->setMovement(QListView::Static);
    mediaListWidget_->setWordWrap(false);
    mediaListWidget_->setAcceptDrops(false);
    mediaListWidget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mediaListWidget_->setTextElideMode(Qt::ElideRight);
    mediaListWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(mediaListWidget_, &QListWidget::itemDoubleClicked, this, &AssetsPanel::onMediaItemDoubleClicked);
    connect(mediaListWidget_, &QListWidget::customContextMenuRequested, [this](const QPoint& pos) {
        auto* item = mediaListWidget_->itemAt(pos);
        if (!item) return;
        std::string assetIdStr = item->data(Qt::UserRole).toString().toStdString();
        core::MediaId assetId(assetIdStr);
        const auto& pal = Theme::instance().palette();
        auto* menu = new QMenu(this);
        menu->setAttribute(Qt::WA_TranslucentBackground, true);
        menu->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        menu->setStyleSheet(QString(R"(
            QMenu {
                background-color: %1;
                color: %2;
                border: 1px solid %3;
                border-radius: 6px;
                padding: 4px;
            }
            QMenu::item {
                background-color: transparent;
                color: %2;
                padding: 6px 14px;
                min-height: 20px;
                border-radius: 4px;
                margin: 1px 0px;
                font-family: 'Inter', sans-serif;
                font-size: 13px;
            }
            QMenu::item:selected {
                background-color: %4;
                color: %2;
            }
            QMenu::separator {
                height: 1px;
                background-color: %5;
                margin: 4px 0px;
            }
        )")
        .arg(pal.popoverBackground.name())
        .arg(pal.popoverForeground.name())
        .arg(pal.popoverBorder.name())
        .arg(pal.popoverHover.name())
        .arg(pal.popoverSeparator.name()));

        menu->addAction(UiIcons::get(UiIcon::Plus, pal.textPrimary, 14), "Add to Timeline", [this, assetId]() {
            onAddMediaToTimeline(assetId);
        });
        menu->addAction(UiIcons::get(UiIcon::Media, pal.textPrimary, 14), "Add as New Layer", [this, assetId]() {
            onAddMediaAsNewLayer(assetId);
        });
        menu->addSeparator();
        menu->addAction(UiIcons::get(UiIcon::Delete, pal.destructive, 14), "Delete from Project", [this, assetId]() {
            mediaLibrary_.removeAsset(assetId);
            refresh();
        });
        menu->popup(mediaListWidget_->mapToGlobal(pos));
    });
    cLayout->addWidget(mediaListWidget_);

    layout->addWidget(contentArea, 1);

    return view;
}

QWidget* AssetsPanel::createAudioView() {
    auto* view = new QWidget(this);
    auto* layout = new QVBoxLayout(view);
    layout->setContentsMargins(0,8,0,0);
    layout->setSpacing(8);

    auto* search=new QLineEdit(view);search->setObjectName("soundSearch");search->setPlaceholderText("Tìm kiếm…");search->setClearButtonEnabled(true);search->setFixedHeight(28);
    auto* searchRow=new QHBoxLayout();searchRow->setContentsMargins(8,0,8,0);searchRow->addWidget(search);layout->addLayout(searchRow);
    auto* tabs=new QWidget(view);auto* tabLayout=new QHBoxLayout(tabs);tabLayout->setContentsMargins(8,0,8,0);tabLayout->setSpacing(0);
    auto* libraryTab=new QPushButton("Hiệu ứng âm thanh",tabs);auto* savedTab=new QPushButton("Đã lưu",tabs);
    auto* tabGroup=new QButtonGroup(tabs);
    for(auto* b:{libraryTab,savedTab}){b->setCheckable(true);b->setFixedHeight(36);b->setProperty("class","browseTab");tabGroup->addButton(b);tabLayout->addWidget(b);}
    libraryTab->setChecked(true);tabLayout->addStretch();layout->addWidget(tabs);
    auto* notice=new QLabel("Thư viện âm thanh trực tuyến chưa được kết nối.",view);
    notice->setProperty("class","sectionCaption");notice->setWordWrap(true);notice->setContentsMargins(16,8,16,8);layout->addWidget(notice);
    auto* localToggle=new QPushButton("Âm thử tổng hợp trên máy  ⌄",view);localToggle->setCheckable(true);localToggle->setProperty("class","sectionToggle");localToggle->setFixedHeight(28);layout->addWidget(localToggle);

    auto* sfxScroll = new QScrollArea(view);
    sfxScroll->setWidgetResizable(true);
    sfxScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* sfxContainer = new QWidget(sfxScroll);
    auto* sfxLayout = new QVBoxLayout(sfxContainer);
    sfxLayout->setContentsMargins(0, 0, 4, 0);
    sfxLayout->setSpacing(6);

    struct SfxEntry {
        QString name;
        QString category;
        double duration;
    };

    std::vector<SfxEntry> sfxList = {
        {"Whoosh Swish", "Transition", 0.8},
        {"Bubble Pop", "UI", 0.5},
        {"Notification Bell", "Alert", 1.2},
        {"Cinematic Boom", "Impact", 2.5},
        {"Subtle Click", "UI", 0.3},
        {"Ambient City Life", "Ambient", 15.0},
        {"Rain & Thunder", "Ambient", 20.0},
        {"Lo-Fi Chill Beat", "Music", 30.0},
        {"Fast Camera Shutter", "Foley", 0.6},
        {"Laser Swoosh", "Sci-Fi", 1.0}
    };

    const auto& pal = Theme::instance().palette();
    for (const auto& sfx : sfxList) {
        auto* row = new QWidget(sfxContainer);
        row->setProperty("class", "cardWidget");
        row->setProperty("searchName",sfx.name);
        auto* rLayout = new QHBoxLayout(row);
        rLayout->setContentsMargins(8, 6, 8, 6);

        auto* ic = new QLabel(row);
        ic->setPixmap(UiIcons::getPixmap(UiIcon::Audio, pal.primaryAccent, 16));
        ic->setFixedSize(20, 20);
        rLayout->addWidget(ic);

        auto* nameLabel = new QLabel(QString("<b>%1</b><br><span style='color:%2;'>%3 • %4s</span>")
            .arg(sfx.name).arg(pal.textSecondary.name()).arg(sfx.category).arg(sfx.duration, 0, 'f', 1), row);
        nameLabel->setTextFormat(Qt::RichText);
        nameLabel->setWordWrap(true);
        rLayout->addWidget(nameLabel, 1);

        auto* addBtn = new QPushButton("Thêm", row);
        addBtn->setToolTip(QString("Add %1 to timeline").arg(sfx.name));
        addBtn->setIcon(UiIcons::get(UiIcon::Plus, pal.textPrimary, 12));
        addBtn->setIconSize(QSize(12, 12));
        addBtn->setFixedSize(68, 28);
        connect(addBtn, &QPushButton::clicked, [this, sfx]() {
            onAddAudioSfx(sfx.name, sfx.duration);
        });
        rLayout->addWidget(addBtn);

        sfxLayout->addWidget(row);
    }
    sfxLayout->addStretch();
    sfxScroll->setWidget(sfxContainer);
    layout->addWidget(sfxScroll, 1);
    sfxScroll->hide();connect(localToggle,&QPushButton::toggled,sfxScroll,&QWidget::setVisible);
    layout->addStretch();
    connect(search,&QLineEdit::textChanged,sfxContainer,[sfxContainer](const QString& query){for(auto* row:sfxContainer->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly))row->setVisible(row->property("searchName").toString().contains(query,Qt::CaseInsensitive));});
    connect(savedTab,&QPushButton::clicked,view,[notice,sfxScroll,localToggle]{notice->setText("Chưa có âm thanh đã lưu.");sfxScroll->hide();localToggle->hide();});
    connect(libraryTab,&QPushButton::clicked,view,[notice,localToggle,sfxScroll]{notice->setText("Thư viện âm thanh trực tuyến chưa được kết nối.");localToggle->show();sfxScroll->setVisible(localToggle->isChecked());});

    return view;
}

QWidget* AssetsPanel::createTextView() {
    auto* view = new QWidget(this);
    auto* layout = new QVBoxLayout(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(createPanelHeaderWidget(view, "Văn bản"));

    auto* scroll = new QScrollArea(view);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* container = new QWidget(scroll);
    auto* cLayout = new QVBoxLayout(container);
    cLayout->setContentsMargins(12, 12, 12, 12);
    cLayout->setSpacing(10);

    auto* defaultText = new AssetTile("Văn bản mặc định",QPixmap(),112,false,container);
    defaultText->setDragPayload(QJsonDocument(QJsonObject{{"type","text"},{"name","Văn bản mặc định"}}).toJson(QJsonDocument::Compact));
    defaultText->setObjectName("defaultTextTile"); defaultText->setFixedSize(112,112);
    defaultText->setToolTip("Thêm vào dòng thời gian");
    defaultText->setStyleSheet(QString("QPushButton {background:%1;border:none;border-radius:6px;font-size:12px;padding:0;} QPushButton:hover {border:1px solid %2;}").arg(Theme::instance().palette().hover.name(),Theme::instance().palette().primaryAccent.name()));
    connect(defaultText,&QPushButton::clicked,this,[this]{insertUiPreset(engine_,mediaLibrary_,QJsonObject{{"type","text"},{"name","Văn bản mặc định"}},engine_.playback().currentTime());});
    cLayout->setContentsMargins(8,8,8,8); cLayout->setSpacing(8);
    cLayout->addWidget(defaultText,0,Qt::AlignLeft);
    auto* extrasToggle = new QPushButton("Mẫu văn bản và giọng đọc  ⌄",container);
    extrasToggle->setObjectName("textExtrasToggle"); extrasToggle->setCheckable(true); extrasToggle->setFixedHeight(28);
    extrasToggle->setProperty("class","sectionToggle"); cLayout->addWidget(extrasToggle);
    auto* extras = new QWidget(container);
    auto* extrasLayout = new QVBoxLayout(extras); extrasLayout->setContentsMargins(0,0,0,0); extrasLayout->setSpacing(8);
    cLayout->addWidget(extras); extras->hide();
    connect(extrasToggle,&QPushButton::toggled,extras,&QWidget::setVisible);

    // Native-only presets remain accessible in a collapsed extension.
    auto* presetsHeader = new QLabel("MẪU VĂN BẢN", container);
    presetsHeader->setStyleSheet("font-size: 11px; font-weight: 700; color: #38bdf8; text-transform: uppercase; letter-spacing: 0.5px;");
    extrasLayout->addWidget(presetsHeader);

    struct TextPreset {
        QString title;
        QString style;
        QString desc;
    };

    std::vector<TextPreset> presets = {
        {"Heading", "bold", "Large, bold title for scene opening"},
        {"Subtitle", "regular", "Standard readable subtitle in center/bottom"},
        {"Glowing Neon", "neon", "Bright neon outline glow effect"},
        {"Cyberpunk Bold", "cyber", "Modern futuristic font style"},
        {"Lower Third", "lower_third", "Cinematic lower-third title card"},
        {"Cursive Callout", "cursive", "Elegant casual handwriting style"}
    };

    const auto& pal = Theme::instance().palette();
    for (const auto& p : presets) {
        auto* card = new QWidget(container);
        card->setProperty("class", "cardWidget");
        auto* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(10, 8, 10, 8);

        auto* lbl = new QLabel(QString("<b>%1</b><br><span style='color:%2; font-size:11px;'>%3</span>")
            .arg(p.title).arg(pal.textSecondary.name()).arg(p.desc), card);
        lbl->setTextFormat(Qt::RichText);
        lbl->setWordWrap(true);
        lbl->setMinimumWidth(0);
        cardLayout->addWidget(lbl, 1);

        auto* btn = new QPushButton("Thêm", card);
        btn->setToolTip(QString("Add '%1' text preset to timeline").arg(p.title));
        btn->setIcon(UiIcons::get(UiIcon::Plus, pal.textPrimary, 12));
        btn->setIconSize(QSize(12, 12));
        btn->setFixedSize(68, 28);
        connect(btn, &QPushButton::clicked, [this, p]() {
            onAddTextPreset(p.title, p.style);
        });
        cardLayout->addWidget(btn);

        extrasLayout->addWidget(card);
    }

    // Section 2: AI Voiceover (Text-to-Speech)
    auto* ttsHeader = new QLabel("CHUYỂN VĂN BẢN THÀNH GIỌNG NÓI", container);
    ttsHeader->setStyleSheet("font-size: 11px; font-weight: 700; color: #38bdf8; text-transform: uppercase; letter-spacing: 0.5px; padding-top: 8px;");
    extrasLayout->addWidget(ttsHeader);

    auto* ttsCard = new QWidget(container);
    ttsCard->setProperty("class", "cardWidget");
    auto* ttsLayout = new QVBoxLayout(ttsCard);
    ttsLayout->setContentsMargins(10, 10, 10, 10);
    ttsLayout->setSpacing(8);

    auto* ttsInput = new QTextEdit(ttsCard);
    ttsInput->setToolTip("Nội dung giọng đọc");
    ttsInput->setPlaceholderText("Nhập văn bản cần đọc…");
    ttsInput->setPlainText("Chào mừng bạn đến với Catchim Video Editor.");
    ttsInput->setFixedHeight(65);
    ttsLayout->addWidget(ttsInput);

    auto* voiceCombo = new WebComboBox(ttsCard);
    voiceCombo->setObjectName("ttsVoice");
    voiceCombo->setToolTip("Chọn giọng đọc");
    for (const auto& v : audio::TtsEngine::instance().voices()) {
        QString label = QString::fromStdString(v.name + " (" + v.gender + " - " + v.language + ")");
        voiceCombo->addItem(label, QString::fromStdString(v.id));
    }
    ttsLayout->addWidget(voiceCombo);

    auto* rateLayout = new QHBoxLayout();
    auto* rateTitle = new QLabel("Tốc độ đọc:", ttsCard);
    rateTitle->setProperty("class", "SecondaryLabel");
    rateLayout->addWidget(rateTitle);

    auto* rateSlider = new QSlider(Qt::Horizontal, ttsCard);
    rateSlider->setToolTip("Điều chỉnh tốc độ đọc");
    rateSlider->setRange(50, 200);
    rateSlider->setValue(100);
    rateLayout->addWidget(rateSlider, 1);

    auto* rateLabel = new QLabel("1.0x", ttsCard);
    rateLabel->setFixedWidth(30);
    rateLayout->addWidget(rateLabel);
    connect(rateSlider, &QSlider::valueChanged, [rateLabel](int val) {
        rateLabel->setText(QString("%1x").arg(val / 100.0, 0, 'f', 1));
    });
    ttsLayout->addLayout(rateLayout);

    auto* genTtsBtn = new QPushButton("Tạo giọng đọc vào dòng thời gian", ttsCard);
    genTtsBtn->setToolTip("Generate voiceover to timeline");
    genTtsBtn->setIcon(UiIcons::get(UiIcon::Audio, QColor("#ffffff"), 16));
    genTtsBtn->setIconSize(QSize(16, 16));
    genTtsBtn->setFixedHeight(32);
    genTtsBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #0284c7;
            color: #ffffff;
            font-weight: 600;
            border-radius: 6px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #0369a1;
        }
    )");

    // There is no speech synthesis backend in this native build. Do not label
    // generated sine tones as a voiceover or add bogus audio clips.
    genTtsBtn->setEnabled(false);
    genTtsBtn->setToolTip("Chưa kết nối dịch vụ tổng hợp giọng nói trong bản desktop");
    auto* ttsStatus=new QLabel("Chưa kết nối dịch vụ tổng hợp giọng nói.",ttsCard);
    ttsStatus->setWordWrap(true); ttsStatus->setProperty("class","SecondaryLabel"); ttsLayout->addWidget(ttsStatus);
    ttsLayout->addWidget(genTtsBtn);
    extrasLayout->addWidget(ttsCard);

    cLayout->addStretch();
    scroll->setWidget(container);
    layout->addWidget(scroll, 1);

    return view;
}

QWidget* AssetsPanel::createStickersView() {
    auto* view=new QWidget(this);
    auto* root=new QVBoxLayout(view);root->setContentsMargins(0,8,0,0);root->setSpacing(8);
    auto* search=new QLineEdit(view);search->setObjectName("stickerSearch");search->setPlaceholderText("Tìm kiếm…");search->setClearButtonEnabled(true);search->setFixedHeight(28);
    auto* searchRow=new QHBoxLayout();searchRow->setContentsMargins(8,0,8,0);searchRow->addWidget(search);root->addLayout(searchRow);
    auto* tabs=new QWidget(view);auto* tabsLayout=new QHBoxLayout(tabs);tabsLayout->setContentsMargins(8,0,8,0);tabsLayout->setSpacing(0);
    auto* group=new QButtonGroup(tabs);
    for(const auto& title:{"Tất cả","Cờ","Hình dạng"}){auto* b=new QPushButton(QString::fromUtf8(title),tabs);b->setProperty("class","browseTab");b->setCheckable(true);b->setFixedHeight(36);group->addButton(b,group->buttons().size());tabsLayout->addWidget(b);}
    group->button(0)->setChecked(true);tabsLayout->addStretch();root->addWidget(tabs);
    auto* scroll=new QScrollArea(view);scroll->setObjectName("stickerScroll");scroll->setWidgetResizable(true);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);root->addWidget(scroll,1);
    QFile countryFile(":/web/countries.ts");countryFile.open(QIODevice::ReadOnly);auto source=countryFile.readAll();const int start=source.indexOf('[',source.indexOf('=',source.indexOf("export const COUNTRIES")));
    const auto countries=QJsonDocument::fromJson(source.mid(start,source.lastIndexOf(']')-start+1)).array();
    auto rebuild=[this,search,scroll,group,countries] {
        auto* content=new QWidget(scroll);auto* column=new QVBoxLayout(content);column->setContentsMargins(16,8,16,16);column->setSpacing(16);
        const auto query=search->text().trimmed();const int category=group->checkedId();
        auto addHeading=[&](const QString& title,int tab){
            auto* row=new QWidget(content);auto* l=new QHBoxLayout(row);l->setContentsMargins(0,0,0,0);
            auto* label=new QLabel(title,row);label->setProperty("class","sectionCaption");l->addWidget(label);l->addStretch();
            auto* all=new QPushButton("Tất cả",row);all->setProperty("class","sectionToggle");all->setFixedHeight(20);connect(all,&QPushButton::clicked,group->button(tab),&QAbstractButton::click);l->addWidget(all);column->addWidget(row);
        };
        if(category!=2) {
            if(category==0 && query.isEmpty())addHeading("Flags",1);
            auto* flagContainer=new QWidget(content);
            QLayout* flagsLayout=nullptr;
            QScrollArea* horizontal=nullptr;
            if(category==0 && query.isEmpty()) {
                horizontal=new QScrollArea(content);horizontal->setFrameShape(QFrame::NoFrame);horizontal->setFixedHeight(88);horizontal->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);horizontal->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
                auto* row=new QHBoxLayout(flagContainer);row->setContentsMargins(0,0,0,0);row->setSpacing(8);flagsLayout=row;
            } else flagsLayout=new FlowLayout(flagContainer);
            int shown=0;
            for(const auto& entry:countries) {
                const auto country=entry.toObject();const QString name=country["name"].toString(),code=country["code"].toString();
                if(!query.isEmpty() && !name.contains(query,Qt::CaseInsensitive) && !code.contains(query,Qt::CaseInsensitive) && !country["region"].toString().contains(query,Qt::CaseInsensitive))continue;
                QPixmap flag(":/web/flags/"+code.toLower()+".svg");if(flag.isNull())continue;
                QPixmap preview(160,160);preview.fill(QColor("#ededed"));
                {QPainter p(&preview);p.setRenderHint(QPainter::SmoothPixmapTransform);QSize size=flag.size();size.scale(112,112,Qt::KeepAspectRatio);p.drawPixmap(QRect(QPoint((160-size.width())/2,(160-size.height())/2),size),flag);}
                auto* tile=new AssetTile(name,preview,80,false,flagContainer);
                const QJsonObject payload{{"type","flag"},{"name",name},{"code",code}};
                tile->setDragPayload(QJsonDocument(payload).toJson(QJsonDocument::Compact));
                connect(tile,&QPushButton::clicked,this,[this,payload]{insertUiPreset(engine_,mediaLibrary_,payload,engine_.playback().currentTime());refresh();});
                flagsLayout->addWidget(tile);++shown;if(horizontal && shown>=12)break;
            }
            if(horizontal){horizontal->setWidget(flagContainer);column->addWidget(horizontal);}else column->addWidget(flagContainer);
        }
        if(category!=1) {
            if(category==0 && query.isEmpty())addHeading("Shapes",2);
            auto* shapeContainer=new QWidget(content);auto* flow=new FlowLayout(shapeContainer);
            const QStringList names={"Rectangle","Ellipse","Polygon","Star"};
            const QStringList ids={"rectangle","circle","polygon","star"};
            for(int i=0;i<names.size();++i) {
                if(!query.isEmpty() && !names[i].contains(query,Qt::CaseInsensitive) && !QString("hình dạng").contains(query,Qt::CaseInsensitive))continue;
                QPixmap preview(160,160);preview.fill(QColor("#ededed"));
                {QPainter p(&preview);p.setRenderHint(QPainter::Antialiasing);p.setPen(Qt::NoPen);p.setBrush(Qt::white);
                    if(i==0)p.drawRect(QRectF(24,24,112,112));
                    if(i==1)p.drawEllipse(QRectF(24,24,112,112));
                    if(i>=2){QPolygonF shape;int sides=i==2?5:10;for(int j=0;j<sides;++j){double angle=-3.14159265358979323846/2+j*2*3.14159265358979323846/sides;double radius=i==3 && j%2?25:56;shape<<QPointF(80+std::cos(angle)*radius,80+std::sin(angle)*radius);}p.drawPolygon(shape);}
                }
                auto* tile=new AssetTile(names[i],preview,80,false,shapeContainer);
                const QJsonObject payload{{"type","shape"},{"name",names[i]},{"shape",ids[i]}};
                tile->setDragPayload(QJsonDocument(payload).toJson(QJsonDocument::Compact));
                connect(tile,&QPushButton::clicked,this,[this,payload]{insertUiPreset(engine_,mediaLibrary_,payload,engine_.playback().currentTime());});flow->addWidget(tile);
            }
            column->addWidget(shapeContainer);
        }
        column->addStretch();auto* previous=scroll->takeWidget();scroll->setWidget(content);if(previous)previous->deleteLater();
    };
    connect(search,&QLineEdit::textChanged,this,[rebuild]{rebuild();});
    connect(group,&QButtonGroup::idClicked,this,[rebuild]{rebuild();});
    rebuild();return view;
}

QWidget* AssetsPanel::createEffectsView() {
    auto* view = new QWidget(this);
    auto* layout = new QVBoxLayout(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(createPanelHeaderWidget(view, "Hiệu ứng"));
    auto* webPreview=new QWidget(view);auto* webPreviewLayout=new QVBoxLayout(webPreview);webPreviewLayout->setContentsMargins(8,8,8,0);
    auto* blurTile=new AssetTile("Blur",QPixmap(":/web/effects/preview.jpg"),96,true,webPreview);
    blurTile->setObjectName("blurEffectTile");blurTile->setToolTip("Bộ dựng desktop chưa hỗ trợ hiệu ứng Blur");blurTile->setEnabled(false);
    connect(blurTile,&QPushButton::clicked,this,[this]{onApplyEffectPreset("Gaussian Blur");});
    webPreviewLayout->addWidget(blurTile,0,Qt::AlignLeft);layout->addWidget(webPreview);
    auto* extrasToggle=new QPushButton("Hiệu ứng desktop  ⌄",view);extrasToggle->setCheckable(true);extrasToggle->setFixedHeight(28);extrasToggle->setProperty("class","sectionToggle");layout->addWidget(extrasToggle);

    auto* scroll = new QScrollArea(view);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* container = new QWidget(scroll);
    auto* cLayout = new QVBoxLayout(container);
    cLayout->setContentsMargins(12, 12, 12, 12);
    cLayout->setSpacing(8);

    struct EffectItem {
        QString name;
        QString category;
        UiIcon icon;
    };

    std::vector<EffectItem> effects = {
        {"Gaussian Blur", "Filter", UiIcon::Contrast},
        {"Color Grading", "Color", UiIcon::Adjustment},
        {"Vignette", "Lens", UiIcon::Sun},
        {"Monochrome", "Color", UiIcon::Contrast},
        {"Invert Colors", "Stylize", UiIcon::Effects},
        {"Zoom In", "Transform", UiIcon::ZoomIn},
        {"Zoom Out", "Transform", UiIcon::ZoomOut},
        {"Sharpen", "Filter", UiIcon::Sliders}
    };

    const auto& pal = Theme::instance().palette();
    for (const auto& eff : effects) {
        auto* card = new QWidget(container);
        card->setProperty("class", "cardWidget");
        auto* cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(10, 8, 10, 8);

        auto* ic = new QLabel(card);
        ic->setPixmap(UiIcons::getPixmap(eff.icon, pal.primaryAccent, 18));
        ic->setFixedSize(22, 22);
        cardLayout->addWidget(ic);

        auto* lbl = new QLabel(QString("<b>%1</b><br><span style='color:%2; font-size:11px;'>%3</span>")
            .arg(eff.name).arg(pal.textSecondary.name()).arg(eff.category), card);
        lbl->setTextFormat(Qt::RichText);
        lbl->setWordWrap(true);
        lbl->setMinimumWidth(0);
        cardLayout->addWidget(lbl, 1);

        auto* btn = new QPushButton("Áp dụng", card);
        btn->setToolTip(QString("Apply %1 effect to selected clip").arg(eff.name));
        btn->setFixedSize(70, 28);
        connect(btn, &QPushButton::clicked, [this, eff]() {
            onApplyEffectPreset(eff.name);
        });
        cardLayout->addWidget(btn);

        cLayout->addWidget(card);
    }
    cLayout->addStretch();
    scroll->setWidget(container);
    layout->addWidget(scroll, 1);
    scroll->hide();connect(extrasToggle,&QPushButton::toggled,scroll,&QWidget::setVisible);
    layout->addStretch();

    return view;
}

QWidget* AssetsPanel::createTransitionsView() {
    auto* view = new QWidget(this);
    auto* layout = new QVBoxLayout(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* notice=new QLabel("Chế độ chuyển cảnh sắp ra mắt…",view);notice->setProperty("class","sectionCaption");notice->setContentsMargins(16,16,16,8);notice->setWordWrap(true);layout->addWidget(notice);
    auto* extrasToggle=new QPushButton("Công cụ desktop  ⌄",view);extrasToggle->setProperty("class","sectionToggle");extrasToggle->setCheckable(true);extrasToggle->setFixedHeight(28);layout->addWidget(extrasToggle);

    auto* contentWidget = new QWidget(view);
    auto* cMainLayout = new QVBoxLayout(contentWidget);
    cMainLayout->setContentsMargins(12, 12, 12, 12);
    cMainLayout->setSpacing(10);

    auto* durBox = new QGroupBox("Thời lượng chuyển cảnh", contentWidget);
    auto* durLayout = new QHBoxLayout(durBox);
    auto* durLabel = new QLabel("0.5s", durBox);
    durLabel->setStyleSheet("color: #38bdf8; font-weight: 600; min-width: 36px;");
    auto* durSlider = new QSlider(Qt::Horizontal, durBox);
    durSlider->setToolTip("Transition duration");
    durSlider->setRange(2, 20); // 0.2s to 2.0s
    durSlider->setValue(5);
    connect(durSlider, &QSlider::valueChanged, [durLabel](int v) {
        durLabel->setText(QString("%1s").arg(v / 10.0, 0, 'f', 1));
    });
    durLayout->addWidget(durSlider);
    durLayout->addWidget(durLabel);
    cMainLayout->addWidget(durBox);

    auto* scroll = new QScrollArea(contentWidget);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* container = new QWidget(scroll);
    auto* cLayout = new QVBoxLayout(container);
    cLayout->setContentsMargins(0, 0, 4, 0);
    cLayout->setSpacing(6);

    struct TransitionItem {
        QString name;
        QString desc;
        UiIcon icon;
    };

    std::vector<TransitionItem> transList = {
        {"Cross Dissolve", "Smooth cross dissolve between scenes", UiIcon::Transitions},
        {"Fade to Black", "Fade out to black then transition", UiIcon::Contrast},
        {"Fade to White", "Flash fade to white into new scene", UiIcon::Sun},
        {"Slide Left", "Smooth slide transition to the left", UiIcon::StepForward},
        {"Slide Right", "Smooth slide transition to the right", UiIcon::StepBack},
        {"Slide Up", "Slide new scene upwards from bottom", UiIcon::Transitions},
        {"Slide Down", "Slide new scene downwards from top", UiIcon::Transitions},
        {"Zoom In", "Zoom into the next scene", UiIcon::ZoomIn},
        {"Zoom Out", "Zoom out transition into new scene", UiIcon::ZoomOut},
        {"Wipe", "Clean horizontal wipe transition", UiIcon::Sliders}
    };

    const auto& tPal = Theme::instance().palette();
    for (const auto& tr : transList) {
        auto* card = new QWidget(container);
        card->setProperty("class", "cardWidget");
        auto* rowLayout = new QHBoxLayout(card);
        rowLayout->setContentsMargins(8, 8, 8, 8);

        auto* icLabel = new QLabel(card);
        icLabel->setPixmap(UiIcons::getPixmap(tr.icon, tPal.primaryAccent, 20));
        rowLayout->addWidget(icLabel);

        auto* infoLabel = new QLabel(QString("<b>%1</b><br><span style='color:%2; font-size:11px;'>%3</span>")
            .arg(tr.name).arg(tPal.textSecondary.name()).arg(tr.desc), card);
        infoLabel->setTextFormat(Qt::RichText);
        infoLabel->setWordWrap(true);
        rowLayout->addWidget(infoLabel, 1);

        auto* applyBtn = new QPushButton("Áp dụng", card);
        applyBtn->setToolTip(QString("Apply %1 transition").arg(tr.name));
        applyBtn->setFixedSize(68, 28);
        connect(applyBtn, &QPushButton::clicked, [this, tr, durSlider]() {
            double dur = durSlider->value() / 10.0;
            onApplyTransitionPreset(tr.name, dur);
        });
        rowLayout->addWidget(applyBtn);

        cLayout->addWidget(card);
    }
    cLayout->addStretch();
    scroll->setWidget(container);
    cMainLayout->addWidget(scroll, 1);
    layout->addWidget(contentWidget, 1);
    contentWidget->hide();connect(extrasToggle,&QPushButton::toggled,contentWidget,&QWidget::setVisible);
    layout->addStretch();

    return view;
}

QWidget* AssetsPanel::createSubtitlesView() {
    auto* view = new QWidget(this);
    auto* layout = new QVBoxLayout(view); layout->setContentsMargins(0,0,0,0); layout->setSpacing(0);
    auto* importButton = new QPushButton("Nhập",view); importButton->setFixedSize(76,28);
    importButton->setIcon(UiIcons::get(UiIcon::CloudUpload,Theme::instance().palette().textPrimary,16));
    importButton->setToolTip("Nhập tệp phụ đề");
    connect(importButton,&QPushButton::clicked,this,&AssetsPanel::onImportSrtClicked);
    layout->addWidget(createPanelHeaderWidget(view,"Phụ đề",importButton));
    auto* content = new QWidget(view);
    auto* body = new QVBoxLayout(content); body->setContentsMargins(16,12,16,16); body->setSpacing(8);
    body->addWidget(new QLabel("Ngôn ngữ",content));
    auto* language = new WebComboBox(content); language->setObjectName("captionLanguage");
    language->setToolTip("Chọn ngôn ngữ"); language->setFixedHeight(28);
    language->setStyleSheet("QComboBox {min-height:20px;max-height:20px;padding-top:3px;padding-bottom:3px;}");
    language->addItem("Tự động nhận diện","auto");
    const QStringList labels={"Tiếng Anh","Tiếng Tây Ban Nha","Tiếng Ý","Tiếng Pháp","Tiếng Đức","Tiếng Bồ Đào Nha","Tiếng Nga","Tiếng Nhật","Tiếng Việt","Tiếng Trung"};
    int i=0;
    for (const auto& lang:subtitles::TranscriptionLanguagesRegistry::getAllLanguages()) {
        language->addItem(i<labels.size()?labels[i]:QString::fromStdString(lang.name),QString::fromStdString(lang.code)); ++i;
    }
    body->addWidget(language); body->addStretch();
    auto* status = new QLabel(content); status->setObjectName("captionStatus"); status->setWordWrap(true); status->hide();
    status->setStyleSheet("color:#b45309;font-size:12px;"); body->addWidget(status);
    auto* generate = new QPushButton("Tạo bản chép lời",content); generate->setObjectName("generateCaptions");
    generate->setFixedHeight(32);
    generate->setStyleSheet(QString("QPushButton {background:%1;color:white;border:none;border-radius:10px;} QPushButton:hover {background:#009bdc;}").arg(Theme::instance().palette().primaryAccent.name()));
    connect(generate,&QPushButton::clicked,this,[this,status] {
        const auto* timeline=engine_.activeTimeline();
        status->setText(!timeline || timeline->totalDuration().toSeconds()<=0
            ? "Chưa có âm thanh trên dòng thời gian để chép lời."
            : "Bản desktop chưa kết nối mô hình nhận dạng giọng nói. Bạn có thể nhập tệp phụ đề đã có bằng nút Nhập.");
        status->show();
    });
    body->addWidget(generate); layout->addWidget(content,1);
    return view;
}

QWidget* AssetsPanel::createAdjustmentView() {
    auto* view = new QWidget(this);
    auto* layout = new QVBoxLayout(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* notice=new QLabel("Chế độ điều chỉnh sắp ra mắt…",view);notice->setProperty("class","sectionCaption");notice->setContentsMargins(16,16,16,8);notice->setWordWrap(true);layout->addWidget(notice);
    auto* extrasToggle=new QPushButton("Công cụ desktop  ⌄",view);extrasToggle->setProperty("class","sectionToggle");extrasToggle->setCheckable(true);extrasToggle->setFixedHeight(28);layout->addWidget(extrasToggle);

    auto* scroll = new QScrollArea(view);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* container = new QWidget(scroll);
    auto* cLayout = new QVBoxLayout(container);
    cLayout->setContentsMargins(12, 12, 12, 12);
    cLayout->setSpacing(8);

    auto makeSliderRow = [container](const QString& title, int min, int max, int def) -> std::pair<QSlider*, QLabel*> {
        auto* box = new QWidget(container);
        box->setProperty("class", "cardWidget");
        auto* bLayout = new QVBoxLayout(box);
        bLayout->setContentsMargins(8, 6, 8, 6);

        auto* topRow = new QHBoxLayout();
        auto* lbl = new QLabel(title, box);
        lbl->setStyleSheet("font-size: 12px; font-weight: 500;");
        auto* valLbl = new QLabel(QString::number(def), box);
        valLbl->setStyleSheet("color: #38bdf8; font-weight: 600; font-size: 12px;");
        topRow->addWidget(lbl);
        topRow->addStretch();
        topRow->addWidget(valLbl);
        bLayout->addLayout(topRow);

        auto* slider = new QSlider(Qt::Horizontal, box);
        slider->setToolTip(title + " adjustment");
        slider->setRange(min, max);
        slider->setValue(def);
        QObject::connect(slider, &QSlider::valueChanged, [valLbl](int v) {
            valLbl->setText(QString::number(v));
        });
        bLayout->addWidget(slider);
        return {slider, valLbl};
    };

    auto [brightSlider, brightLbl] = makeSliderRow("Độ sáng", -100, 100, 0);
    auto [contrastSlider, contrastLbl] = makeSliderRow("Độ tương phản", -100, 100, 0);
    auto [satSlider, satLbl] = makeSliderRow("Độ bão hòa", -100, 100, 0);
    auto [tempSlider, tempLbl] = makeSliderRow("Nhiệt độ màu", -100, 100, 0);
    auto [tintSlider, tintLbl] = makeSliderRow("Sắc độ", -100, 100, 0);

    cLayout->addWidget(brightSlider->parentWidget());
    cLayout->addWidget(contrastSlider->parentWidget());
    cLayout->addWidget(satSlider->parentWidget());
    cLayout->addWidget(tempSlider->parentWidget());
    cLayout->addWidget(tintSlider->parentWidget());

    auto* applyBtn = new QPushButton("Áp dụng cho clip đã chọn", container);
    applyBtn->setToolTip("Apply color adjustment to selected clip");
    applyBtn->setFixedHeight(36);
    applyBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #0284c7;
            color: #ffffff;
            font-weight: 600;
            border-radius: 6px;
        }
        QPushButton:hover {
            background-color: #0369a1;
        }
    )");
    connect(applyBtn, &QPushButton::clicked, [this, brightSlider, contrastSlider, satSlider, tempSlider, tintSlider]() {
        onApplyAdjustment(
            brightSlider->value(),
            contrastSlider->value(),
            satSlider->value(),
            tempSlider->value(),
            tintSlider->value()
        );
    });
    cLayout->addWidget(applyBtn);

    auto* resetBtn = new QPushButton("Đặt lại mặc định", container);
    resetBtn->setToolTip("Reset adjustments to default");
    resetBtn->setFixedHeight(30);
    connect(resetBtn, &QPushButton::clicked, [=]() {
        brightSlider->setValue(0);
        contrastSlider->setValue(0);
        satSlider->setValue(0);
        tempSlider->setValue(0);
        tintSlider->setValue(0);
    });
    cLayout->addWidget(resetBtn);

    auto* addLayerBtn = new QPushButton("Thêm lớp điều chỉnh", container);
    addLayerBtn->setToolTip("Add dedicated adjustment layer to timeline");
    addLayerBtn->setIcon(UiIcons::get(UiIcon::Plus, QColor("#38bdf8"), 16));
    addLayerBtn->setIconSize(QSize(16, 16));
    addLayerBtn->setFixedHeight(34);
    connect(addLayerBtn, &QPushButton::clicked, this, &AssetsPanel::onAddAdjustmentLayer);
    cLayout->addWidget(addLayerBtn);

    cLayout->addStretch();
    scroll->setWidget(container);
    layout->addWidget(scroll, 1);
    scroll->hide();connect(extrasToggle,&QPushButton::toggled,scroll,&QWidget::setVisible);
    layout->addStretch();

    return view;
}

QWidget* AssetsPanel::createSettingsView() {
    auto* view = new QWidget(this);
    auto* layout = new QVBoxLayout(view);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* tabs = new QWidget(view);
    tabs->setObjectName("settingsTabs");
    tabs->setFixedHeight(44);
    auto* tabsLayout = new QHBoxLayout(tabs);
    tabsLayout->setContentsMargins(18,0,8,0);
    tabsLayout->setSpacing(4);
    auto* infoTab = new QPushButton("Thông tin dự án", tabs);
    auto* backgroundTab = new QPushButton("Nền", tabs);
    auto* tabGroup = new QButtonGroup(tabs);
    for (auto* tab : {infoTab,backgroundTab}) {
        tab->setProperty("class","underlineTab");
        tab->setCheckable(true); tab->setFixedHeight(26);
        tabGroup->addButton(tab); tabsLayout->addWidget(tab);
    }
    infoTab->setChecked(true);
    tabsLayout->addStretch();
    layout->addWidget(tabs);

    auto* scroll = new QScrollArea(view);
    scroll->setObjectName("settingsScroll");
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* container = new QWidget(scroll);
    auto* cLayout = new QVBoxLayout(container);
    cLayout->setContentsMargins(0,0,0,0);
    cLayout->setSpacing(0);
    auto* pages = new QStackedWidget(container);
    auto* infoPage = new QWidget(pages);
    auto* infoLayout = new QVBoxLayout(infoPage);
    infoLayout->setContentsMargins(0,0,0,0); infoLayout->setSpacing(0);
    auto addRow = [infoPage,infoLayout](const QString& title,QWidget* control) {
        auto* row = new QWidget(infoPage); row->setProperty("class","webSectionRow");
        row->setFixedHeight(44);
        auto* l = new QHBoxLayout(row); l->setContentsMargins(14,0,14,0);
        auto* label = new QLabel(title,row); label->setProperty("class","sectionCaption");
        l->addWidget(label); l->addStretch(); l->addWidget(control);
        infoLayout->addWidget(row);
    };
    auto* nameValLabel = new QLabel(infoPage);
    nameValLabel->setObjectName("settingsProjectName");
    nameValLabel->setMinimumWidth(0);
    nameValLabel->setFixedWidth(150);
    nameValLabel->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    nameValLabel->setMaximumWidth(220);
    nameValLabel->setText(engine_.project().name()=="New project" ? "Dự án mới" : QString::fromStdString(engine_.project().name()));
    addRow("Tên",nameValLabel);

    auto* language = new WebComboBox(infoPage);
    language->addItem("Tiếng Việt","vi");
    language->addItem("English","en");
    language->setCurrentIndex(editorEnglish()?1:0);
    connect(language,&QComboBox::currentIndexChanged,this,[this,language](int){setEditorLanguage(language->currentData().toString());engine_.notifyProjectChanged();});
    language->setObjectName("settingsLanguage");
    language->setFixedWidth(106);
    language->setProperty("class","inlineSelect");
    addRow("Ngôn ngữ",language);

    auto* fpsCombo = new WebComboBox(infoPage);
    fpsCombo->setObjectName("settingsFps");
    fpsCombo->setFixedWidth(82);
    fpsCombo->setProperty("class","inlineSelect");
    for (int fps : {24,25,30,60,120}) fpsCombo->addItem(QString("%1 fps").arg(fps),fps);
    int curFps = static_cast<int>(std::round(engine_.project().settings().fps.toFps()));
    int fpsIdx = fpsCombo->findData(curFps);
    if (fpsIdx >= 0) fpsCombo->setCurrentIndex(fpsIdx);
    connect(fpsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, fpsCombo](int idx) {
        int fpsVal = fpsCombo->itemData(idx).toInt();
        engine_.project().settings().fps = core::FrameRate{fpsVal, 1};
        engine_.project().setDirty(true);
        engine_.notifyProjectChanged();
    });
    addRow("Tốc độ khung hình",fpsCombo);

    // 2. Aspect Ratio Presets
    auto* aspectGroup = new QWidget(infoPage);
    aspectGroup->setProperty("class","webSectionRow");
    auto* aLayout = new QVBoxLayout(aspectGroup);
    aLayout->setContentsMargins(0,0,0,8); aLayout->setSpacing(4);
    auto* ratioHeader = new SectionToggle("Tỷ lệ khung hình",aspectGroup);
    ratioHeader->setProperty("class","sectionToggle"); ratioHeader->setFixedHeight(44);
    ratioHeader->setCheckable(true); ratioHeader->setChecked(true);
    aLayout->addWidget(ratioHeader);
    auto* ratioContent = new QWidget(aspectGroup);
    auto* ratioLayout = new QVBoxLayout(ratioContent);
    ratioLayout->setContentsMargins(8,0,8,0); ratioLayout->setSpacing(4);
    aLayout->addWidget(ratioContent);
    connect(ratioHeader,&QPushButton::toggled,ratioContent,&QWidget::setVisible);
    auto* ratioButtons = new QButtonGroup(aspectGroup);

    struct RatioPreset {
        QString label;
        int width;
        int height;
    };

    std::vector<RatioPreset> ratios = {
        {"16:9", 1920, 1080},
        {"9:16", 1080, 1920},
        {"1:1", 1080, 1080},
        {"4:3", 1440, 1080}
    };

    int curW = engine_.project().settings().canvasSize.width;
    int curH = engine_.project().settings().canvasSize.height;

    for (const auto& r : ratios) {
        auto* btn = new CanvasPresetButton(r.label,r.width,r.height,ratioContent);
        btn->setProperty("class","ratioPreset");
        btn->setProperty("canvasWidth",r.width); btn->setProperty("canvasHeight",r.height);
        QPixmap preview(24,24); preview.fill(Qt::transparent);
        { QPainter p(&preview); p.setRenderHint(QPainter::Antialiasing); p.setPen(QPen(Theme::instance().palette().textSecondary,1.2));
          QSizeF s(r.width,r.height); s.scale(20,20,Qt::KeepAspectRatio); p.drawRoundedRect(QRectF((24-s.width())/2,(24-s.height())/2,s.width(),s.height()),2,2); }
        btn->setIcon(QIcon(preview)); btn->setIconSize(QSize(24,24));
        btn->setFixedHeight(34); ratioButtons->addButton(btn);
        btn->setCheckable(true);
        btn->setChecked(curW == r.width && curH == r.height);
        connect(btn, &QPushButton::clicked, [this, r]() {
            engine_.project().settings().canvasSizeMode = "preset";
            onCanvasAspectChanged(r.width, r.height);
            refresh();
        });
        ratioLayout->addWidget(btn);
    }
    auto* custom = new CanvasPresetButton("Tùy chỉnh",0,0,ratioContent);
    ratioButtons->addButton(custom);
    custom->setObjectName("customCanvasButton"); custom->setProperty("class","ratioPreset"); custom->setFixedHeight(34);
    ratioLayout->addWidget(custom);
    connect(custom,&QPushButton::clicked,this,[this]{
        AppDialog dialog("Kích thước khung vẽ tùy chỉnh",this); dialog.setFixedWidth(420);
        auto* l=dialog.bodyLayout();
        auto* form=new QFormLayout();
        auto* w=new QSpinBox(&dialog); auto* h=new QSpinBox(&dialog);
        const auto size=engine_.project().settings().lastCustomCanvasSize.value_or(engine_.project().settings().canvasSize);
        w->setRange(2,16384); h->setRange(2,16384); w->setValue(size.width); h->setValue(size.height);
        w->setButtonSymbols(QAbstractSpinBox::NoButtons);h->setButtonSymbols(QAbstractSpinBox::NoButtons);
        w->setFixedHeight(32);h->setFixedHeight(32);
        form->addRow("Chiều rộng khung vẽ",w); form->addRow("Chiều cao khung vẽ",h); l->addLayout(form);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Cancel|QDialogButtonBox::Apply,&dialog);
        buttons->button(QDialogButtonBox::Cancel)->setText("Hủy"); buttons->button(QDialogButtonBox::Apply)->setText("Áp dụng");
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,&QDialog::accept); l->addWidget(buttons);
        if (dialog.exec()==QDialog::Accepted) {
            engine_.project().settings().canvasSizeMode="custom";
            engine_.project().settings().lastCustomCanvasSize=editor::CanvasSize{w->value(),h->value()};
            onCanvasAspectChanged(w->value(),h->value()); refresh();
        }
    });
    infoLayout->addWidget(aspectGroup); infoLayout->addStretch();
    pages->addWidget(infoPage);

    // Background sections mirror settings/background.tsx; unsupported renderers
    // are explicit rather than accepting values the preview/export cannot render.
    auto* backgroundPage = new QWidget(pages);
    auto* backgroundLayout = new QVBoxLayout(backgroundPage);
    backgroundLayout->setContentsMargins(0,0,0,0);backgroundLayout->setSpacing(0);
    auto addSection=[&](const QString& name,QWidget* body,bool open) {
        auto* section=new QWidget(backgroundPage);section->setProperty("class","webSectionRow");
        auto* l=new QVBoxLayout(section);l->setContentsMargins(0,0,0,0);l->setSpacing(0);
        auto* header=new SectionToggle(name,section);header->setProperty("class","sectionToggle");header->setChecked(open);
        l->addWidget(header);l->addWidget(body);body->setVisible(open);connect(header,&QPushButton::toggled,body,&QWidget::setVisible);backgroundLayout->addWidget(section);
    };
    auto* blurBody=new QWidget(backgroundPage);auto* blurColumn=new QVBoxLayout(blurBody);blurColumn->setContentsMargins(16,0,16,16);
    auto* blurTiles=new QWidget(blurBody);auto* blurLayout=new FlowLayout(blurTiles);blurColumn->addWidget(blurTiles);
    const QPixmap source(":/web/effects/preview.jpg");
    for(const auto& title:{"Nhẹ","Trung bình","Mạnh"}) {
        auto* tile=new AssetTile(QString::fromUtf8(title),source,80,true,blurTiles);
        tile->setEnabled(false);tile->setToolTip("Bộ dựng desktop chưa hỗ trợ nền làm mờ. Hãy chọn nền màu.");
        blurLayout->addWidget(tile);
    }
    addSection("Làm mờ",blurBody,true);
    auto* colorsBody=new QWidget(backgroundPage);auto* colorsColumn=new QVBoxLayout(colorsBody);colorsColumn->setContentsMargins(16,0,16,16);
    auto* swatches=new QWidget(colorsBody);auto* colorsLayout=new FlowLayout(swatches);
    auto* pick=new QPushButton("＋",swatches);pick->setFixedSize(80,80);pick->setToolTip("Chọn màu nền tùy chỉnh");
    pick->setStyleSheet("QPushButton {background:qconicalgradient(cx:0.5,cy:0.5,angle:180,stop:0 #ff5e5e,stop:0.2 #fff26b,stop:0.4 #6bff8f,stop:0.6 #5ee7ff,stop:0.8 #d76bff,stop:1 #ff5e5e);font-size:24px;color:white;border:1px solid #bfc0c1;border-radius:6px;}");
    connect(pick,&QPushButton::clicked,this,[this]{QColor col=chooseEditorColor(QColor(QString::fromStdString(engine_.project().settings().background.color)),this,"Chọn màu nền tùy chỉnh");if(col.isValid())onCanvasBgColorChanged(col);});colorsLayout->addWidget(pick);
    QFile colorsFile(":/web/colors.ts");colorsFile.open(QIODevice::ReadOnly);
    const QString colorsSource=QString::fromUtf8(colorsFile.readAll());
    auto matches=QRegularExpression("#[a-fA-F0-9]{6}").globalMatch(colorsSource);
    while(matches.hasNext()) {
        const QString hex=matches.next().captured();auto* color=new QPushButton(swatches);color->setFixedSize(80,80);color->setToolTip("Chọn nền "+hex);
        color->setProperty("backgroundColor",hex);color->setCheckable(true);
        color->setStyleSheet(QString("QPushButton {background:%1;border:1px solid #bfc0c1;border-radius:6px;} QPushButton:hover,QPushButton:checked {border:2px solid #17aaf3;}").arg(hex));
        connect(color,&QPushButton::clicked,this,[this,hex]{onCanvasBgColorChanged(QColor(hex));refresh();});colorsLayout->addWidget(color);
    }
    colorsColumn->addWidget(swatches);addSection("Màu sắc",colorsBody,false);
    for(const auto& title:{"Họa tiết","Syntax UI"}) {
        auto* message=new QLabel("Bộ dựng nền này chưa được hỗ trợ trong bản desktop.",backgroundPage);
        message->setWordWrap(true);message->setContentsMargins(16,0,16,16);message->setProperty("class","sectionCaption");
        addSection(QString::fromUtf8(title),message,false);
    }
    backgroundLayout->addStretch();pages->addWidget(backgroundPage);
    connect(infoTab,&QPushButton::clicked,pages,[pages]{pages->setCurrentIndex(0);});
    connect(backgroundTab,&QPushButton::clicked,pages,[pages]{pages->setCurrentIndex(1);});
    cLayout->addWidget(pages);

    scroll->setWidget(container);
    layout->addWidget(scroll, 1);

    return view;
}

void AssetsPanel::revealMedia(const core::MediaId& mediaId) {
    tabList_->setCurrentRow(0);
    refresh();
    for(int i=0;i<mediaListWidget_->count();++i) {
        auto* item=mediaListWidget_->item(i);
        if(item->data(Qt::UserRole).toString()==QString::fromStdString(mediaId.str())) {
            mediaListWidget_->setCurrentItem(item);
            mediaListWidget_->scrollToItem(item);
            return;
        }
    }
}

void AssetsPanel::refresh() {
    const auto& pal = Theme::instance().palette();
    int curRow = tabList_->currentRow();
    if (auto* name=findChild<QLabel*>("settingsProjectName"))
        name->setText(engine_.project().name()=="New project" ? "Dự án mới" : QString::fromStdString(engine_.project().name()));
    if (auto* fps=findChild<QComboBox*>("settingsFps")) {
        const QSignalBlocker blocker(fps);
        fps->setCurrentIndex(fps->findData(qRound(engine_.project().settings().fps.toFps())));
    }
    if (auto* custom=findChild<QPushButton*>("customCanvasButton")) custom->setChecked(engine_.project().settings().canvasSizeMode=="custom");
    for (auto* button:findChildren<QPushButton*>()) {
        if(button->property("backgroundColor").isValid())button->setChecked(button->property("backgroundColor").toString()==QString::fromStdString(engine_.project().settings().background.color));
        if (!button->property("canvasWidth").isValid()) continue;
        const auto& settings=engine_.project().settings();
        button->setChecked(settings.canvasSizeMode=="preset" && button->property("canvasWidth").toInt()==settings.canvasSize.width && button->property("canvasHeight").toInt()==settings.canvasSize.height);
    }
    for (int i = 0; i < tabList_->count(); ++i) {
        auto* it = tabList_->item(i);
        if (it && i < static_cast<int>(kTabDefs.size())) {
            QColor col = (i == curRow) ? pal.primaryAccent : pal.textSecondary;
            it->setIcon(UiIcons::get(kTabDefs[i].second, col, 20));
        }
    }

    bool hasAssets = !mediaLibrary_.assets().empty();
    if (mediaEmptyOverlay_) {
        mediaEmptyOverlay_->setVisible(!hasAssets);
    }
    if (mediaListWidget_) {
        mediaListWidget_->setVisible(hasAssets);
    }

    applySort();
}

void AssetsPanel::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void AssetsPanel::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void AssetsPanel::dropEvent(QDropEvent* event) {
    const QMimeData* mime = event->mimeData();
    if (!mime || !mime->hasUrls()) {
        event->ignore();
        return;
    }

    bool addedAny = false;
    for (const QUrl& url : mime->urls()) {
        if (!url.isLocalFile()) continue;
        QString localPath = url.toLocalFile();
        if (localPath.isEmpty()) continue;

        auto probeRes = media::MediaProbe::probe(localPath.toStdString());
        if (probeRes.ok()) {
            auto asset = probeRes.unwrap();
            mediaLibrary_.addAsset(asset);
            addedAny = true;

            // Auto add to main track if project is empty
            auto* tl = engine_.activeTimeline();
            if (tl && tl->mainTrack().clips().empty()) {
                editor::Clip clip(
                    core::ClipId::generate(),
                    (asset->type() == media::MediaType::Video) ? editor::ClipType::Video :
                    (asset->type() == media::MediaType::Audio) ? editor::ClipType::Audio : editor::ClipType::Image,
                    asset->fileName(),
                    core::TimelineTime::fromSeconds(0),
                    asset->duration()
                );
                initializeImportedClip(clip,*asset);
                engine_.insertElement(std::move(clip), core::TimelineTime::fromSeconds(0));
            }
        }
    }

    if (addedAny) {
        refresh();
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void AssetsPanel::onImportClicked() {
    QString filter = "All Media Files (*.mp4 *.mov *.mkv *.avi *.mp3 *.wav *.png *.jpg *.jpeg *.webp *.m4v *.wmv *.flv);;Video (*.mp4 *.mov *.mkv *.avi *.m4v *.wmv);;Audio (*.mp3 *.wav *.aac *.m4a *.flac *.ogg);;Images (*.png *.jpg *.jpeg *.webp *.bmp);;All Files (*.*)";
#if defined(__EMSCRIPTEN__)
    QPointer<AssetsPanel> target(this);
    QFileDialog::getOpenFileContent(filter, [target](const QString& fileName, const QByteArray& content) {
        if (!target || fileName.isEmpty() || content.isEmpty()) return;

        const QString mediaDir = QDir::homePath() + QStringLiteral("/.catchim/media");
        if (!QDir().mkpath(mediaDir)) return;
        const QString safeName = QFileInfo(fileName).fileName();
        const QString path = mediaDir + QLatin1Char('/') + QUuid::createUuid().toString(QUuid::WithoutBraces)
            + QLatin1Char('_') + safeName;
        QFile storedFile(path);
        if (!storedFile.open(QIODevice::WriteOnly) || storedFile.write(content) != content.size()) return;
        storedFile.close();
        EM_ASM({ if (Module.catchimPersistNow) Module.catchimPersistNow(); });

        auto probeRes = media::MediaProbe::probe(path.toStdString());
        if (!probeRes.ok()) return;
        auto asset = probeRes.unwrap();
        target->mediaLibrary_.addAsset(asset);

        auto* tl = target->engine_.activeTimeline();
        if (tl && tl->mainTrack().clips().empty()) {
            editor::Clip clip(
                core::ClipId::generate(),
                (asset->type() == media::MediaType::Video) ? editor::ClipType::Video :
                (asset->type() == media::MediaType::Audio) ? editor::ClipType::Audio : editor::ClipType::Image,
                asset->fileName(),
                core::TimelineTime::fromSeconds(0),
                asset->duration()
            );
            initializeImportedClip(clip, *asset);
            target->engine_.insertElement(std::move(clip), core::TimelineTime::fromSeconds(0));
        }
        target->refresh();
    });
#else
    QStringList paths = QFileDialog::getOpenFileNames(this, "Import Media Files", "", filter);
    for (const auto& path : paths) {
        if (path.isEmpty()) continue;
        auto probeRes = media::MediaProbe::probe(path.toStdString());
        if (probeRes.ok()) {
            auto asset = probeRes.unwrap();
            mediaLibrary_.addAsset(asset);

            // Auto add to main track if project is empty
            auto* tl = engine_.activeTimeline();
            if (tl && tl->mainTrack().clips().empty()) {
                editor::Clip clip(
                    core::ClipId::generate(),
                    (asset->type() == media::MediaType::Video) ? editor::ClipType::Video :
                    (asset->type() == media::MediaType::Audio) ? editor::ClipType::Audio : editor::ClipType::Image,
                    asset->fileName(),
                    core::TimelineTime::fromSeconds(0),
                    asset->duration()
                );
                initializeImportedClip(clip,*asset);
                engine_.insertElement(std::move(clip), core::TimelineTime::fromSeconds(0));
            }
        }
    }
    refresh();
#endif
}

void AssetsPanel::onMediaItemDoubleClicked(QListWidgetItem* item) {
    if (!item) return;
    std::string assetIdStr = item->data(Qt::UserRole).toString().toStdString();
    onAddMediaToTimeline(core::MediaId(assetIdStr));
}

void AssetsPanel::onAddMediaToTimeline(const core::MediaId& id) {
    auto asset = mediaLibrary_.findAsset(id);
    if (!asset) return;

    core::TimelineTime insertTime = engine_.playback().currentTime();
    editor::Clip clip(
        core::ClipId::generate(),
        (asset->type() == media::MediaType::Video) ? editor::ClipType::Video :
        (asset->type() == media::MediaType::Audio) ? editor::ClipType::Audio : editor::ClipType::Image,
        asset->fileName(),
        insertTime,
        asset->duration()
    );
    initializeImportedClip(clip,*asset);
    if (asset->width() > 0 && asset->height() > 0) {
        clip.setParam("sourceWidth", asset->width());
        clip.setParam("sourceHeight", asset->height());
        clip.setParam("width", asset->width());
        clip.setParam("height", asset->height());
    }
    if (asset->fps().toFps() > 0.0) {
        clip.setParam("fps", asset->fps().toFps());
    }

    engine_.insertElement(std::move(clip), insertTime);
    engine_.project().setDirty(true);
    engine_.notifyTimelineChanged();
}

void AssetsPanel::onAddMediaAsNewLayer(const core::MediaId& id) {
    auto asset = mediaLibrary_.findAsset(id);
    if (!asset) return;

    auto* tl = engine_.activeTimeline();
    if (!tl) return;

    core::TimelineTime insertTime = engine_.playback().currentTime();
    editor::Clip clip(
        core::ClipId::generate(),
        (asset->type() == media::MediaType::Video) ? editor::ClipType::Video :
        (asset->type() == media::MediaType::Audio) ? editor::ClipType::Audio : editor::ClipType::Image,
        asset->fileName(),
        insertTime,
        asset->duration()
    );
    initializeImportedClip(clip,*asset);
    if (asset->width() > 0 && asset->height() > 0) {
        clip.setParam("sourceWidth", asset->width());
        clip.setParam("sourceHeight", asset->height());
        clip.setParam("width", asset->width());
        clip.setParam("height", asset->height());
    }
    if (asset->fps().toFps() > 0.0) {
        clip.setParam("fps", asset->fps().toFps());
    }

    engine_.insertElement(std::move(clip),insertTime,std::nullopt,true);
    engine_.project().setDirty(true);
    engine_.notifyTimelineChanged();
}

static core::MediaId createSfxAsset(media::MediaLibrary& mediaLibrary, const QString& name, double durationSec) {
    constexpr uint32_t sampleRate = 44100;
    size_t numSamples = static_cast<size_t>(std::max(0.1, durationSec) * sampleRate);
    std::vector<float> samples(numSamples, 0.0f);
    constexpr double kPi = 3.14159265358979323846;

    if (name.contains("Whoosh", Qt::CaseInsensitive)) {
        // Filtered noise sweep
        uint32_t rng = 12345;
        for (size_t i = 0; i < numSamples; ++i) {
            double t = static_cast<double>(i) / numSamples;
            double env = std::sin(kPi * t);
            env = env * env;
            rng = rng * 1664525u + 1013904223u;
            double white = (static_cast<double>(rng & 0x7FFF) / 16384.0) - 1.0;
            double centerFreq = 300.0 + 1200.0 * std::sin(kPi * t);
            double tone = std::sin(2.0 * kPi * centerFreq * (static_cast<double>(i) / sampleRate));
            samples[i] = static_cast<float>((white * 0.4 + tone * 0.4) * env);
        }
    } else if (name.contains("Bubble", Qt::CaseInsensitive)) {
        // Rising pitch pop
        for (size_t i = 0; i < numSamples; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double env = std::exp(-18.0 * t);
            double freq = 250.0 + 600.0 * (static_cast<double>(i) / numSamples);
            samples[i] = static_cast<float>(std::sin(2.0 * kPi * freq * t) * env * 0.8);
        }
    } else if (name.contains("Bell", Qt::CaseInsensitive)) {
        // Two-tone bell chime
        for (size_t i = 0; i < numSamples; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double env = std::exp(-3.5 * t);
            double s1 = std::sin(2.0 * kPi * 880.0 * t);
            double s2 = 0.5 * std::sin(2.0 * kPi * 1760.0 * t);
            samples[i] = static_cast<float>((s1 + s2) * env * 0.7);
        }
    } else if (name.contains("Boom", Qt::CaseInsensitive)) {
        // Sub-bass impact
        for (size_t i = 0; i < numSamples; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double env = std::exp(-2.2 * t);
            double freq = 120.0 * std::exp(-3.0 * t) + 30.0;
            samples[i] = static_cast<float>(std::sin(2.0 * kPi * freq * t) * env * 0.9);
        }
    } else if (name.contains("Click", Qt::CaseInsensitive)) {
        // Crisp click
        for (size_t i = 0; i < numSamples; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double env = std::exp(-120.0 * t);
            samples[i] = static_cast<float>(std::sin(2.0 * kPi * 1800.0 * t) * env * 0.8);
        }
    } else if (name.contains("Laser", Qt::CaseInsensitive)) {
        // Downward laser chirp
        for (size_t i = 0; i < numSamples; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double env = std::exp(-4.0 * t);
            double freq = 1800.0 * std::exp(-5.0 * t) + 150.0;
            samples[i] = static_cast<float>(std::sin(2.0 * kPi * freq * t) * env * 0.8);
        }
    } else {
        // Ambient / Melody / Shutter default
        for (size_t i = 0; i < numSamples; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double env = std::min(1.0, std::min(t / 0.1, (durationSec - t) / 0.1));
            double s = std::sin(2.0 * kPi * 440.0 * t) * 0.4 +
                       std::sin(2.0 * kPi * 554.37 * t) * 0.3 +
                       std::sin(2.0 * kPi * 659.25 * t) * 0.3;
            samples[i] = static_cast<float>(s * env * 0.6);
        }
    }

    auto wavBytes = audio::TtsServiceEngine::encodePcmToWav(samples, sampleRate);
    QString cacheDir = QDir::tempPath() + "/catchim_audio_cache";
    QDir().mkpath(cacheDir);
    QString safeName = name;
    safeName.replace(" ", "_");
    QString wavFilePath = QString("%1/sfx_%2_%3.wav").arg(cacheDir).arg(safeName).arg(QDateTime::currentMSecsSinceEpoch());

    std::ofstream wavOut(wavFilePath.toStdString(), std::ios::binary);
    if (wavOut.is_open()) {
        wavOut.write(reinterpret_cast<const char*>(wavBytes.data()), static_cast<std::streamsize>(wavBytes.size()));
        wavOut.close();
    }

    auto probeRes = media::MediaProbe::probe(wavFilePath.toStdString());
    if (probeRes.ok()) {
        auto asset = probeRes.unwrap();
        mediaLibrary.addAsset(asset);
        return asset->id();
    }
    return core::MediaId();
}

void AssetsPanel::onAddAudioSfx(const QString& name, double durationSec) {
    core::TimelineTime insertTime = engine_.playback().currentTime();
    editor::Clip clip(
        core::ClipId::generate(),
        editor::ClipType::Audio,
        name.toStdString(),
        insertTime,
        core::TimelineTime::fromSeconds(durationSec)
    );

    core::MediaId assetId = createSfxAsset(mediaLibrary_, name, durationSec);
    if (!assetId.isEmpty()) {
        clip.setMediaId(assetId);
    }

    engine_.insertElement(std::move(clip), insertTime);
    engine_.project().setDirty(true);
    engine_.notifyTimelineChanged();
    refresh();
}

void AssetsPanel::onAddTextPreset(const QString& title, const QString& fontStyle) {
    core::TimelineTime insertTime = engine_.playback().currentTime();
    editor::Clip clip(
        core::ClipId::generate(),
        editor::ClipType::Text,
        title.toStdString(),
        insertTime,
        core::TimelineTime::fromSeconds(5.0)
    );
    clip.setParam("text.content", title.toStdString());
    clip.setParam("text.style", fontStyle.toStdString());
    clip.setParam("text.fontSize", 48.0);
    clip.setParam("text.color", std::string("#FFFFFF"));

    engine_.insertElement(std::move(clip), insertTime);
    engine_.project().setDirty(true);
    engine_.notifyTimelineChanged();
}

void AssetsPanel::onAddGraphicPreset(const QString& name, const QString& shapeType) {
    core::TimelineTime insertTime = engine_.playback().currentTime();
    editor::Clip clip(
        core::ClipId::generate(),
        editor::ClipType::Graphic,
        name.toStdString(),
        insertTime,
        core::TimelineTime::fromSeconds(4.0)
    );
    clip.setParam("graphic.shape", shapeType.toStdString());
    clip.setParam("graphic.color", std::string("#38bdf8"));

    engine_.insertElement(std::move(clip), insertTime);
    engine_.project().setDirty(true);
    engine_.notifyTimelineChanged();
}

void AssetsPanel::onApplyEffectPreset(const QString& effectName) {
    const auto& sel = engine_.selectedClips();
    if (sel.empty()) return;

    auto* tl = engine_.activeTimeline();
    if (!tl) return;

    for (const auto& cid : sel) {
        auto* clip = tl->findClip(cid);
        if (clip) {
            clip->setParam("effect.name", effectName.toStdString());
            clip->setParam("effect.intensity", 1.0);
        }
    }
    engine_.project().setDirty(true);
    engine_.notifyProjectChanged();
}

void AssetsPanel::onApplyTransitionPreset(const QString& name, double durationSec) {
    const auto& sel = engine_.selectedClips();
    if (sel.empty()) return;

    auto* tl = engine_.activeTimeline();
    if (!tl) return;

    for (const auto& cid : sel) {
        auto* clip = tl->findClip(cid);
        if (clip) {
            clip->setParam("transition.type", name.toStdString());
            clip->setParam("transition.duration", durationSec);
        }
    }
    engine_.project().setDirty(true);
    engine_.notifyProjectChanged();
}

void AssetsPanel::onApplyAdjustment(double brightness, double contrast, double saturation, double temperature, double tint) {
    const auto& sel = engine_.selectedClips();
    if (sel.empty()) return;

    auto* tl = engine_.activeTimeline();
    if (!tl) return;

    for (const auto& cid : sel) {
        auto* clip = tl->findClip(cid);
        if (clip) {
            clip->setParam("adjustment.brightness", brightness);
            clip->setParam("adjustment.contrast", contrast);
            clip->setParam("adjustment.saturation", saturation);
            clip->setParam("adjustment.temperature", temperature);
            clip->setParam("adjustment.tint", tint);
        }
    }
    engine_.project().setDirty(true);
    engine_.notifyProjectChanged();
}

void AssetsPanel::onAddAdjustmentLayer() {
    auto* tl = engine_.activeTimeline();
    if (!tl) return;

    core::TimelineTime insertTime = engine_.playback().currentTime();
    editor::Clip clip(
        core::ClipId::generate(),
        editor::ClipType::Graphic,
        "Adjustment Layer",
        insertTime,
        core::TimelineTime::fromSeconds(5.0)
    );
    clip.setParam("isAdjustmentLayer", true);
    clip.setParam("adjustment.brightness", 0.0);
    clip.setParam("adjustment.contrast", 0.0);
    clip.setParam("adjustment.saturation", 0.0);

    engine_.insertElement(std::move(clip), insertTime);
    engine_.project().setDirty(true);
    engine_.notifyTimelineChanged();
}

void AssetsPanel::onImportSrtClicked() {
    QString path = QFileDialog::getOpenFileName(this, "Open Subtitle File", "", "Subtitle Files (*.srt *.vtt);;All Files (*.*)");
    if (path.isEmpty()) return;

    std::ifstream file(path.toStdString());
    if (!file.is_open()) return;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    auto result = subtitles::SrtParser::parse(content);
    if (result.cues.empty()) return;

    auto* tl = engine_.activeTimeline();
    if (!tl) return;

    for (const auto& cue : result.cues) {
        editor::Clip clip(
            core::ClipId::generate(),
            editor::ClipType::Text,
            cue.text,
            cue.startTime,
            cue.duration
        );
        clip.params()["text.content"] = cue.text;
        engine_.insertElement(std::move(clip), cue.startTime);
    }
    engine_.project().setDirty(true);
    engine_.notifyTimelineChanged();
}

void AssetsPanel::onCanvasAspectChanged(int width, int height) {
    engine_.project().settings().canvasSize = {width, height};
    engine_.project().setDirty(true);
    engine_.notifyProjectChanged();
}

void AssetsPanel::onCanvasBgColorChanged(const QColor& color) {
    engine_.project().settings().background.type=editor::ProjectBackground::Type::Color;
    engine_.project().settings().background.color = color.name().toStdString();
    engine_.project().setDirty(true);
    engine_.notifyProjectChanged();
}

} // namespace catchim::ui
#endif
