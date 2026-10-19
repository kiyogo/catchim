#include "NativeVideoDecoder.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <chrono>
#if defined(__EMSCRIPTEN__)
#include <atomic>
#include <emscripten/em_asm.h>
#include <emscripten/threading.h>
#endif
#if defined(_M_X64) || defined(__SSE2__)
#include <emmintrin.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>
#include <wrl/client.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "ole32.lib")
#endif

#if defined(HAVE_QT6)
#include <QImage>
#include <QString>
#endif

namespace catchim::media {

#if defined(_M_X64) || defined(__SSE2__)
// SSE2 is baseline on x64. Convert eight pixels at once, preserving exactly
// the scalar matrix/range arithmetic, including saturation and rounding.
static int convertNv12Row(const uint8_t* y,const uint8_t* uv,uint8_t* dst,int width,
                         int yScale,int yOffset,int rv,int gu,int gv,int bu) {
    const auto zero=_mm_setzero_si128();int x=0;
    const auto multiply=[&](__m128i value,int coefficient,bool high) {
        return _mm_madd_epi16(high?_mm_unpackhi_epi16(value,zero):_mm_unpacklo_epi16(value,zero),_mm_set1_epi32(coefficient));
    };
    const auto pack=[](__m128i low,__m128i high) {
        return _mm_packus_epi16(_mm_packs_epi32(_mm_srai_epi32(low,8),_mm_srai_epi32(high,8)),_mm_setzero_si128());
    };
    for(;x+7<width;x+=8) {
        const auto yy=_mm_sub_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(y+x)),zero),_mm_set1_epi16(static_cast<short>(yOffset)));
        auto chroma=_mm_sub_epi16(_mm_unpacklo_epi8(_mm_loadl_epi64(reinterpret_cast<const __m128i*>(uv+x)),zero),_mm_set1_epi16(128));
        const auto u=_mm_shufflehi_epi16(_mm_shufflelo_epi16(chroma,_MM_SHUFFLE(2,2,0,0)),_MM_SHUFFLE(2,2,0,0));
        const auto v=_mm_shufflehi_epi16(_mm_shufflelo_epi16(chroma,_MM_SHUFFLE(3,3,1,1)),_MM_SHUFFLE(3,3,1,1));
        const auto lo=_mm_add_epi32(multiply(yy,yScale,false),_mm_set1_epi32(128));
        const auto hi=_mm_add_epi32(multiply(yy,yScale,true),_mm_set1_epi32(128));
        const auto r=pack(_mm_add_epi32(lo,multiply(v,rv,false)),_mm_add_epi32(hi,multiply(v,rv,true)));
        const auto g=pack(_mm_sub_epi32(_mm_sub_epi32(lo,multiply(u,gu,false)),multiply(v,gv,false)),_mm_sub_epi32(_mm_sub_epi32(hi,multiply(u,gu,true)),multiply(v,gv,true)));
        const auto b=pack(_mm_add_epi32(lo,multiply(u,bu,false)),_mm_add_epi32(hi,multiply(u,bu,true)));
        const auto rg=_mm_unpacklo_epi8(r,g),ba=_mm_unpacklo_epi8(b,_mm_set1_epi8(-1));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+x*4),_mm_unpacklo_epi16(rg,ba));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst+x*4+16),_mm_unpackhi_epi16(rg,ba));
    }
    return x;
}
#endif

#if defined(_WIN32)
struct NativeVideoDecoder::D3D11Context {
    ID3D11Device* pDevice{nullptr};
    ID3D11DeviceContext* pContext{nullptr};
    IMFDXGIDeviceManager* pDXGIManager{nullptr};
    UINT resetToken{0};

    ~D3D11Context() {
        if (pDXGIManager) {
            pDXGIManager->Release();
            pDXGIManager = nullptr;
        }
        if (pContext) {
            pContext->Release();
            pContext = nullptr;
        }
        if (pDevice) {
            pDevice->Release();
            pDevice = nullptr;
        }
    }
};
#else
struct NativeVideoDecoder::D3D11Context {};
#endif

#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
static std::atomic<uint32_t> nextBrowserStreamId{0};
#endif
struct NativeVideoDecoder::SourceReaderContext {
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
    uint32_t browserStreamId{++nextBrowserStreamId}, browserCache{0}, browserSequence{0};
    bool browserPlaying{false};
#endif
#if defined(_WIN32)
    IMFSourceReader* pReader{nullptr};
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> videoDevice;
    Microsoft::WRL::ComPtr<ID3D11VideoContext> videoContext;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> videoEnumerator;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessor> videoProcessor;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> gpuOutput,readback;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> outputView;
#endif
    int width{0};
    int height{0};
    int visibleWidth{0},visibleHeight{0},cropX{0},cropY{0};
    bool bt709{false},fullRange{false};
    // Use fixed-width C++ types here: these cache fields are shared by the
    // Windows Media Foundation decoder and the browser decoder backend.
    std::int32_t stride{0};
    std::int64_t currentSampleHns{-1};
    double lastTimestamp{-1.0};
    std::vector<uint8_t> lastFrame;
    bool isImage{false};
    bool isNv12{false};
    int frameWidth{0},frameHeight{0};

    ~SourceReaderContext() {
#if defined(_WIN32)
        if (pReader) {
            pReader->Release();
            pReader = nullptr;
        }
#endif
    }
};

