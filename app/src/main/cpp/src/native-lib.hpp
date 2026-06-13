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

}