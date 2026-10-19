#include "MediaProbe.h"
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <functional>

#if defined(_WIN32)
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#endif

namespace catchim::media {

namespace {

std::string toLower(std::string_view sv) {
    std::string s(sv);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

uint32_t readBigEndianU32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
           (static_cast<uint32_t>(p[3]));
}

uint64_t readBigEndianU64(const uint8_t* p) {
    return (static_cast<uint64_t>(readBigEndianU32(p)) << 32) |
           (static_cast<uint64_t>(readBigEndianU32(p + 4)));
}

uint32_t readLittleEndianU32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0])) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t readLittleEndianU16(const uint8_t* p) {
    return (static_cast<uint16_t>(p[0])) |
           (static_cast<uint16_t>(p[1]) << 8);
}

// 1. Native MP4 / MOV Box Parser
bool probeMp4(const std::filesystem::path& path, double& outDuration, int& outWidth, int& outHeight, double& outFps,bool& outHasVideo,bool& outHasAudio) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    int64_t fileSize = static_cast<int64_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    if (fileSize < 16) return false;

    bool foundMoov = false;
    double durationSec = 0.0;
    int videoWidth = 0;
    int videoHeight = 0;
    double fps = 30.0;
    bool videoStream=false,audioStream=false;

    int64_t pos = 0;
    while (pos + 8 <= fileSize) {
        file.seekg(static_cast<std::streamoff>(pos));
        uint8_t header[8];
        file.read(reinterpret_cast<char*>(header), 8);
        if (file.gcount() < 8) break;

        uint64_t boxSize = readBigEndianU32(header);
        char boxType[5] = {0};
        std::memcpy(boxType, header + 4, 4);

        uint64_t headerSize = 8;
        if (boxSize == 1) {
            uint8_t extSize[8];
            file.read(reinterpret_cast<char*>(extSize), 8);
            if (file.gcount() < 8) break;
            boxSize = readBigEndianU64(extSize);
            headerSize = 16;
        } else if (boxSize == 0) {
            boxSize = static_cast<uint64_t>(fileSize - pos);
        }

        if (boxSize < headerSize || pos + static_cast<int64_t>(boxSize) > fileSize) {
            break;
        }

        if (std::strcmp(boxType, "moov") == 0) {
            foundMoov = true;
            uint64_t payloadSize = boxSize - headerSize;
            if (payloadSize > 0 && payloadSize < 64 * 1024 * 1024) {
                std::vector<uint8_t> moovData(static_cast<size_t>(payloadSize));
                file.read(reinterpret_cast<char*>(moovData.data()), static_cast<std::streamsize>(payloadSize));
                if (file.gcount() == static_cast<std::streamsize>(payloadSize)) {
                    size_t mPos = 0;
                    while (mPos + 8 <= moovData.size()) {
                        uint32_t subSize = readBigEndianU32(moovData.data() + mPos);
                        if (subSize < 8 || mPos + subSize > moovData.size()) break;
                        char subType[5] = {0};
                        std::memcpy(subType, moovData.data() + mPos + 4, 4);

                        if (std::strcmp(subType, "mvhd") == 0 && subSize >= 24) {
                            uint8_t version = moovData[mPos + 8];
                            if (version == 0 && subSize >= 28) {
                                uint32_t timeScale = readBigEndianU32(moovData.data() + mPos + 20);
                                uint32_t duration = readBigEndianU32(moovData.data() + mPos + 24);
                                if (timeScale > 0) {
                                    durationSec = static_cast<double>(duration) / timeScale;
                                }
                            } else if (version == 1 && subSize >= 40) {
                                uint32_t timeScale = readBigEndianU32(moovData.data() + mPos + 28);
                                uint64_t duration = readBigEndianU64(moovData.data() + mPos + 32);
                                if (timeScale > 0) {
                                    durationSec = static_cast<double>(duration) / timeScale;
                                }
                            }
                        } else if (std::strcmp(subType, "trak") == 0) {
                            bool trackVideo=false,trackAudio=false;uint32_t mediaScale=0;
                            uint64_t sampleCount=0,sampleTicks=0;
                            std::function<void(size_t,size_t)> readMediaBoxes=[&](size_t begin,size_t end){
                                for(size_t cursor=begin;cursor+8<=end;){
                                    const uint32_t size=readBigEndianU32(moovData.data()+cursor);
                                    if(size<8||size>end-cursor)break;
                                    const auto* type=moovData.data()+cursor+4;
                                    if(!std::memcmp(type,"mdia",4)||!std::memcmp(type,"minf",4)||!std::memcmp(type,"stbl",4))readMediaBoxes(cursor+8,cursor+size);
                                    else if(!std::memcmp(type,"hdlr",4)&&size>=20){trackVideo|=!std::memcmp(moovData.data()+cursor+16,"vide",4);trackAudio|=!std::memcmp(moovData.data()+cursor+16,"soun",4);}
                                    else if(!std::memcmp(type,"mdhd",4)&&size>=28){const size_t offset=moovData[cursor+8]?28:20;if(offset+4<=size)mediaScale=readBigEndianU32(moovData.data()+cursor+offset);}
                                    else if(!std::memcmp(type,"stts",4)&&size>=16){
                                        const auto entries=readBigEndianU32(moovData.data()+cursor+12);
                                        for(uint32_t entry=0;entry<entries&&16+uint64_t(entry)*8+8<=size;++entry){
                                            const auto* pair=moovData.data()+cursor+16+size_t(entry)*8;
                                            const uint64_t count=readBigEndianU32(pair),delta=readBigEndianU32(pair+4);sampleCount+=count;sampleTicks+=count*delta;
                                        }
                                    }
                                    cursor+=size;
                                }
                            };
                            readMediaBoxes(mPos+8,mPos+subSize);
                            videoStream|=trackVideo;audioStream|=trackAudio;
                            if(trackVideo&&mediaScale&&sampleCount&&sampleTicks)fps=double(sampleCount)*mediaScale/sampleTicks;
                            size_t tPos = mPos + 8;
                            size_t tEnd = mPos + subSize;
                            while (tPos + 8 <= tEnd) {
                                uint32_t trackSubSize = readBigEndianU32(moovData.data() + tPos);
                                if (trackSubSize < 8 || tPos + trackSubSize > tEnd) break;
                                char trackSubType[5] = {0};
                                std::memcpy(trackSubType, moovData.data() + tPos + 4, 4);

                                if (std::strcmp(trackSubType, "tkhd") == 0 && trackSubSize >= 84) {
                                    uint8_t tVersion = moovData[tPos + 8];
                                    size_t dimOffset = (tVersion == 0) ? (tPos + trackSubSize - 8) : (tPos + trackSubSize - 8);
                                    if (dimOffset + 8 <= tPos + trackSubSize) {
                                        uint32_t w = readBigEndianU32(moovData.data() + dimOffset) >> 16;
                                        uint32_t h = readBigEndianU32(moovData.data() + dimOffset + 4) >> 16;
                                        if (w > 0 && h > 0 && videoWidth == 0) {
                                            videoWidth = static_cast<int>(w);
                                            videoHeight = static_cast<int>(h);
                                        }
                                    }
                                }
                                tPos += trackSubSize;
                            }
                        }
                        mPos += subSize;
                    }
                }
            }
            break;
        }

        pos += static_cast<int64_t>(boxSize);
    }

    if (foundMoov && durationSec > 0.0) {
        outDuration = durationSec;
        outWidth = (videoWidth > 0) ? videoWidth : 1920;
        outHeight = (videoHeight > 0) ? videoHeight : 1080;
        outFps = fps;
        outHasVideo=videoStream||videoWidth>0;outHasAudio=audioStream;
        return true;
    }
    return false;
}

