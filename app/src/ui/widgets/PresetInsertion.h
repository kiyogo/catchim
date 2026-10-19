#pragma once
#include "editor/EditorEngine.h"
#include "media/MediaLibrary.h"
#include "media/probe/MediaProbe.h"
#include <QJsonObject>
#include <QPixmap>
#include <QDir>
#include <QFile>
#include <QRegularExpression>

namespace catchim::ui {
inline bool insertUiPreset(editor::EditorEngine& engine,media::MediaLibrary& library,const QJsonObject& preset,core::TimelineTime time) {
    const QString type=preset["type"].toString();
    const QString name=preset["name"].toString();
    if (type!="text" && type!="shape" && type!="flag") return false;
    editor::Clip clip(core::ClipId::generate(),type=="text"?editor::ClipType::Text:type=="shape"?editor::ClipType::Graphic:editor::ClipType::Image,name.toStdString(),time,core::TimelineTime::fromSeconds(5));
    if(type=="text") {
        clip.setParam("text.content",name.toStdString());clip.setParam("text.fontSize",15.0);clip.setParam("text.color",std::string("#ffffff"));
        clip.setParam("text.webLayout",true);clip.setParam("text.fontFamily",std::string("Arial"));clip.setParam("text.style",std::string("center"));
    } else if(type=="shape") {
        clip.setParam("graphic.shape",preset["shape"].toString().toStdString());clip.setParam("graphic.color",std::string("#ffffff"));
    } else {
        const QString code=preset["code"].toString().toLower();
        if(!QRegularExpression("^[a-z]{2}$").match(code).hasMatch())return false;
        QPixmap flag(":/web/flags/"+code+".svg");if(flag.isNull())return false;
        const QString cache=QDir::tempPath()+"/catchim_stickers";
        if(!QDir().mkpath(cache))return false;
        const QString file=cache+"/"+code+".png";
        if(!QFile::exists(file) && !flag.save(file,"PNG"))return false;
        auto asset=library.findAssetByPath(file.toStdString());
        if(!asset) {
            auto probe=media::MediaProbe::probe(file.toStdString());if(!probe.ok())return false;
            asset=probe.unwrap();library.addAsset(asset);
        }
        clip.setMediaId(asset->id());clip.setParam("sourceWidth",flag.width());clip.setParam("sourceHeight",flag.height());
    }
    engine.insertElement(std::move(clip),time);engine.project().setDirty(true);engine.notifyTimelineChanged();return true;
}
}
