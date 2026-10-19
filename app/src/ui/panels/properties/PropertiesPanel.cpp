#include "PropertiesPanel.h"
#if defined(HAVE_QT6)
#include "ui/theme/Theme.h"
#include "ui/widgets/WebComboBox.h"
#include "editor/history/commands/ElementCommands.h"
#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QColorDialog>
#include "ui/widgets/ColorPickerDialog.h"
#include "ui/widgets/SideToolButton.h"
#include <QLabel>
#include <QPainter>
#include <QSignalBlocker>
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
    scrollArea_->setWidget(content); row->addWidget(scrollArea_,1); root->addWidget(inspectorView_,1);
    content->setStyleSheet(QString("QDoubleSpinBox,QLineEdit,QTextEdit{background:%1;border:1px solid transparent;border-radius:10px;padding:4px 10px;min-width:0px;} QDoubleSpinBox:focus,QLineEdit:focus,QTextEdit:focus{border-color:%2;}").arg(pal.hover.name(),pal.primaryAccent.name()));
    auto field=[&](QVBoxLayout* layout,const QString& label,QWidget* control) {
        auto* wrapper=new QWidget;auto* fields=new QVBoxLayout(wrapper);fields->setContentsMargins(0,0,0,0);fields->setSpacing(8);
        if(!label.isEmpty()) { auto* caption=new QLabel(label);caption->setProperty("propertyCaption",true); caption->setStyleSheet(QString("color:%1;font-size:12px;").arg(pal.textSecondary.name())); fields->addWidget(caption); }
        fields->addWidget(control);layout->addWidget(wrapper);control->setMinimumWidth(0);
        control->setSizePolicy(QSizePolicy::Ignored,control->sizePolicy().verticalPolicy());
        wrapper->setMinimumWidth(0);wrapper->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    };
    auto number=[&](QVBoxLayout* layout,const QString& label,const char* key,double def,double min,double max,double step,double multiplier=1.) {
        auto* spin=new QDoubleSpinBox; spin->setObjectName(QString("property_")+key); spin->setRange(min,max); spin->setSingleStep(step);
        spin->setDecimals(step<1?2:0); spin->setButtonSymbols(QAbstractSpinBox::NoButtons); spin->setFixedHeight(28); spin->setKeyboardTracking(false);
        field(layout,label,spin);
        readers_.push_back([spin,key=std::string(key),def,multiplier](const editor::Clip& clip){
            QSignalBlocker guard(spin);double value=clip.getParam<double>(key,def);
            if(key=="volume"&&clip.getParam<std::string>("audio.volumeUnit","")!="db"&&value>0&&value<=2)value=20*std::log10(value);
            spin->setValue(value*multiplier);
        });
        connect(spin,&QDoubleSpinBox::valueChanged,this,[this,key=std::string(key),multiplier](double value) { patch({{key,value/multiplier}}); });
    };
    auto select=[&](QVBoxLayout* layout,const QString& label,const char* key,const QStringList& names,const QStringList& values,const QString& def) {
        auto* combo=new WebComboBox; combo->setObjectName(QString("property_")+key); combo->setFixedHeight(32); combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon); combo->setMinimumContentsLength(1);
        for(int i=0;i<names.size();++i)combo->addItem(names[i],values[i]); field(layout,label,combo);
        readers_.push_back([combo,key=std::string(key),def](const editor::Clip& clip){QSignalBlocker guard(combo); combo->setCurrentIndex(combo->findData(QString::fromStdString(clip.getParam<std::string>(key,def.toStdString()))));});
        connect(combo,&QComboBox::currentIndexChanged,this,[this,combo,key=std::string(key)](int i){if(i>=0)patch({{key,combo->currentData().toString().toStdString()}});});
    };
    auto color=[&](QVBoxLayout* layout,const QString& label,const char* key,const char* def) {
        auto* button=new QPushButton; button->setObjectName(QString("property_")+key); button->setFixedHeight(30); button->setStyleSheet(QString("text-align:left;background:%1;border-radius:9px;padding:0 8px;").arg(pal.hover.name()));
        button->setProperty("colorField",true);
        field(layout,label,button);
        readers_.push_back([button,key=std::string(key),def=std::string(def)](const editor::Clip& clip) { QColor c(QString::fromStdString(clip.getParam<std::string>(key,def))); QPixmap swatch(16,16); swatch.fill(c);button->setIcon(QIcon(swatch)); button->setText(c.name().mid(1).toUpper()); button->setProperty("color",c); });
        connect(button,&QPushButton::clicked,this,[this,button,key=std::string(key)] {auto c=chooseEditorColor(button->property("color").value<QColor>(),this); if(c.isValid())patch({{key,c.name().toStdString()}});});
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
    color(text,"Màu","text.color","#FFFFFF");
    select(text,"Căn chỉnh văn bản","text.textAlign",{"Trái","Giữa","Phải"},{"left","center","right"},"center");
    select(text,"Độ đậm","text.fontWeight",{"Bình thường","Đậm"},{"normal","bold"},"normal");
    select(text,"Kiểu chữ","text.fontStyle",{"Bình thường","Nghiêng"},{"normal","italic"},"normal");
    select(text,"Trang trí","text.textDecoration",{"Không","Gạch chân","Gạch ngang"},{"none","underline","line-through"},"none");
    auto* tts=addTab("tts","Chuyển văn bản thành giọng nói",UiIcon::Audio);
    auto* ttsInfo=new QLabel("Chưa kết nối bộ tổng hợp giọng nói. Không tạo âm thanh giả thay lời đọc."); ttsInfo->setWordWrap(true);tts->addWidget(ttsInfo);
    auto* transform=addTab("transform","Biến đổi",UiIcon::Transform);
    number(transform,"Vị trí X","transform.positionX",0,-100000,100000,1);
    number(transform,"Vị trí Y","transform.positionY",0,-100000,100000,1);
    number(transform,"Tỷ lệ X","transform.scaleX",1,.01,10000,.1,100);
    number(transform,"Tỷ lệ Y","transform.scaleY",1,.01,10000,.1,100);
    number(transform,"Xoay","transform.rotate",0,-360,360,1);
    auto* audio=addTab("audio","Âm thanh",UiIcon::Music);
    number(audio,"Âm lượng","volume",0,-60,12,.1);
    auto* mute=new ToggleSwitch(content);mute->setObjectName("property_muted");field(audio,"Tắt tiếng",mute);
    readers_.push_back([mute](const editor::Clip& c){QSignalBlocker guard(mute);mute->setChecked(c.isMuted()||c.getParam<bool>("muted",false));});
    connect(mute,&QCheckBox::toggled,this,[this](bool v){patch({{"muted",v}},v);});
    auto* speed=addTab("speed","Tốc độ",UiIcon::Speed);
    number(speed,"Tốc độ phát","speed",1,.1,10,.1);
    auto* reverse=new ToggleSwitch(content); field(speed,"Phát ngược",reverse);
    readers_.push_back([reverse](const editor::Clip& c){QSignalBlocker guard(reverse);reverse->setChecked(c.getParam<bool>("reversed",false));});
    connect(reverse,&QCheckBox::toggled,this,[this](bool v){patch({{"reversed",v}});});
    auto* blend=addTab("blending","Hòa trộn",UiIcon::Drop);
    number(blend,"Độ mờ","opacity",1,0,100,1,100);
    select(blend,"Chế độ hòa trộn","transform.blendModeName",{"Bình thường","Nhân","Màn hình","Phủ","Làm tối","Làm sáng","Né màu"},{"normal","multiply","screen","overlay","darken","lighten","color_dodge"},"normal");
    auto* graphic=addTab("graphic","Hình dạng",UiIcon::ShapeRect);
    select(graphic,"Hình dạng","graphic.shape",{"Chữ nhật","Tròn","Ngũ giác","Ngôi sao","Mũi tên"},{"rectangle","circle","polygon","star","arrow"},"rectangle");
    color(graphic,"Màu tô","graphic.color","#FFFFFF");
    number(graphic,"Bo góc","graphic.cornerRadius",0,0,200,1);
    number(graphic,"Độ dày viền","graphic.strokeWidth",0,0,100,1);
    auto* masks=addTab("masks","Mặt nạ",UiIcon::Layers);
    auto* maskInfo=new QLabel("Bộ dựng C++ chưa hỗ trợ mặt nạ. Các dữ liệu mặt nạ hiện có vẫn được giữ nguyên.");maskInfo->setWordWrap(true);masks->addWidget(maskInfo);
    auto* effects=addTab("effects","Hiệu ứng",UiIcon::Effects);
    auto* effectInfo=new QLabel;effectInfo->setWordWrap(true);effects->addWidget(effectInfo);
    readers_.push_back([effectInfo](const editor::Clip& c){effectInfo->setText(QString("Hiệu ứng: %1\nChuyển cảnh: %2").arg(QString::fromStdString(c.getParam<std::string>("effect.name","Không có")),QString::fromStdString(c.getParam<std::string>("transition.type","Không có"))));});
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
    selectTab(tab);for(const auto& read:readers_)read(*clip);isUpdatingUi_=false;
}
void PropertiesPanel::patch(const nlohmann::json& values,std::optional<bool> muted) {
    if(isUpdatingUi_||engine_.selectedClips().size()!=1)return;
    auto* tl=engine_.activeTimeline();if(!tl)return;
    const auto id=engine_.selectedClips().front();const auto* clip=tl->findClip(id);if(!clip)return;
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