// 2. Native RIFF WAV Parser
bool probeWav(const std::filesystem::path& path, double& outDuration, int& outChannels, int& outSampleRate) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    uint8_t header[12];
    file.read(reinterpret_cast<char*>(header), 12);
    if (file.gcount() < 12) return false;

    if (std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0) {
        return false;
    }

    uint32_t byteRate = 0;
    uint32_t dataSize = 0;
    int channels = 2;
    int sampleRate = 44100;

    while (file.good()) {
        uint8_t chunkHeader[8];
        file.read(reinterpret_cast<char*>(chunkHeader), 8);
        if (file.gcount() < 8) break;

        char chunkId[5] = {0};
        std::memcpy(chunkId, chunkHeader, 4);
        uint32_t chunkSize = readLittleEndianU32(chunkHeader + 4);

        if (std::strcmp(chunkId, "fmt ") == 0 && chunkSize >= 16) {
            std::vector<uint8_t> fmtData(chunkSize);
            file.read(reinterpret_cast<char*>(fmtData.data()), chunkSize);
            if (file.gcount() == static_cast<std::streamsize>(chunkSize)) {
                channels = readLittleEndianU16(fmtData.data() + 2);
                sampleRate = readLittleEndianU32(fmtData.data() + 4);
                byteRate = readLittleEndianU32(fmtData.data() + 8);
            }
        } else if (std::strcmp(chunkId, "data") == 0) {
            dataSize = chunkSize;
            break;
        } else {
            file.seekg(chunkSize, std::ios::cur);
        }
    }

    if (byteRate > 0 && dataSize > 0) {
        outDuration = static_cast<double>(dataSize) / byteRate;
        outChannels = channels;
        outSampleRate = sampleRate;
        return true;
    }
    return false;
}