NativeVideoDecoder& NativeVideoDecoder::instance() {
    static NativeVideoDecoder s_instance;
    return s_instance;
}
NativeVideoDecoder& NativeVideoDecoder::thumbnailInstance() {
    static NativeVideoDecoder decoder;
    static const bool configured=[] {decoder.setPreviewSize(240,136);return true;}();
    (void)configured;return decoder;
}
void NativeVideoDecoder::setPreviewSize(int width,int height) {
    std::lock_guard guard(mutex_);
    if(width==previewWidth_&&height==previewHeight_)return;
    previewWidth_=std::max(0,width);previewHeight_=std::max(0,height);
    readers_.clear();
}
NativeVideoDecoder::Statistics NativeVideoDecoder::statistics() const {
    std::lock_guard guard(mutex_);return statistics_;
}

void NativeVideoDecoder::initD3D11() {
#if defined(_WIN32)
    if (d3d11Ctx_) return;

    auto ctx = std::make_unique<D3D11Context>();
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL featureLevel;
    UINT creationFlags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    // A virtual-display adapter can precede the real Radeon/NVIDIA adapter.
    // Select a hardware adapter that actually advertises video decode profiles.
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> selected;
    SIZE_T bestMemory=0;
    if(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))for(UINT i=0;;++i) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        if(FAILED(factory->EnumAdapters1(i,&adapter)))break;
        DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        Microsoft::WRL::ComPtr<ID3D11Device> candidate;
        Microsoft::WRL::ComPtr<ID3D11VideoDevice> video;
        if(FAILED(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,creationFlags,featureLevels,ARRAYSIZE(featureLevels),D3D11_SDK_VERSION,&candidate,nullptr,nullptr))||
           FAILED(candidate.As(&video))||video->GetVideoDecoderProfileCount()==0)continue;
        if(!selected||desc.DedicatedVideoMemory>bestMemory){selected=adapter;bestMemory=desc.DedicatedVideoMemory;}
    }
    HRESULT hr = D3D11CreateDevice(
        selected.Get(),
        selected?D3D_DRIVER_TYPE_UNKNOWN:D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &ctx->pDevice,
        &featureLevel,
        &ctx->pContext
    );

    if (FAILED(hr) || !ctx->pDevice) return;
    if(selected) {
        DXGI_ADAPTER_DESC1 desc{};selected->GetDesc1(&desc);
        const int size=WideCharToMultiByte(CP_UTF8,0,desc.Description,-1,nullptr,0,nullptr,nullptr);
        std::string name(std::max(1,size),'\0');WideCharToMultiByte(CP_UTF8,0,desc.Description,-1,name.data(),size,nullptr,nullptr);
        if(!name.empty()&&name.back()=='\0')name.pop_back();statistics_.adapter=std::move(name);
    }

    // Enable multithread protection on D3D11 device
    ID3D10Multithread* pMultithread = nullptr;
    if (SUCCEEDED(ctx->pDevice->QueryInterface(IID_PPV_ARGS(&pMultithread)))) {
        pMultithread->SetMultithreadProtected(TRUE);
        pMultithread->Release();
    }

    hr = MFCreateDXGIDeviceManager(&ctx->resetToken, &ctx->pDXGIManager);
    if (FAILED(hr) || !ctx->pDXGIManager) return;

    hr = ctx->pDXGIManager->ResetDevice(ctx->pDevice, ctx->resetToken);
    if (FAILED(hr)) return;

    d3d11Ctx_ = std::move(ctx);
#endif
}

void NativeVideoDecoder::cleanupD3D11() {
    d3d11Ctx_.reset();
}

NativeVideoDecoder::NativeVideoDecoder() {
#if defined(_WIN32)
    MFStartup(MF_VERSION);
    initD3D11();
#endif
}

NativeVideoDecoder::~NativeVideoDecoder() {
    clearCache();
    cleanupD3D11();
#if defined(_WIN32)
    MFShutdown();
#endif
}

void NativeVideoDecoder::clearCache() {
    std::lock_guard<std::mutex> lock(mutex_);
    readers_.clear();
}

