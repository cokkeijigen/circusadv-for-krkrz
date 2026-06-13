#include <native-lib.hpp>
#include <image_crx.hpp>

namespace circusadv
{

    static auto TVPLoadCRX(const TVP::Graphic::GraphicLoadingContext& context) noexcept -> void
    {
        std::vector<uint8_t> buffer{};
        auto size = context.src->GetSize();
        if(size ==0)
        {
            LOGD("无法获取文件大小");
            return;
        }

        buffer.resize(size);

        if(context.src->Read(buffer.data(), size) != size)
        {
            LOGD("无法读取文件");
            return;
        }
        adv::crx::image_crx crx{ buffer };
        if (!crx.is_valid())
        {
            LOGD("无法解析crx文件");
        }
        const int w{ crx.width()  };
        const int h{ crx.height() };
        context.sizecallback(w, h, TVP::Graphic::gpfRGBA);

        if (context.mode == TVP::Graphic::glmPalettized && crx.bpp() == 1)  // Palettized mode: pass raw indexed data
        {
            std::vector<uint8_t> raw{};
            if (!crx.unpack(raw))
            {
                LOGD("[mode == glmPalettized && crx.bpp() == 1] 无法解压crx数据！");
                return;
            }
            const int src_stride{ crx.stride() };
            for (int y{}; y < h; ++y)
            {
                void* scanline{ context.scanlinecallback(y) };
                if (scanline == nullptr)
                {
                    break;
                }
                std::memcpy(scanline, raw.data() + y * src_stride, w);
            }
            context.scanlinecallback(-1);
            return;
        }

        else if (context.mode == TVP::Graphic::glmGrayscale && crx.bpp() == 1) // Grayscale mode: convert palette to luminance
        {
            std::vector<uint8_t> raw{};
            if (!crx.unpack(raw))
            {
                LOGD("[mode == glmGrayscale && crx.bpp() == 1] 无法解压crx数据！");
                return;
            }
            const auto& pal{ crx.palette() };
            const int src_stride{ crx.stride() };

            for (int y{}; y < h; ++y)
            {
                void* scanline{ context.scanlinecallback(y) };
                if (scanline == nullptr)
                {
                    break;
                }
                auto          row_out{ static_cast<uint8_t*>(scanline) };
                const uint8_t* row_in{ raw.data() + y * src_stride };
                for (int x{}; x < w; ++x)
                {
                    const uint8_t  idx{ row_in[x] };
                    const size_t pal_off{ static_cast<size_t>(idx < pal.length ? idx : 0) * pal.size };
                    const auto luminance
                    {
                            static_cast<uint16_t>(pal.data[pal_off + 2]) * 77  +  // B
                            static_cast<uint16_t>(pal.data[pal_off + 1]) * 150 +  // G
                            static_cast<uint16_t>(pal.data[pal_off + 0]) * 29     // R
                    };
                    row_out[x] = static_cast<uint8_t>(luminance >> 8);
                }
            }
            context.scanlinecallback(-1);
            return;
        }
        else
        {
            // General mode: decode to 32-bit BGRA
            int stride{};
            const uint8_t* src_data{};
            std::vector<uint8_t> pixels{};

            if (!crx.decode(pixels, static_cast<uint8_t>(context.keyidx)))
            {
                LOGD("CRX: failed to decode");
                return;
            }

            src_data = pixels.data();
            stride   = w * 4;

            for (int y{}; y < h; ++y)
            {
                void* scanline{ context.scanlinecallback(y) };
                if (scanline == nullptr)
                {
                    break;
                }
                std::memcpy(scanline, src_data + static_cast<size_t>(y) * stride, w * 4);
            }
            context.scanlinecallback(-1);
        }

    }

    extern "C" JNIEXPORT auto K2A_OnLoad(void* modbase, JavaVM*) -> void
    {
        if(kr2android::init(modbase))
        {
            logd("kr2android init success!\n");
            TVP::Graphic::HandlerType crx
            {
                .IsPlugin      = false,
                .Extension     = ".crx",
                .LoadHandler   = TVP::Graphic::LoadingHandlerWrapper<TVPLoadCRX>::Call,
                .HeaderHandler = nullptr,
                .SaveHandler   = nullptr,
                .AcceptHandler = nullptr,
            };

            tvp::graphic::register_loading_handler(crx);

            const ttstr* native_dir = tvp::project::get_native_dir();
            const ttstr* project_dir = tvp::project::get_dir();
            if(native_dir != nullptr)
            {
                auto&& _str = native_dir->AsStdString();
                logd("native_dir: %s\n", _str.c_str());
            }

            if(project_dir != nullptr)
            {
                auto&& _str = project_dir->AsStdString();
                logd("project_dir: %s\n", _str.c_str());
            }
        }
        else
        {
            logd("kr2android init failed!\n");
        }
    }

    #if 0
    extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
    {
          if(kr2android::init())
          {
              logd("[circusadv] kr2android init success!\n");
          }
          else
          {
              logd("[circusadv] kr2android init failed!\n");
          }
        return JNI_VERSION_1_6;
    }
    #endif
}