// 3. Native MP3 Parser
bool probeMp3(const std::filesystem::path& path, double& outDuration) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    int64_t fileSize = static_cast<int64_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    if (fileSize < 128) return false;

    int64_t startPos = 0;
    uint8_t id3[10];
    file.read(reinterpret_cast<char*>(id3), 10);
    if (file.gcount() == 10 && std::memcmp(id3, "ID3", 3) == 0) {
        uint32_t tagSize = ((id3[6] & 0x7F) << 21) |
                           ((id3[7] & 0x7F) << 14) |
                           ((id3[8] & 0x7F) << 7)  |
                           (id3[9] & 0x7F);
        startPos = 10 + static_cast<int64_t>(tagSize);
    }

    double bitrateKbps = 128.0;
    int64_t audioDataSize = fileSize - startPos;
    if (audioDataSize > 0) {
        outDuration = (static_cast<double>(audioDataSize) * 8.0) / (bitrateKbps * 1000.0);
        return true;
    }
    return false;
}

// 4. Native Image Dimension Parsers (PNG, JPEG, BMP)
bool probeImage(const std::filesystem::path& path, int& outWidth, int& outHeight) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    uint8_t header[32];
    file.read(reinterpret_cast<char*>(header), 32);
    if (file.gcount() < 16) return false;

    if (header[0] == 0x89 && header[1] == 'P' && header[2] == 'N' && header[3] == 'G') {
        outWidth = static_cast<int>(readBigEndianU32(header + 16));
        outHeight = static_cast<int>(readBigEndianU32(header + 20));
        return (outWidth > 0 && outHeight > 0);
    }

    if (header[0] == 'B' && header[1] == 'M') {
        outWidth = static_cast<int>(readLittleEndianU32(header + 18));
        outHeight = static_cast<int>(readLittleEndianU32(header + 22));
        return (outWidth > 0 && outHeight > 0);
    }

    if (header[0] == 0xFF && header[1] == 0xD8) {
        file.seekg(2);
        while (file.good()) {
            uint8_t marker[4];
            file.read(reinterpret_cast<char*>(marker), 4);
            if (file.gcount() < 4) break;
            if (marker[0] != 0xFF) break;

            uint8_t m = marker[1];
            uint16_t len = (static_cast<uint16_t>(marker[2]) << 8) | marker[3];

            if ((m >= 0xC0 && m <= 0xC3) || (m >= 0xC5 && m <= 0xC7) ||
                (m >= 0xC9 && m <= 0xCB) || (m >= 0xCD && m <= 0xCF)) {
                uint8_t sof[5];
                file.read(reinterpret_cast<char*>(sof), 5);
                if (file.gcount() == 5) {
                    outHeight = (static_cast<int>(sof[1]) << 8) | sof[2];
                    outWidth = (static_cast<int>(sof[3]) << 8) | sof[4];
                    return (outWidth > 0 && outHeight > 0);
                }
                break;
            } else {
                if (len < 2) break;
                file.seekg(len - 2, std::ios::cur);
            }
        }
    }

    return false;
}

