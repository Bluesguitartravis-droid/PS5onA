#include <jni.h>
#include <android/log.h>
#include <vulkan/vulkan.h>

#include <sstream>
#include <string>

namespace {
constexpr const char* TAG = "PS5onA";
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_ps5ona_app_MainActivity_nativeStatus(JNIEnv* env, jobject) {
    __android_log_print(ANDROID_LOG_INFO, TAG, "PS5onA native bootstrap started");

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
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", status.c_str());

    return env->NewStringUTF(status.c_str());
}