bool NativeVideoDecoder::convertGpuFrame(SourceReaderContext& ctx,void* rawBuffer) {
#if defined(_WIN32)
    if(!d3d11Ctx_||previewWidth_<=0||previewHeight_<=0)return false;
    using Microsoft::WRL::ComPtr;
    auto fail=[&](HRESULT error){statistics_.lastGpuError=static_cast<uint32_t>(error);return false;};
    ComPtr<IMFDXGIBuffer> buffer;
    HRESULT hr=static_cast<IMFMediaBuffer*>(rawBuffer)->QueryInterface(IID_PPV_ARGS(&buffer));if(FAILED(hr))return fail(hr);
    ComPtr<ID3D11Texture2D> input;UINT subresource=0;
    hr=buffer->GetResource(IID_PPV_ARGS(&input));if(FAILED(hr))return fail(hr);
    hr=buffer->GetSubresourceIndex(&subresource);if(FAILED(hr))return fail(hr);++statistics_.gpuSurfaceFrames;
    D3D11_TEXTURE2D_DESC inputDesc{};input->GetDesc(&inputDesc);
    const double scale=std::min({1.,static_cast<double>(previewWidth_)/ctx.visibleWidth,static_cast<double>(previewHeight_)/ctx.visibleHeight});
    const int width=std::max(2,static_cast<int>(std::round(ctx.visibleWidth*scale)));
    const int height=std::max(2,static_cast<int>(std::round(ctx.visibleHeight*scale)));
    if(!ctx.outputView) {
        ctx.videoDevice.Reset();ctx.videoContext.Reset();ctx.videoEnumerator.Reset();ctx.videoProcessor.Reset();
        ctx.gpuOutput.Reset();ctx.readback.Reset();ctx.outputView.Reset();
        hr=d3d11Ctx_->pDevice->QueryInterface(IID_PPV_ARGS(&ctx.videoDevice));if(FAILED(hr))return fail(hr);
        hr=d3d11Ctx_->pContext->QueryInterface(IID_PPV_ARGS(&ctx.videoContext));if(FAILED(hr))return fail(hr);
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC content{};
        content.InputFrameFormat=D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
        content.InputWidth=inputDesc.Width;content.InputHeight=inputDesc.Height;
        content.OutputWidth=width;content.OutputHeight=height;
        content.InputFrameRate={30,1};content.OutputFrameRate={30,1};content.Usage=D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
        hr=ctx.videoDevice->CreateVideoProcessorEnumerator(&content,&ctx.videoEnumerator);if(FAILED(hr))return fail(hr);
        hr=ctx.videoDevice->CreateVideoProcessor(ctx.videoEnumerator.Get(),0,&ctx.videoProcessor);if(FAILED(hr))return fail(hr);
        D3D11_TEXTURE2D_DESC output{};output.Width=width;output.Height=height;output.MipLevels=1;output.ArraySize=1;
        output.Format=DXGI_FORMAT_B8G8R8A8_UNORM;output.SampleDesc.Count=1;
        output.Usage=D3D11_USAGE_DEFAULT;output.BindFlags=D3D11_BIND_RENDER_TARGET;
        hr=d3d11Ctx_->pDevice->CreateTexture2D(&output,nullptr,&ctx.gpuOutput);if(FAILED(hr))return fail(hr);
        output.Usage=D3D11_USAGE_STAGING;output.BindFlags=0;output.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        hr=d3d11Ctx_->pDevice->CreateTexture2D(&output,nullptr,&ctx.readback);if(FAILED(hr))return fail(hr);
        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC view{};view.ViewDimension=D3D11_VPOV_DIMENSION_TEXTURE2D;
        hr=ctx.videoDevice->CreateVideoProcessorOutputView(ctx.gpuOutput.Get(),ctx.videoEnumerator.Get(),&view,&ctx.outputView);if(FAILED(hr))return fail(hr);
    }
    ComPtr<ID3D11VideoProcessorInputView> inputView;
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC view{};view.ViewDimension=D3D11_VPIV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipSlice=subresource%inputDesc.MipLevels;view.Texture2D.ArraySlice=subresource/inputDesc.MipLevels;
    hr=ctx.videoDevice->CreateVideoProcessorInputView(input.Get(),ctx.videoEnumerator.Get(),&view,&inputView);if(FAILED(hr))return fail(hr);
    auto* context=ctx.videoContext.Get();auto* processor=ctx.videoProcessor.Get();
    const RECT source{ctx.cropX,ctx.cropY,ctx.cropX+ctx.visibleWidth,ctx.cropY+ctx.visibleHeight},destination{0,0,width,height};
    context->VideoProcessorSetStreamFrameFormat(processor,0,D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
    context->VideoProcessorSetStreamAutoProcessingMode(processor,0,FALSE);
    context->VideoProcessorSetStreamSourceRect(processor,0,TRUE,&source);
    context->VideoProcessorSetStreamDestRect(processor,0,TRUE,&destination);
    context->VideoProcessorSetOutputTargetRect(processor,TRUE,&destination);
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE inputColor{};inputColor.YCbCr_Matrix=ctx.bt709?1:0;
    inputColor.Nominal_Range=ctx.fullRange?D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255:D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE outputColor{};outputColor.Nominal_Range=D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255;
    context->VideoProcessorSetStreamColorSpace(processor,0,&inputColor);context->VideoProcessorSetOutputColorSpace(processor,&outputColor);
    context->VideoProcessorSetOutputAlphaFillMode(processor,D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE_OPAQUE,0);
    D3D11_VIDEO_PROCESSOR_STREAM stream{};stream.Enable=TRUE;stream.pInputSurface=inputView.Get();
    hr=context->VideoProcessorBlt(processor,ctx.outputView.Get(),0,1,&stream);if(FAILED(hr))return fail(hr);
    d3d11Ctx_->pContext->CopyResource(ctx.readback.Get(),ctx.gpuOutput.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};hr=d3d11Ctx_->pContext->Map(ctx.readback.Get(),0,D3D11_MAP_READ,0,&mapped);if(FAILED(hr))return fail(hr);
    ctx.lastFrame.resize(static_cast<size_t>(width)*height*4);
#if defined(HAVE_QT6)
    const QImage bgra(static_cast<const uchar*>(mapped.pData),width,height,mapped.RowPitch,QImage::Format_ARGB32);
    const QImage rgba=bgra.convertToFormat(QImage::Format_RGBA8888);
    for(int y=0;y<height;++y)std::memcpy(ctx.lastFrame.data()+static_cast<size_t>(y)*width*4,rgba.constScanLine(y),width*4);
#else
    for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
        const auto* src=static_cast<const uint8_t*>(mapped.pData)+y*mapped.RowPitch+x*4;
        auto* dst=ctx.lastFrame.data()+(static_cast<size_t>(y)*width+x)*4;
        dst[0]=src[2];dst[1]=src[1];dst[2]=src[0];dst[3]=255;
    }
#endif
    d3d11Ctx_->pContext->Unmap(ctx.readback.Get(),0);
    ctx.frameWidth=width;ctx.frameHeight=height;++statistics_.gpuProcessedFrames;
    statistics_.readbackBytes+=static_cast<uint64_t>(width)*height*4;statistics_.lastGpuError=0;return true;
#else
    return false;
#endif
}

