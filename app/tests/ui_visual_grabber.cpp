#include <QApplication>
#include <QPixmap>
#include <iostream>

#include "app/Application.h"
#include "editor/EditorEngine.h"
#include "media/MediaLibrary.h"
#include "render/RenderEngine.h"
#include "ui/MainWindow.h"
#include "ui/theme/Theme.h"
#include "ui/projects/ProjectsHubDialog.h"
#include "ui/panels/assets/AssetsPanel.h"
#include "ui/export/ExportDialog.h"
#include "ui/widgets/WebComboBox.h"
#include "ui/widgets/PresetInsertion.h"
#include <QScrollArea>
#include <QScrollBar>
#include <QKeyEvent>
#include <QScreen>
#include <QPainter>
#include <QMenu>
#include <QComboBox>
#include <QAbstractItemView>
#include <QFontInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QHelpEvent>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    app.setStyle("Fusion");

    catchim::ui::Theme::instance().setTheme(catchim::ui::ThemeMode::Light);

    catchim::editor::EditorEngine editorEngine;
    catchim::media::MediaLibrary mediaLibrary;
    catchim::render::RenderEngine renderEngine(1920, 1080);

    catchim::ui::MainWindow mainWindow(editorEngine, mediaLibrary, renderEngine);
    mainWindow.resize(1913, 942);
    mainWindow.show();

    // Process events so layout and styles settle
    app.processEvents();

    if (app.arguments().contains("--screens")) {
        QString output="E:/DATA/source/catchim/app/qa/panels-parity/dpi-"+QString::number(app.devicePixelRatio());
        QDir().mkpath(output);
        bool passed=true;
        QJsonArray checks;
        auto check=[&](const QString& name,bool value){checks.append(QJsonObject{{"name",name},{"passed",value}});passed=value&&passed;};
        auto* nav=mainWindow.findChild<QListWidget*>("tabList");
        mainWindow.resize(1912,938); app.processEvents();
        auto save=[&](const QString& name){app.processEvents();app.processEvents();check("capture-"+name,mainWindow.grab().save(output+"/"+name+".png"));};
        auto popup=[&](const QString& objectName){
            auto* combo=mainWindow.findChild<QComboBox*>(objectName);
            auto* styled=dynamic_cast<catchim::ui::WebComboBox*>(combo);
            check("custom-select-"+objectName,styled!=nullptr);
            if (!styled) return;
            styled->showPopup(); app.processEvents();
            auto* surface=styled->popupWidget(); check("popup-visible-"+objectName,surface->isVisible());
            check("popup-image-"+objectName,surface->grab().save(output+"/"+objectName+"-popup.png"));
            auto image=mainWindow.grab(); QPainter painter(&image);
            surface->render(&painter,mainWindow.mapFromGlobal(surface->pos())); painter.end(); image.save(output+"/"+objectName+"-context.png");
            // Arrow navigation selects an item; Enter commits through the same path as a click.
            auto* list=surface->findChild<QListView*>(); const int initial=combo->currentIndex();
            QKeyEvent down(QEvent::KeyPress,Qt::Key_Down,Qt::NoModifier); QApplication::sendEvent(list,&down);
            QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier); QApplication::sendEvent(list,&enter);
            check("keyboard-select-"+objectName,combo->currentIndex()==std::min(initial+1,combo->count()-1) && !surface->isVisible());
            combo->setCurrentIndex(initial);
        };
        for (int i=0;i<nav->count();++i) {nav->setCurrentRow(i);save(QString("tab-%1").arg(i));}
        for(const QSize size:{QSize(1280,720),QSize(1366,768)}) {
            mainWindow.resize(size);app.processEvents();
            for(int i=0;i<nav->count();++i){nav->setCurrentRow(i);app.processEvents();check(QString("responsive-%1x%2-tab%3").arg(size.width()).arg(size.height()).arg(i),mainWindow.size()==size);}
            save(QString("settings-%1x%2").arg(size.width()).arg(size.height()));
            if(auto* settingsScroll=mainWindow.findChild<QScrollArea*>("settingsScroll"))check(QString("settings-no-horizontal-clip-%1").arg(size.width()),settingsScroll->widget()->width()<=settingsScroll->viewport()->width());
        }
        mainWindow.resize(1912,938);app.processEvents();
        QFile countriesResource(":/web/countries.ts");check("countries-resource",countriesResource.open(QIODevice::ReadOnly));
        const auto countryData=countriesResource.readAll();const int countriesStart=countryData.indexOf('[',countryData.indexOf('=',countryData.indexOf("export const COUNTRIES")));
        check("countries-parsed",QJsonDocument::fromJson(countryData.mid(countriesStart,countryData.lastIndexOf(']')-countriesStart+1)).array().size()>200);
        check("flag-resource",!QPixmap(":/web/flags/ad.svg").isNull());
        popup("settingsFps");
        auto* ratio=mainWindow.findChild<QPushButton*>("customCanvasButton"); check("custom-canvas-present",ratio!=nullptr);
        for (auto* button:mainWindow.findChildren<QPushButton*>()) {
            if (button->property("canvasWidth").toInt()!=1080 || button->property("canvasHeight").toInt()!=1920) continue;
            button->click(); check("portrait-updates-project",editorEngine.project().settings().canvasSize.width==1080 && editorEngine.project().settings().canvasSize.height==1920);
            save("settings-portrait");
        }
        for (auto* button:mainWindow.findChildren<QPushButton*>()) if(button->property("canvasWidth").toInt()==1920)button->click();
        for (auto* button:mainWindow.findChildren<QPushButton*>()) if(button->text()=="Nền") {button->click();save("settings-background");}
        nav->setCurrentRow(6);app.processEvents();popup("captionLanguage");
        const auto countBefore=editorEngine.activeTimeline()->allTracks().size();
        mainWindow.findChild<QPushButton*>("generateCaptions")->click();
        check("no-fake-captions",editorEngine.activeTimeline()->allTracks().size()==countBefore);
        save("captions-empty-status");
        nav->setCurrentRow(2); app.processEvents();
        mainWindow.findChild<QPushButton*>("textExtrasToggle")->click(); app.processEvents();
        for (auto* scroll:mainWindow.findChildren<QScrollArea*>()) if(scroll->isVisible())scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        save("text-tts");popup("ttsVoice");
        nav->setCurrentRow(0); app.processEvents(); popup("previewZoomCombo");
        for (const auto mode:{catchim::ui::ThemeMode::Light,catchim::ui::ThemeMode::Dark}) {
            catchim::ui::Theme::instance().setTheme(mode);
            catchim::ui::ExportDialog dialog(editorEngine,mediaLibrary,renderEngine,&mainWindow);
            dialog.show();app.processEvents();
            check("export-centered",std::abs(dialog.mapToGlobal(dialog.rect().center()).x()-mainWindow.mapToGlobal(mainWindow.rect().center()).x())<=1);
            dialog.grab().save(output+(mode==catchim::ui::ThemeMode::Light?"/export-light.png":"/export-dark.png"));
            auto context=mainWindow.grab();QPainter painter(&context);dialog.render(&painter,mainWindow.mapFromGlobal(dialog.pos()));painter.end();context.save(output+(mode==catchim::ui::ThemeMode::Light?"/export-light-context.png":"/export-dark-context.png"));
            auto* resolution=dialog.findChild<QComboBox*>("exportResolution");
            resolution->setCurrentIndex(resolution->findData("1080p"));
            check("export-resolution-data",dialog.findChild<QLabel*>("exportOutputSize")->text().contains("1920 x 1080"));
            for(const QString name:{"exportResolution","exportFormat","exportQuality","exportAudio"}){
                auto* combo=dynamic_cast<catchim::ui::WebComboBox*>(dialog.findChild<QComboBox*>(name));check("export-select-"+name,combo!=nullptr);
                if(combo){combo->showPopup();app.processEvents();combo->popupWidget()->grab().save(output+"/"+name+(mode==catchim::ui::ThemeMode::Light?"-light.png":"-dark.png"));combo->hidePopup();}
            }
            auto* card=dialog.findChild<QWidget*>("exportCard");
            auto pixel=card->grab().toImage().pixelColor(20,60);
            check(mode==catchim::ui::ThemeMode::Light?"export-light-background":"export-dark-background",mode==catchim::ui::ThemeMode::Light?pixel.lightness()>230:pixel.lightness()<60);
            dialog.close();
            QDialog ordinary(&mainWindow);ordinary.show();app.processEvents();
            check("ordinary-dialog-opaque",!ordinary.testAttribute(Qt::WA_TranslucentBackground)); ordinary.close();
        }
        catchim::ui::Theme::instance().setTheme(catchim::ui::ThemeMode::Light);
        nav->setCurrentRow(2);app.processEvents();
        auto countClips=[&]{int count=0;for(const auto* track:editorEngine.activeTimeline()->allTracks())count+=static_cast<int>(track->clips().size());return count;};
        const int before=countClips();
        mainWindow.findChild<QPushButton*>("defaultTextTile")->click();
        check("default-text-insert",countClips()==before+1 && editorEngine.canUndo());
        for(const auto* track:editorEngine.activeTimeline()->allTracks())for(const auto& clip:track->clips())if(clip.type()==catchim::editor::ClipType::Text)editorEngine.selectClip(clip.id());
        app.processEvents();save("text-inspector");
        check("undo-text-insert",editorEngine.undo() && countClips()==before);
        check("insert-flag",catchim::ui::insertUiPreset(editorEngine,mediaLibrary,QJsonObject{{"type","flag"},{"name","Andorra"},{"code","AD"}},catchim::core::TimelineTime::fromSeconds(0)));
        check("flag-has-media",!mediaLibrary.assets().empty());
        check("undo-flag-insert",editorEngine.undo() && countClips()==before);
        QJsonObject report{{"passed",passed},{"checks",checks}};
        QFile file(output+"/checks.json");if(file.open(QIODevice::WriteOnly))file.write(QJsonDocument(report).toJson());
        std::cout<<QJsonDocument(report).toJson().toStdString(); return passed?0:1;
    }

    if (app.arguments().contains("--parity")) {
        QString output = "E:/DATA/source/catchim/app/qa/editor-parity";
        if (app.devicePixelRatio() != 1) output += "/dpi-" + QString::number(app.devicePixelRatio());
        QDir().mkpath(output);
        QJsonArray captures;
        auto* nav = mainWindow.findChild<QListWidget*>("tabList");
        bool passed = nav && nav->count() == 9 && QFontInfo(app.font()).family().startsWith("Inter");
        const QList<QSize> viewports = app.devicePixelRatio() == 1
            ? QList<QSize>{QSize(1912, 938), QSize(1280, 720), QSize(1366, 768), QSize(1440, 900), QSize(1920, 1080), QSize(2560, 1440), QSize(3840, 2160)}
            : QList<QSize>{QSize(1912, 938), QSize(1280, 720)};
        for (const QSize size : viewports) {
            mainWindow.resize(size);
            app.processEvents();
            app.processEvents();
            QJsonObject capture;
            capture["width"] = mainWindow.width();
            capture["height"] = mainWindow.height();
            capture["font"] = QFontInfo(app.font()).family();
            capture["device_pixel_ratio"] = app.devicePixelRatio();
            QJsonObject widgets;
            for (const auto& name : {"assetsPanel", "previewPanel", "propertiesPanel", "timelinePanel", "mediaDropCard", "previewPlayBtn", "sceneSplitWidget", "previewZoomCombo"}) {
                if (auto* widget = mainWindow.findChild<QWidget*>(name)) {
                    const QPoint pos = widget->mapTo(&mainWindow, QPoint());
                    widgets[name] = QJsonArray{pos.x(), pos.y(), widget->width(), widget->height()};
                }
            }
            capture["widgets"] = widgets;
            auto* preview = mainWindow.findChild<QWidget*>("previewPanel");
            auto* play = mainWindow.findChild<QWidget*>("previewPlayBtn");
            auto* timeline = mainWindow.findChild<QWidget*>("timelinePanel");
            auto* scene = mainWindow.findChild<QWidget*>("sceneSplitWidget");
            if (preview && play && timeline && scene) {
                passed = std::abs(play->mapTo(preview, QPoint(play->width()/2, 0)).x() - preview->width()/2) <= 2 && passed;
                const auto* leftTools = timeline->findChild<QWidget*>("timelineLeftTools");
                const auto* rightTools = timeline->findChild<QWidget*>("timelineRightTools");
                const int sceneCenter = timeline->width()/2 + (leftTools->width() - rightTools->width())/2;
                passed = std::abs(scene->mapTo(timeline, QPoint(scene->width()/2, 0)).x() - sceneCenter) <= 2 && passed;
            } else passed = false;
            const QString imagePath = output + QString("/editor-%1x%2.png").arg(size.width()).arg(size.height());
            passed = mainWindow.size() == size && mainWindow.grab().save(imagePath) && passed;
            capture["image"] = imagePath;
            captures.append(capture);
        }
        mainWindow.resize(1912, 938);
        app.processEvents();
        bool foundTooltip = false;
        for (auto* button : mainWindow.findChildren<QPushButton*>()) {
            if (!button->toolTip().startsWith("Thêm dấu mốc")) continue;
            QHelpEvent hover(QEvent::ToolTip, button->rect().center(), button->mapToGlobal(button->rect().center()));
            QApplication::sendEvent(button, &hover);
            app.processEvents();
            for (auto* top : QApplication::topLevelWidgets()) {
                if (top->objectName() != "globalTooltipBubble" || !top->isVisible()) continue;
                foundTooltip = true;
                passed = top->geometry().bottom() - 6 <= button->mapToGlobal(QPoint()).y() && passed;
                auto composite = mainWindow.grab();
                QPainter painter(&composite);
                top->render(&painter, mainWindow.mapFromGlobal(top->pos()));
                painter.end();
                composite.save(output + "/tooltip-above.png");
                QEvent leave(QEvent::Leave);
                QApplication::sendEvent(button, &leave);
                passed = !top->isVisible() && passed;
            }
            button->click();
            passed = editorEngine.activeTimeline()->bookmarks().size() == 1 && button->isChecked() && passed;
            button->click();
            passed = editorEngine.activeTimeline()->bookmarks().empty() && !button->isChecked() && passed;
        }
        passed = foundTooltip && passed;
        if (nav) {
            nav->setCurrentRow(1);
            app.processEvents();
            const auto* stack = mainWindow.findChild<QStackedWidget*>();
            passed = stack && stack->currentIndex() == 1 && passed;
            nav->setCurrentRow(0);
        }
        if (auto* edit = mainWindow.findChild<QLineEdit*>("headerNameEdit")) {
            const auto storedName = editorEngine.project().name();
            QMetaObject::invokeMethod(edit, "editingFinished", Qt::DirectConnection);
            passed = editorEngine.project().name() == storedName && passed;
        } else passed = false;
        auto* splitter = mainWindow.findChild<QSplitter*>("horizontalSplitter");
        splitter->setSizes({500, 850, 500});
        const auto before = splitter->sizes();
        mainWindow.resize(1700, 900);
        app.processEvents();
        const auto after = splitter->sizes();
        const double oldTotal = before[0] + before[1] + before[2];
        const double newTotal = after[0] + after[1] + after[2];
        passed = std::abs(before[0]/oldTotal - after[0]/newTotal) < 0.015 && passed;
        QJsonObject report{{"passed", passed}, {"captures", captures}};
        QFile reportFile(output + "/geometry.json");
        if (reportFile.open(QIODevice::WriteOnly)) reportFile.write(QJsonDocument(report).toJson());
        std::cout << QJsonDocument(report).toJson().toStdString();
        return passed ? 0 : 1;
    }

    if (auto* ap = mainWindow.findChild<catchim::ui::AssetsPanel*>()) {
        ap->showTabTooltip(1); // Show "Text" tooltip matching 1.png
        app.processEvents();
    }

    std::cout << "MainWindow size: " << mainWindow.width() << "x" << mainWindow.height() << std::endl;
    for (auto* sp : mainWindow.findChildren<QSplitter*>()) {
        std::cout << "Splitter " << sp->objectName().toStdString() << " geo: "
                  << sp->x() << "," << sp->y() << " " << sp->width() << "x" << sp->height()
                  << " sizes: ";
        for (int s : sp->sizes()) std::cout << s << " ";
        std::cout << std::endl;
    }
    for (auto* w : mainWindow.findChildren<QWidget*>()) {
        if (w->property("class") == "Panel" || w->objectName().contains("Panel") || w->objectName().contains("editorBody")) {
            std::cout << "Widget " << w->objectName().toStdString() << " geo: "
                      << w->x() << "," << w->y() << " " << w->width() << "x" << w->height()
                      << " parent: " << (w->parentWidget() ? w->parentWidget()->objectName().toStdString() : "none")
                      << std::endl;
        }
    }

    QPixmap mainPix = mainWindow.grab();
    for (auto* topWidget : QApplication::topLevelWidgets()) {
        if (topWidget != &mainWindow && topWidget->isVisible()) {
            QPoint posInMain = mainWindow.mapFromGlobal(topWidget->pos());
            QPainter p(&mainPix);
            topWidget->render(&p, posInMain);
        }
    }
    bool s1 = mainPix.save("E:/DATA/source/catchim/app/screenshot_mainwindow.png");
    mainPix.save("E:/DATA/source/catchim/app/screenshot_mainwindow_light.png");
    std::cout << "MainWindow grab saved: " << (s1 ? "YES" : "NO") << " size: " << mainPix.width() << "x" << mainPix.height() << std::endl;

    // Grab Dropdown Menu
    auto* logoBtn = mainWindow.findChild<QPushButton*>("headerLogoBtn");
    if (logoBtn && logoBtn->menu()) {
        logoBtn->menu()->popup(logoBtn->mapToGlobal(QPoint(0, logoBtn->height() + 4)));
        app.processEvents();
        QPixmap menuPix = logoBtn->menu()->grab();
        menuPix.save("E:/DATA/source/catchim/app/screenshot_dropdown_menu.png");
        std::cout << "Dropdown menu grab saved: size " << menuPix.width() << "x" << menuPix.height() << std::endl;
        logoBtn->menu()->close();
        app.processEvents();
    }

    // Grab Sort Menu
    if (auto* ap = mainWindow.findChild<catchim::ui::AssetsPanel*>()) {
        for (auto* btn : ap->findChildren<QPushButton*>()) {
            if (btn->toolTip().contains("Sort", Qt::CaseInsensitive)) {
                btn->click();
                app.processEvents();
                for (auto* topW : QApplication::topLevelWidgets()) {
                    if (auto* m = qobject_cast<QMenu*>(topW)) {
                        QPixmap sortPix = m->grab();
                        sortPix.save("E:/DATA/source/catchim/app/screenshot_sort_menu.png");
                        std::cout << "Sort menu grab saved: size " << sortPix.width() << "x" << sortPix.height() << std::endl;
                        m->close();
                        app.processEvents();
                        break;
                    }
                }
                break;
            }
        }
    }

    // Grab ComboBox Popup
    auto* zoomCombo = mainWindow.findChild<QComboBox*>("previewZoomCombo");
    if (zoomCombo) {
        zoomCombo->showPopup();
        app.processEvents();
        if (auto* view = zoomCombo->view()) {
            QWidget* popupWin = view->window();
            if (popupWin) {
                QPixmap comboPix = popupWin->grab();
                comboPix.save("E:/DATA/source/catchim/app/screenshot_combobox_popup.png");
                std::cout << "ComboBox popup grab saved: size " << comboPix.width() << "x" << comboPix.height() << std::endl;
            }
        }
        zoomCombo->hidePopup();
        app.processEvents();
    }

    catchim::ui::ProjectsHubDialog hubDialog(&mainWindow);
    hubDialog.resize(1000, 680);
    hubDialog.show();
    app.processEvents();

    QPixmap hubPix = hubDialog.grab();
    bool s2 = hubPix.save("E:/DATA/source/catchim/app/screenshot_projectshub.png");
    std::cout << "ProjectsHub grab saved: " << (s2 ? "YES" : "NO") << " size: " << hubPix.width() << "x" << hubPix.height() << std::endl;

    return 0;
}
