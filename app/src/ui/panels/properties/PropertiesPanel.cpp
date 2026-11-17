#include "PropertiesPanel.h"
#if defined(HAVE_QT6)
#include "ui/theme/Theme.h"
#include "ui/widgets/WebComboBox.h"
#include "ui/widgets/BalancedScrollMargins.h"
#include "editor/history/commands/ElementCommands.h"
#include "editor/history/commands/EffectCommands.h"
#include "editor/history/commands/KeyframeCommands.h"
#include "editor/history/commands/EffectKeyframeCommands.h"
#include "editor/animation/EffectParamAnimationEngine.h"
#include "render/effects/ClipMotionPresets.h"
#include "ui/widgets/ClipMotionPresetPreview.h"
#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QListWidget>
#include <QComboBox>
#include <QGridLayout>
#include <QScrollArea>
#include <QTabWidget>
#include <QTextEdit>
#include <QCheckBox>
#include <QColorDialog>
#include "ui/widgets/ColorPickerDialog.h"
#include "ui/widgets/SideToolButton.h"
#include <QLabel>
#include <QPainter>
#include <QSignalBlocker>
#include <QTimer>
#include <algorithm>
#include <array>
#include <cmath>

namespace catchim::ui {
namespace {
QString toolRailButtonStyle(const Palette& palette) {
    const auto& accent=palette.primaryAccent;
    return QString("QPushButton{border:1px solid transparent;border-radius:6px;background:transparent;padding:0;}"
                   "QPushButton:hover{background:%1;}"
                   "QPushButton:checked{background:rgba(%2,%3,%4,20);border:1px solid rgba(%2,%3,%4,51);}")
        .arg(palette.hover.name()).arg(accent.red()).arg(accent.green()).arg(accent.blue());
}
class ToggleSwitch : public QCheckBox {
public:
    explicit ToggleSwitch(QWidget* parent) : QCheckBox(parent) { setFixedSize(36,20); setCursor(Qt::PointingHandCursor); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        const auto& pal=Theme::instance().palette();
        p.setPen(Qt::NoPen); p.setBrush(isChecked()?pal.primaryAccent:pal.secondary); p.drawRoundedRect(rect(),10,10);
        p.setBrush(Qt::white); p.drawEllipse(QRectF(isChecked()?18:2,2,16,16));
    }
    bool hitButton(const QPoint& point) const override { return rect().contains(point); }
};

class MotionPresetVideoPreview final : public QWidget {
public:
    MotionPresetVideoPreview(QString slot, QString type, double duration, QWidget* parent = nullptr)
        : QWidget(parent), slot_(std::move(slot)), type_(std::move(type)), duration_(duration) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumHeight(48);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        timer_ = new QTimer(this);
        timer_->setInterval(80);
        connect(timer_, &QTimer::timeout, this, [this] { advanceFrame(); });
    }

    QSize sizeHint() const override { return QSize(180, 102); }

    void setDuration(double duration) {
        const double next = std::clamp(duration, .05, 10.0);
        if (std::abs(next - duration_) < 1e-6) return;
        duration_ = next;
        elapsedSeconds_ = 0.0;
        if (!visibleRegion().isEmpty()) renderFrame();
        update();
    }

protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);
        if (!visibleRegion().isEmpty() && !timer_->isActive()) {
            renderFrame();
            timer_->start();
        }
    }

    void hideEvent(QHideEvent* event) override {
        timer_->stop();
        QWidget::hideEvent(event);
    }

    void paintEvent(QPaintEvent*) override {
        if (visibleRegion().isEmpty()) {
            timer_->stop();
        } else if (!timer_->isActive()) {
            renderFrame();
            timer_->start();
        }
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        if (!frame_.isNull()) painter.drawPixmap(rect(), frame_);
    }

private:
    void advanceFrame() {
        if (!isVisible() || visibleRegion().isEmpty()) {
            timer_->stop();
            return;
        }
        elapsedSeconds_ += timer_->interval() / 1000.0;
        if (slot_ == "loop") {
            const double period = std::max(.05, duration_);
            renderFrame(std::fmod(elapsedSeconds_ / period, 1.0));
        } else {
            if (elapsedSeconds_ > duration_ + .45) elapsedSeconds_ = 0.0;
            renderFrame(std::clamp(elapsedSeconds_ / duration_, 0.0, 1.0));
        }
        update();
    }

    void renderFrame(double progress = 0.0) {
        if (slot_ == "loop") {
            progress = std::fmod(elapsedSeconds_ / std::max(.05, duration_), 1.0);
        } else {
            progress = std::clamp(progress, 0.0, 1.0);
        }
        frame_ = clipMotionPresetPreview(slot_, type_, duration_, progress);
    }

    QString slot_;
    QString type_;
    double duration_{.5};
    double elapsedSeconds_{0.0};
    QPixmap frame_;
    QTimer* timer_{nullptr};
};


const editor::EffectInstance* selectedEffect(const editor::Clip& clip, const std::string& id) {
    const auto& effects = clip.effects();
    if (effects.empty()) return nullptr;
    if (!id.empty()) {
        const auto it = std::find_if(effects.begin(), effects.end(), [&id](const auto& effect) {
            return effect.id == id;
        });
        if (it != effects.end()) return &*it;
    }
    return &effects.back();
}