bool NativeVideoDecoder::getFrame(
    const std::filesystem::path& path,
    double timestampSec,
    int& outWidth,
    int& outHeight,
    std::vector<uint8_t>& outRgba,
    const std::string& clipId,
    int* originalWidth,int* originalHeight
) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string pathKey = path.string();
    std::string ext = path.extension().string();
    for (char& c : ext) c = static_cast<char>(::tolower(c));

    std::string readerKey = clipId.empty() ? pathKey : (clipId + "@" + pathKey);

    // 1. Image formats check
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".webp") {
        auto it = readers_.find(readerKey);
        if (it != readers_.end() && it->second->isImage && !it->second->lastFrame.empty()) {
            outWidth = it->second->width;
            outHeight = it->second->height;
            if(originalWidth)*originalWidth=outWidth;if(originalHeight)*originalHeight=outHeight;
            outRgba = it->second->lastFrame;
            return true;
        }

#if defined(HAVE_QT6)
        QImage img(QString::fromStdString(pathKey));
        if (!img.isNull()) {
            QImage conv = img.convertToFormat(QImage::Format_RGBA8888);
            outWidth = conv.width();
            outHeight = conv.height();
            if(originalWidth)*originalWidth=outWidth;if(originalHeight)*originalHeight=outHeight;
            outRgba.resize(outWidth * outHeight * 4);
            std::memcpy(outRgba.data(), conv.bits(), outRgba.size());

            auto ctx = std::make_shared<SourceReaderContext>();
            ctx->width = outWidth;
            ctx->height = outHeight;
            ctx->isImage = true;
            ctx->lastFrame = outRgba;
            readers_[readerKey] = ctx;
            return true;
        }
#endif
        return false;
    }

#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
    // Browser video decoding is asynchronous. Keep the synchronous decoder API
    // used by the renderer, but wait only on this worker (never on the UI
    // thread) while the browser's native media decoder services the seek.
    auto it = readers_.find(readerKey);
    std::shared_ptr<SourceReaderContext> ctx;
    if (it != readers_.end()) ctx = it->second;
    else {
        ctx = std::make_shared<SourceReaderContext>();
        readers_[readerKey] = ctx;
    }
    if (ctx->browserPlaying == playbackMode_ && ctx->lastTimestamp >= 0.0 && std::abs(ctx->lastTimestamp - timestampSec) < 0.001 && !ctx->lastFrame.empty()) {
        outWidth = ctx->frameWidth;
        outHeight = ctx->frameHeight;
        if (originalWidth) *originalWidth = ctx->visibleWidth;
        if (originalHeight) *originalHeight = ctx->visibleHeight;
        outRgba = ctx->lastFrame;
        return true;
    }

    const int maxWidth = previewWidth_ > 0 ? previewWidth_ : 1920;
    const int maxHeight = previewHeight_ > 0 ? previewHeight_ : 1080;
    if (playbackMode_ && ctx->browserPlaying && ctx->browserCache && maxWidth == 480 && maxHeight == 270) {
        auto* head = reinterpret_cast<std::atomic<uint32_t>*>(static_cast<uintptr_t>(ctx->browserCache));
        auto sequence = head[0].load(std::memory_order_acquire);
        const auto originalSequence = sequence;
        if (!(sequence & 1) && head[0].compare_exchange_strong(sequence, sequence + 1, std::memory_order_acq_rel)) {
            const int w = head[2].load(std::memory_order_relaxed), h = head[3].load(std::memory_order_relaxed);
            const int sw = head[4].load(std::memory_order_relaxed), sh = head[5].load(std::memory_order_relaxed);
            const double seconds = head[6].load(std::memory_order_relaxed) / 1000.;
            const bool valid = head[1].load(std::memory_order_relaxed) == ctx->browserStreamId &&
                head[7].load(std::memory_order_relaxed) && w > 0 && w <= 480 && h > 0 && h <= 270 &&
                std::abs(seconds - timestampSec) < .35;
            if (valid) {
                const auto* data = reinterpret_cast<const uint8_t*>(head) + 32;
                outRgba.assign(data, data + static_cast<size_t>(w) * h * 4);
            }
            head[0].store(originalSequence, std::memory_order_release);
            if (valid) {
                if (ctx->browserSequence != sequence) ++statistics_.cpuFallbackFrames;
                ctx->browserSequence = sequence;
                ctx->lastFrame = outRgba; ctx->lastTimestamp = timestampSec;
                ctx->frameWidth = outWidth = w; ctx->frameHeight = outHeight = h;
                if (originalWidth) *originalWidth = sw;
                if (originalHeight) *originalHeight = sh;
                return true;
            }
        }
    }
    std::vector<uint8_t> pixels(static_cast<size_t>(maxWidth) * maxHeight * 4);
    int decodedWidth = 0, decodedHeight = 0, sourceWidth = 0, sourceHeight = 0;
    std::atomic<uint32_t> completed{0};
    const auto pathUtf8 = path.u8string();
    const auto* pathData = reinterpret_cast<const char*>(pathUtf8.c_str());
    MAIN_THREAD_ASYNC_EM_ASM({
        const done = $9 >> 2;
        const signal = (ok) => {
            Atomics.store(HEAP32, done, ok ? 1 : -1);
            Atomics.notify(HEAP32, done);
        };
        Module.catchimDecodeVideoFrame(
            UTF8ToString($0), $1, $2, $3, $4, $5, $6, $7, $8, UTF8ToString($10), !!$11, $12, $13
        ).then(() => signal(true)).catch((error) => {
            console.warn("[Catchim] Browser video frame decode failed:", error);
            signal(false);
        });
    }, pathData, timestampSec, maxWidth, maxHeight, pixels.data(),
       &decodedWidth, &decodedHeight, &sourceWidth, &sourceHeight, &completed, readerKey.c_str(), playbackMode_,
       &ctx->browserCache, ctx->browserStreamId);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (completed.load(std::memory_order_acquire) == 0 && std::chrono::steady_clock::now() < deadline) {
        emscripten_futex_wait(reinterpret_cast<volatile void*>(&completed), 0, 1000.0);
    }
    if (completed.load(std::memory_order_acquire) != 1 || decodedWidth <= 0 || decodedHeight <= 0 ||
        sourceWidth <= 0 || sourceHeight <= 0) return false;

    pixels.resize(static_cast<size_t>(decodedWidth) * decodedHeight * 4);
    ctx->lastFrame = pixels;
    ctx->browserPlaying = playbackMode_;
    if (playbackMode_) ++statistics_.cpuFallbackFrames;
    ctx->lastTimestamp = timestampSec;
    ctx->width = sourceWidth;
    ctx->height = sourceHeight;
    ctx->visibleWidth = sourceWidth;
    ctx->visibleHeight = sourceHeight;
    ctx->frameWidth = decodedWidth;
    ctx->frameHeight = decodedHeight;
    outWidth = decodedWidth;
    outHeight = decodedHeight;
    if (originalWidth) *originalWidth = sourceWidth;
    if (originalHeight) *originalHeight = sourceHeight;
    outRgba = std::move(pixels);
    return true;
