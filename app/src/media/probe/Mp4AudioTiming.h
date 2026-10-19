#pragma once
#include <filesystem>
#include <fstream>
#include <array>
#include <cstdint>
#include <cstring>

namespace catchim::media {
struct Mp4AudioTiming {double mediaStartSeconds{0},presentationSeconds{0};};
// Only the common single, unit-rate audio edit is interpreted here. Complex
// edit lists are left untouched. All reads are bounded by their parent box.
inline Mp4AudioTiming readMp4AudioTiming(const std::filesystem::path& path) {
    struct Box {uint64_t start{0},payload{0},end{0};bool valid() const{return end>payload;}};
    std::ifstream file(path,std::ios::binary);if(!file)return {};
    file.seekg(0,std::ios::end);const auto length=file.tellg();if(length<8)return {};
    const uint64_t fileEnd=static_cast<uint64_t>(length);
    auto number=[&](uint64_t offset,int size)->uint64_t {
        std::array<unsigned char,8> data{};file.clear();file.seekg(offset);file.read(reinterpret_cast<char*>(data.data()),size);
        if(file.gcount()!=size)return 0;uint64_t value=0;for(int i=0;i<size;++i)value=(value<<8)|data[i];return value;
    };
    auto find=[&](uint64_t start,uint64_t end,const char* kind)->Box {
        while(start+8<=end) {
            uint64_t size=number(start,4),header=8;char type[4]{};file.read(type,4);
            if(size==1){if(start+16>end)return {};size=number(start+8,8);header=16;}
            if(size==0)size=end-start;
            if(size<header||size>end-start)return {};
            if(std::memcmp(type,kind,4)==0)return {start,start+header,start+size};start+=size;
        }return {};
    };
    const auto moov=find(0,fileEnd,"moov");if(!moov.valid())return {};
    auto timescale=[&](Box box)->uint64_t {
        if(!box.valid())return 0;const auto version=number(box.payload,1);const uint64_t offset=version==1?20:12;
        return box.payload+offset+4<=box.end?number(box.payload+offset,4):0;
    };
    const auto movieScale=timescale(find(moov.payload,moov.end,"mvhd"));if(!movieScale)return {};
    uint64_t position=moov.payload;
    while(position<moov.end) {
        const auto track=find(position,moov.end,"trak");if(!track.valid())break;position=track.end;
        const auto mdia=find(track.payload,track.end,"mdia");if(!mdia.valid())continue;
        const auto hdlr=find(mdia.payload,mdia.end,"hdlr");if(hdlr.payload+12>hdlr.end)continue;
        if(number(hdlr.payload+8,4)!=0x736f756e)continue; // 'soun'
        const auto audioScale=timescale(find(mdia.payload,mdia.end,"mdhd"));if(!audioScale)return {};
        const auto edts=find(track.payload,track.end,"edts");if(!edts.valid())return {};
        const auto elst=find(edts.payload,edts.end,"elst");if(elst.payload+20>elst.end)return {};
        const auto version=number(elst.payload,1);if(version>1||number(elst.payload+4,4)!=1)return {};
        const int size=version==1?8:4;if(elst.payload+8+size*2+4>elst.end)return {};
        const uint64_t duration=number(elst.payload+8,size),mediaTime=number(elst.payload+8+size,size);
        if(number(elst.payload+8+size*2,4)!=0x00010000||mediaTime>audioScale*60)return {};
        return {static_cast<double>(mediaTime)/audioScale,static_cast<double>(duration)/movieScale};
    }
    return {};
}
}
