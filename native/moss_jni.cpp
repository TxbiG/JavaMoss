#pragma once

#include <jni.h>

#include <Moss/Moss_stdinc.h>
#include <Moss/Moss_Platform.h>
#include <Moss/Moss_Audio.h>
#include <Moss/Moss_GPU.h>
#include <Moss/Moss_Renderer.h>
#include <Moss/Moss_GUI.h>
#include <Moss/Moss_Physics.h>
#include <Moss/Moss_XR.h>
#include <Moss/Moss_Navigation.h>

#include <Moss/Variants/Vector/Vec2.h>
#include <Moss/Variants/Vector/Vec3.h>
#include <Moss/Variants/Vector/Vec4.h>
#include <Moss/Variants/Matrix/Mat22.h>
#include <Moss/Variants/Matrix/Mat33.h>
#include <Moss/Variants/Matrix/Mat44.h>
#include <Moss/Variants/Color.h>
#include <Moss/Variants/Quat.h>
#include <Moss/Variants/Rect.h>
#include <Moss/Variants/TArray.h>
#include <Moss/Variants/Math/Real.h>

#include <cstdint>

namespace {
Moss_Window *to_window(jlong handle) { return reinterpret_cast<Moss_Window *>(static_cast<intptr_t>(handle)); }
jlong to_handle(Moss_Window *window) { return static_cast<jlong>(reinterpret_cast<intptr_t>(window)); }

class UtfChars final {
public:
    UtfChars(JNIEnv *env, jstring value) : env_(env), value_(value), chars_(value == nullptr ? nullptr : env->GetStringUTFChars(value, nullptr)) {}
    ~UtfChars() { if (chars_ != nullptr) env_->ReleaseStringUTFChars(value_, chars_); }
    UtfChars(const UtfChars &) = delete;
    const char *get() const { return chars_; }
private:
    JNIEnv *env_; jstring value_; const char *chars_;
};

void throw_illegal_argument(JNIEnv *env, const char *message) {
    jclass type = env->FindClass("java/lang/IllegalArgumentException");
    if (type != nullptr) env->ThrowNew(type, message);
}
}

extern "C" {
JNIEXPORT jlong JNICALL Java_dev_moss_MossWindow_nCreate(JNIEnv *env, jclass, jstring title, jint width, jint height) {
    if (title == nullptr || width <= 0 || height <= 0) { throw_illegal_argument(env, "title must be non-null and dimensions must be positive"); return 0; }
    UtfChars native_title(env, title);
    return native_title.get() == nullptr ? 0 : to_handle(Moss_CreateWindow(native_title.get(), width, height, nullptr, nullptr));
}
JNIEXPORT void JNICALL Java_dev_moss_MossWindow_nDestroy(JNIEnv *, jclass, jlong window) { if (window != 0) Moss_TerminateWindow(to_window(window)); }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossWindow_nShouldClose(JNIEnv *, jclass, jlong window) { return window != 0 && Moss_ShouldWindowClose(to_window(window)) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT void JNICALL Java_dev_moss_MossWindow_nRequestClose(JNIEnv *, jclass, jlong window) { if (window != 0) Moss_CloseWindow(to_window(window)); }
JNIEXPORT void JNICALL Java_dev_moss_MossWindow_nSetTitle(JNIEnv *env, jclass, jlong window, jstring title) {
    if (window == 0 || title == nullptr) { throw_illegal_argument(env, "window must be open and title must be non-null"); return; }
    UtfChars native_title(env, title); if (native_title.get() != nullptr) Moss_SetWindowTitle(to_window(window), native_title.get());
}
JNIEXPORT jint JNICALL Java_dev_moss_MossWindow_nWidth(JNIEnv *, jclass) { return Moss_GetWindowWidth(); }
JNIEXPORT jint JNICALL Java_dev_moss_MossWindow_nHeight(JNIEnv *, jclass) { return Moss_GetWindowHeight(); }
JNIEXPORT void JNICALL Java_dev_moss_Moss_nPollEvents(JNIEnv *, jclass) { Moss_PollEvents(); }
JNIEXPORT jboolean JNICALL Java_dev_moss_Moss_nSetClipboardText(JNIEnv *env, jclass, jstring text) {
    if (text == nullptr) { throw_illegal_argument(env, "text must be non-null"); return JNI_FALSE; }
    UtfChars native_text(env, text); return native_text.get() != nullptr && Moss_SetClipboardText(native_text.get()) ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT jstring JNICALL Java_dev_moss_Moss_nGetClipboardText(JNIEnv *env, jclass) { const char *text = Moss_GetClipboardText(); return text == nullptr ? nullptr : env->NewStringUTF(text); }
JNIEXPORT jint JNICALL Java_dev_moss_Moss_nAvailableCpuCores(JNIEnv *, jclass) { return Moss_GetAvailableCPUCores(); }
JNIEXPORT jint JNICALL Java_dev_moss_Moss_nCpuCacheLineSize(JNIEnv *, jclass) { return Moss_GetCPUCacheLineSize(); }
JNIEXPORT jint JNICALL Java_dev_moss_Moss_nSystemRamMiB(JNIEnv *, jclass) { return Moss_GetSystemRAM(); }
}
