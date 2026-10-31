#include <jni.h>
#include <vulkan/vulkan.h>

#include <sstream>
#include <string>

#include "diagnostics/crash_handler.h"
#include "diagnostics/logger.h"

namespace {

std::string ToStdString(JNIEnv* env, jstring value) {
    if (value == nullptr) {
        return {};
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    std::string result = chars != nullptr ? chars : "";
    if (chars != nullptr) {
        env->ReleaseStringUTFChars(value, chars);
    }
    return result;
}

}  // namespace

extern "C"
JNIEXPORT jstring JNICALL
Java_com_ps5ona_app_MainActivity_nativeStatus(JNIEnv* env, jobject) {
    LOGI("PS5onA native bootstrap started");

    uint32_t api_version = VK_API_VERSION_1_0;
    const VkResult result = vkEnumerateInstanceVersion(&api_version);

    std::ostringstream out;
    out << "Native ARM64 library loaded successfully.\n";
    out << "Vulkan loader probe: ";

    if (result == VK_SUCCESS) {
        out << VK_VERSION_MAJOR(api_version)
            << "."
            << VK_VERSION_MINOR(api_version)
            << "."
            << VK_VERSION_PATCH(api_version);
    } else {
        out << "failed (VkResult " << static_cast<int>(result) << ")";
    }

#if defined(__aarch64__)
    out << "\nHost architecture: arm64";
#else
    out << "\nHost architecture: unexpected";
#endif

    const std::string status = out.str();
    LOGI("%s", status.c_str());

    return env->NewStringUTF(status.c_str());
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ps5ona_app_diagnostics_NativeDiagnostics_nativeInit(JNIEnv* env, jobject, jstring log_dir,
                                                             jstring crash_dir, jstring header) {
    ps5ona::log::Init(ToStdString(env, log_dir));
    ps5ona::crash::Install(ToStdString(env, crash_dir), ToStdString(env, header));
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ps5ona_app_diagnostics_NativeDiagnostics_nativeLog(JNIEnv* env, jobject, jint level, jstring tag,
                                                            jstring message) {
    const std::string tag_text = ToStdString(env, tag);
    const std::string message_text = ToStdString(env, message);
    ps5ona::log::Write(static_cast<ps5ona::log::Level>(level), tag_text.c_str(), "%s", message_text.c_str());
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ps5ona_app_diagnostics_NativeDiagnostics_nativeFlush(JNIEnv*, jobject) {
    ps5ona::log::Flush();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_ps5ona_app_diagnostics_NativeDiagnostics_nativeTestCrash(JNIEnv*, jobject) {
    ps5ona::crash::TriggerTestCrash();
}
