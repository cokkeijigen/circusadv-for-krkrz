#include <native-lib.hpp>
#include <image_crx.hpp>

namespace circusadv
{

    bool tTVPGraphicHandlerType::operator==(const tTVPGraphicHandlerType & ref) const
    {
        return this->FormatData == ref.FormatData &&
               this->IsPlugin == ref.IsPlugin &&
               this->LoadHandler == ref.LoadHandler &&
               this->HeaderHandler == ref.HeaderHandler &&
               this->SaveHandler == ref.SaveHandler &&
               this->AcceptHandler == ref.AcceptHandler &&
               this->Extension == ref.Extension;
    }

    void tTVPGraphicType::ReCreateHash()
    {
        std::vector<tTVPGraphicHandlerType>::iterator i;
        for(i = this->Handlers.begin();
            i!= this->Handlers.end(); i++)
        {
            this->Hash.Add(i->Extension, *i);
        }
    }

    void tTVPGraphicType::Register(const tTVPGraphicHandlerType& hander )
    {
        this->Handlers.push_back(hander);
        this->ReCreateHash();
    }

    void tTVPGraphicType::Unregister( const tTVPGraphicHandlerType& hander )
    {
        std::vector<tTVPGraphicHandlerType>::iterator i;
        if(this->Handlers.size() > 0)
        {
            for(i = this->Handlers.begin(); i != this->Handlers.end(); i++)
            {
                if(hander == *i)
                {
                    this->Handlers.erase(i);
                    break;
                }
            }
        }
        this->ReCreateHash();
    }

    auto tTVPGraphicType::GetGlobal() -> tTVPGraphicType*
    {
        const auto modbase{ k2a::get_base() };
        if(modbase.ptr == nullptr)
        {
            return nullptr;
        }
        return reinterpret_cast<tTVPGraphicType*>(modbase.uintptr + 0x1AD84B8);
    }

    static void TVPLoadCRX(void* formatdata, void *callbackdata,  tTVPGraphicSizeCallback sizecallback,
           tTVPGraphicScanLineCallback scanlinecallback, tTVPMetaInfoPushCallback metainfopushcallback,
           tTJSBinaryStream *src, tjs_int keyidx, tTVPGraphicLoadMode mode)
    {
        std::vector<uint8_t> buffer{};
        auto size = src->GetSize();
        if(size ==0)
        {
            LOGD("无法获取文件大小");
            return;
        }

        buffer.resize(size);

        if(src->Read(buffer.data(), size) != size)
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
        sizecallback(callbackdata, w, h, gpfRGBA);

        if (mode == glmPalettized && crx.bpp() == 1)  // Palettized mode: pass raw indexed data
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
                void* scanline{ scanlinecallback(callbackdata, y) };
                if (scanline == nullptr)
                {
                    break;
                }
                std::memcpy(scanline, raw.data() + y * src_stride, w);
            }
            scanlinecallback(callbackdata, -1);
            return;
        }

        else if (mode == glmGrayscale && crx.bpp() == 1) // Grayscale mode: convert palette to luminance
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
                void* scanline{ scanlinecallback(callbackdata, y) };
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
            scanlinecallback(callbackdata, -1);
            return;
        }
        else
        {
            // General mode: decode to 32-bit BGRA
            int stride{};
            const uint8_t* src_data{};
            std::vector<uint8_t> pixels{};

            if (!crx.decode(pixels, static_cast<uint8_t>(keyidx)))
            {
                LOGD("CRX: failed to decode");
                return;
            }

            src_data = pixels.data();
            stride   = w * 4;

            for (int y{}; y < h; ++y)
            {
                void* scanline{ scanlinecallback(callbackdata, y) };
                if (scanline == nullptr)
                {
                    break;
                }
                std::memcpy(scanline, src_data + static_cast<size_t>(y) * stride, w * 4);
            }
            scanlinecallback(callbackdata, -1);
        }

    }

    extern "C" JNIEXPORT auto K2A_OnLoad(void* modbase, JavaVM*) -> void
    {
        if(kr2android::init(modbase))
        {
            logd("kr2android init success!\n");
            auto TVPGraphicType = tTVPGraphicType::GetGlobal();
            tTVPGraphicHandlerType crx
            {
                .IsPlugin      = false,
                .Extension     = ".crx",
                .LoadHandler   = TVPLoadCRX,
                .HeaderHandler = nullptr,
                .SaveHandler   = nullptr,
                .AcceptHandler = nullptr,
            };
            TVPGraphicType->Register(crx);
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