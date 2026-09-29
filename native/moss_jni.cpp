#pragma once

#include <jni.h>
#include <cstdint>

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

namespace {

JavaVM* g_vm = nullptr;

#define DEFINE_GLOBAL_REF(name) static jobject name = nullptr
DEFINE_GLOBAL_REF(g_framebufferResizeCallback);
DEFINE_GLOBAL_REF(g_windowSizeCallback);
DEFINE_GLOBAL_REF(g_windowResizeCallback);
DEFINE_GLOBAL_REF(g_windowPositionCallback);
DEFINE_GLOBAL_REF(g_windowFocusCallback);
DEFINE_GLOBAL_REF(g_windowContentScaleCallback);
DEFINE_GLOBAL_REF(g_monitorCallback);
DEFINE_GLOBAL_REF(g_directoryCallback);
DEFINE_GLOBAL_REF(g_enumerateDirectoryCallback);
DEFINE_GLOBAL_REF(g_dialogCallback);
DEFINE_GLOBAL_REF(g_microphoneCallback);
DEFINE_GLOBAL_REF(g_streamCallback);
#undef DEFINE_GLOBAL_REF

static JNIEnv* callback_env(bool* attached = nullptr) {
    if (attached) *attached = false;
    if (!g_vm) return nullptr;

    JNIEnv* env = nullptr;
    if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) == JNI_OK)
        return env;

#if defined(__ANDROID__) || defined(JNI_VERSION_1_8)
    if (g_vm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) == JNI_OK) {
        if (attached) *attached = true;
        return env;
    }
#endif
    return nullptr;
}

static void release_callback_env(bool attached) {
    if (attached && g_vm)
        g_vm->DetachCurrentThread();
}

static void replace_global_ref(JNIEnv* env, jobject& slot, jobject value) {
    if (slot) {
        env->DeleteGlobalRef(slot);
        slot = nullptr;
    }
    if (value)
        slot = env->NewGlobalRef(value);
}

template <typename T>
T* from_handle(jlong handle) {
    return reinterpret_cast<T*>(static_cast<intptr_t>(handle));
}

template <typename T>
jlong to_handle(T* value) {
    return static_cast<jlong>(reinterpret_cast<intptr_t>(value));
}

class UtfChars final {
public:
    UtfChars(JNIEnv* env, jstring value)
        : env_(env), value_(value), chars_(value ? env->GetStringUTFChars(value, nullptr) : nullptr) {}

    ~UtfChars() {
        if (chars_) env_->ReleaseStringUTFChars(value_, chars_);
    }

    UtfChars(const UtfChars&) = delete;
    UtfChars& operator=(const UtfChars&) = delete;

    const char* get() const { return chars_; }

private:
    JNIEnv* env_;
    jstring value_;
    const char* chars_;
};

static jstring to_jstring(JNIEnv* env, const char* value) {
    return value ? env->NewStringUTF(value) : nullptr;
}

static jobject direct_buffer(JNIEnv* env, void* data, jlong size) {
    if (!data || size <= 0) return nullptr;
    return env->NewDirectByteBuffer(data, size);
}

static bool get_direct_buffer(JNIEnv* env, jobject buffer, void** out, jlong min_size = 0) {
    if (!buffer || !out) return false;
    void* p = env->GetDirectBufferAddress(buffer);
    jlong size = env->GetDirectBufferCapacity(buffer);
    if (!p || (min_size > 0 && size < min_size)) return false;
    *out = p;
    return true;
}

static jlongArray make_jlong_array(JNIEnv* env, const std::vector<jlong>& values) {
    jlongArray out = env->NewLongArray(static_cast<jsize>(values.size()));
    if (out && !values.empty())
        env->SetLongArrayRegion(out, 0, static_cast<jsize>(values.size()), values.data());
    return out;
}

static jobjectArray make_string_array(JNIEnv* env, const std::vector<std::string>& values) {
    jclass string_class = env->FindClass("java/lang/String");
    if (!string_class) return nullptr;
    jobjectArray out = env->NewObjectArray(static_cast<jsize>(values.size()), string_class, nullptr);
    for (jsize i = 0; out && i < static_cast<jsize>(values.size()); ++i)
        env->SetObjectArrayElement(out, i, env->NewStringUTF(values[static_cast<size_t>(i)].c_str()));
    return out;
}

static jobjectArray path_info_to_java(JNIEnv* env, const Moss_PathInfo& info) {
    // [type, size, createTime, modifyTime, accessTime, readable, writable, executable]
    jlong values[8] = {
        static_cast<jlong>(info.type),
        static_cast<jlong>(info.size),
        static_cast<jlong>(info.create_time),
        static_cast<jlong>(info.modify_time),
        static_cast<jlong>(info.access_time),
        info.readable ? 1 : 0,
        info.writable ? 1 : 0,
        info.executable ? 1 : 0
    };

    jclass long_class = env->FindClass("java/lang/Long");
    if (!long_class) return nullptr;
    jmethodID value_of = env->GetStaticMethodID(long_class, "valueOf", "(J)Ljava/lang/Long;");
    if (!value_of) return nullptr;

    jobjectArray out = env->NewObjectArray(8, long_class, nullptr);
    for (jsize i = 0; out && i < 8; ++i) {
        jobject v = env->CallStaticObjectMethod(long_class, value_of, values[i]);
        env->SetObjectArrayElement(out, i, v);
        env->DeleteLocalRef(v);
    }
    return out;
}

static void throw_illegal(JNIEnv* env, const char* message) {
    jclass type = env->FindClass("java/lang/IllegalArgumentException");
    if (type) env->ThrowNew(type, message);
}

static void throw_state(JNIEnv* env, const char* message) {
    jclass type = env->FindClass("java/lang/IllegalStateException");
    if (type) env->ThrowNew(type, message);
}

static bool invoke_void_II(jobject callback, int a, int b) {
    if (!callback) return true;
    bool attached = false;
    JNIEnv* env = callback_env(&attached);
    if (!env) return false;
    jclass cls = env->GetObjectClass(callback);
    jmethodID mid = env->GetMethodID(cls, "invoke", "(II)V");
    if (mid) env->CallVoidMethod(callback, mid, static_cast<jint>(a), static_cast<jint>(b));
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(cls);
    release_callback_env(attached);
    return mid != nullptr;
}

static bool invoke_void_FF(jobject callback, float a, float b) {
    if (!callback) return true;
    bool attached = false;
    JNIEnv* env = callback_env(&attached);
    if (!env) return false;
    jclass cls = env->GetObjectClass(callback);
    jmethodID mid = env->GetMethodID(cls, "invoke", "(FF)V");
    if (mid) env->CallVoidMethod(callback, mid, static_cast<jfloat>(a), static_cast<jfloat>(b));
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(cls);
    release_callback_env(attached);
    return mid != nullptr;
}

static bool invoke_void_I(jobject callback, bool value) {
    if (!callback) return true;
    bool attached = false;
    JNIEnv* env = callback_env(&attached);
    if (!env) return false;
    jclass cls = env->GetObjectClass(callback);
    jmethodID mid = env->GetMethodID(cls, "invoke", "(Z)V");
    if (mid) env->CallVoidMethod(callback, mid, value ? JNI_TRUE : JNI_FALSE);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(cls);
    release_callback_env(attached);
    return mid != nullptr;
}