#endif

#if defined(_WIN32)
    auto it = readers_.find(readerKey);
    std::shared_ptr<SourceReaderContext> ctx;
    if (it != readers_.end()) {
        ctx = it->second;
    } else {
        ctx = std::make_shared<SourceReaderContext>();

        bool useD3D = (d3d11Ctx_ && d3d11Ctx_->pDXGIManager);
        IMFAttributes* pAttributes = nullptr;
        MFCreateAttributes(&pAttributes, useD3D ? 3 : 2);
        if (pAttributes) {
            pAttributes->SetUINT32(MF_LOW_LATENCY, TRUE);
            if (useD3D) {
                pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
                pAttributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, d3d11Ctx_->pDXGIManager);
            } else {
                pAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
            }
        }

        std::wstring wpath = path.wstring();
        HRESULT hr = MFCreateSourceReaderFromURL(wpath.c_str(), pAttributes, &ctx->pReader);
        if (pAttributes) pAttributes->Release();

        if (FAILED(hr) || !ctx->pReader) {
            // Software fallback with video processing enabled
            IMFAttributes* swAttr = nullptr;
            MFCreateAttributes(&swAttr, 2);
            if (swAttr) {
                swAttr->SetUINT32(MF_LOW_LATENCY, TRUE);
                swAttr->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
            }
            hr = MFCreateSourceReaderFromURL(wpath.c_str(), swAttr, &ctx->pReader);
            if (swAttr) swAttr->Release();
            useD3D = false;
        }

        if (FAILED(hr) || !ctx->pReader) {
            return false;
        }

        ctx->pReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS,FALSE);
        ctx->pReader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM,TRUE);
        IMFMediaType* nativeType=nullptr;
        if(SUCCEEDED(ctx->pReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,&nativeType))) {
            UINT32 w=0,h=0;MFGetAttributeSize(nativeType,MF_MT_FRAME_SIZE,&w,&h);
            ctx->visibleWidth=w;ctx->visibleHeight=h;
            ctx->bt709=MFGetAttributeUINT32(nativeType,MF_MT_YUV_MATRIX,MFVideoTransferMatrix_BT601)==MFVideoTransferMatrix_BT709;
            ctx->fullRange=MFGetAttributeUINT32(nativeType,MF_MT_VIDEO_NOMINAL_RANGE,0)==MFNominalRange_0_255;
            for(const auto& key:{MF_MT_MINIMUM_DISPLAY_APERTURE,MF_MT_GEOMETRIC_APERTURE}) {
                MFVideoArea area{};UINT32 bytes=0;
                if(SUCCEEDED(nativeType->GetBlob(key,reinterpret_cast<UINT8*>(&area),sizeof(area),&bytes))&&area.Area.cx>0&&area.Area.cy>0) {
                    ctx->visibleWidth=area.Area.cx;ctx->visibleHeight=area.Area.cy;ctx->cropX=area.OffsetX.value;ctx->cropY=area.OffsetY.value;
                }
            }
            nativeType->Release();
        }

        // Configure output type: try NV12 (for D3D11 hardware decoding) first
        bool setTypeOk = false;
        if (useD3D) {
            IMFMediaType* pMediaType = nullptr;
            MFCreateMediaType(&pMediaType);
            if (pMediaType) {
                pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
                pMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
                hr = ctx->pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, pMediaType);
                if (SUCCEEDED(hr)) {
                    ctx->isNv12 = true;
                    setTypeOk = true;
                }
                pMediaType->Release();
            }
        }

        if (!setTypeOk) {
            // Fallback to RGB32 (32-bit B,G,R,A)
            IMFMediaType* pMediaType = nullptr;
            MFCreateMediaType(&pMediaType);
            if (pMediaType) {
                pMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
                pMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
                hr=ctx->pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), nullptr, pMediaType);
                ctx->isNv12 = false;
                pMediaType->Release();
                if(FAILED(hr))return false;
            }
        }

        // Query decoded width, height and default stride
        IMFMediaType* pCurrentType = nullptr;
        if (SUCCEEDED(ctx->pReader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), &pCurrentType)) && pCurrentType) {
            UINT32 w = 0, h = 0;
            MFGetAttributeSize(pCurrentType, MF_MT_FRAME_SIZE, &w, &h);
            ctx->width = static_cast<int>(w);
            ctx->height = static_cast<int>(h);
            MFVideoArea area{};UINT32 areaBytes=0;
            if(SUCCEEDED(pCurrentType->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE,reinterpret_cast<UINT8*>(&area),sizeof(area),&areaBytes))&&
               area.Area.cx>0&&area.Area.cy>0&&area.Area.cx<=ctx->visibleWidth&&area.Area.cy<=ctx->visibleHeight&&area.OffsetX.value>=0&&area.OffsetY.value>=0&&
               area.OffsetX.value+area.Area.cx<=ctx->width&&area.OffsetY.value+area.Area.cy<=ctx->height) {
                ctx->visibleWidth=area.Area.cx;ctx->visibleHeight=area.Area.cy;
                ctx->cropX=area.OffsetX.value;ctx->cropY=area.OffsetY.value;
            }
            if(ctx->visibleWidth<=0||ctx->visibleWidth>ctx->width)ctx->visibleWidth=ctx->width;
            if(ctx->visibleHeight<=0||ctx->visibleHeight>ctx->height)ctx->visibleHeight=ctx->height;
            ctx->bt709=MFGetAttributeUINT32(pCurrentType,MF_MT_YUV_MATRIX,ctx->bt709?MFVideoTransferMatrix_BT709:MFVideoTransferMatrix_BT601)==MFVideoTransferMatrix_BT709;
            ctx->fullRange=MFGetAttributeUINT32(pCurrentType,MF_MT_VIDEO_NOMINAL_RANGE,ctx->fullRange?MFNominalRange_0_255:MFNominalRange_16_235)==MFNominalRange_0_255;

            UINT32 defStride = 0;
            if (SUCCEEDED(pCurrentType->GetUINT32(MF_MT_DEFAULT_STRIDE, &defStride))) {
                ctx->stride = static_cast<LONG>(defStride);
            }
            pCurrentType->Release();
        }

        if (ctx->width <= 0 || ctx->height <= 0) {
            ctx->width = 1920;
            ctx->height = 1080;
        }

        readers_[readerKey] = ctx;
    }

    if (!ctx->pReader) return false;

    // Fast check: if requested time matches current cached frame within ~33ms (1 frame @ 30fps), return cached frame
    if (ctx->lastTimestamp >= 0.0 && std::abs(ctx->lastTimestamp - timestampSec) < 0.001 && !ctx->lastFrame.empty()) {
        outWidth = ctx->frameWidth?ctx->frameWidth:ctx->visibleWidth;
        outHeight = ctx->frameHeight?ctx->frameHeight:ctx->visibleHeight;
        if(originalWidth)*originalWidth=ctx->visibleWidth;if(originalHeight)*originalHeight=ctx->visibleHeight;
        outRgba = ctx->lastFrame;
        return true;
    }

    LONGLONG targetHns = static_cast<LONGLONG>(std::max(0.0, timestampSec) * 10000000.0);

    // Determine if seek is required:
    // 1) First read (currentSampleHns < 0)
    // 2) Backward jump (> 250ms behind current sample)
    // 3) Forward jump (> 2.5s ahead of current sample)
    bool needSeek = false;
    if (ctx->currentSampleHns < 0) {
        needSeek = true;
    } else if (targetHns < ctx->currentSampleHns - 10000) {
        needSeek = true;
    } else if (targetHns > ctx->currentSampleHns + 10000000) {
        needSeek = true;
    }

    if (needSeek) {
        PROPVARIANT var;
        PropVariantInit(&var);
        var.vt = VT_I8;
        var.hVal.QuadPart = targetHns;
        ctx->pReader->SetCurrentPosition(GUID_NULL, var);
        PropVariantClear(&var);
        ctx->currentSampleHns = -1;
    }

    // Read samples sequentially until sampleTimestamp reaches or passes targetHns (with tolerance ~20ms)
    const LONGLONG toleranceHns = 200000; // 20ms
    // A seek starts at a keyframe, which can be many seconds before the target.
    // The old 60-sample cap returned a wrong frame even at a 2-second GOP edge.
    int safetyLoops = 5000;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);

    while (safetyLoops-- > 0&&std::chrono::steady_clock::now()<deadline) {
        // If we already have a sample close enough to or beyond target, we are done
        if (ctx->currentSampleHns >= 0 && ctx->currentSampleHns >= targetHns - toleranceHns) {
            break;
        }

        DWORD streamIndex = 0, flags = 0;
        LONGLONG sampleTimestamp = 0;
        IMFSample* pSample = nullptr;

        HRESULT hr = ctx->pReader->ReadSample(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
            0,
            &streamIndex,
            &flags,
            &sampleTimestamp,
            &pSample
        );

        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            if (pSample) pSample->Release();
            break;
        }

        if (flags & MF_SOURCE_READERF_STREAMTICK) {
            if (pSample) pSample->Release();
            continue;
        }

        if (pSample) {
            bool isTargetFrame = (sampleTimestamp >= targetHns - toleranceHns) || (safetyLoops <= 0);

            // OPTIMIZATION: Only decode and copy pixels for the target frame!
            // Intermediate frames during seeking are skipped without expensive pixel copies.
            if (isTargetFrame) {
                IMFMediaBuffer* pBuffer = nullptr;
                hr = pSample->GetBufferByIndex(0,&pBuffer);
                if (SUCCEEDED(hr) && pBuffer) {
                    if(convertGpuFrame(*ctx,pBuffer)) {
                        ctx->currentSampleHns=sampleTimestamp;ctx->lastTimestamp=sampleTimestamp/10000000.;
                        pBuffer->Release();pSample->Release();break;
                    }
                    ++statistics_.cpuFallbackFrames;
                    ctx->frameWidth=ctx->visibleWidth;ctx->frameHeight=ctx->visibleHeight;
                    BYTE* pData = nullptr;
                    DWORD currentLength = 0;
                    bool locked=false,packed=false;
                    std::vector<BYTE> contiguous;
                    IMF2DBuffer* surface=nullptr;
                    // GPU surfaces can pad both pitch and storage height. Buffer
                    // byte length / visible height is NOT a valid image stride.
                    // ContiguousCopyTo removes surface padding using the driver's
                    // true plane layout (IMF2DBuffer contract).
                    if(SUCCEEDED(pBuffer->QueryInterface(IID_PPV_ARGS(&surface)))) {
                        hr=surface->GetContiguousLength(&currentLength);
                        if(SUCCEEDED(hr)){
                            contiguous.resize(currentLength);
                            hr=surface->ContiguousCopyTo(contiguous.data(),currentLength);
                            if(SUCCEEDED(hr)){pData=contiguous.data();packed=true;}
                        }
                        surface->Release();
                    }
                    if(!packed){hr=pBuffer->Lock(&pData,nullptr,&currentLength);locked=SUCCEEDED(hr);}
                    if (SUCCEEDED(hr) && pData) {
                        size_t minExpected = ctx->isNv12 ?
                            static_cast<size_t>(ctx->width * ctx->height * 3 / 2) :
                            static_cast<size_t>(ctx->width * ctx->height * 4);

                        if (currentLength >= minExpected && ctx->width > 0 && ctx->height > 0) {
                            const int visibleW=ctx->visibleWidth,visibleH=ctx->visibleHeight;
                            ctx->lastFrame.resize(visibleW * visibleH * 4);

                            if (ctx->isNv12) {
                            // Hardware decoded NV12 -> RGBA
                            LONG stride = packed?ctx->width:(ctx->stride>=ctx->width?ctx->stride:ctx->width);
                            if (stride < ctx->width) {
                                stride = (ctx->stride >= ctx->width) ? ctx->stride : ctx->width;
                            }

                            const BYTE* yPlane = pData+ctx->cropY*stride+ctx->cropX;
                            // Some DXGI-backed buffers retain the decoder's coded
                            // height (e.g. 192 rows for 180 visible rows) even in
                            // contiguous form. Chroma follows that padded Y plane.
                            const int storageHeight=packed?std::max(ctx->height,static_cast<int>((currentLength/stride)*2/3)):ctx->height;
                            const BYTE* uvPlane = pData + (stride * storageHeight)+(ctx->cropY/2)*stride+(ctx->cropX&~1);
                            BYTE* dst = ctx->lastFrame.data();
                            const int yScale=ctx->fullRange?256:298,yOffset=ctx->fullRange?0:16;
                            const int rv=ctx->fullRange?(ctx->bt709?403:359):(ctx->bt709?459:409);
                            const int gu=ctx->fullRange?(ctx->bt709?48:88):(ctx->bt709?55:100);
                            const int gv=ctx->fullRange?(ctx->bt709?120:183):(ctx->bt709?136:208);
                            const int bu=ctx->fullRange?(ctx->bt709?475:454):(ctx->bt709?541:516);

                            for (int y = 0; y < visibleH; ++y) {
                                const BYTE* yRow = yPlane + y * stride;
                                const BYTE* uvRow = uvPlane + (y / 2) * stride;
                                BYTE* dstRow = dst + y * (visibleW * 4);

                                int x = 0;
#if defined(_M_X64) || defined(__SSE2__)
                                x=convertNv12Row(yRow,uvRow,dstRow,visibleW,yScale,yOffset,rv,gu,gv,bu);
#endif
                                for (; x + 1 < visibleW; x += 2) {
                                    int Y0 = yRow[x + 0];
                                    int Y1 = yRow[x + 1];
                                    int U  = uvRow[x + 0];
                                    int V  = uvRow[x + 1];

                                    int D = U - 128;
                                    int E = V - 128;
                                    int rOffset = rv * E + 128;
                                    int gOffset = -gu * D - gv * E + 128;
                                    int bOffset = bu * D + 128;

                                    int base0 = yScale * (Y0 - yOffset);
                                    int base1 = yScale * (Y1 - yOffset);

                                    int R0 = (base0 + rOffset) >> 8;
                                    int G0 = (base0 + gOffset) >> 8;
                                    int B0 = (base0 + bOffset) >> 8;

                                    int R1 = (base1 + rOffset) >> 8;
                                    int G1 = (base1 + gOffset) >> 8;
                                    int B1 = (base1 + bOffset) >> 8;

                                    dstRow[(x + 0) * 4 + 0] = static_cast<uint8_t>(std::clamp(R0, 0, 255));
                                    dstRow[(x + 0) * 4 + 1] = static_cast<uint8_t>(std::clamp(G0, 0, 255));
                                    dstRow[(x + 0) * 4 + 2] = static_cast<uint8_t>(std::clamp(B0, 0, 255));
                                    dstRow[(x + 0) * 4 + 3] = 255;

                                    dstRow[(x + 1) * 4 + 0] = static_cast<uint8_t>(std::clamp(R1, 0, 255));
                                    dstRow[(x + 1) * 4 + 1] = static_cast<uint8_t>(std::clamp(G1, 0, 255));
                                    dstRow[(x + 1) * 4 + 2] = static_cast<uint8_t>(std::clamp(B1, 0, 255));
                                    dstRow[(x + 1) * 4 + 3] = 255;
                                }
                                for (; x < visibleW; ++x) {
                                    int Y = yRow[x];
                                    int U = uvRow[x & ~1];
                                    int V = uvRow[(x & ~1) + 1];
                                    int C = Y - yOffset, D = U - 128, E = V - 128;
                                    dstRow[x * 4 + 0] = static_cast<uint8_t>(std::clamp((yScale * C + rv * E + 128) >> 8, 0, 255));
                                    dstRow[x * 4 + 1] = static_cast<uint8_t>(std::clamp((yScale * C - gu * D - gv * E + 128) >> 8, 0, 255));
                                    dstRow[x * 4 + 2] = static_cast<uint8_t>(std::clamp((yScale * C + bu * D + 128) >> 8, 0, 255));
                                    dstRow[x * 4 + 3] = 255;
                                }
                            }
                        } else {
                            // Software/Direct RGB32 (BGRA -> RGBA)
                            LONG stride = packed?ctx->width*4:(std::abs(ctx->stride)>=ctx->width*4?ctx->stride:ctx->width*4);
                            if(packed&&ctx->stride<0)stride=-stride;

                            bool bottomUp = (stride < 0);
                            LONG absStride = std::abs(stride);

                            for (int y = 0; y < visibleH; ++y) {
                                int srcY = bottomUp ? (ctx->height - 1 - y-ctx->cropY) : y+ctx->cropY;
                                const BYTE* srcRow = pData + srcY * absStride+ctx->cropX*4;
                                BYTE* dstRow = ctx->lastFrame.data() + y * (visibleW * 4);

                                for (int x = 0; x < visibleW; ++x) {
                                    uint8_t b = srcRow[x * 4 + 0];
                                    uint8_t g = srcRow[x * 4 + 1];
                                    uint8_t r = srcRow[x * 4 + 2];
                                    uint8_t a = srcRow[x * 4 + 3];

                                    dstRow[x * 4 + 0] = r;
                                    dstRow[x * 4 + 1] = g;
                                    dstRow[x * 4 + 2] = b;
                                    dstRow[x * 4 + 3] = (a == 0) ? 255 : a;
                                }
                            }
                        }
                    }

                    if(locked)pBuffer->Unlock();
                    ctx->currentSampleHns = sampleTimestamp;
                    ctx->lastTimestamp = static_cast<double>(sampleTimestamp) / 10000000.0;
                }
                pBuffer->Release();
            }
            } else {
                ctx->currentSampleHns = sampleTimestamp;
            }

            pSample->Release();

            if (sampleTimestamp >= targetHns - toleranceHns) {
                break;
            }
        }
    }

    if (!ctx->lastFrame.empty()) {
        outWidth = ctx->frameWidth?ctx->frameWidth:ctx->visibleWidth;
        outHeight = ctx->frameHeight?ctx->frameHeight:ctx->visibleHeight;
        if(originalWidth)*originalWidth=ctx->visibleWidth;if(originalHeight)*originalHeight=ctx->visibleHeight;
        outRgba = ctx->lastFrame;
        return true;
    }
#endif

    return false;
}

} // namespace catchim::media