#if defined(_WIN32)
// 5. Windows Media Foundation Generic Probe Fallback
bool probeWindowsMediaFoundation(const std::filesystem::path& path, double& outDuration, int& outWidth, int& outHeight, double& outFps, bool& hasVideo, bool& hasAudio) {
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) return false;

    std::wstring wpath = path.wstring();
    for (auto& c : wpath) {
        if (c == L'/') c = L'\\';
    }

    bool success = false;

    // Try IMFSourceReader first
    IMFSourceReader* pReader = nullptr;
    hr = MFCreateSourceReaderFromURL(wpath.c_str(), nullptr, &pReader);
    if (SUCCEEDED(hr) && pReader) {
        PROPVARIANT var;
        PropVariantInit(&var);
        if (SUCCEEDED(pReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var))) {
            if (var.vt == VT_UI8 && var.uhVal.QuadPart > 0) {
                outDuration = static_cast<double>(var.uhVal.QuadPart) / 10000000.0;
                success = true;
            }
            PropVariantClear(&var);
        }

        IMFMediaType* pVideoType = nullptr;
        if (SUCCEEDED(pReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &pVideoType)) && pVideoType) {
            hasVideo = true;
            UINT32 w = 0, h = 0;
            if (SUCCEEDED(MFGetAttributeSize(pVideoType, MF_MT_FRAME_SIZE, &w, &h)) && w > 0 && h > 0) {
                outWidth = static_cast<int>(w);
                outHeight = static_cast<int>(h);
            }
            UINT32 num = 0, den = 0;
            if (SUCCEEDED(MFGetAttributeRatio(pVideoType, MF_MT_FRAME_RATE, &num, &den)) && den > 0) {
                outFps = static_cast<double>(num) / den;
            }
            pVideoType->Release();
        }

        IMFMediaType* pAudioType = nullptr;
        if (SUCCEEDED(pReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &pAudioType)) && pAudioType) {
            hasAudio = true;
            pAudioType->Release();
        }

        pReader->Release();
    }

    if (!success) {
        // Fallback: MFCreateSourceResolver
        IMFSourceResolver* pSourceResolver = nullptr;
        IUnknown* pSource = nullptr;
        IMFMediaSource* pMediaSource = nullptr;
        IMFPresentationDescriptor* pPD = nullptr;

        hr = MFCreateSourceResolver(&pSourceResolver);
        if (SUCCEEDED(hr)) {
            MF_OBJECT_TYPE ObjectType = MF_OBJECT_INVALID;
            hr = pSourceResolver->CreateObjectFromURL(
                wpath.c_str(),
                MF_RESOLUTION_MEDIASOURCE,
                nullptr,
                &ObjectType,
                &pSource
            );

            if (SUCCEEDED(hr) && pSource) {
                hr = pSource->QueryInterface(IID_PPV_ARGS(&pMediaSource));
                if (SUCCEEDED(hr) && pMediaSource) {
                    hr = pMediaSource->CreatePresentationDescriptor(&pPD);
                    if (SUCCEEDED(hr) && pPD) {
                        UINT64 duration100ns = 0;
                        hr = pPD->GetUINT64(MF_PD_DURATION, &duration100ns);
                        if (SUCCEEDED(hr) && duration100ns > 0) {
                            outDuration = static_cast<double>(duration100ns) / 10000000.0;
                            success = true;
                        }

                        DWORD streamCount = 0;
                        pPD->GetStreamDescriptorCount(&streamCount);
                        for (DWORD i = 0; i < streamCount; ++i) {
                            BOOL selected = FALSE;
                            IMFStreamDescriptor* pSD = nullptr;
                            if (SUCCEEDED(pPD->GetStreamDescriptorByIndex(i, &selected, &pSD)) && pSD) {
                                IMFMediaTypeHandler* pHandler = nullptr;
                                if (SUCCEEDED(pSD->GetMediaTypeHandler(&pHandler)) && pHandler) {
                                    GUID majorType = GUID_NULL;
                                    pHandler->GetMajorType(&majorType);
                                    if (majorType == MFMediaType_Video) {
                                        hasVideo = true;
                                        IMFMediaType* pMediaType = nullptr;
                                        if (SUCCEEDED(pHandler->GetCurrentMediaType(&pMediaType)) && pMediaType) {
                                            UINT32 w = 0, h = 0;
                                            if (SUCCEEDED(MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &w, &h))) {
                                                if (w > 0 && h > 0) {
                                                    outWidth = static_cast<int>(w);
                                                    outHeight = static_cast<int>(h);
                                                }
                                            }
                                            UINT32 num = 0, den = 0;
                                            if (SUCCEEDED(MFGetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, &num, &den)) && den > 0) {
                                                outFps = static_cast<double>(num) / den;
                                            }
                                            pMediaType->Release();
                                        }
                                    } else if (majorType == MFMediaType_Audio) {
                                        hasAudio = true;
                                    }
                                    pHandler->Release();
                                }
                                pSD->Release();
                            }
                        }
                        pPD->Release();
                    }
                    pMediaSource->Release();
                }
                pSource->Release();
            }
            pSourceResolver->Release();
        }
    }

    MFShutdown();
    return success;
}
#endif

} // namespace

