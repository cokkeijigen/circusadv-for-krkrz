#pragma once
#include <functional>
#include <jni.h>
#include <android/log.h>
#include <kr2android.hpp>
#include <tjsHashSearch.h>

#define TAG "kr2patch::circusadv"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, "[circusadv] " __VA_ARGS__)
#define logd(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, "[circusadv] " __VA_ARGS__)
#define noinline __attribute__((noinline))

namespace circusadv
{
    enum tTVPGraphicPixelFormat
    {
        gpfLuminance,
        gpfPalette,
        gpfRGB,
        gpfRGBA
    };

    enum tTVPGraphicLoadMode
    {
        glmNormal, // normal, ie. 32bit ARGB graphic
        glmPalettized, // palettized 8bit mode
        glmGrayscale // grayscale 8bit mode
    };

    using tTVPGraphicScanLineCallback = void*(*)(void* callbackdata, tjs_int y);
    using tTVPGraphicSizeCallback     = int  (*)(void* callbackdata, tjs_uint w, tjs_uint h, tTVPGraphicPixelFormat fmt);
    using tTVPMetaInfoPushCallback    = void (*)(void* callbackdata, const ttstr& name, const ttstr& value);

    using tTVPGraphicLoadingHandler = void (*)(
            void* formatdata,
            void* callbackdata,
            tTVPGraphicSizeCallback sizecallback,
            tTVPGraphicScanLineCallback scanlinecallback,
            tTVPMetaInfoPushCallback metainfopushcallback,
            tTJSBinaryStream* src,
            tjs_int32 keyidx,
            tTVPGraphicLoadMode mode
    );

    using tTVPGraphicHeaderLoadingHandler = void (*)(
            void* formatdata,
            tTJSBinaryStream* src,
            iTJSDispatch2** dic
    );

    using tTVPGraphicSaveHandler = void (*)(
            void* formatdata,
            tTJSBinaryStream* dst,
            class iTVPBaseBitmap* image,
            const ttstr& mode,
            iTJSDispatch2* meta
    );

    using tTVPGraphicAcceptSaveHandler = bool (*)(
            void* formatdata,
            const ttstr& type,
            iTJSDispatch2** dic
    );

    struct tTVPImageLoadCommand
    {
        TJS::iTJSDispatch2*	      wner;
        class tTJSNI_Bitmap*	   bmp;
        TJS::ttstr			      path;
        class tTVPTmpBitmapImage* dest;
        TJS::ttstr			    result;
    };

    struct tTVPGraphicHandlerType
    {
        bool IsPlugin;
        TJS::ttstr Extension;
        tTVPGraphicLoadingHandler         LoadHandler;
        tTVPGraphicHeaderLoadingHandler HeaderHandler;
        tTVPGraphicSaveHandler SaveHandler;
        tTVPGraphicAcceptSaveHandler AcceptHandler;
        void *FormatData;
        bool operator == (const tTVPGraphicHandlerType & ref) const;
    };

    struct tTVPGraphicType
    {
        tTJSHashTable<ttstr, tTVPGraphicHandlerType> Hash;
        std::vector<tTVPGraphicHandlerType> Handlers;

        void ReCreateHash();
        void Register(const tTVPGraphicHandlerType& hander);
        void Unregister(const tTVPGraphicHandlerType& hander);
        static auto GetGlobal() -> tTVPGraphicType*;
    };


}