static void framebuffer_resize_cb(int width, int height) {
    (void)invoke_void_II(g_framebufferResizeCallback, width, height);
}
static void window_size_cb(int width, int height) {
    (void)invoke_void_II(g_windowSizeCallback, width, height);
}
static void window_resize_cb(int width, int height) {
    (void)invoke_void_II(g_windowResizeCallback, width, height);
}
static void window_position_cb(int x, int y) {
    (void)invoke_void_II(g_windowPositionCallback, x, y);
}
static void window_focus_cb(bool focused) {
    invoke_void_I(g_windowFocusCallback, focused);
}
static void window_scale_cb(float xscale, float yscale) {
    invoke_void_FF(g_windowContentScaleCallback, xscale, yscale);
}
static void monitor_cb(const char* name, bool connected) {
    if (!g_monitorCallback) return;
    bool attached = false;
    JNIEnv* env = callback_env(&attached);
    if (!env) return;
    jclass cls = env->GetObjectClass(g_monitorCallback);
    jmethodID mid = env->GetMethodID(cls, "invoke", "(Ljava/lang/String;Z)V");
    if (mid) {
        jstring jname = to_jstring(env, name);
        env->CallVoidMethod(g_monitorCallback, mid, jname, connected ? JNI_TRUE : JNI_FALSE);
        if (jname) env->DeleteLocalRef(jname);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(cls);
    release_callback_env(attached);
}

static bool directory_cb(const Moss_PathInfo* info, const char* path, void*) {
    if (!g_directoryCallback) return true;
    bool attached = false;
    JNIEnv* env = callback_env(&attached);
    if (!env) return false;
    jclass cls = env->GetObjectClass(g_directoryCallback);
    jmethodID mid = env->GetMethodID(cls, "invoke", "(Ljava/nio/ByteBuffer;Ljava/lang/String;)Z");
    bool result = true;
    if (mid) {
        jobject info_buffer = direct_buffer(env, const_cast<Moss_PathInfo*>(info), sizeof(Moss_PathInfo));
        jstring jpath = to_jstring(env, path);
        result = env->CallBooleanMethod(g_directoryCallback, mid, info_buffer, jpath) == JNI_TRUE;
        if (jpath) env->DeleteLocalRef(jpath);
        if (info_buffer) env->DeleteLocalRef(info_buffer);
    }
    if (env->ExceptionCheck()) { env->ExceptionClear(); result = false; }
    env->DeleteLocalRef(cls);
    release_callback_env(attached);
    return result;
}

static bool enumerate_directory_cb(const Moss_PathInfo* info, const char* path, void*) {
    if (!g_enumerateDirectoryCallback) return true;
    bool attached = false;
    JNIEnv* env = callback_env(&attached);
    if (!env) return false;
    jclass cls = env->GetObjectClass(g_enumerateDirectoryCallback);
    jmethodID mid = env->GetMethodID(cls, "invoke", "(Ljava/nio/ByteBuffer;Ljava/lang/String;)Z");
    bool result = true;
    if (mid) {
        jobject info_buffer = direct_buffer(env, const_cast<Moss_PathInfo*>(info), sizeof(Moss_PathInfo));
        jstring jpath = to_jstring(env, path);
        result = env->CallBooleanMethod(g_enumerateDirectoryCallback, mid, info_buffer, jpath) == JNI_TRUE;
        if (jpath) env->DeleteLocalRef(jpath);
        if (info_buffer) env->DeleteLocalRef(info_buffer);
    }
    if (env->ExceptionCheck()) { env->ExceptionClear(); result = false; }
    env->DeleteLocalRef(cls);
    release_callback_env(attached);
    return result;
}

static void dialog_cb(void*, const char* const* filelist, int filter) {
    if (!g_dialogCallback) return;
    bool attached = false;
    JNIEnv* env = callback_env(&attached);
    if (!env) return;
    std::vector<std::string> files;
    if (filelist) {
        for (const char* const* p = filelist; *p; ++p)
            files.emplace_back(*p);
    }
    jobjectArray arr = make_string_array(env, files);
    jclass cls = env->GetObjectClass(g_dialogCallback);
    jmethodID mid = env->GetMethodID(cls, "invoke", "([Ljava/lang/String;I)V");
    if (mid) env->CallVoidMethod(g_dialogCallback, mid, arr, static_cast<jint>(filter));
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (arr) env->DeleteLocalRef(arr);
    env->DeleteLocalRef(cls);
    release_callback_env(attached);
}

static void microphone_cb(const float* buffer, int samples, void*) {
    if (!g_microphoneCallback) return;
    bool attached = false;
    JNIEnv* env = callback_env(&attached);
    if (!env) return;
    jclass cls = env->GetObjectClass(g_microphoneCallback);
    jmethodID mid = env->GetMethodID(cls, "invoke", "(Ljava/nio/ByteBuffer;I)V");
    if (mid) {
        jobject data = direct_buffer(env, const_cast<float*>(buffer), static_cast<jlong>(samples) * static_cast<jlong>(sizeof(float)));
        env->CallVoidMethod(g_microphoneCallback, mid, data, static_cast<jint>(samples));
        if (data) env->DeleteLocalRef(data);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(cls);
    release_callback_env(attached);
}

static void stream_cb(float* buffer, int frames, void*) {
    if (!g_streamCallback) return;
    bool attached = false;
    JNIEnv* env = callback_env(&attached);
    if (!env) return;
    jclass cls = env->GetObjectClass(g_streamCallback);
    jmethodID mid = env->GetMethodID(cls, "invoke", "(Ljava/nio/ByteBuffer;I)V");
    if (mid) {
        jobject data = direct_buffer(env, buffer, static_cast<jlong>(frames) * static_cast<jlong>(sizeof(float)));
        env->CallVoidMethod(g_streamCallback, mid, data, static_cast<jint>(frames));
        if (data) env->DeleteLocalRef(data);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(cls);
    release_callback_env(attached);
}

static std::vector<Moss_DialogFileFilter> make_filters(JNIEnv* env, jobjectArray names, jobjectArray patterns,
                                                        std::vector<UtfChars>& name_chars,
                                                        std::vector<UtfChars>& pattern_chars) {
    const jsize count = names ? env->GetArrayLength(names) : 0;
    const jsize pattern_count = patterns ? env->GetArrayLength(patterns) : 0;
    if (count != pattern_count) return {};

    name_chars.reserve(static_cast<size_t>(count));
    pattern_chars.reserve(static_cast<size_t>(count));
    std::vector<Moss_DialogFileFilter> filters;
    filters.reserve(static_cast<size_t>(count));
    for (jsize i = 0; i < count; ++i) {
        jstring jname = static_cast<jstring>(env->GetObjectArrayElement(names, i));
        jstring jpattern = static_cast<jstring>(env->GetObjectArrayElement(patterns, i));
        name_chars.emplace_back(env, jname);
        pattern_chars.emplace_back(env, jpattern);
        filters.push_back({name_chars.back().get(), pattern_chars.back().get()});
        if (jname) env->DeleteLocalRef(jname);
        if (jpattern) env->DeleteLocalRef(jpattern);
    }
    return filters;
}

static std::vector<std::string> c_string_array(char** values, int count) {
    std::vector<std::string> out;
    if (!values || count <= 0) return out;
    out.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        if (values[i]) out.emplace_back(values[i]);
    return out;
}

} // namespace

extern "C" {

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;
    return JNI_VERSION_1_8;
}

// -----------------------------------------------------------------------------
// Window / platform core
// -----------------------------------------------------------------------------
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nCreateWindow(JNIEnv* env, jclass,
    jstring title, jint width, jint height, jlong monitor, jlong share) {
    if (!title || width <= 0 || height <= 0) { throw_illegal(env, "invalid window arguments"); return 0; }
    UtfChars t(env, title);
    return to_handle(Moss_CreateWindow(t.get(), width, height,
        from_handle<Moss_Monitor>(monitor), from_handle<Moss_Window>(share)));
}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nTerminateWindow(JNIEnv*, jclass, jlong window) {
    if (window) Moss_TerminateWindow(from_handle<Moss_Window>(window));
}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nCreateMessageBox(JNIEnv* env, jclass,
    jstring title, jstring message, jint flags, jlong window) {
    if (!title || !message) { throw_illegal(env, "title and message must be non-null"); return JNI_FALSE; }
    UtfChars t(env, title), m(env, message);
    return Moss_CreateMessageBox(t.get(), m.get(), static_cast<Moss_MessageBoxFlags>(flags),
        from_handle<Moss_Window>(window)) ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nShouldWindowClose(JNIEnv*, jclass, jlong window) {
    return window && Moss_ShouldWindowClose(from_handle<Moss_Window>(window)) ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nPollEvents(JNIEnv*, jclass) { Moss_PollEvents(); }
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetWindowWidth(JNIEnv*, jclass) { return Moss_GetWindowWidth(); }
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetWindowHeight(JNIEnv*, jclass) { return Moss_GetWindowHeight(); }
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetWindowTitle(JNIEnv* env, jclass, jlong window, jstring title) {
    if (!window || !title) { throw_illegal(env, "window and title must be non-null"); return; }
    UtfChars t(env, title); Moss_SetWindowTitle(from_handle<Moss_Window>(window), t.get());
}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nSetWindowIcon(JNIEnv* env, jclass,
    jlong window, jint width, jint height, jobject pixels) {
    void* data = nullptr;
    if (!window || width < 0 || height < 0 || !get_direct_buffer(env, pixels, &data)) {
        throw_illegal(env, "invalid image arguments"); return JNI_FALSE;
    }
    Moss_Image image{width, height, static_cast<unsigned char*>(data)};
    Moss_SetWindowIcon(from_handle<Moss_Window>(window), image);
    return JNI_TRUE;
}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nCloseWindow(JNIEnv*, jclass, jlong window) {
    if (window) Moss_CloseWindow(from_handle<Moss_Window>(window));
}

// -----------------------------------------------------------------------------
// Monitor / input
// -----------------------------------------------------------------------------
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nMonitorGetPrimary(JNIEnv*, jclass) { return to_handle(Moss_MonitorGetPrimary()); }
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nMonitorGetSecondary(JNIEnv*, jclass) { return to_handle(Moss_MonitorGetSecondary()); }
JNIEXPORT jintArray JNICALL Java_dev_moss_MossNative_nMonitorGetPhysicalSize(JNIEnv* env, jclass, jlong monitor) {
    int w=0,h=0; Moss_MonitorGetPhysicalSize(from_handle<Moss_Monitor>(monitor), &w, &h);
    jint v[2]={w,h}; jintArray out=env->NewIntArray(2); if(out) env->SetIntArrayRegion(out,0,2,v); return out;
}
JNIEXPORT jfloatArray JNICALL Java_dev_moss_MossNative_nMonitorGetContentScale(JNIEnv* env, jclass, jlong monitor) {
    float x=0,y=0; Moss_MonitorGetContentScale(from_handle<Moss_Monitor>(monitor), &x, &y);
    jfloat v[2]={x,y}; jfloatArray out=env->NewFloatArray(2); if(out) env->SetFloatArrayRegion(out,0,2,v); return out;
}
JNIEXPORT jintArray JNICALL Java_dev_moss_MossNative_nMonitorGetPosition(JNIEnv* env, jclass, jlong monitor) {
    int x=0,y=0; Moss_MonitorGetPosition(from_handle<Moss_Monitor>(monitor), &x, &y);
    jint v[2]={x,y}; jintArray out=env->NewIntArray(2); if(out) env->SetIntArrayRegion(out,0,2,v); return out;
}
JNIEXPORT jstring JNICALL Java_dev_moss_MossNative_nMonitorGetName(JNIEnv* env, jclass, jlong monitor) { return to_jstring(env, Moss_MonitorGetName(from_handle<Moss_Monitor>(monitor))); }
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nMonitorSetGammaRamp(JNIEnv* env, jclass, jlong monitor,
    jobject size, jobject red, jobject green, jobject blue) {
    void *s=nullptr,*r=nullptr,*g=nullptr,*b=nullptr;
    if (!get_direct_buffer(env,size,&s) || !get_direct_buffer(env,red,&r) || !get_direct_buffer(env,green,&g) || !get_direct_buffer(env,blue,&b)) {
        throw_illegal(env,"all gamma ramp buffers must be direct buffers"); return;
    }
    Moss_GammaRamp ramp{static_cast<uint8_t*>(s),static_cast<uint8_t*>(r),static_cast<uint8_t*>(g),static_cast<uint8_t*>(b)};
    Moss_MonitorSetGammaRamp(from_handle<Moss_Monitor>(monitor), &ramp);
}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nMonitorGetGammaRamp(JNIEnv*, jclass, jlong monitor) { return to_handle(Moss_MonitorGetGammaRamp(from_handle<Moss_Monitor>(monitor))); }
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nMonitorSetGamma(JNIEnv*, jclass, jlong monitor, jfloat gamma) { Moss_MonitorSetGamma(from_handle<Moss_Monitor>(monitor), gamma); }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nIsKeyPressed(JNIEnv*, jclass, jint key) { return Moss_IsKeyPressed(static_cast<Keyboard>(key)) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nIsReleased(JNIEnv*, jclass, jint key) { return Moss_IsReleased(static_cast<Keyboard>(key)) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nIsKeyJustPressed(JNIEnv*, jclass, jint key) { return Moss_IsKeyJustPressed(static_cast<Keyboard>(key)) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nIsKeyJustReleased(JNIEnv*, jclass, jint key) { return Moss_IsKeyJustReleased(static_cast<Keyboard>(key)) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nInputGetKey(JNIEnv*, jclass) { return static_cast<jint>(Moss_InputGetKey()); }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nIsMousePressed(JNIEnv*, jclass, jint b) { return Moss_IsMousePressed(static_cast<Mouse>(b)) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nIsMouseReleased(JNIEnv*, jclass, jint b) { return Moss_IsMouseReleased(static_cast<Mouse>(b)) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nIsMouseJustPressed(JNIEnv*, jclass, jint b) { return Moss_IsMouseJustPressed(static_cast<Mouse>(b)) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nIsMouseJustReleased(JNIEnv*, jclass, jint b) { return Moss_IsMouseJustReleased(static_cast<Mouse>(b)) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nInputGetMouseButton(JNIEnv*, jclass) { return static_cast<jint>(Moss_InputGetMouseButton()); }
JNIEXPORT jintArray JNICALL Java_dev_moss_MossNative_nGetMousePosition(JNIEnv* env, jclass) { int x=0,y=0; Moss_GetMousePosition(&x,&y); jint v[2]={x,y}; jintArray a=env->NewIntArray(2); if(a) env->SetIntArrayRegion(a,0,2,v); return a; }
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetMousePosition(JNIEnv*, jclass, jint x, jint y) { Moss_SetMousePosition(x,y); }
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetMouseVisible(JNIEnv*, jclass, jboolean visible) { Moss_SetMouseVisible(visible==JNI_TRUE); }

// -----------------------------------------------------------------------------
// Gamepad / touch / pen
// -----------------------------------------------------------------------------
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetNumGamepads(JNIEnv*, jclass) { return Moss_GetNumGamepads(); }
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nOpenGamepad(JNIEnv*, jclass, jlong id) { return to_handle(Moss_OpenGamepad(static_cast<Moss_GamepadID>(id))); }
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nCloseGamepad(JNIEnv*, jclass, jlong gp) { if(gp) Moss_CloseGamepad(from_handle<Moss_Gamepad>(gp)); }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nGamepadConnected(JNIEnv*, jclass, jlong gp) { return Moss_GamepadConnected(from_handle<Moss_Gamepad>(gp)) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nUpdateGamepads(JNIEnv*, jclass) { Moss_UpdateGamepads(); }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nIsGamepadButtonPressed(JNIEnv*, jclass, jlong gp, jint button) { return Moss_IsGamepadButtonPressed(from_handle<Moss_Gamepad>(gp), static_cast<Moss_GamepadButton>(button)) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nIsGamepadButtonJustPressed(JNIEnv*, jclass, jlong gp, jint button) { return Moss_IsGamepadButtonJustPressed(from_handle<Moss_Gamepad>(gp), static_cast<Moss_GamepadButton>(button)) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nIsGamepadButtonJustReleased(JNIEnv*, jclass, jlong gp, jint button) { return Moss_IsGamepadButtonJustReleased(from_handle<Moss_Gamepad>(gp), static_cast<Moss_GamepadButton>(button)) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jfloat JNICALL Java_dev_moss_MossNative_nGetGamepadAxis(JNIEnv*, jclass, jlong gp, jint axis) { return Moss_GetGamepadAxis(from_handle<Moss_Gamepad>(gp), static_cast<GamepadAxis>(axis)); }
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetGamepadAxisDeadzone(JNIEnv*, jclass, jint axis, jfloat dz) { Moss_SetGamepadAxisDeadzone(static_cast<GamepadAxis>(axis), dz); }
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetGamepadAxisInverted(JNIEnv*, jclass, jint axis, jboolean inverted) { Moss_SetGamepadAxisInverted(static_cast<GamepadAxis>(axis), inverted==JNI_TRUE); }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nRumbleGamepad(JNIEnv*, jclass, jlong gp, jint low, jint high, jlong duration) { return Moss_RumbleGamepad(from_handle<Moss_Gamepad>(gp), static_cast<uint16_t>(low), static_cast<uint16_t>(high), static_cast<uint32_t>(duration)) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nRumbleGamepadTriggers(JNIEnv*, jclass, jlong gp, jint left, jint right, jlong duration) { return Moss_RumbleGamepadTriggers(from_handle<Moss_Gamepad>(gp), static_cast<uint16_t>(left), static_cast<uint16_t>(right), static_cast<uint32_t>(duration)) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nSetGamepadLED(JNIEnv*, jclass, jlong gp, jint r, jint g, jint b) { return Moss_SetGamepadLED(from_handle<Moss_Gamepad>(gp), static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)) ? JNI_TRUE : JNI_FALSE; }
JNIEXPORT jstring JNICALL Java_dev_moss_MossNative_nGetGamepadName(JNIEnv* env, jclass, jlong gp) { return to_jstring(env, Moss_GetGamepadName(from_handle<Moss_Gamepad>(gp))); }
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nGetGamepadID(JNIEnv*, jclass, jlong gp) { return static_cast<jlong>(Moss_GetGamepadID(from_handle<Moss_Gamepad>(gp))); }
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetGamepadPlayerIndex(JNIEnv*, jclass, jlong gp) { return Moss_GetGamepadPlayerIndex(from_handle<Moss_Gamepad>(gp)); }
JNIEXPORT jintArray JNICALL Java_dev_moss_MossNative_nGetGamepadPowerInfo(JNIEnv* env, jclass, jlong gp) { int pct=0; int state=static_cast<int>(Moss_GetGamepadPowerInfo(from_handle<Moss_Gamepad>(gp),&pct)); jint v[2]={state,pct}; jintArray a=env->NewIntArray(2); if(a) env->SetIntArrayRegion(a,0,2,v); return a; }
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetNumGamepadTouchpads(JNIEnv*, jclass, jlong gp) { return Moss_GetNumGamepadTouchpads(from_handle<Moss_Gamepad>(gp)); }
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetNumGamepadTouchpadFingers(JNIEnv*, jclass, jlong gp) { return Moss_GetNumGamepadTouchpadFingers(from_handle<Moss_Gamepad>(gp)); }
JNIEXPORT jfloatArray JNICALL Java_dev_moss_MossNative_nGetGamepadTouchpadFinger(JNIEnv* env, jclass, jlong gp, jint pad, jint finger) { bool down=false; float x=0,y=0,p=0; Moss_GetGamepadTouchpadFinger(from_handle<Moss_Gamepad>(gp),pad,finger,&down,&x,&y,&p); jfloat v[4]={down?1.0f:0.0f,x,y,p}; jfloatArray a=env->NewFloatArray(4); if(a) env->SetFloatArrayRegion(a,0,4,v); return a; }
JNIEXPORT jstring JNICALL Java_dev_moss_MossNative_nGetGamepadMapping(JNIEnv* env, jclass, jlong gp) { return to_jstring(env, Moss_GetGamepadMapping(from_handle<Moss_Gamepad>(gp))); }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nSetGamepadMapping(JNIEnv* env, jclass, jlong gp, jstring mapping) { if(!mapping){throw_illegal(env,"mapping must be non-null");return JNI_FALSE;} UtfChars m(env,mapping); return Moss_SetGamepadMapping(from_handle<Moss_Gamepad>(gp),m.get())?JNI_TRUE:JNI_FALSE; }
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nReloadGamepadMappings(JNIEnv*, jclass) { Moss_ReloadGamepadMappings(); }
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nInputGetGamepadButton(JNIEnv*, jclass) { return static_cast<jint>(Moss_InputGetGamepadButton()); }
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nInputGetGamepadAxis(JNIEnv*, jclass) { return static_cast<jint>(Moss_InputGetGamepadAxis()); }
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetPenDeviceType(JNIEnv*, jclass, jlong id) { return static_cast<jint>(Moss_GetPenDeviceType(static_cast<Moss_PenID>(id))); }
JNIEXPORT jstring JNICALL Java_dev_moss_MossNative_nGetTouchDeviceName(JNIEnv* env, jclass, jlong id) { return to_jstring(env, Moss_GetTouchDeviceName(static_cast<Moss_TouchID>(id))); }
JNIEXPORT jlongArray JNICALL Java_dev_moss_MossNative_nGetTouchDevices(JNIEnv* env, jclass) { int count=0; Moss_TouchID* ids=Moss_GetTouchDevices(&count); std::vector<jlong> v; if(ids && count>0){v.reserve(count);for(int i=0;i<count;++i)v.push_back(static_cast<jlong>(ids[i]));} return make_jlong_array(env,v); }
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetTouchDeviceType(JNIEnv*, jclass, jlong id) { return static_cast<jint>(Moss_GetTouchDeviceType(static_cast<Moss_TouchID>(id))); }
JNIEXPORT jlongArray JNICALL Java_dev_moss_MossNative_nGetTouchFingers(JNIEnv* env, jclass, jlong id) { int count=0; Moss_Finger** fingers=Moss_GetTouchFingers(static_cast<Moss_TouchID>(id),&count); std::vector<jlong> v; if(fingers && count>0){v.reserve(count);for(int i=0;i<count;++i)v.push_back(to_handle(fingers[i]));} return make_jlong_array(env,v); }

// -----------------------------------------------------------------------------
// Haptics
// -----------------------------------------------------------------------------
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nOpenHaptic(JNIEnv*, jclass, jlong id) { return to_handle(Moss_OpenHaptic(static_cast<Moss_HapticID>(id))); }
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nCloseHaptic(JNIEnv*, jclass, jlong h) { if(h) Moss_CloseHaptic(from_handle<Moss_Haptic>(h)); }
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nCreateHapticEffect(JNIEnv*, jclass, jlong h) { return static_cast<jlong>(Moss_CreateHapticEffect(from_handle<Moss_Haptic>(h))); }
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nDestroyHapticEffect(JNIEnv*, jclass, jlong h) { Moss_DestroyHapticEffect(from_handle<Moss_Haptic>(h)); }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nGetHapticEffectStatus(JNIEnv*, jclass, jlong h) { return Moss_GetHapticEffectStatus(from_handle<Moss_Haptic>(h))?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nGetHapticFeatures(JNIEnv*, jclass, jlong h) { return static_cast<jlong>(Moss_GetHapticFeatures(from_handle<Moss_Haptic>(h))); }
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nGetHapticFromID(JNIEnv*, jclass, jlong h) { return to_handle(Moss_GetHapticFromID(from_handle<Moss_Haptic>(h))); }
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nGetHapticID(JNIEnv*, jclass, jlong h) { return to_handle(Moss_GetHapticID(from_handle<Moss_Haptic>(h))); }
JNIEXPORT jstring JNICALL Java_dev_moss_MossNative_nGetHapticName(JNIEnv* env, jclass, jlong h) { return to_jstring(env,Moss_GetHapticName(from_handle<Moss_Haptic>(h))); }
JNIEXPORT jstring JNICALL Java_dev_moss_MossNative_nGetHapticNameForID(JNIEnv* env, jclass, jlong h) { return to_jstring(env,Moss_GetHapticNameForID(from_handle<Moss_Haptic>(h))); }
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nGetHaptics(JNIEnv*, jclass, jlong h) { return to_handle(Moss_GetHaptics(from_handle<Moss_Haptic>(h))); }
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetMaxHapticEffects(JNIEnv*, jclass, jlong h) { return Moss_GetMaxHapticEffects(from_handle<Moss_Haptic>(h)); }
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetMaxHapticEffectsPlaying(JNIEnv*, jclass, jlong h) { return Moss_GetMaxHapticEffectsPlaying(from_handle<Moss_Haptic>(h)); }
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetNumHapticAxes(JNIEnv*, jclass, jlong h) { return Moss_GetNumHapticAxes(from_handle<Moss_Haptic>(h)); }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nHapticEffectSupported(JNIEnv*, jclass, jlong h) { return Moss_HapticEffectSupported(from_handle<Moss_Haptic>(h))?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nHapticRumbleSupported(JNIEnv*, jclass, jlong h) { return Moss_HapticRumbleSupported(from_handle<Moss_Haptic>(h))?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nInitHapticRumble(JNIEnv*, jclass, jlong h) { return Moss_InitHapticRumble(from_handle<Moss_Haptic>(h))?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nIsJoystickHaptic(JNIEnv*, jclass, jlong joystick) { return Moss_IsJoystickHaptic(from_handle<Moss_GamepadAxis>(joystick))?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nIsMouseHaptic(JNIEnv*, jclass) { return Moss_IsMouseHaptic()?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nOpenHapticFromJoystick(JNIEnv*, jclass, jlong joystick) { return to_handle(Moss_OpenHapticFromJoystick(from_handle<Moss_GamepadAxis>(joystick))); }
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nOpenHapticFromMouse(JNIEnv*, jclass) { return to_handle(Moss_OpenHapticFromMouse()); }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nPauseHaptic(JNIEnv*, jclass, jlong h) { return Moss_PauseHaptic(from_handle<Moss_Haptic>(h))?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nPlayHapticRumble(JNIEnv*, jclass, jlong h, jfloat strength, jlong length) { return Moss_PlayHapticRumble(from_handle<Moss_Haptic>(h),strength,static_cast<uint32_t>(length))?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nResumeHaptic(JNIEnv*, jclass, jlong h) { return Moss_ResumeHaptic(from_handle<Moss_Haptic>(h))?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nRunHapticEffect(JNIEnv*, jclass, jlong h, jlong iterations) { return Moss_RunHapticEffect(from_handle<Moss_Haptic>(h),static_cast<uint32_t>(iterations))?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nSetHapticAutocenter(JNIEnv*, jclass, jlong h, jint center) { return Moss_SetHapticAutocenter(from_handle<Moss_Haptic>(h),center)?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nSetHapticGain(JNIEnv*, jclass, jlong h, jint gain) { return Moss_SetHapticGain(from_handle<Moss_Haptic>(h),gain)?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nStopHapticEffect(JNIEnv*, jclass, jlong h, jlong effect) { return Moss_StopHapticEffect(from_handle<Moss_Haptic>(h),static_cast<Moss_HapticEffectID>(effect))?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nStopHapticEffects(JNIEnv*, jclass, jlong h) { return Moss_StopHapticEffects(from_handle<Moss_Haptic>(h))?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nStopHapticRumble(JNIEnv*, jclass, jlong h) { return Moss_StopHapticRumble(from_handle<Moss_Haptic>(h))?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nUpdateHapticEffect(JNIEnv* env, jclass, jlong h, jlong effect, jobject data) {
    void* p=nullptr; if(!get_direct_buffer(env,data,&p,sizeof(Moss_HapticEffect))){throw_illegal(env,"haptic effect must be a direct buffer large enough for Moss_HapticEffect");return JNI_FALSE;}
    return Moss_UpdateHapticEffect(from_handle<Moss_Haptic>(h),static_cast<Moss_HapticEffectID>(effect),static_cast<const Moss_HapticEffect*>(p))?JNI_TRUE:JNI_FALSE;
}

// -----------------------------------------------------------------------------
// CPU / OS / dynamic library
// -----------------------------------------------------------------------------
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetAvailableCPUCores(JNIEnv*, jclass) { return Moss_GetAvailableCPUCores(); }
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetCPUCacheLineSize(JNIEnv*, jclass) { return Moss_GetCPUCacheLineSize(); }
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetSystemRAM(JNIEnv*, jclass) { return Moss_GetSystemRAM(); }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nOpenURL(JNIEnv* env, jclass, jstring url) { if(!url){throw_illegal(env,"url must be non-null");return JNI_FALSE;} UtfChars u(env,url); return Moss_OpenURL(u.get())?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jobjectArray JNICALL Java_dev_moss_MossNative_nGetLocale(JNIEnv* env, jclass) { Moss_Locale* l=Moss_GetLocale(); std::vector<std::string> v; if(l){v.push_back(l->country?l->country:"");v.push_back(l->language?l->language:"");} return make_string_array(env,v); }
JNIEXPORT jintArray JNICALL Java_dev_moss_MossNative_nGetPowerInfo(JNIEnv* env, jclass) { int s=0,p=0; int state=static_cast<int>(Moss_GetPowerInfo(&s,&p)); jint v[3]={state,s,p}; jintArray a=env->NewIntArray(3); if(a) env->SetIntArrayRegion(a,0,3,v); return a; }
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nIsProcessRunningByName(JNIEnv* env, jclass, jstring path) { if(!path){throw_illegal(env,"path must be non-null");return JNI_FALSE;} UtfChars p(env,path); return Moss_IsProcessRunningByName(p.get())?JNI_TRUE:JNI_FALSE; }
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nLoadDynamicLibrary(JNIEnv* env, jclass, jstring path) { if(!path){throw_illegal(env,"path must be non-null");return 0;} UtfChars p(env,path); return to_handle(Moss_LoadDynamicLibrary(p.get())); }
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nGetLibrarySymbol(JNIEnv* env, jclass, jlong handle, jstring name) { if(!name){throw_illegal(env,"symbol name must be non-null");return 0;} UtfChars n(env,name); return to_handle(Moss_GetLibrarySymbol(from_handle<void>(handle),n.get())); }
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nUnloadDynamicLibrary(JNIEnv*, jclass, jlong handle) { if(handle) Moss_UnloadDynamicLibrary(from_handle<void>(handle)); }

// -----------------------------------------------------------------------------
// Callback registration
// Java callback objects must expose invoke(...) with the documented signature.
// -----------------------------------------------------------------------------
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetFramebufferResizeCallback(JNIEnv* env,jclass,jobject cb){replace_global_ref(env,g_framebufferResizeCallback,cb);Moss_SetFramebufferResizeCallback(cb?[](int w,int h){framebuffer_resize_cb(w,h);}:nullptr);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetWindowSizeCallback(JNIEnv* env,jclass,jobject cb){replace_global_ref(env,g_windowSizeCallback,cb);Moss_SetWindowSizeCallback(cb?[](int w,int h){window_size_cb(w,h);}:nullptr);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetWindowResizeCallback(JNIEnv* env,jclass,jobject cb){replace_global_ref(env,g_windowResizeCallback,cb);Moss_SetWindowResizeCallback(cb?[](int w,int h){window_resize_cb(w,h);}:nullptr);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetWindowPositionCallback(JNIEnv* env,jclass,jobject cb){replace_global_ref(env,g_windowPositionCallback,cb);Moss_SetWindowPositionCallback(cb?[](int x,int y){window_position_cb(x,y);}:nullptr);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetWindowFocusCallback(JNIEnv* env,jclass,jobject cb){replace_global_ref(env,g_windowFocusCallback,cb);Moss_SetWindowFocusCallback(cb?window_focus_cb:nullptr);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetWindowContentScaleCallback(JNIEnv* env,jclass,jobject cb){replace_global_ref(env,g_windowContentScaleCallback,cb);Moss_SetWindowContentScaleCallback(cb?window_scale_cb:nullptr);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetMonitorCallback(JNIEnv* env,jclass,jobject cb){replace_global_ref(env,g_monitorCallback,cb);Moss_SetMonitorCallback(cb?monitor_cb:nullptr);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetDirectoryCallback(JNIEnv* env,jclass,jobject cb){replace_global_ref(env,g_directoryCallback,cb);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetEnumerateDirectoryCallback(JNIEnv* env,jclass,jobject cb){replace_global_ref(env,g_enumerateDirectoryCallback,cb);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetDialogCallback(JNIEnv* env,jclass,jobject cb){replace_global_ref(env,g_dialogCallback,cb);}

// -----------------------------------------------------------------------------
// Camera
// -----------------------------------------------------------------------------
JNIEXPORT jlongArray JNICALL Java_dev_moss_MossNative_nGetCameras(JNIEnv* env,jclass){int count=0;Moss_CameraID* ids=Moss_GetCameras(&count);std::vector<jlong> v;if(ids&&count>0){v.reserve(count);for(int i=0;i<count;++i)v.push_back(static_cast<jlong>(ids[i]));}return make_jlong_array(env,v);}
JNIEXPORT jstring JNICALL Java_dev_moss_MossNative_nGetCameraName(JNIEnv* env,jclass,jlong id){return to_jstring(env,Moss_GetCameraName(static_cast<Moss_CameraID>(id)));}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetCameraPosition(JNIEnv*,jclass,jlong id){return static_cast<jint>(Moss_GetCameraPosition(static_cast<Moss_CameraID>(id)));}
JNIEXPORT jstring JNICALL Java_dev_moss_MossNative_nGetCurrentCameraDriver(JNIEnv* env,jclass){return to_jstring(env,Moss_GetCurrentCameraDriver());}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetNumCameraDrivers(JNIEnv*,jclass){return Moss_GetNumCameraDrivers();}
JNIEXPORT jintArray JNICALL Java_dev_moss_MossNative_nGetCameraSupportedFormats(JNIEnv* env,jclass,jlong id){int count=0;const Moss_CameraSpec* specs=Moss_GetCameraSupportedFormats(static_cast<Moss_CameraID>(id),&count);if(!specs||count<=0)return env->NewIntArray(0);std::vector<jint> v;v.reserve(static_cast<size_t>(count)*6);for(int i=0;i<count;++i){v.push_back(static_cast<jint>(specs[i].format));v.push_back(static_cast<jint>(specs[i].colorspace));v.push_back(specs[i].width);v.push_back(specs[i].height);v.push_back(specs[i].framerate_numerator);v.push_back(specs[i].framerate_denominator);}jintArray a=env->NewIntArray(static_cast<jsize>(v.size()));if(a&&!v.empty())env->SetIntArrayRegion(a,0,static_cast<jsize>(v.size()),v.data());return a;}
JNIEXPORT jlongArray JNICALL Java_dev_moss_MossNative_nAcquireCameraFrame(JNIEnv* env,jclass,jlong camera){uint64_t ts=0;Moss_Surface* s=Moss_AcquireCameraFrame(from_handle<Moss_Capture>(camera),&ts);jlong v[2]={to_handle(s),static_cast<jlong>(ts)};jlongArray a=env->NewLongArray(2);if(a)env->SetLongArrayRegion(a,0,2,v);return a;}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nReleaseCameraFrame(JNIEnv*,jclass,jlong camera,jlong frame){Moss_ReleaseCameraFrame(from_handle<Moss_Capture>(camera),from_handle<Moss_Surface>(frame));}
JNIEXPORT jintArray JNICALL Java_dev_moss_MossNative_nGetCameraFormat(JNIEnv* env,jclass,jlong camera){Moss_CameraSpec s{};bool ok=Moss_GetCameraFormat(from_handle<Moss_Capture>(camera),&s);jint v[7]={ok?1:0,static_cast<jint>(s.format),static_cast<jint>(s.colorspace),s.width,s.height,s.framerate_numerator,s.framerate_denominator};jintArray a=env->NewIntArray(7);if(a)env->SetIntArrayRegion(a,0,7,v);return a;}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetCameraPermissionState(JNIEnv*,jclass,jlong camera){return static_cast<jint>(Moss_GetCameraPermissionState(from_handle<Moss_Capture>(camera)));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nGetCameraProperties(JNIEnv*,jclass,jlong camera){return static_cast<jlong>(Moss_GetCameraProperties(from_handle<Moss_Capture>(camera)));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nCloseCamera(JNIEnv*,jclass,jlong camera){Moss_CloseCamera(from_handle<Moss_Capture>(camera));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nGetCameraID(JNIEnv*,jclass,jlong camera){return static_cast<jlong>(Moss_GetCameraID(from_handle<Moss_Capture>(camera)));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nOpenCapture(JNIEnv*,jclass,jlong id,jint format,jint colorspace,jint width,jint height,jint fpsNum,jint fpsDen){Moss_CameraSpec s{static_cast<PixelFormat>(format),static_cast<Colorspace>(colorspace),width,height,fpsNum,fpsDen};return to_handle(Moss_OpenCapture(static_cast<Moss_CameraID>(id),&s));}

// -----------------------------------------------------------------------------
// Filesystem / dialogs
// -----------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nCopyFile(JNIEnv* env,jclass,jstring src,jstring dst,jboolean overwrite){if(!src||!dst){throw_illegal(env,"paths must be non-null");return JNI_FALSE;}UtfChars s(env,src),d(env,dst);return Moss_CopyFile(s.get(),d.get(),overwrite==JNI_TRUE)?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nCreateDirectory(JNIEnv* env,jclass,jstring path,jboolean recursive){if(!path){throw_illegal(env,"path must be non-null");return JNI_FALSE;}UtfChars p(env,path);return Moss_CreateDirectory(p.get(),recursive==JNI_TRUE)?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nRemovePath(JNIEnv* env,jclass,jstring path,jboolean recursive){if(!path){throw_illegal(env,"path must be non-null");return JNI_FALSE;}UtfChars p(env,path);return Moss_RemovePath(p.get(),recursive==JNI_TRUE)?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nRenamePath(JNIEnv* env,jclass,jstring oldPath,jstring newPath,jboolean overwrite){if(!oldPath||!newPath){throw_illegal(env,"paths must be non-null");return JNI_FALSE;}UtfChars a(env,oldPath),b(env,newPath);return Moss_RenamePath(a.get(),b.get(),overwrite==JNI_TRUE)?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jlongArray JNICALL Java_dev_moss_MossNative_nGetPathInfo(JNIEnv* env,jclass,jstring path){if(!path){throw_illegal(env,"path must be non-null");return nullptr;}UtfChars p(env,path);Moss_PathInfo info{};bool ok=Moss_GetPathInfo(p.get(),&info);jlong v[9]={ok?1:0,static_cast<jlong>(info.type),static_cast<jlong>(info.size),static_cast<jlong>(info.create_time),static_cast<jlong>(info.modify_time),static_cast<jlong>(info.access_time),info.readable?1:0,info.writable?1:0,info.executable?1:0};jlongArray a=env->NewLongArray(9);if(a)env->SetLongArrayRegion(a,0,9,v);return a;}

static jstring get_path_helper(JNIEnv* env, bool (*fn)(char*,int)) {
    std::vector<char> buf(4096,'\0');
    if (!fn(buf.data(), static_cast<int>(buf.size()))) return nullptr;
    return env->NewStringUTF(buf.data());
}

JNIEXPORT jstring JNICALL Java_dev_moss_MossNative_nGetCurrentDirectory(JNIEnv* env,jclass){return get_path_helper(env,[](char*b,int n){return Moss_GetCurrentDirectory(b,n);});}
JNIEXPORT jstring JNICALL Java_dev_moss_MossNative_nGetBasePath(JNIEnv* env,jclass){return get_path_helper(env,[](char*b,int n){return Moss_GetBasePath(b,n);});}
JNIEXPORT jstring JNICALL Java_dev_moss_MossNative_nGetUserFolder(JNIEnv* env,jclass,jint folder){std::vector<char> b(4096,'\0');if(!Moss_GetUserFolder(static_cast<Moss_UserFolder>(folder),b.data(),static_cast<int>(b.size())))return nullptr;return env->NewStringUTF(b.data());}
JNIEXPORT jstring JNICALL Java_dev_moss_MossNative_nGetPrefPath(JNIEnv* env,jclass,jstring org,jstring app){if(!org||!app){throw_illegal(env,"org and app must be non-null");return nullptr;}UtfChars o(env,org),a(env,app);std::vector<char>b(4096,'\0');if(!Moss_GetPrefPath(o.get(),a.get(),b.data(),static_cast<int>(b.size())))return nullptr;return env->NewStringUTF(b.data());}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nEnumerateDirectory(JNIEnv* env,jclass,jstring path,jboolean recursive,jobject callback){if(!path){throw_illegal(env,"path must be non-null");return JNI_FALSE;}replace_global_ref(env,g_enumerateDirectoryCallback,callback);UtfChars p(env,path);return Moss_EnumerateDirectory(p.get(),recursive==JNI_TRUE,callback?enumerate_directory_cb:nullptr,nullptr)?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nGlobDirectory(JNIEnv* env,jclass,jstring pattern,jobject callback){if(!pattern){throw_illegal(env,"pattern must be non-null");return JNI_FALSE;}replace_global_ref(env,g_directoryCallback,callback);UtfChars p(env,pattern);return Moss_GlobDirectory(p.get(),callback?directory_cb:nullptr,nullptr)?JNI_TRUE:JNI_FALSE;}

JNIEXPORT void JNICALL Java_dev_moss_MossNative_nShowFileDialogWithProperties(JNIEnv* env,jclass,jobject callback,jlong window,jobjectArray names,jobjectArray patterns,jstring location){replace_global_ref(env,g_dialogCallback,callback);std::vector<UtfChars> nc,pc;auto filters=make_filters(env,names,patterns,nc,pc);UtfChars loc(env,location);Moss_ShowFileDialogWithProperties(callback?dialog_cb:nullptr,nullptr,from_handle<Moss_Window>(window),filters.empty()?nullptr:filters.data(),static_cast<int>(filters.size()),location?loc.get():nullptr);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nShowOpenFolderDialog(JNIEnv* env,jclass,jobject callback,jlong window,jobjectArray names,jobjectArray patterns,jstring location,jboolean allowMany){replace_global_ref(env,g_dialogCallback,callback);std::vector<UtfChars> nc,pc;auto filters=make_filters(env,names,patterns,nc,pc);UtfChars loc(env,location);Moss_ShowOpenFolderDialog(callback?dialog_cb:nullptr,nullptr,from_handle<Moss_Window>(window),filters.empty()?nullptr:filters.data(),static_cast<int>(filters.size()),location?loc.get():nullptr,allowMany==JNI_TRUE);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nShowOpenFileDialog(JNIEnv* env,jclass,jobject callback,jlong window,jstring location,jboolean allowMany){replace_global_ref(env,g_dialogCallback,callback);UtfChars loc(env,location);Moss_ShowOpenFileDialog(callback?dialog_cb:nullptr,nullptr,from_handle<Moss_Window>(window),location?loc.get():nullptr,allowMany==JNI_TRUE);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nShowSaveFileDialog(JNIEnv* env,jclass,jint type,jobject callback,jlong props){replace_global_ref(env,g_dialogCallback,callback);Moss_ShowSaveFileDialog(static_cast<Moss_FileDialogType>(type),callback?dialog_cb:nullptr,nullptr,static_cast<Moss_PropertiesID>(props));}

// -----------------------------------------------------------------------------
// Storage
// -----------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nCloseStorage(JNIEnv*,jclass,jlong s){return Moss_CloseStorage(from_handle<Moss_Storage>(s))?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nCopyStorageFile(JNIEnv* env,jclass,jlong s,jstring oldp,jstring newp){if(!oldp||!newp){throw_illegal(env,"paths must be non-null");return JNI_FALSE;}UtfChars a(env,oldp),b(env,newp);return Moss_CopyStorageFile(from_handle<Moss_Storage>(s),a.get(),b.get())?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nCreateStorageDirectory(JNIEnv* env,jclass,jlong s,jstring path){if(!path){throw_illegal(env,"path must be non-null");return JNI_FALSE;}UtfChars p(env,path);return Moss_CreateStorageDirectory(from_handle<Moss_Storage>(s),p.get())?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nEnumerateStorageDirectory(JNIEnv* env,jclass,jlong s,jstring path,jobject callback){if(!path){throw_illegal(env,"path must be non-null");return JNI_FALSE;}replace_global_ref(env,g_enumerateDirectoryCallback,callback);UtfChars p(env,path);return Moss_EnumerateStorageDirectory(from_handle<Moss_Storage>(s),p.get(),callback?enumerate_directory_cb:nullptr,nullptr)?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jlongArray JNICALL Java_dev_moss_MossNative_nGetStorageFileSize(JNIEnv* env,jclass,jlong s,jstring path){if(!path){throw_illegal(env,"path must be non-null");return nullptr;}UtfChars p(env,path);uint64 len=0;bool ok=Moss_GetStorageFileSize(from_handle<Moss_Storage>(s),p.get(),&len);jlong v[2]={ok?1:0,static_cast<jlong>(len)};jlongArray a=env->NewLongArray(2);if(a)env->SetLongArrayRegion(a,0,2,v);return a;}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nGetStorageSpaceRemaining(JNIEnv*,jclass,jlong s){return static_cast<jlong>(Moss_GetStorageSpaceRemaining(from_handle<Moss_Storage>(s)));}
JNIEXPORT jlongArray JNICALL Java_dev_moss_MossNative_nGetStoragePathInfo(JNIEnv* env,jclass,jlong s,jstring path){if(!path){throw_illegal(env,"path must be non-null");return nullptr;}UtfChars p(env,path);Moss_PathInfo info{};bool ok=Moss_GetStoragePathInfo(from_handle<Moss_Storage>(s),p.get(),&info);jlong v[9]={ok?1:0,static_cast<jlong>(info.type),static_cast<jlong>(info.size),static_cast<jlong>(info.create_time),static_cast<jlong>(info.modify_time),static_cast<jlong>(info.access_time),info.readable?1:0,info.writable?1:0,info.executable?1:0};jlongArray a=env->NewLongArray(9);if(a)env->SetLongArrayRegion(a,0,9,v);return a;}
JNIEXPORT jobjectArray JNICALL Java_dev_moss_MossNative_nGlobStorageDirectory(JNIEnv* env,jclass,jlong s,jstring path,jstring pattern,jint flags){if(!path||!pattern){throw_illegal(env,"path and pattern must be non-null");return nullptr;}UtfChars p(env,path),pat(env,pattern);int count=0;char** values=Moss_GlobStorageDirectory(from_handle<Moss_Storage>(s),p.get(),pat.get(),static_cast<Moss_GlobFlags>(flags),&count);auto strings=c_string_array(values,count);return make_string_array(env,strings);}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nOpenFileStorage(JNIEnv* env,jclass,jstring path){if(!path){throw_illegal(env,"path must be non-null");return 0;}UtfChars p(env,path);return to_handle(Moss_OpenFileStorage(p.get()));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nOpenStorage(JNIEnv*,jclass,jlong iface,jlong userdata){return to_handle(Moss_OpenStorage(reinterpret_cast<const Moss_Storage*>(static_cast<intptr_t>(iface)),reinterpret_cast<void*>(static_cast<intptr_t>(userdata))));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nOpenTitleStorage(JNIEnv* env,jclass,jstring overridePath,jlong props){UtfChars p(env,overridePath);return to_handle(Moss_OpenTitleStorage(overridePath?p.get():nullptr,static_cast<Moss_PropertiesID>(props)));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nOpenUserStorage(JNIEnv* env,jclass,jstring org,jstring app,jlong props){if(!org||!app){throw_illegal(env,"org and app must be non-null");return 0;}UtfChars o(env,org),a(env,app);return to_handle(Moss_OpenUserStorage(o.get(),a.get(),static_cast<Moss_PropertiesID>(props)));}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nReadStorageFile(JNIEnv* env,jclass,jlong s,jstring path,jobject destination,jlong length){if(!path||length<0){throw_illegal(env,"invalid read arguments");return JNI_FALSE;}void* data=nullptr;if(!get_direct_buffer(env,destination,&data,length)){throw_illegal(env,"destination must be a direct buffer large enough for length");return JNI_FALSE;}UtfChars p(env,path);return Moss_ReadStorageFile(from_handle<Moss_Storage>(s),p.get(),data,static_cast<uint64>(length))?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nRemoveStoragePath(JNIEnv* env,jclass,jlong s,jstring path){if(!path){throw_illegal(env,"path must be non-null");return JNI_FALSE;}UtfChars p(env,path);return Moss_RemoveStoragePath(from_handle<Moss_Storage>(s),p.get())?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nRenameStoragePath(JNIEnv* env,jclass,jlong s,jstring oldp,jstring newp){if(!oldp||!newp){throw_illegal(env,"paths must be non-null");return JNI_FALSE;}UtfChars a(env,oldp),b(env,newp);return Moss_RenameStoragePath(from_handle<Moss_Storage>(s),a.get(),b.get())?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nStorageReady(JNIEnv*,jclass,jlong s){return Moss_StorageReady(from_handle<Moss_Storage>(s))?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nWriteStorageFile(JNIEnv* env,jclass,jlong s,jstring path,jobject source,jlong length){if(!path||length<0){throw_illegal(env,"invalid write arguments");return JNI_FALSE;}void* data=nullptr;if(!get_direct_buffer(env,source,&data,length)){throw_illegal(env,"source must be a direct buffer large enough for length");return JNI_FALSE;}UtfChars p(env,path);return Moss_WriteStorageFile(from_handle<Moss_Storage>(s),p.get(),data,static_cast<uint64>(length))?JNI_TRUE:JNI_FALSE;}

// -----------------------------------------------------------------------------
// Audio
// -----------------------------------------------------------------------------
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nInitAudio(JNIEnv*,jclass){return Moss_Init_Audio();}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nTerminateAudio(JNIEnv*,jclass){Moss_Terminate_Audio();}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioUpdate(JNIEnv*,jclass,jfloat dt){Moss_AudioUpdate(dt);}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nAudioLoadWavFile(JNIEnv* env,jclass,jstring filename){if(!filename){throw_illegal(env,"filename must be non-null");return 0;}UtfChars f(env,filename);return to_handle(Moss_AudioLoadWavFile(f.get()));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nAudioLoadWav(JNIEnv*,jclass){return to_handle(Moss_AudioLoadWav());}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nAudioLoadOgg(JNIEnv* env,jclass,jstring filename,jint type){if(!filename){throw_illegal(env,"filename must be non-null");return 0;}UtfChars f(env,filename);return to_handle(Moss_AudioLoadOgg(f.get(),static_cast<AudioLoadType>(type)));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nAudioLoadMP3(JNIEnv* env,jclass,jstring filename){if(!filename){throw_illegal(env,"filename must be non-null");return 0;}UtfChars f(env,filename);return to_handle(Moss_AudioLoadMP3(f.get()));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioSourceDestroy(JNIEnv*,jclass,jlong source){Moss_AudioSourceDestroy(from_handle<Moss_AudioSource>(source));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nAudioCaptureMicrophone(JNIEnv*,jclass,jlong mic){return to_handle(Moss_AudioCaptureMicrophone(from_handle<Moss_Microphone>(mic)));}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nAudioCreateChannel(JNIEnv*,jclass,jlong channel){return static_cast<jint>(Moss_AudioCreateChannel(static_cast<ChannelID>(channel)));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioRemoveChannel(JNIEnv*,jclass,jlong channel){Audio_RemoveChannel(static_cast<ChannelID>(channel));}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nAudioGetMasterChannel(JNIEnv*,jclass){return static_cast<jint>(Moss_AudioGetMasterChannel());}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioSetChannelVolume(JNIEnv*,jclass,jlong channel,jfloat volume){Moss_AudioSetChannelVolume(static_cast<ChannelID>(channel),volume);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioSetChannelMute(JNIEnv*,jclass,jlong channel,jboolean mute){Moss_AudioSetChannelMute(static_cast<ChannelID>(channel),mute==JNI_TRUE);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioAddChannelEffect(JNIEnv*,jclass,jlong channel,jlong effect){Moss_AudioAddChannelEffect(static_cast<ChannelID>(channel),from_handle<AudioEffect>(effect));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioRemoveChannelEffect(JNIEnv*,jclass,jlong channel,jlong effect){Moss_AudioRemoveChannelEffect(static_cast<ChannelID>(channel),from_handle<AudioEffect>(effect));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioRemoveAllChannelEffects(JNIEnv*,jclass,jlong channel){Moss_AudioRemoveAllChannelEffects(static_cast<ChannelID>(channel));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nAudioStreamCreate(JNIEnv*,jclass){return to_handle(Moss_AudioStreamCreate());}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStreamPlay(JNIEnv*,jclass,jlong stream){Moss_AudioStreamPlay(from_handle<AudioStream>(stream));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStreamStop(JNIEnv*,jclass,jlong stream){Moss_AudioStreamStop(from_handle<AudioStream>(stream));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStreamSetVolume(JNIEnv*,jclass,jlong stream,jfloat volume){Moss_AudioStreamSetVolume(from_handle<AudioStream>(stream),volume);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStreamSetPitch(JNIEnv*,jclass,jlong stream,jfloat pitch){Moss_AudioStreamSetPitch(from_handle<AudioStream>(stream),pitch);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStreamSetPlaybackRate(JNIEnv*,jclass,jlong stream,jfloat rate){Moss_AudioStreamSetPlaybackRate(from_handle<AudioStream>(stream),rate);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStreamSetPan(JNIEnv*,jclass,jlong stream,jfloat pan){Moss_AudioStreamSetPan(from_handle<AudioStream>(stream),pan);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStreamSetLoop(JNIEnv*,jclass,jlong stream,jboolean loop){Moss_AudioStreamSetLoop(from_handle<AudioStream>(stream),loop==JNI_TRUE);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStreamRemove(JNIEnv*,jclass,jlong stream){Moss_AudioStreamRemove(from_handle<AudioStream>(stream));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nAudioStream2DCreate(JNIEnv*,jclass){return to_handle(Moss_AudioStream2DCreate());}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream2DPlay(JNIEnv*,jclass,jlong stream){Moss_AudioStream2DPlay(from_handle<AudioStream2D>(stream));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream2DStop(JNIEnv*,jclass,jlong stream){Moss_AudioStream2DStop(from_handle<AudioStream2D>(stream));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream2DSetVolume(JNIEnv*,jclass,jlong stream,jfloat volume){Moss_AudioStream2DSetVolume(from_handle<AudioStream2D>(stream),volume);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream2DSetPitch(JNIEnv*,jclass,jlong stream,jfloat pitch){Moss_AudioStream2DSetPitch(from_handle<AudioStream2D>(stream),pitch);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream2DSetPlaybackRate(JNIEnv*,jclass,jlong stream,jfloat rate){Moss_AudioStream2DSetPlaybackRate(from_handle<AudioStream2D>(stream),rate);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream2DSetPan(JNIEnv*,jclass,jlong stream,jfloat pan){Moss_AudioStream2DSetPan(from_handle<AudioStream2D>(stream),pan);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream2DSetLoop(JNIEnv*,jclass,jlong stream,jboolean loop){Moss_AudioStream2DSetLoop(from_handle<AudioStream2D>(stream),loop==JNI_TRUE);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream3DSetPosition(JNIEnv*,jclass,jlong stream){Moss_AudioStream3DSetPosition(from_handle<AudioStream2D>(stream));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream3DSetVelocity(JNIEnv*,jclass,jlong stream){Moss_AudioStream3DSetVelocity(from_handle<AudioStream2D>(stream));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream3DSetMaxDistance(JNIEnv*,jclass,jlong stream){Moss_AudioStream3DSetMaxDistance(from_handle<AudioStream2D>(stream));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream2DRemove(JNIEnv*,jclass,jlong stream){Moss_AudioStream2DRemove(from_handle<AudioStream2D>(stream));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nAudioStream3DCreate(JNIEnv*,jclass){return to_handle(Moss_AudioStream3DCreate());}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream3DPlay(JNIEnv*,jclass,jlong stream){Moss_AudioStream3DPlay(from_handle<AudioStream3D>(stream));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream3DStop(JNIEnv*,jclass,jlong stream){Moss_AudioStream3DStop(from_handle<AudioStream3D>(stream));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream3DSetVolume(JNIEnv*,jclass,jlong stream,jfloat volume){Moss_AudioStream3DSetVolume(from_handle<AudioStream3D>(stream),volume);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream3DSetPitch(JNIEnv*,jclass,jlong stream,jfloat pitch){Moss_AudioStream3DSetPitch(from_handle<AudioStream3D>(stream),pitch);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream3DSetPlaybackRate(JNIEnv*,jclass,jlong stream,jfloat rate){Moss_AudioStream3DSetPlaybackRate(from_handle<AudioStream3D>(stream),rate);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream3DSetPan(JNIEnv*,jclass,jlong stream,jfloat pan){Moss_AudioStream3DSetPan(from_handle<AudioStream3D>(stream),pan);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream3DSetLoop(JNIEnv*,jclass,jlong stream,jboolean loop){Moss_AudioStream3DSetLoop(from_handle<AudioStream3D>(stream),loop==JNI_TRUE);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream3DSetDistanceModel(JNIEnv*,jclass,jlong stream,jint model){Moss_AudioStream3DSetDistanceModel(from_handle<AudioStream3D>(stream),static_cast<DistanceModel>(model));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioStream3DRemove(JNIEnv*,jclass,jlong stream){Moss_AudioStream3DRemove(from_handle<AudioStream3D>(stream));}

JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioRemoveEffect(JNIEnv*,jclass,jlong effect){Moss_AudioRemoveEffect(from_handle<AudioEffect>(effect));}

// Value-type audio listener creation uses simple float arguments instead of exposing C++ Vec/Float structs to Java.
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nAudioCreateAudioListener2D(JNIEnv*,jclass,jfloat x,jfloat y){return to_handle(Moss_AudioCreateAudioListener2D(Float2{x,y}));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nAudioCreateAudioListener3D(JNIEnv*,jclass,jfloat x,jfloat y,jfloat z){return to_handle(Moss_AudioCreateAudioListener3D(Float3{x,y,z}));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nAudioCreateRayAudioListener2D(JNIEnv*,jclass,jfloat x,jfloat y){return to_handle(Moss_AudioCreateRayAudioListener2D(Float2{x,y}));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nAudioCreateRayAudioListener3D(JNIEnv*,jclass,jfloat x,jfloat y,jfloat z){return to_handle(Moss_AudioCreateRayAudioListener3D(Float3{x,y,z}));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioRemoveAudioListener2D(JNIEnv*,jclass,jlong p){Moss_AudioRemoveAudioListener2D(from_handle<AudioListener2D>(p));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioRemoveAudioListener3D(JNIEnv*,jclass,jlong p){Moss_AudioRemoveAudioListener3D(from_handle<AudioListener3D>(p));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioRemoveRayAudioListener2D(JNIEnv*,jclass,jlong p){Moss_AudioRemoveRayAudioListener2D(from_handle<RayAudioListener2D>(p));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioRemoveRayAudioListener3D(JNIEnv*,jclass,jlong p){Moss_AudioRemoveRayAudioListener3D(from_handle<RayAudioListener3D>(p));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioActivateAudioListener2D(JNIEnv*,jclass,jlong p,jboolean a){Moss_AudioActivateAudioListener2D(from_handle<AudioListener2D>(p),a==JNI_TRUE);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioActivateAudioListener3D(JNIEnv*,jclass,jlong p,jboolean a){Moss_AudioActivateAudioListener3D(from_handle<AudioListener3D>(p),a==JNI_TRUE);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioActivateRayAudioListener2D(JNIEnv*,jclass,jlong p,jboolean a){Moss_AudioActivateRayAudioListener2D(from_handle<RayAudioListener2D>(p),a==JNI_TRUE);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioActivateRayAudioListener3D(JNIEnv*,jclass,jlong p,jboolean a){Moss_AudioActivateRayAudioListener3D(from_handle<RayAudioListener3D>(p),a==JNI_TRUE);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioListenerSetOrientation(JNIEnv*,jclass,jlong listener,jfloat fx,jfloat fy,jfloat fz,jfloat ux,jfloat uy,jfloat uz){Vec3 forward(fx,fy,fz),up(ux,uy,uz);Moss_AudioListenerSetOrientation(from_handle<AudioListener3D>(listener),forward,up);}

JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioRayListener2DSetPhysics(JNIEnv*,jclass,jlong listener,jlong physics){Moss_AudioRayListener2DSetPhysics(from_handle<RayAudioListener2D>(listener),from_handle<PhysicsSystem>(physics),nullptr,nullptr);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioRayListener3DSetPhysics(JNIEnv*,jclass,jlong listener,jlong physics){Moss_AudioRayListener3DSetPhysics(from_handle<RayAudioListener3D>(listener),from_handle<PhysicsSystem>(physics),nullptr,nullptr);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioRayListener2DSetTraceSettings(JNIEnv*,jclass,jlong listener,jboolean enabled,jfloat maxDistance){Moss_AudioRayListener2DSetTraceSettings(from_handle<RayAudioListener2D>(listener),enabled==JNI_TRUE,maxDistance);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioRayListener3DSetTraceSettings(JNIEnv*,jclass,jlong listener,jboolean enabled,jfloat maxDistance){Moss_AudioRayListener3DSetTraceSettings(from_handle<RayAudioListener3D>(listener),enabled==JNI_TRUE,maxDistance);}

JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nAudioRayTrace2D(JNIEnv* env,jclass,jlong physics,jfloat lx,jfloat ly,jfloat sx,jfloat sy,jfloat maxDistance,jint reflectionRays,jfloat directOcclusion,jfloat reflectionStrength,jfloat airAbsorption,jobject outResult){void* out=nullptr;if(!get_direct_buffer(env,outResult,&out,sizeof(Moss_AudioRayTraceResult))){throw_illegal(env,"outResult must be a direct buffer large enough for Moss_AudioRayTraceResult");return JNI_FALSE;}Moss_AudioRayTrace2DDesc d{};d.physics=from_handle<PhysicsSystem>(physics);d.listener_position=Vec2(lx,ly);d.source_position=Vec2(sx,sy);d.max_distance=maxDistance;d.reflection_rays=static_cast<uint32>(reflectionRays);d.direct_occlusion_strength=directOcclusion;d.reflection_strength=reflectionStrength;d.air_absorption=airAbsorption;return Moss_AudioRayTrace2D(&d,static_cast<Moss_AudioRayTraceResult*>(out))?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nAudioRayTrace3D(JNIEnv* env,jclass,jlong physics,jfloat lx,jfloat ly,jfloat lz,jfloat sx,jfloat sy,jfloat sz,jfloat maxDistance,jint reflectionRays,jfloat directOcclusion,jfloat reflectionStrength,jfloat airAbsorption,jobject outResult){void* out=nullptr;if(!get_direct_buffer(env,outResult,&out,sizeof(Moss_AudioRayTraceResult))){throw_illegal(env,"outResult must be a direct buffer large enough for Moss_AudioRayTraceResult");return JNI_FALSE;}Moss_AudioRayTrace3DDesc d{};d.physics=from_handle<PhysicsSystem>(physics);d.listener_position=Vec3(lx,ly,lz);d.source_position=Vec3(sx,sy,sz);d.max_distance=maxDistance;d.reflection_rays=static_cast<uint32>(reflectionRays);d.direct_occlusion_strength=directOcclusion;d.reflection_strength=reflectionStrength;d.air_absorption=airAbsorption;return Moss_AudioRayTrace3D(&d,static_cast<Moss_AudioRayTraceResult*>(out))?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nAudioRayTraceFromListener2D(JNIEnv* env,jclass,jlong listener,jfloat sx,jfloat sy,jobject outResult){void* out=nullptr;if(!get_direct_buffer(env,outResult,&out,sizeof(Moss_AudioRayTraceResult))){throw_illegal(env,"outResult must be a direct buffer large enough for Moss_AudioRayTraceResult");return JNI_FALSE;}Vec2 pos(sx,sy);return Moss_AudioRayTraceFromListener2D(from_handle<RayAudioListener2D>(listener),&pos,static_cast<Moss_AudioRayTraceResult*>(out))?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nAudioRayTraceFromListener3D(JNIEnv* env,jclass,jlong listener,jfloat sx,jfloat sy,jfloat sz,jobject outResult){void* out=nullptr;if(!get_direct_buffer(env,outResult,&out,sizeof(Moss_AudioRayTraceResult))){throw_illegal(env,"outResult must be a direct buffer large enough for Moss_AudioRayTraceResult");return JNI_FALSE;}Vec3 pos(sx,sy,sz);return Moss_AudioRayTraceFromListener3D(from_handle<RayAudioListener3D>(listener),&pos,static_cast<Moss_AudioRayTraceResult*>(out))?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jfloat JNICALL Java_dev_moss_MossNative_nAudioRayTraceComputeGain(JNIEnv*,jclass,jboolean audible,jboolean occluded,jfloat distance,jfloat attenuation,jfloat occlusion,jfloat transmission,jfloat lowpass,jfloat reflectionGain,jfloat reflectionDelay,jfloat delay){Moss_AudioRayTraceResult r{};r.audible=audible==JNI_TRUE;r.occluded=occluded==JNI_TRUE;r.distance=distance;r.attenuation=attenuation;r.occlusion=occlusion;r.transmission_gain=transmission;r.lowpass=lowpass;r.reflection_gain=reflectionGain;r.reflection_delay_seconds=reflectionDelay;r.delay_seconds=delay;return Moss_AudioRayTraceComputeGain(&r);}

JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nIsSpeakerDeviceReady(JNIEnv*,jclass){return Moss_IsSpeakerDeviceReady()?JNI_TRUE:JNI_FALSE;}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioSpeakerOpen(JNIEnv*,jclass){Moss_AudioSpeakerOpen();}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioSpeakerPause(JNIEnv*,jclass){Moss_AudioSpeakerPause();}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioSpeakerResume(JNIEnv*,jclass){Moss_AudioSpeakerResume();}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nAudioSpeakerIsPaused(JNIEnv*,jclass){return Moss_AudioSpeakerIsPaused()?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nAudioSelectSpeakerDevice(JNIEnv*,jclass,jint id){return Moss_AudioSelectSpeakerDevice(id)?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetCurrentSpeakerDeviceID(JNIEnv*,jclass){return Moss_GetCurrentSpeakerDeviceID();}
JNIEXPORT jstring JNICALL Java_dev_moss_MossNative_nGetSpeakerDeviceName(JNIEnv* env,jclass,jint id){return to_jstring(env,Moss_GetSpeakerDeviceName(id));}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nListSpeakerDevices(JNIEnv*,jclass){return Moss_ListSpeakerDevices();}

JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nMicrophoneGetDeviceCount(JNIEnv*,jclass){return static_cast<jint>(Moss_MicrophoneGetDeviceCount());}
JNIEXPORT jstring JNICALL Java_dev_moss_MossNative_nMicrophoneGetDeviceName(JNIEnv* env,jclass,jint index){return to_jstring(env,Moss_MicrophoneGetDeviceName(static_cast<uint32>(index)));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nMicrophoneOpen(JNIEnv*,jclass,jint deviceIndex,jint sampleRate,jint channels,jint bufferFrames,jint ringBufferFrames,jboolean startImmediately,jboolean enableVoiceMetrics){Moss_MicrophoneDesc d{};d.device_index=static_cast<uint32>(deviceIndex);d.sample_rate=static_cast<uint32>(sampleRate);d.channels=static_cast<uint32>(channels);d.buffer_frames=static_cast<uint32>(bufferFrames);d.ring_buffer_frames=static_cast<uint32>(ringBufferFrames);d.start_immediately=startImmediately==JNI_TRUE;d.enable_voice_metrics=enableVoiceMetrics==JNI_TRUE;return to_handle(Moss_MicrophoneOpen(&d));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nMicrophoneClose(JNIEnv*,jclass,jlong mic){Moss_MicrophoneClose(from_handle<Moss_Microphone>(mic));}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nMicrophoneStart(JNIEnv*,jclass,jlong mic){return Moss_MicrophoneStart(from_handle<Moss_Microphone>(mic))?JNI_TRUE:JNI_FALSE;}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nMicrophoneStop(JNIEnv*,jclass,jlong mic){Moss_MicrophoneStop(from_handle<Moss_Microphone>(mic));}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nMicrophoneRead(JNIEnv* env,jclass,jlong mic,jobject samples,jint maxFrames){void* p=nullptr;if(!get_direct_buffer(env,samples,&p,static_cast<jlong>(maxFrames)*static_cast<jlong>(sizeof(float)))){throw_illegal(env,"samples must be a direct float buffer");return 0;}return static_cast<jint>(Moss_MicrophoneRead(from_handle<Moss_Microphone>(mic),static_cast<float*>(p),static_cast<uint32>(maxFrames)));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nMicrophoneSetGain(JNIEnv*,jclass,jlong mic,jfloat gain){Moss_MicrophoneSetGain(from_handle<Moss_Microphone>(mic),gain);}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nMicrophoneGetSampleRate(JNIEnv*,jclass,jlong mic){return static_cast<jint>(Moss_MicrophoneGetSampleRate(from_handle<Moss_Microphone>(mic)));}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nMicrophoneGetChannels(JNIEnv*,jclass,jlong mic){return static_cast<jint>(Moss_MicrophoneGetChannels(from_handle<Moss_Microphone>(mic)));}
JNIEXPORT jfloatArray JNICALL Java_dev_moss_MossNative_nMicrophoneGetLevels(JNIEnv* env,jclass,jlong mic){Moss_MicrophoneLevels l=Moss_MicrophoneGetLevels(from_handle<Moss_Microphone>(mic));jfloat v[4]={l.rms,l.peak,l.smoothed_volume,l.voice_activity};jfloatArray a=env->NewFloatArray(4);if(a)env->SetFloatArrayRegion(a,0,4,v);return a;}
JNIEXPORT jfloat JNICALL Java_dev_moss_MossNative_nMicrophoneGetLevelRMS(JNIEnv*,jclass,jlong mic){return Moss_MicrophoneGetLevelRMS(from_handle<Moss_Microphone>(mic));}
JNIEXPORT jfloat JNICALL Java_dev_moss_MossNative_nMicrophoneGetLevelPeak(JNIEnv*,jclass,jlong mic){return Moss_MicrophoneGetLevelPeak(from_handle<Moss_Microphone>(mic));}
JNIEXPORT jfloat JNICALL Java_dev_moss_MossNative_nMicrophoneGetSmoothedVolume(JNIEnv*,jclass,jlong mic){return Moss_MicrophoneGetSmoothedVolume(from_handle<Moss_Microphone>(mic));}
JNIEXPORT jfloat JNICALL Java_dev_moss_MossNative_nMicrophoneGetVoiceActivity(JNIEnv*,jclass,jlong mic){return Moss_MicrophoneGetVoiceActivity(from_handle<Moss_Microphone>(mic));}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nIsMicrophoneDeviceReady(JNIEnv*,jclass){return Moss_IsMicrophoneDeviceReady()?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nAudioMicrophoneOpen(JNIEnv*,jclass){return Moss_AudioMicrophoneOpen();}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioMicrophoneClose(JNIEnv*,jclass){Moss_AudioMicrophoneClose();}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioMicrophonePlay(JNIEnv*,jclass){Moss_AudioMicrophonePlay();}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioMicrophoneStop(JNIEnv*,jclass){Moss_AudioMicrophoneStop();}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nAudioMicrophoneID(JNIEnv*,jclass){return Moss_AudioMicrophoneID();}
JNIEXPORT jboolean JNICALL Java_dev_moss_MossNative_nAudioSelectMicrophoneDevice(JNIEnv*,jclass,jint id){return Moss_AudioSelectMicrophoneDevice(id)?JNI_TRUE:JNI_FALSE;}
JNIEXPORT jstring JNICALL Java_dev_moss_MossNative_nGetMicrophoneDeviceName(JNIEnv* env,jclass,jint index){return to_jstring(env,Moss_GetMicrophoneDeviceName(index));}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nListMicrophoneDevices(JNIEnv*,jclass){return Moss_ListMicrophoneDevices();}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nAudioMicrophoneSetGain(JNIEnv*,jclass,jlong mic,jfloat gain){Moss_AudioMicrophoneSetGain(from_handle<Moss_Microphone>(mic),gain);}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nAudioMicrophoneGetSampleRate(JNIEnv*,jclass,jlong mic){return Moss_AudioMicrophoneGetSampleRate(from_handle<Moss_Microphone>(mic));}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nAudioMicrophoneGetChannels(JNIEnv*,jclass,jlong mic){return Moss_AudioMicrophoneGetChannels(from_handle<Moss_Microphone>(mic));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetMicrophoneCallback(JNIEnv* env,jclass,jlong mic,jobject cb){replace_global_ref(env,g_microphoneCallback,cb);Moss_MicrophoneSetCallback(from_handle<Moss_Microphone>(mic),cb?microphone_cb:nullptr,nullptr);}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSetAudioStreamCallback(JNIEnv* env,jclass,jlong stream,jobject cb){replace_global_ref(env,g_streamCallback,cb);Moss_AudioStreamSetCallback(from_handle<AudioStream>(stream),cb?stream_cb:nullptr,nullptr);}

// -----------------------------------------------------------------------------
// stdinc inline helpers which are active in the current public header.
// Exported C++ ABI functions in the current time/random section are commented
// out in Moss_stdinc.h and therefore are intentionally not fabricated here.
// -----------------------------------------------------------------------------
JNIEXPORT jfloat JNICALL Java_dev_moss_MossNative_nDegreesToRadians(JNIEnv*,jclass,jfloat value){return DegreesToRadians(value);}
JNIEXPORT jfloat JNICALL Java_dev_moss_MossNative_nRadiansToDegrees(JNIEnv*,jclass,jfloat value){return RadiansToDegrees(value);}
JNIEXPORT jfloat JNICALL Java_dev_moss_MossNative_nCenterAngleAroundZero(JNIEnv*,jclass,jfloat value){return CenterAngleAroundZero(value);}

// -----------------------------------------------------------------------------
// Conditional graphics entry points
// -----------------------------------------------------------------------------
#if defined(MOSS_GRAPHICS_OPENGL) || defined(MOSS_GRAPHICS_OPENGLES)
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nMakeContextCurrent(JNIEnv*,jclass,jlong window){Moss_MakeContextCurrent(from_handle<Moss_Window>(window));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSwapBuffers(JNIEnv*,jclass){Moss_SwapBuffers();}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nSwapBuffersInterval(JNIEnv*,jclass,jint interval){Moss_SwapBuffersInterval(interval);}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nGetProcAddress(JNIEnv* env,jclass,jstring name){if(!name){throw_illegal(env,"procname must be non-null");return 0;}UtfChars n(env,name);return to_handle(Moss_GetProcAddress(n.get()));}
#endif

#if defined(MOSS_GRAPHICS_VULKAN)
JNIEXPORT jlongArray JNICALL Java_dev_moss_MossNative_nCreateWindowSurface(JNIEnv* env,jclass,jlong window,jlong instance,jlong allocator){VkSurfaceKHR surface{};VkResult result=Moss_CreateWindowSurface(from_handle<Moss_Window>(window),reinterpret_cast<VkInstance>(static_cast<intptr_t>(instance)),reinterpret_cast<const VkAllocationCallbacks*>(static_cast<intptr_t>(allocator)),&surface);jint r=static_cast<jint>(result);jlong s=static_cast<jlong>(surface);jlong rv[2]={static_cast<jlong>(r),s};jlongArray a=env->NewLongArray(2);if(a)env->SetLongArrayRegion(a,0,2,rv);return a;}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nVulkanSupported(JNIEnv*,jclass){return Moss_VulkanSupported();}
JNIEXPORT jobjectArray JNICALL Java_dev_moss_MossNative_nGetRequiredInstanceExtensions(JNIEnv* env,jclass){uint32_t count=0;const char** exts=Moss_GetRequiredInstanceExtensions(&count);std::vector<std::string> v;if(exts){for(uint32_t i=0;i<count;++i)if(exts[i])v.emplace_back(exts[i]);}return make_string_array(env,v);}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nGetInstanceProcAddress(JNIEnv* env,jclass,jlong instance,jstring name){if(!name){throw_illegal(env,"procname must be non-null");return 0;}UtfChars n(env,name);return to_handle(Moss_GetInstanceProcAddress(reinterpret_cast<VkInstance>(static_cast<intptr_t>(instance)),n.get()));}
JNIEXPORT jint JNICALL Java_dev_moss_MossNative_nGetPhysicalDevicePresentationSupport(JNIEnv*,jclass,jlong window,jlong device,jlong queueFamily){return Moss_GetPhysicalDevicePresentationSupport(from_handle<Moss_Window>(window),reinterpret_cast<VkPhysicalDevice>(static_cast<intptr_t>(device)),static_cast<uint32_t>(queueFamily));}
#endif

#if defined(MOSS_GRAPHICS_METAL)
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nMetalCreateView(JNIEnv*,jclass,jlong window){return to_handle(Moss_Metal_CreateView(from_handle<Moss_Window>(window)));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nMetalDestroyView(JNIEnv*,jclass,jlong view){Moss_Metal_DestroyView(from_handle<void>(view));}
JNIEXPORT jlong JNICALL Java_dev_moss_MossNative_nMetalGetLayer(JNIEnv*,jclass,jlong view){return to_handle(Moss_Metal_GetLayer(from_handle<void>(view)));}
JNIEXPORT void JNICALL Java_dev_moss_MossNative_nMetalResize(JNIEnv*,jclass,jlong view,jint width,jint height){Moss_Metal_Resize(from_handle<void>(view),static_cast<uint32_t>(width),static_cast<uint32_t>(height));}
#endif

} // extern "C"


// -----------------------------------------------------------------------------
// Compatibility entry points for the original JavaMoss classes.
// These preserve the JNI names currently used by Moss.java and MossWindow.java.
// -----------------------------------------------------------------------------

extern "C" JNIEXPORT jlong JNICALL
Java_dev_moss_MossWindow_nCreate(JNIEnv* env, jclass, jstring title, jint width, jint height, jlong monitor, jlong share)
{
    UtfChars t(env, title);
    if (!t.get()) return 0;
    Moss_Window* w = Moss_CreateWindow(t.get(), static_cast<int>(width), static_cast<int>(height),
                                       from_handle<Moss_Monitor>(monitor), from_handle<Moss_Window>(share));
    return to_handle(w);
}

extern "C" JNIEXPORT void JNICALL
Java_dev_moss_MossWindow_nDestroy(JNIEnv*, jclass, jlong window)
{
    Moss_TerminateWindow(from_handle<Moss_Window>(window));
}

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_moss_MossWindow_nShouldClose(JNIEnv*, jclass, jlong window)
{
    return Moss_ShouldWindowClose(from_handle<Moss_Window>(window)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_moss_MossWindow_nRequestClose(JNIEnv*, jclass, jlong window)
{
    Moss_CloseWindow(from_handle<Moss_Window>(window));
}

extern "C" JNIEXPORT void JNICALL
Java_dev_moss_MossWindow_nSetTitle(JNIEnv* env, jclass, jlong window, jstring title)
{
    UtfChars t(env, title);
    if (!t.get()) return;
    Moss_SetWindowTitle(from_handle<Moss_Window>(window), t.get());
}

extern "C" JNIEXPORT jint JNICALL
Java_dev_moss_MossWindow_nWidth(JNIEnv*, jclass, jlong window)
{
    return static_cast<jint>(Moss_GetWindowWidth(from_handle<Moss_Window>(window)));
}

extern "C" JNIEXPORT jint JNICALL
Java_dev_moss_MossWindow_nHeight(JNIEnv*, jclass, jlong window)
{
    return static_cast<jint>(Moss_GetWindowHeight(from_handle<Moss_Window>(window)));
}

extern "C" JNIEXPORT void JNICALL
Java_dev_moss_Moss_nPollEvents(JNIEnv*, jclass)
{
    Moss_PollEvents();
}

extern "C" JNIEXPORT jint JNICALL
Java_dev_moss_Moss_nAvailableCpuCores(JNIEnv*, jclass)
{
    return static_cast<jint>(Moss_GetAvailableCPUCores());
}

extern "C" JNIEXPORT jint JNICALL
Java_dev_moss_Moss_nCpuCacheLineSize(JNIEnv*, jclass)
{
    return static_cast<jint>(Moss_GetCPUCacheLineSize());
}

extern "C" JNIEXPORT jint JNICALL
Java_dev_moss_Moss_nSystemRamMiB(JNIEnv*, jclass)
{
    return static_cast<jint>(Moss_GetSystemRAM());
}