MediaType MediaProbe::detectType(const std::filesystem::path& path) {
    std::string ext = toLower(path.extension().string());
    if (ext == ".mp4" || ext == ".mov" || ext == ".mkv" || ext == ".webm" ||
        ext == ".avi" || ext == ".m4v" || ext == ".wmv" || ext == ".flv" || ext == ".3gp") {
        return MediaType::Video;
    }
    if (ext == ".mp3" || ext == ".wav" || ext == ".aac" || ext == ".m4a" ||
        ext == ".flac" || ext == ".ogg" || ext == ".wma" || ext == ".opus") {
        return MediaType::Audio;
    }
    return MediaType::Image;
}

core::Result<std::shared_ptr<MediaAsset>> MediaProbe::probe(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec)) {
        return core::Result<std::shared_ptr<MediaAsset>>(core::ErrorCode::FileNotFound, "Media file does not exist");
    }

    MediaType type = detectType(path);
    auto asset = std::make_shared<MediaAsset>(core::MediaId::generate(), path, type);

    std::string ext = toLower(path.extension().string());

    double durationSec = 0.0;
    int width = 1920;
    int height = 1080;
    double fps = 30.0;
    bool hasVideo = (type == MediaType::Video);
    bool hasAudio = (type == MediaType::Audio || type == MediaType::Video);

    bool probed = false;

    // 1. Try native container parsers first (fastest, direct)
    if (ext == ".mp4" || ext == ".mov" || ext == ".m4v" || ext == ".3gp") {
        probed = probeMp4(path, durationSec, width, height, fps,hasVideo,hasAudio);
    } else if (ext == ".wav") {
        int ch = 2, sr = 44100;
        probed = probeWav(path, durationSec, ch, sr);
        hasVideo = false;
        hasAudio = true;
    } else if (ext == ".mp3") {
        probed = probeMp3(path, durationSec);
        hasVideo = false;
        hasAudio = true;
    } else if (type == MediaType::Image) {
        int imgW = 0, imgH = 0;
        if (probeImage(path, imgW, imgH)) {
            width = imgW;
            height = imgH;
        }
        durationSec = 5.0; // Default still image duration
        hasVideo = false;
        hasAudio = false;
        probed = true;
    }

#if defined(_WIN32)
    // 2. If native probe didn't resolve duration, use Windows Media Foundation
    if (!probed || durationSec <= 0.0) {
        double mfDur = 0.0;
        int mfW = 0, mfH = 0;
        double mfFps = 30.0;
        bool mfHasV = false, mfHasA = false;
        if (probeWindowsMediaFoundation(path, mfDur, mfW, mfH, mfFps, mfHasV, mfHasA)) {
            if (mfDur > 0.0) durationSec = mfDur;
            if (mfW > 0 && mfH > 0) { width = mfW; height = mfH; }
            if (mfFps > 0.0) fps = mfFps;
            if (mfHasV) hasVideo = true;
            if (mfHasA) hasAudio = true;
            probed = true;
        }
    }
#endif

    // Fallbacks if duration is still unprobed
    if (durationSec <= 0.0) {
        if (type == MediaType::Video) durationSec = 10.0;
        else if (type == MediaType::Audio) durationSec = 30.0;
        else durationSec = 5.0;
    }

    asset->setDimensions(width, height);
    asset->setFps(core::FrameRate{static_cast<int32_t>(fps * 1000.0), 1000});
    asset->setHasVideo(hasVideo);
    asset->setHasAudio(hasAudio);
    asset->setDuration(core::TimelineTime::fromSeconds(durationSec));

    return core::Result<std::shared_ptr<MediaAsset>>(std::move(asset));
}

} // namespace catchim::media