QString effectDisplayName(const std::string& type) {
    if (type == "blur" || type == "gaussian_blur") return "Blur";
    if (type == "vignette") return "Vignette";
    if (type == "chroma-key" || type == "chroma_key") return "Chroma key";
    if (type == "color-grading" || type == "color_grading") return "Color grading";
    if (type == "sharpen") return "Sharpen";
    if (type == "grayscale" || type == "monochrome") return "Monochrome";
    if (type == "invert" || type == "invert-colors" || type == "invert_colors") return "Invert Colors";
    return QString::fromStdString(type);
}
}
PropertiesPanel::PropertiesPanel(editor::EditorEngine& engine,QWidget* parent) : QWidget(parent),engine_(engine) {
    setAttribute(Qt::WA_StyledBackground,true); setupUi(); refresh();
}
QVBoxLayout* PropertiesPanel::addTab(const QString& key,const QString& title,UiIcon icon) {
    const auto& pal=Theme::instance().palette();
    auto* button=new SideToolButton(title,rail_); button->setObjectName("propertyTab_"+key);
    button->setFixedSize(32,32); button->setCheckable(true);button->setAccessibleName(title);
    button->setIcon(UiIcons::get(icon,pal.textSecondary,18)); button->setIconSize(QSize(18,18));
    button->setProperty("propertyIcon",static_cast<int>(icon));
    button->setStyleSheet(toolRailButtonStyle(pal));
    auto* railLayout=static_cast<QVBoxLayout*>(rail_->layout());
    railLayout->insertWidget(railLayout->count()-1,button);
    auto* page=new QWidget; page->setObjectName("propertyPage_"+key);
    page->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);page->setMinimumWidth(0);
    auto* layout=new QVBoxLayout(page); layout->setContentsMargins(16,16,16,16); layout->setSpacing(14);
    static_cast<QVBoxLayout*>(scrollArea_->widget()->layout())->addWidget(page);
    tabs_.emplace(key,Tab{button,page});
    connect(button,&QPushButton::clicked,this,[this,key] { selectTab(key); });
    return layout;
}
void PropertiesPanel::setupUi() {
    auto* root=new QVBoxLayout(this); root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    const auto& pal=Theme::instance().palette();
    emptyView_=new QWidget(this);
    auto* empty=new QVBoxLayout(emptyView_); empty->setContentsMargins(16,16,16,16); empty->setSpacing(12); empty->addStretch();
    auto* icon=new QLabel;icon->setObjectName("emptyPropertiesIcon"); icon->setAlignment(Qt::AlignCenter); icon->setFixedHeight(40);
    auto iconColor=pal.textSecondary; iconColor.setAlphaF(.75);
    icon->setPixmap(UiIcons::getPixmap(UiIcon::SlidersBox,iconColor,40)); empty->addWidget(icon);
    auto* textBlock=new QWidget; auto* texts=new QVBoxLayout(textBlock); texts->setContentsMargins(0,0,0,0); texts->setSpacing(8);
    auto* title=new QLabel("Chưa có gì ở đây"); title->setAlignment(Qt::AlignCenter); title->setFixedHeight(28);
    title->setStyleSheet("font-size:18px;font-weight:500;"); texts->addWidget(title);
    auto* desc=new QLabel("Chọn một phần tử trên dòng thời gian để chỉnh thuộc tính");desc->setObjectName("emptyPropertiesDescription"); desc->setAlignment(Qt::AlignCenter); desc->setWordWrap(true);
    desc->setStyleSheet(QString("font-size:12.64px;color:%1;").arg(pal.textSecondary.name())); texts->addWidget(desc);
    empty->addWidget(textBlock); empty->addStretch(); root->addWidget(emptyView_);
    multiSelection_=new QLabel;multiSelection_->setAlignment(Qt::AlignCenter);root->addWidget(multiSelection_,1);
    inspectorView_=new QWidget(this); auto* row=new QHBoxLayout(inspectorView_); row->setContentsMargins(0,0,0,0); row->setSpacing(0);
    rail_=new QWidget; rail_->setObjectName("propertiesTabRail"); rail_->setFixedWidth(41);
    rail_->setStyleSheet(QString("#propertiesTabRail{border-right:1px solid %1;}").arg(pal.border.name()));
    auto* railLayout=new QVBoxLayout(rail_); railLayout->setContentsMargins(4,4,4,4); railLayout->setSpacing(2); railLayout->addStretch();
    row->addWidget(rail_);
    scrollArea_=new QScrollArea; scrollArea_->setWidgetResizable(true); scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea_->setStyleSheet("QScrollArea{border:none;background:transparent;}");
    scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    auto* content=new QWidget;content->setObjectName("propertyContent"); auto* pages=new QVBoxLayout(content); pages->setContentsMargins(0,0,0,0); pages->setSpacing(0); pages->setAlignment(Qt::AlignTop);
    scrollArea_->setWidget(content);
    balanceScrollContentMargins(scrollArea_);
    row->addWidget(scrollArea_,1); root->addWidget(inspectorView_,1);
    content->setStyleSheet(QString("QDoubleSpinBox,QLineEdit,QTextEdit{background:%1;border:1px solid transparent;border-radius:10px;padding:4px 10px;min-width:0px;} QDoubleSpinBox:focus,QLineEdit:focus,QTextEdit:focus{border-color:%2;}").arg(pal.hover.name(),pal.primaryAccent.name()));
    auto field=[&](QVBoxLayout* layout,const QString& label,QWidget* control,QPushButton* keyframeToggle=nullptr) {
        auto* wrapper=new QWidget;auto* fields=new QVBoxLayout(wrapper);fields->setContentsMargins(0,0,0,0);fields->setSpacing(8);
        if(!label.isEmpty()) {
            auto* caption=new QLabel(label);caption->setProperty("propertyCaption",true); caption->setStyleSheet(QString("color:%1;font-size:12px;").arg(pal.textSecondary.name()));
            if(keyframeToggle) {
                auto* labelRow=new QWidget(wrapper);auto* rowLayout=new QHBoxLayout(labelRow);
                rowLayout->setContentsMargins(0,0,0,0);rowLayout->setSpacing(4);
                rowLayout->addWidget(keyframeToggle);rowLayout->addWidget(caption,1);fields->addWidget(labelRow);
            } else fields->addWidget(caption);
        }
        fields->addWidget(control);layout->addWidget(wrapper);control->setMinimumWidth(0);
        control->setSizePolicy(QSizePolicy::Ignored,control->sizePolicy().verticalPolicy());
        wrapper->setMinimumWidth(0);wrapper->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    };
    auto number=[&](QVBoxLayout* layout,const QString& label,const char* key,double def,double min,double max,double step,double multiplier=1.,bool keyframable=false) -> QDoubleSpinBox* {
        auto* spin=new QDoubleSpinBox; spin->setObjectName(QString("property_")+key); spin->setRange(min,max); spin->setSingleStep(step);
        spin->setDecimals(step<1?2:0); spin->setButtonSymbols(QAbstractSpinBox::NoButtons); spin->setFixedHeight(28); spin->setKeyboardTracking(false);
        QPushButton* keyframeToggle=nullptr;
        if(keyframable) {
            keyframeToggle=new QPushButton(QStringLiteral("◇"));
            keyframeToggle->setObjectName(QStringLiteral("keyframe_")+key);
            keyframeToggle->setFixedSize(20,20);keyframeToggle->setFlat(true);
            keyframeToggle->setAccessibleName(QStringLiteral("Keyframe: ")+label);
            keyframeToggle->setFocusPolicy(Qt::StrongFocus);
        }
        field(layout,label,spin,keyframeToggle);
        readers_.push_back([spin,key=std::string(key),def,multiplier](const editor::Clip& clip){
            QSignalBlocker guard(spin);double value=clip.getParam<double>(key,def);
            if(key=="volume"&&clip.getParam<std::string>("audio.volumeUnit","")!="db"&&value>0&&value<=2)value=20*std::log10(value);
            spin->setValue(value*multiplier);
        });
        if(keyframable) {
            auto reader=[this,spin,keyframeToggle,key=std::string(key),label,multiplier](const editor::Clip& clip){
                const auto localTime=localPlayheadTime(clip);
                const auto propertyPath=animationPathForClip(clip,key);
                const auto* channel=localTime&&propertyPath?clip.findAnimationChannel(*propertyPath):nullptr;
                const bool hasAnimatedValue=channel&&!channel->empty();
                if(hasAnimatedValue) {
                    QSignalBlocker guard(spin);
                    spin->setValue(channel->getValueAt(*localTime)*multiplier);
                }
                const bool inRange=localTime.has_value();
                const bool active=inRange&&channel&&channel->findKeyframeAt(*localTime).has_value();
                const auto& palette=Theme::instance().palette();
                const QString style=QString("QPushButton{border:none;padding:0;background:transparent;color:%1;font-size:15px;} QPushButton:hover{background:%2;border-radius:4px;} QPushButton:disabled{color:%3;}")
                    .arg(active?palette.primaryAccent.name():palette.textSecondary.name(),palette.hover.name(),palette.textSecondary.name());
                if(keyframeToggle->styleSheet()!=style)keyframeToggle->setStyleSheet(style);
                const QString glyph=active?QStringLiteral("◆"):QStringLiteral("◇");
                if(keyframeToggle->text()!=glyph)keyframeToggle->setText(glyph);
                if(keyframeToggle->isEnabled()!=inRange)keyframeToggle->setEnabled(inRange);
                const QString tip=active?QStringLiteral("Xóa keyframe: %1").arg(label):QStringLiteral("Thêm keyframe: %1").arg(label);
                if(keyframeToggle->toolTip()!=tip)keyframeToggle->setToolTip(tip);
                if(keyframeToggle->accessibleName()!=tip)keyframeToggle->setAccessibleName(tip);
            };
            animatedReaders_.push_back(reader);
        }
        connect(spin,&QDoubleSpinBox::valueChanged,this,[this,key=std::string(key),multiplier,keyframable](double value) {
            const double resolved=value/multiplier;
            if(keyframable)setNumericPropertyValue(key,resolved);
            else patch({{key,resolved}});
        });
        if(keyframeToggle)connect(keyframeToggle,&QPushButton::clicked,this,[this,key=std::string(key),spin,multiplier]{toggleNumericKeyframe(key,spin->value()/multiplier);});
        return spin;
    };
    auto select=[&](QVBoxLayout* layout,const QString& label,const char* key,const QStringList& names,const QStringList& values,const QString& def) {
        auto* combo=new WebComboBox; combo->setObjectName(QString("property_")+key); combo->setFixedHeight(32); combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); combo->setMinimumContentsLength(1);
        for(int i=0;i<names.size();++i)combo->addItem(names[i],values[i]); field(layout,label,combo);
        readers_.push_back([combo,key=std::string(key),def](const editor::Clip& clip){QSignalBlocker guard(combo); combo->setCurrentIndex(combo->findData(QString::fromStdString(clip.getParam<std::string>(key,def.toStdString()))));});
        connect(combo,&QComboBox::currentIndexChanged,this,[this,combo,key=std::string(key)](int i){if(i>=0)patch({{key,combo->currentData().toString().toStdString()}});});
    };
    auto color=[&](QVBoxLayout* layout,const QString& label,const char* key,const char* def,bool allowAlpha=false) -> QPushButton* {
        auto* button=new QPushButton; button->setObjectName(QString("property_")+key); button->setFixedHeight(30); button->setStyleSheet(QString("text-align:left;background:%1;border-radius:9px;padding:0 8px;").arg(pal.hover.name()));
        button->setProperty("colorField",true);
        field(layout,label,button);
        readers_.push_back([this,button,key=std::string(key),def=std::string(def)](const editor::Clip& clip) {
            std::string value=clip.getParam<std::string>(key,def);
            if(key.rfind("effect.",0)==0) {
                if(const auto* effect=selectedEffect(clip,selectedEffectId_)) {
                    const auto it=effect->params.find(key.substr(7));
                    if(it!=effect->params.end()&&it->is_string())value=it->get<std::string>();
                }
            }
            QColor c=editorColorFromCss(QString::fromStdString(value)); QPixmap swatch(16,16); swatch.fill(c);button->setIcon(QIcon(swatch)); button->setText(editorColorToCss(c,true).mid(1)); button->setProperty("color",c);
        });
        connect(button,&QPushButton::clicked,this,[this,button,key=std::string(key),allowAlpha] {
#if defined(__EMSCRIPTEN__)
            chooseEditorColorAsync(button->property("color").value<QColor>(),this,"Chọn màu",
                [this,key,allowAlpha](QColor c){if(c.isValid())patch({{key,editorColorToCss(c,allowAlpha).toLower().toStdString()}});},allowAlpha);
#else
            auto c=chooseEditorColor(button->property("color").value<QColor>(),this,"Chọn màu",allowAlpha);
            if(c.isValid())patch({{key,editorColorToCss(c,allowAlpha).toLower().toStdString()}});
#endif
        });
        return button;
    };
    auto* text=addTab("text","Văn bản",UiIcon::Font);
    auto* edit=new QTextEdit; edit->setObjectName("property_text.content"); edit->setFixedHeight(60); edit->setPlaceholderText("Nhập văn bản…"); field(text,"",edit);
    readers_.push_back([edit](const editor::Clip& c){const auto value=QString::fromStdString(c.getParam<std::string>("text.content",c.name()));if(edit->toPlainText()!=value){QSignalBlocker guard(edit);edit->setPlainText(value);}});
    connect(edit,&QTextEdit::textChanged,this,[this,edit]{patch({{"text.content",edit->toPlainText().toStdString()}});});
    auto* ttsJump=new QPushButton("Chuyển đổi văn bản thành lời nói"); ttsJump->setFixedHeight(28); ttsJump->setStyleSheet("border:1px dashed #dedede;border-radius:5px;background:transparent;"); text->addWidget(ttsJump);
    ttsJump->setMinimumWidth(0);ttsJump->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    connect(ttsJump,&QPushButton::clicked,this,[this]{selectTab("tts");});
    auto* family=new QLineEdit; family->setObjectName("property_text.fontFamily"); family->setFixedHeight(36);field(text,"Phông chữ",family);
    readers_.push_back([family](const editor::Clip& c){if(!family->hasFocus())family->setText(QString::fromStdString(c.getParam<std::string>("text.fontFamily","Arial")));});
    connect(family,&QLineEdit::editingFinished,this,[this,family]{if(!family->text().trimmed().isEmpty())patch({{"text.fontFamily",family->text().trimmed().toStdString()}});});
    number(text,"Cỡ chữ","text.fontSize",15,1,500,1);
    color(text,"Màu","text.color","#FFFFFF",true);
    select(text,"Căn chỉnh văn bản","text.textAlign",{"Trái","Giữa","Phải"},{"left","center","right"},"center");
    select(text,"Độ đậm","text.fontWeight",{"Bình thường","Đậm"},{"normal","bold"},"normal");
    select(text,"Kiểu chữ","text.fontStyle",{"Bình thường","Nghiêng"},{"normal","italic"},"normal");
    select(text,"Trang trí","text.textDecoration",{"Không","Gạch chân","Gạch ngang"},{"none","underline","line-through"},"none");
    number(text,"Khoảng cách chữ","text.letterSpacing",0,-100,1000,.1,1.,true);
    number(text,"Chiều cao dòng","text.lineHeight",1.2,.1,10,.1,1.,true);
    auto* textBackgroundEnabled=new ToggleSwitch(content);
    textBackgroundEnabled->setObjectName("property_background.enabled");
    field(text,"Bật nền",textBackgroundEnabled);
    auto* textBackgroundColor=color(text,"Màu nền","background.color","#000000",true);
    auto* textBackgroundRadius=number(text,"Bo góc nền","background.cornerRadius",0,0,100,1,1.,true);
    auto* textBackgroundPaddingX=number(text,"Đệm nền X","background.paddingX",30,0,100000,1,1.,true);
    auto* textBackgroundPaddingY=number(text,"Đệm nền Y","background.paddingY",42,0,100000,1,1.,true);
    auto* textBackgroundOffsetX=number(text,"Dịch nền X","background.offsetX",0,-100000,100000,1,1.,true);
    auto* textBackgroundOffsetY=number(text,"Dịch nền Y","background.offsetY",0,-100000,100000,1,1.,true);
    const auto textBackgroundRows=std::vector<QWidget*>{
        textBackgroundColor->parentWidget(),textBackgroundRadius->parentWidget(),
        textBackgroundPaddingX->parentWidget(),textBackgroundPaddingY->parentWidget(),
        textBackgroundOffsetX->parentWidget(),textBackgroundOffsetY->parentWidget()};
    for(auto* row:textBackgroundRows)row->hide();
    connect(textBackgroundEnabled,&QCheckBox::toggled,this,[this,textBackgroundRows](bool enabled){
        for(auto* row:textBackgroundRows)row->setVisible(enabled);
        patch({{"background.enabled",enabled}});
    });
    readers_.push_back([textBackgroundEnabled,textBackgroundRows](const editor::Clip& clip){
        const bool enabled=clip.getParam<bool>("background.enabled",false);
        QSignalBlocker guard(textBackgroundEnabled);
        textBackgroundEnabled->setChecked(enabled);
        for(auto* row:textBackgroundRows)row->setVisible(enabled);
    });
    auto* tts=addTab("tts","Chuyển văn bản thành giọng nói",UiIcon::Audio);
    auto* ttsInfo=new QLabel("Chưa kết nối bộ tổng hợp giọng nói. Không tạo âm thanh giả thay lời đọc."); ttsInfo->setWordWrap(true);tts->addWidget(ttsInfo);
    auto* transform=addTab("transform","Biến đổi",UiIcon::Transform);
    number(transform,"Vị trí X","transform.positionX",0,-100000,100000,1,1.,true);
    number(transform,"Vị trí Y","transform.positionY",0,-100000,100000,1,1.,true);
    number(transform,"Tỷ lệ X","transform.scaleX",1,.01,10000,.1,100,true);
    number(transform,"Tỷ lệ Y","transform.scaleY",1,.01,10000,.1,100,true);
    number(transform,"Xoay","transform.rotate",0,-360,360,1,1.,true);
    auto* audio=addTab("audio","Âm thanh",UiIcon::Music);
    number(audio,"Âm lượng","volume",0,-60,12,.1,1.,true);
    auto* mute=new ToggleSwitch(content);mute->setObjectName("property_muted");field(audio,"Tắt tiếng",mute);
    readers_.push_back([mute](const editor::Clip& c){QSignalBlocker guard(mute);mute->setChecked(c.isMuted()||c.getParam<bool>("muted",false));});
    connect(mute,&QCheckBox::toggled,this,[this](bool v){patch({{"muted",v}},v);});
    auto* speed=addTab("speed","Tốc độ",UiIcon::Speed);
    number(speed,"Tốc độ phát","speed",1,.01,5,.01);
    auto* changePitch=new ToggleSwitch(content);changePitch->setObjectName("property_changePitch");
    changePitch->setToolTip("Tắt để giữ cao độ khi thay đổi tốc độ");
    field(speed,"Đổi cao độ",changePitch);
    readers_.push_back([changePitch](const editor::Clip& c){
        QSignalBlocker guard(changePitch);
        changePitch->setChecked(!c.getParam<bool>("maintainPitch",false));
    });
    connect(changePitch,&QCheckBox::toggled,this,[this](bool change){patch({{"maintainPitch",!change}});});
    auto* reverse=new ToggleSwitch(content); field(speed,"Phát ngược",reverse);
    readers_.push_back([reverse](const editor::Clip& c){QSignalBlocker guard(reverse);reverse->setChecked(c.getParam<bool>("reversed",false));});
    connect(reverse,&QCheckBox::toggled,this,[this](bool v){patch({{"reversed",v}});});
    auto* blend=addTab("blending","Hòa trộn",UiIcon::Drop);
    number(blend,"Độ mờ","opacity",1,0,100,1,100,true);
    select(blend,"Chế độ hòa trộn","transform.blendModeName",{"Bình thường","Nhân","Màn hình","Phủ","Làm tối","Làm sáng","Né màu"},{"normal","multiply","screen","overlay","darken","lighten","color_dodge"},"normal");
    auto* graphic=addTab("graphic","Hình dạng",UiIcon::ShapeRect);
    select(graphic,"Hình dạng","graphic.shape",{"Chữ nhật","Tròn","Ngũ giác","Ngôi sao","Mũi tên"},{"rectangle","circle","polygon","star","arrow"},"rectangle");
    color(graphic,"Màu tô","graphic.color","#FFFFFF");
    number(graphic,"Bo góc","graphic.cornerRadius",0,0,200,1);
    number(graphic,"Độ dày viền","graphic.strokeWidth",0,0,100,1);
    auto* masks=addTab("masks","Mặt nạ",UiIcon::Layers);
    auto* maskInfo=new QLabel("Bộ dựng C++ chưa hỗ trợ mặt nạ. Các dữ liệu mặt nạ hiện có vẫn được giữ nguyên.");maskInfo->setWordWrap(true);masks->addWidget(maskInfo);
    auto* effects=addTab("effects","Hiệu ứng",UiIcon::Effects);
    auto* effectStack=new QListWidget;effectStack->setObjectName("clipEffectStack");effectStack->setMaximumHeight(120);effectStack->setSelectionMode(QAbstractItemView::SingleSelection);effects->addWidget(effectStack);
    auto* effectActions=new QWidget;auto* effectActionsLayout=new QHBoxLayout(effectActions);effectActionsLayout->setContentsMargins(0,0,0,0);effectActionsLayout->setSpacing(6);
    auto* moveEffectUp=new QPushButton("↑");moveEffectUp->setToolTip("Di chuyển lên");moveEffectUp->setFixedSize(34,28);
    auto* moveEffectDown=new QPushButton("↓");moveEffectDown->setToolTip("Di chuyển xuống");moveEffectDown->setFixedSize(34,28);
    auto* toggleEffect=new QPushButton("Tắt");toggleEffect->setObjectName("toggleClipEffect");toggleEffect->setFixedHeight(28);
    auto* removeEffect=new QPushButton("Xóa");removeEffect->setObjectName("removeClipEffect");removeEffect->setFixedHeight(28);
    effectActionsLayout->addWidget(moveEffectUp);effectActionsLayout->addWidget(moveEffectDown);effectActionsLayout->addWidget(toggleEffect,1);effectActionsLayout->addWidget(removeEffect);effects->addWidget(effectActions);
    auto* effectInfo=new QLabel;effectInfo->setWordWrap(true);effects->addWidget(effectInfo);
    struct MotionPresetCardUi {
        QString type;
        QString searchText;
        QPushButton* button{nullptr};
        MotionPresetVideoPreview* preview{nullptr};
    };
    struct MotionSlotUi {
        QString slot;
        QDoubleSpinBox* duration{nullptr};
        QPushButton* clear{nullptr};
        std::vector<MotionPresetCardUi> cards;
    };
    auto* motionTabs=new QTabWidget;
    motionTabs->setObjectName("clipMotionTabs");
    motionTabs->setDocumentMode(true);
    motionTabs->setMinimumHeight(380);
    motionTabs->setMaximumHeight(520);
    effects->addWidget(motionTabs);
    std::vector<MotionSlotUi> motionSlots;
    const std::array<std::pair<QString,QString>,3> motionSlotTabs{{
        {QStringLiteral("in"),QStringLiteral("Vào")},
        {QStringLiteral("loop"),QStringLiteral("Lặp")},
        {QStringLiteral("out"),QStringLiteral("Ra")}
    }};
    for(const auto& [slot,tabTitle]:motionSlotTabs) {
        std::vector<const render::ClipMotionPreset*> presets;
        for(const auto& group:render::clipMotionPresetGroups())
            for(const auto& item:group.presets)
                if(item.slot==slot.toStdString())presets.push_back(&item);

        auto* page=new QWidget(motionTabs);
        auto* pageLayout=new QVBoxLayout(page);
        pageLayout->setContentsMargins(4,6,4,4);
        pageLayout->setSpacing(6);
        auto* durationRow=new QWidget(page);
        auto* durationLayout=new QHBoxLayout(durationRow);
        durationLayout->setContentsMargins(0,0,0,0);
        durationLayout->setSpacing(6);
        durationLayout->addWidget(new QLabel(slot=="loop"?"Chu kỳ lặp (giây)":"Thời lượng (giây)",durationRow));
        auto* duration=new QDoubleSpinBox(durationRow);
        duration->setObjectName("motionDuration_"+slot);
        duration->setRange(.05,10.0);
        duration->setSingleStep(.1);
        duration->setDecimals(2);
        duration->setButtonSymbols(QAbstractSpinBox::NoButtons);
        duration->setFixedHeight(28);
        duration->setKeyboardTracking(false);
        durationLayout->addWidget(duration,1);
        auto* clear=new QPushButton("Gỡ hiệu ứng",durationRow);
        clear->setObjectName("clearMotion_"+slot);
        clear->setFixedHeight(28);
        durationLayout->addWidget(clear);
        pageLayout->addWidget(durationRow);

        auto* search=new QLineEdit(page);
        search->setObjectName("motionPresetSearch_"+slot);
        search->setPlaceholderText(QString("Tìm trong %1 hiệu ứng…").arg(presets.size()));
        search->setClearButtonEnabled(true);
        search->setFixedHeight(30);
        search->setAccessibleName(QString("Tìm hiệu ứng %1").arg(tabTitle));
        pageLayout->addWidget(search);

        auto* cardScroll=new QScrollArea(page);
        cardScroll->setObjectName("motionPresetScroll_"+slot);
        cardScroll->setWidgetResizable(true);
        cardScroll->setFrameShape(QFrame::NoFrame);
        cardScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* cardsHost=new QWidget(cardScroll);
        auto* grid=new QGridLayout(cardsHost);
        grid->setContentsMargins(2,2,4,2);
        grid->setHorizontalSpacing(6);
        grid->setVerticalSpacing(6);

        MotionSlotUi motion{slot,duration,clear,{}};
        motion.cards.reserve(presets.size());
        for(const auto* item:presets) {
            const QString name=QString::fromStdString(item->name);
            const QString type=QString::fromStdString(item->type);
            const QString description=QString::fromStdString(item->description);
            auto* card=new QPushButton(cardsHost);
            card->setObjectName("motionPresetCard_"+slot+QLatin1Char('_')+type);
            card->setAccessibleName(name);
            card->setToolTip(description);
            card->setCheckable(true);
            card->setCursor(Qt::PointingHandCursor);
            card->setProperty("class","cardWidget");
            card->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
            card->setMinimumHeight(108);
            auto* cardLayout=new QVBoxLayout(card);
            cardLayout->setContentsMargins(5,5,5,4);
            cardLayout->setSpacing(4);
            auto* preview=new MotionPresetVideoPreview(slot,type,.5,card);
            preview->setObjectName("motionPresetVideo_"+slot+QLatin1Char('_')+type);
            preview->setFixedHeight(64);
            cardLayout->addWidget(preview);
            auto* title=new QLabel(name,card);
            title->setAlignment(Qt::AlignCenter);
            title->setWordWrap(true);
            title->setFixedHeight(30);
            title->setStyleSheet("font-weight:600;");
            cardLayout->addWidget(title);
            grid->addWidget(card,static_cast<int>(motion.cards.size()/2),
                            static_cast<int>(motion.cards.size()%2));
            motion.cards.push_back({type,name+QLatin1Char(' ')+description+QLatin1Char(' ')+type,card,preview});
        }
        auto* noMatches=new QLabel("Không tìm thấy hiệu ứng phù hợp.",cardsHost);
        noMatches->setObjectName("noMotionPresetMatches_"+slot);
        noMatches->setAlignment(Qt::AlignCenter);
        noMatches->setProperty("class","SecondaryLabel");
        noMatches->hide();
        grid->addWidget(noMatches,static_cast<int>(motion.cards.size()/2),0,1,2);
        cardScroll->setWidget(cardsHost);
        pageLayout->addWidget(cardScroll,1);

        const auto filterCards=[grid,noMatches,cards=motion.cards](const QString& query){
            int visibleIndex=0;
            const QString term=query.trimmed();
            grid->removeWidget(noMatches);
            for(const auto& card:cards) {
                grid->removeWidget(card.button);
                if(term.isEmpty()||card.searchText.contains(term,Qt::CaseInsensitive)) {
                    card.button->show();
                    grid->addWidget(card.button,visibleIndex/2,visibleIndex%2);
                    ++visibleIndex;
                } else card.button->hide();
            }
            noMatches->setVisible(visibleIndex==0);
            if(visibleIndex==0)grid->addWidget(noMatches,0,0,1,2);
        };
        connect(search,&QLineEdit::textChanged,cardsHost,filterCards);
        const QString tabLabel=QStringLiteral("%1 (%2)").arg(tabTitle).arg(presets.size());
        motionTabs->addTab(page,tabLabel);
        motionSlots.push_back(std::move(motion));
    }
    readers_.push_back([this,effectStack,moveEffectUp,moveEffectDown,toggleEffect,removeEffect](const editor::Clip& c){
        QSignalBlocker guard(effectStack);const auto previousId=selectedEffectId_;effectStack->clear();int selectedRow=-1;
        for(size_t i=0;i<c.effects().size();++i){const auto& effect=c.effects()[i];auto* item=new QListWidgetItem(effectDisplayName(effect.type)+(effect.enabled?QString():QStringLiteral(" · Tắt")),effectStack);item->setData(Qt::UserRole,QString::fromStdString(effect.id));if(effect.id==previousId)selectedRow=static_cast<int>(i);}
        if(selectedRow<0&&!c.effects().empty())selectedRow=static_cast<int>(c.effects().size()-1);
        effectStack->setCurrentRow(selectedRow);selectedEffectId_=selectedRow>=0?effectStack->item(selectedRow)->data(Qt::UserRole).toString().toStdString():std::string{};
        const auto* effect=selectedEffect(c,selectedEffectId_);const int row=effectStack->currentRow();
        moveEffectUp->setEnabled(effect&&row>0);moveEffectDown->setEnabled(effect&&row>=0&&row+1<effectStack->count());
        toggleEffect->setEnabled(effect);toggleEffect->setText(effect&&!effect->enabled?QStringLiteral("Bật"):QStringLiteral("Tắt"));removeEffect->setEnabled(effect);
    });
    readers_.push_back([this,effectInfo](const editor::Clip& c){
        QString effectName=QString::fromStdString(c.getParam<std::string>("effect.name","Không có"));
        if(const auto* effect=selectedEffect(c,selectedEffectId_))effectName=effectDisplayName(effect->type);
        const auto motionLabel=[&c](const char* slot){
            const auto value=c.getParam<std::string>(std::string("transition.")+slot+".type","");
            if(value.empty())return QString("—");
            if(value=="slide_left")return QString("Trượt trái");
            if(value=="slide_right")return QString("Trượt phải");
            if(value=="slide_top")return QString::fromLatin1(slot)=="in"?QString("Trượt từ trên"):QString("Trượt lên");
            if(value=="slide_bottom")return QString::fromLatin1(slot)=="in"?QString("Trượt từ dưới"):QString("Trượt xuống");
            if(value=="wipe_left")return QString("Mở từ trái");
            if(value=="wipe_right")return QString("Mở từ phải");
            if(value=="zoom_out")return QString("Thu nhỏ");
            if(value=="zoom")return QString("Phóng to");
            if(value=="drop_in")return QString("Rơi vào");
            if(value=="drop_out")return QString("Rơi ra");
            if(value=="pop_in")return QString("Pop In");
            if(value=="pop_out")return QString("Pop Out");
            if(value=="spin")return QString::fromLatin1(slot)=="in"?QString("Xoay vào"):QString("Xoay ra");
            if(value=="bounce")return QString::fromLatin1(slot)=="in"?QString("Bật nảy vào"):QString("Bật nảy ra");
            if(value=="zoom_pulse")return QString("Zoom Pulse");
            if(value=="rotate_loop")return QString("Xoay đều");
            if(value=="float")return QString("Lơ lửng");
            if(value=="sway")return QString("Đung đưa");
            if(value=="heartbeat")return QString("Nhịp tim");
            if(value=="flicker")return QString("Chập chờn");
            if(value=="pendulum")return QString("Con lắc");
            if(value=="drift")return QString("Trôi ngang");
            if(value=="breathe")return QString("Nhịp thở");
            if(value=="wobble")return QString("Lắc nghiêng");
            if(value=="bounce_loop")return QString("Nảy liên tục");
            if(value=="elastic_in"||value=="elastic_out")return QString("Co giãn đàn hồi");
            if(value=="flip_in_x"||value=="flip_out_x")return QString("Lật ngang");
            if(value=="flip_in_y"||value=="flip_out_y")return QString("Lật dọc");
            if(value=="tilt_in"||value=="tilt_out")return QString("Nghiêng");
            if(value=="whip_left")return QString("Whip trái");
            if(value=="whip_right")return QString("Whip phải");
            if(value=="jitter")return QString("Rung nhanh");
            if(value=="orbit")return QString("Quỹ đạo tròn");
            if(value=="squash")return QString("Co giãn ngang dọc");
            if(value=="flash")return QString("Chớp sáng");
            if(value=="spin_pulse")return QString("Xoay theo nhịp");
            return QString::fromStdString(value);
        };
        const auto legacy=QString::fromStdString(c.getParam<std::string>("transition.type","Không có"));
        effectInfo->setText(QString("Hiệu ứng: %1\nChuyển cảnh cũ: %2\nVào: %3\nRa: %4\nLặp: %5")
            .arg(effectName,legacy,motionLabel("in"),motionLabel("out"),motionLabel("loop")));
    });
    const auto applyMotionSlot=[this](const QString& slot,QDoubleSpinBox* durationInput,
                                      const QString& selectedType,bool remove) {
        if(engine_.selectedClips().size()!=1)return;
        auto* timeline=engine_.activeTimeline();if(!timeline)return;
        const auto clipId=engine_.selectedClips().front();
        const auto* clip=timeline->findClip(clipId);
        const auto* track=timeline->findTrackContainingClip(clipId);
        if(!clip||!track||track->isLocked()||track->isHidden()||clip->isHidden()||
           (clip->type()!=editor::ClipType::Video&&clip->type()!=editor::ClipType::Image))return;
        if(!remove&&selectedType.isEmpty())return;
        const bool isLoop=slot=="loop";
        const std::string typeKey="transition."+slot.toStdString()+".type";
        const std::string durationKey="transition."+slot.toStdString()+(isLoop?".period":".duration");
        const double maximum=isLoop?10.0:std::max(.05,clip->duration().toSeconds());
        const double duration=std::clamp(durationInput->value(),.05,maximum);
        const std::string type=remove?std::string{}:selectedType.toStdString();
        if(clip->getParam<std::string>(typeKey,"")==type&&
           std::abs(clip->getParam<double>(durationKey,isLoop?.8:.5)-duration)<1e-6)return;
        editor::ElementPatch update{};
        update.clipId=clipId;
        update.patchParams[typeKey]=type;
        update.patchParams[durationKey]=duration;
        if(!engine_.history().execute(std::make_unique<editor::UpdateElementsCommand>(
                *timeline,std::vector<editor::ElementPatch>{std::move(update)})))return;
        engine_.project().setDirty(true);
        engine_.notifyTimelineChanged();
        refresh();
    };
    for(const auto& motion:motionSlots) {
        for(const auto& card:motion.cards) {
            connect(card.button,&QPushButton::clicked,this,[applyMotionSlot,motion,card]{
                applyMotionSlot(motion.slot,motion.duration,card.type,false);
            });
        }
        connect(motion.clear,&QPushButton::clicked,this,[applyMotionSlot,motion]{
            applyMotionSlot(motion.slot,motion.duration,QString(),true);
        });
        connect(motion.duration,&QDoubleSpinBox::valueChanged,this,[motion](double value){
            for(const auto& card:motion.cards)card.preview->setDuration(value);
        });
        connect(motion.duration,&QDoubleSpinBox::editingFinished,this,[this,applyMotionSlot,motion]{
            auto* timeline=engine_.activeTimeline();
            if(!timeline||engine_.selectedClips().size()!=1)return;
            const auto* clip=timeline->findClip(engine_.selectedClips().front());
            if(!clip)return;
            const std::string key="transition."+motion.slot.toStdString()+".type";
            const auto type=clip->getParam<std::string>(key,"");
            if(!type.empty())applyMotionSlot(motion.slot,motion.duration,QString::fromStdString(type),false);
        });
    }
    readers_.push_back([this,motionTabs,motionSlots](const editor::Clip& c){
        const bool visual=c.type()==editor::ClipType::Video||c.type()==editor::ClipType::Image;
        motionTabs->setVisible(visual);
        const bool single=engine_.selectedClips().size()==1;
        const auto* timeline=engine_.activeTimeline();
        const auto* track=timeline?timeline->findTrackContainingClip(c.id()):nullptr;
        const bool editable=single&&visual&&track&&!track->isLocked()&&!track->isHidden()&&!c.isHidden();
        for(const auto& motion:motionSlots) {
            const std::string prefix="transition."+motion.slot.toStdString()+".";
            const std::string durationKey=prefix+(motion.slot=="loop"?"period":"duration");
            const std::string type=c.getParam<std::string>(prefix+"type","");
            const double maxDuration=motion.slot=="loop"?10.0:std::max(.05,c.duration().toSeconds());
            {
                QSignalBlocker durationGuard(motion.duration);
                motion.duration->setMaximum(std::min(10.0,maxDuration));
                motion.duration->setValue(std::clamp(c.getParam<double>(durationKey,motion.slot=="loop"?.8:.5),
                                                     .05,motion.duration->maximum()));
            }
            for(const auto& card:motion.cards) {
                QSignalBlocker cardGuard(card.button);
                card.button->setChecked(type==card.type.toStdString());
                card.preview->setDuration(motion.duration->value());
            }
            motion.clear->setEnabled(editable&&!type.empty());
        }
    });
    connect(effectStack,&QListWidget::currentRowChanged,this,[this,effectStack](int row){if(row<0)return;selectedEffectId_=effectStack->item(row)->data(Qt::UserRole).toString().toStdString();refresh();});
    auto executeEffectCommand=[this,effectStack](const std::function<std::unique_ptr<editor::EditorCommand>(editor::Timeline&,const core::TrackId&,const core::ClipId&,const std::string&,size_t)>& factory){
        if(isUpdatingUi_||effectStack->currentRow()<0)return;auto* timeline=engine_.activeTimeline();if(!timeline||engine_.selectedClips().size()!=1)return;
        const auto clipId=engine_.selectedClips().front();const auto* track=timeline->findTrackContainingClip(clipId);if(!track)return;
        const auto effectId=effectStack->currentItem()->data(Qt::UserRole).toString().toStdString();const size_t row=static_cast<size_t>(effectStack->currentRow());
        if(!engine_.history().execute(factory(*timeline,track->id(),clipId,effectId,row)))return;
        engine_.project().setDirty(true);engine_.notifyTimelineChanged();refresh();
    };
    connect(moveEffectUp,&QPushButton::clicked,this,[executeEffectCommand](bool){executeEffectCommand([](editor::Timeline& timeline,const core::TrackId& track,const core::ClipId& clip,const std::string&,size_t row){return std::make_unique<editor::ReorderClipEffectsCommand>(timeline,track,clip,row,row-1);});});
    connect(moveEffectDown,&QPushButton::clicked,this,[executeEffectCommand](bool){executeEffectCommand([](editor::Timeline& timeline,const core::TrackId& track,const core::ClipId& clip,const std::string&,size_t row){return std::make_unique<editor::ReorderClipEffectsCommand>(timeline,track,clip,row,row+1);});});
    connect(toggleEffect,&QPushButton::clicked,this,[executeEffectCommand](bool){executeEffectCommand([](editor::Timeline& timeline,const core::TrackId& track,const core::ClipId& clip,const std::string& effectId,size_t){return std::make_unique<editor::ToggleClipEffectCommand>(timeline,track,clip,effectId);});});
    connect(removeEffect,&QPushButton::clicked,this,[executeEffectCommand](bool){executeEffectCommand([](editor::Timeline& timeline,const core::TrackId& track,const core::ClipId& clip,const std::string& effectId,size_t){return std::make_unique<editor::RemoveClipEffectCommand>(timeline,track,clip,effectId);});});
    auto bindEffectNumber=[this](QDoubleSpinBox* spin,const std::string& key,double fallback){
        readers_.push_back([this,spin,key,fallback](const editor::Clip& c){
            QSignalBlocker guard(spin);double value=c.getParam<double>("effect."+key,fallback);
            if(const auto* effect=selectedEffect(c,selectedEffectId_)){const auto it=effect->params.find(key);if(it!=effect->params.end()&&it->is_number())value=it->get<double>();}
            spin->setValue(value);
        });
    };
    auto bindEffectColor=[this](QPushButton* button,const std::string& key,const std::string& fallback){
        readers_.push_back([this,button,key,fallback](const editor::Clip& c){
            std::string value=c.getParam<std::string>("effect."+key,fallback);
            if(const auto* effect=selectedEffect(c,selectedEffectId_)){const auto it=effect->params.find(key);if(it!=effect->params.end()&&it->is_string())value=it->get<std::string>();}
            QColor color(QString::fromStdString(value));QPixmap swatch(16,16);swatch.fill(color);button->setIcon(QIcon(swatch));button->setText(color.name().mid(1).toUpper());button->setProperty("color",color);
        });
    };
    auto* blurIntensity=number(effects,"Cường độ mờ","effect.intensity",15,0,100,1,1.,true);bindEffectNumber(blurIntensity,"intensity",15);
    auto* blurIntensityRow=blurIntensity->parentWidget();
    readers_.push_back([this,blurIntensityRow](const editor::Clip& c){
        const auto* selected=selectedEffect(c,selectedEffectId_);const auto effectName=selected?effectDisplayName(selected->type):QString::fromStdString(c.getParam<std::string>("effect.name",""));
        blurIntensityRow->setVisible(effectName.contains("blur",Qt::CaseInsensitive)||effectName.contains("mờ",Qt::CaseInsensitive));
    });
    auto* sharpenAmount=number(effects,"Mức độ tăng nét","effect.amount",0.35,0,2,0.05,1.,true);bindEffectNumber(sharpenAmount,"amount",0.35);
    auto* sharpenAmountRow=sharpenAmount->parentWidget();
    readers_.push_back([this,sharpenAmountRow](const editor::Clip& c){
        const auto* selected=selectedEffect(c,selectedEffectId_);const auto effectName=selected?effectDisplayName(selected->type):QString::fromStdString(c.getParam<std::string>("effect.name",""));
        sharpenAmountRow->setVisible(effectName.contains("sharpen",Qt::CaseInsensitive)||effectName.contains("làm nét",Qt::CaseInsensitive));
    });
    auto* gradeBrightness=number(effects,"Độ sáng","effect.brightness",0,-1,1,0.01,1.,true);bindEffectNumber(gradeBrightness,"brightness",0);
    auto* gradeContrast=number(effects,"Độ tương phản","effect.contrast",0,-1,1,0.01,1.,true);bindEffectNumber(gradeContrast,"contrast",0);
    auto* gradeSaturation=number(effects,"Độ bão hòa","effect.saturation",0,-1,2,0.01,1.,true);bindEffectNumber(gradeSaturation,"saturation",0);
    auto* gradeExposure=number(effects,"Phơi sáng","effect.exposure",0,-4,4,0.1,1.,true);bindEffectNumber(gradeExposure,"exposure",0);
    auto* gradeTemperature=number(effects,"Nhiệt độ màu","effect.temperature",0,-1,1,0.01,1.,true);bindEffectNumber(gradeTemperature,"temperature",0);
    auto* gradeTint=number(effects,"Sắc độ","effect.tint",0,-1,1,0.01,1.,true);bindEffectNumber(gradeTint,"tint",0);
    auto* gradeHue=number(effects,"Dịch chuyển sắc độ","effect.hue",0,-180,180,1,1.,true);bindEffectNumber(gradeHue,"hue",0);
    auto* gradeGamma=number(effects,"Gamma","effect.gamma",1,0.1,3,0.01,1.,true);bindEffectNumber(gradeGamma,"gamma",1);
    const std::vector<QWidget*> colorGradeRows{
        gradeBrightness->parentWidget(), gradeContrast->parentWidget(), gradeSaturation->parentWidget(),
        gradeExposure->parentWidget(), gradeTemperature->parentWidget(), gradeTint->parentWidget(),
        gradeHue->parentWidget(), gradeGamma->parentWidget()};
    readers_.push_back([this,colorGradeRows](const editor::Clip& c){
        const auto* selected=selectedEffect(c,selectedEffectId_);const auto effectName=selected?effectDisplayName(selected->type):QString::fromStdString(c.getParam<std::string>("effect.name",""));
        const bool isColorGrading=effectName.contains("color grading",Qt::CaseInsensitive);
        for(auto* row:colorGradeRows)row->setVisible(isColorGrading);
    });
    auto* vignetteAmount=number(effects,"Độ tối viền","effect.amount",0.45,0,1,0.01,1.,true);bindEffectNumber(vignetteAmount,"amount",0.45);
    vignetteAmount->setObjectName("property_effect_vignette_amount");
    auto* vignetteSoftness=number(effects,"Độ mềm","effect.softness",0.55,0.01,1,0.01,1.,true);bindEffectNumber(vignetteSoftness,"softness",0.55);
    auto* vignetteRoundness=number(effects,"Độ tròn","effect.roundness",1.0,0,1,0.01,1.,true);bindEffectNumber(vignetteRoundness,"roundness",1.0);
    const std::vector<QWidget*> vignetteRows{
        vignetteAmount->parentWidget(),vignetteSoftness->parentWidget(),vignetteRoundness->parentWidget()};
    readers_.push_back([this,vignetteRows](const editor::Clip& c){
        const auto* selected=selectedEffect(c,selectedEffectId_);
        const auto effectName=selected?effectDisplayName(selected->type):QString::fromStdString(c.getParam<std::string>("effect.name",""));
        const bool isVignette=effectName.compare("Vignette",Qt::CaseInsensitive)==0;
        for(auto* row:vignetteRows)row->setVisible(isVignette);
    });
    auto* keyColor=color(effects,"Màu cần xóa","effect.keyColor","#00FF00");bindEffectColor(keyColor,"keyColor","#00FF00");
    auto* keyColorRow=keyColor->parentWidget();
    auto* similarity=number(effects,"Độ tương đồng","effect.similarity",0.4,0.01,1.0,0.01,1.,true);bindEffectNumber(similarity,"similarity",0.4);
    auto* similarityRow=similarity->parentWidget();
    auto* smoothness=number(effects,"Độ mềm cạnh","effect.smoothness",0.1,0.001,1.0,0.001,1.,true);bindEffectNumber(smoothness,"smoothness",0.1);
    auto* smoothnessRow=smoothness->parentWidget();
    auto* spill=number(effects,"Khử tràn màu","effect.spill",0.1,0.0,1.0,0.01,1.,true);bindEffectNumber(spill,"spill",0.1);
    auto* spillRow=spill->parentWidget();
    readers_.push_back([this,keyColorRow,similarityRow,smoothnessRow,spillRow](const editor::Clip& c){
        const auto* selected=selectedEffect(c,selectedEffectId_);const auto effectName=selected?effectDisplayName(selected->type):QString::fromStdString(c.getParam<std::string>("effect.name",""));
        const bool isChromaKey=effectName.contains("chroma",Qt::CaseInsensitive);
        keyColorRow->setVisible(isChromaKey);
        similarityRow->setVisible(isChromaKey);
        smoothnessRow->setVisible(isChromaKey);
        spillRow->setVisible(isChromaKey);
    });

    auto* animatedRefreshTimer=new QTimer(this);
    animatedRefreshTimer->setInterval(33);
    connect(animatedRefreshTimer,&QTimer::timeout,this,&PropertiesPanel::refreshAnimatedProperties);
    animatedRefreshTimer->start();
}
std::optional<core::TimelineTime> PropertiesPanel::localPlayheadTime(const editor::Clip& clip) const {
    const auto playhead=engine_.playback().currentTime();
    if(playhead<clip.startTime()||playhead>=clip.endTime())return std::nullopt;
    return playhead-clip.startTime();
}
std::optional<std::string> PropertiesPanel::animationPathForClip(const editor::Clip& clip,const std::string& property) const {
    if(property.rfind("effect.",0)==0) {
        const auto* effect=selectedEffect(clip,selectedEffectId_);
        if(!effect)return std::nullopt;
        return editor::EffectParamAnimationEngine::buildEffectParamPath(effect->id,property.substr(7));
    }
    return property;
}
void PropertiesPanel::setNumericPropertyValue(const std::string& property,double value) {
    if(isUpdatingUi_||engine_.selectedClips().size()!=1)return;
    auto* timeline=engine_.activeTimeline();if(!timeline)return;
    const auto clipId=engine_.selectedClips().front();const auto* clip=timeline->findClip(clipId);if(!clip)return;
    const auto localTime=localPlayheadTime(*clip);
    const auto propertyPath=animationPathForClip(*clip,property);
    const auto* channel=localTime&&propertyPath?clip->findAnimationChannel(*propertyPath):nullptr;
    if(localTime&&propertyPath&&channel&&!channel->empty()) {
        const auto* track=timeline->findTrackContainingClip(clipId);if(!track)return;
        std::unique_ptr<editor::EditorCommand> command;
        if(property.rfind("effect.",0)==0) {
            if(const auto* effect=selectedEffect(*clip,selectedEffectId_)) {
                command=std::make_unique<editor::UpsertEffectParamKeyframeCommand>(
                    *timeline,track->id(),clipId,effect->id,property.substr(7),*localTime,value);
            }
        } else {
            command=std::make_unique<editor::UpsertKeyframeCommand>(
                *timeline,track->id(),clipId,*propertyPath,*localTime,value);
        }
        if(command&&engine_.history().execute(std::move(command))) {
            engine_.project().setDirty(true);engine_.notifyTimelineChanged();
            lastAnimatedUiTimeTicks_=-1;refresh();
        }
        return;
    }
    patch({{property,value}});
}
void PropertiesPanel::toggleNumericKeyframe(const std::string& property,double value) {
    if(isUpdatingUi_||engine_.selectedClips().size()!=1)return;
    auto* timeline=engine_.activeTimeline();if(!timeline)return;
    const auto clipId=engine_.selectedClips().front();const auto* clip=timeline->findClip(clipId);if(!clip)return;
    const auto localTime=localPlayheadTime(*clip);if(!localTime)return;
    const auto propertyPath=animationPathForClip(*clip,property);if(!propertyPath)return;
    const auto* track=timeline->findTrackContainingClip(clipId);if(!track)return;
    const auto* channel=clip->findAnimationChannel(*propertyPath);
    const bool hasKeyframe=channel&&channel->findKeyframeAt(*localTime).has_value();
    std::unique_ptr<editor::EditorCommand> command;
    if(property.rfind("effect.",0)==0) {
        const auto* effect=selectedEffect(*clip,selectedEffectId_);if(!effect)return;
        if(hasKeyframe) {
            command=std::make_unique<editor::RemoveEffectParamKeyframeCommand>(
                *timeline,track->id(),clipId,effect->id,property.substr(7),*localTime);
        } else {
            command=std::make_unique<editor::UpsertEffectParamKeyframeCommand>(
                *timeline,track->id(),clipId,effect->id,property.substr(7),*localTime,value);
        }
    } else if(hasKeyframe) {
        command=std::make_unique<editor::RemoveKeyframeCommand>(
            *timeline,track->id(),clipId,*propertyPath,*localTime);
    } else {
        command=std::make_unique<editor::UpsertKeyframeCommand>(
            *timeline,track->id(),clipId,*propertyPath,*localTime,value);
    }
    if(command&&engine_.history().execute(std::move(command))) {
        engine_.project().setDirty(true);engine_.notifyTimelineChanged();
        lastAnimatedUiTimeTicks_=-1;refresh();
    }
}
void PropertiesPanel::refreshAnimatedProperties() {
    auto* timeline=engine_.activeTimeline();
    if(!timeline||engine_.selectedClips().size()!=1)return;
    const auto* clip=timeline->findClip(engine_.selectedClips().front());if(!clip)return;
    const auto now=engine_.playback().currentTime();
    const auto clipId=clip->id().str();
    if(lastAnimatedUiTimeTicks_==now.ticks()&&lastAnimatedUiClipId_==clipId)return;
    lastAnimatedUiTimeTicks_=now.ticks();lastAnimatedUiClipId_=clipId;
    const bool wasUpdating=isUpdatingUi_;isUpdatingUi_=true;
    for(const auto& read:animatedReaders_)read(*clip);
    isUpdatingUi_=wasUpdating;
}
void PropertiesPanel::selectTab(const QString& key) {
    if(!tabs_.count(key))return;
    activeTab_=key;
    if(auto* tl=engine_.activeTimeline();tl&&engine_.selectedClips().size()==1)
        if(auto* clip=tl->findClip(engine_.selectedClips().front()))rememberedTabs_[clip->type()]=key;
    const auto& pal=Theme::instance().palette();
    for(auto& [name,tab]:tabs_){
        tab.button->setChecked(name==key);tab.page->setVisible(name==key);
        tab.button->setIcon(UiIcons::get(static_cast<UiIcon>(tab.button->property("propertyIcon").toInt()),name==key?pal.primaryAccent:pal.textSecondary,18));
    }
}
void PropertiesPanel::refresh() {
    const auto& pal=Theme::instance().palette();
    const auto signature=pal.background.name()+pal.hover.name();
    if(themeSignature_!=signature) {
        themeSignature_=signature;
        auto* content=findChild<QWidget*>("propertyContent");
        content->setStyleSheet(QString("QDoubleSpinBox,QLineEdit,QTextEdit{background:%1;border:1px solid transparent;border-radius:10px;padding:4px 10px;min-width:0px;} QDoubleSpinBox:focus,QLineEdit:focus,QTextEdit:focus{border-color:%2;}").arg(pal.hover.name(),pal.primaryAccent.name()));
        rail_->setStyleSheet(QString("#propertiesTabRail{border-right:1px solid %1;}").arg(pal.border.name()));
        for(auto& [key,tab]:tabs_)tab.button->setStyleSheet(toolRailButtonStyle(pal));
        for(auto* caption:findChildren<QLabel*>())if(caption->property("propertyCaption").toBool())caption->setStyleSheet(QString("color:%1;font-size:12px;").arg(pal.textSecondary.name()));
        for(auto* color:findChildren<QPushButton*>())if(color->property("colorField").toBool())color->setStyleSheet(QString("text-align:left;background:%1;border-radius:9px;padding:0 8px;").arg(pal.hover.name()));
        auto iconColor=pal.textSecondary;iconColor.setAlphaF(.75);
        findChild<QLabel*>("emptyPropertiesIcon")->setPixmap(UiIcons::getPixmap(UiIcon::SlidersBox,iconColor,40));
        findChild<QLabel*>("emptyPropertiesDescription")->setStyleSheet(QString("font-size:12.64px;color:%1;").arg(pal.textSecondary.name()));
    }
    auto* tl=engine_.activeTimeline();const auto& selection=engine_.selectedClips();
    const auto* clip=tl&&selection.size()==1?tl->findClip(selection.front()):nullptr;
    multiSelection_->setText(QString("Đã chọn %1 phần tử").arg(selection.size()));
    multiSelection_->setVisible(selection.size()>1);
    emptyView_->setVisible(!clip&&selection.size()<2);inspectorView_->setVisible(clip);
    if(!clip)return;
    isUpdatingUi_=true;
    using T=editor::ClipType; std::vector<QString> visible;
    switch(clip->type()) {
    case T::Text:visible={"text","tts","transform","blending"};break;
    case T::Video:visible={"transform","audio","speed","blending","masks","effects"};break;
    case T::Audio:visible={"audio","speed"};break;
    case T::Graphic:visible={"graphic","transform","blending","masks","effects"};break;
    case T::Effect:visible={"effects"};break;
    case T::Sticker:visible={"transform","blending","effects"};break;
    default:visible={"transform","blending","masks","effects"};break;
    }
    if(clip->type()==T::Video&&!clip->getParam<bool>("hasAudio",true))visible.erase(std::remove(visible.begin(),visible.end(),QString("audio")),visible.end());
    auto* layout=static_cast<QVBoxLayout*>(rail_->layout());
    QString tabSignature;for(const auto& key:visible)tabSignature+=key+";";
    if(tabSignature!=visibleTabsSignature_){
        visibleTabsSignature_=tabSignature;
        for(auto& [key,tab]:tabs_)tab.button->hide();
        for(int i=0;i<static_cast<int>(visible.size());++i){auto* b=tabs_.at(visible[i]).button;layout->removeWidget(b);layout->insertWidget(i,b);b->show();}
    }
    QString tab=rememberedTabs_.count(clip->type())?rememberedTabs_[clip->type()]:visible.front();
    if(std::find(visible.begin(),visible.end(),tab)==visible.end())tab=visible.front();
    selectTab(tab);for(const auto& read:readers_)read(*clip);isUpdatingUi_=false;lastAnimatedUiTimeTicks_=-1;refreshAnimatedProperties();
}
void PropertiesPanel::patch(const nlohmann::json& values,std::optional<bool> muted) {
    if(isUpdatingUi_||engine_.selectedClips().size()!=1)return;
    auto* tl=engine_.activeTimeline();if(!tl)return;
    const auto id=engine_.selectedClips().front();const auto* clip=tl->findClip(id);if(!clip)return;
    bool onlyEffectParams=!values.empty();
    for(const auto& [key,value]:values.items()){(void)value;if(key.rfind("effect.",0)!=0){onlyEffectParams=false;break;}}
    if(onlyEffectParams) {
        if(const auto* effect=selectedEffect(*clip,selectedEffectId_)) {
            nlohmann::json effectPatch=nlohmann::json::object();
            for(const auto& [key,value]:values.items())effectPatch[key.substr(7)]=value;
            bool effectChanged=false;
            for(const auto& [key,value]:effectPatch.items()){const auto it=effect->params.find(key);if(it==effect->params.end()||*it!=value){effectChanged=true;break;}}
            if(!effectChanged)return;
            const auto* track=tl->findTrackContainingClip(id);if(!track)return;
            if(engine_.history().execute(std::make_unique<editor::UpdateClipEffectParamsCommand>(*tl,track->id(),id,effect->id,std::move(effectPatch)))){engine_.project().setDirty(true);engine_.notifyTimelineChanged();refresh();}
            return;
        }
    }
    bool changed=(muted&&*muted!=clip->isMuted())||(values.contains("volume")&&clip->getParam<std::string>("audio.volumeUnit","")!="db");
    for(const auto& [key,value]:values.items())if(!clip->hasParam(key)||clip->params()[key]!=value)changed=true;
    if(!changed)return;
    editor::ElementPatch update{};update.clipId=id;update.patchParams=values;update.muted=muted;
    if(values.contains("text.content"))update.name=values["text.content"].get<std::string>();
    if(values.contains("volume"))update.patchParams["audio.volumeUnit"]="db";
    engine_.history().execute(std::make_unique<editor::UpdateElementsCommand>(*tl,std::vector<editor::ElementPatch>{update}));
}
}
#endif
