// HttpsURLConnection transport (Android): Blood2Activity.httpsGet writes the file on the calling
// thread and reports progress through downloadProgress below.
#include "app/https_download.h"
#include "engine/config.h"
#include <SDL3/SDL_system.h>
#include <cstdlib>
#include <jni.h>

namespace
{
    struct Transfer
    {
        const std::function<bool(std::uint64_t)>* progress = nullptr;
        bool cancelled = false;
    };
} // namespace

extern "C" JNIEXPORT jboolean JNICALL Java_net_demonixis_openblood2_Blood2Activity_downloadProgress(JNIEnv*, jclass,
                                                                                                    jlong handle,
                                                                                                    jlong bytes)
{
    auto& transfer = *reinterpret_cast<Transfer*>(static_cast<std::intptr_t>(handle));
    transfer.cancelled = transfer.cancelled || !(*transfer.progress)(std::uint64_t(bytes));
    return transfer.cancelled ? JNI_FALSE : JNI_TRUE;
}

namespace OpenBlood2
{
    bool https_get(std::string_view url, const std::filesystem::path& file, std::uint64_t limit,
                   const std::function<bool(std::uint64_t)>& progress, HttpsResponse& response, std::string& error)
    {
        response = {};
        auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
        auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
        if (!env || !activity)
        {
            error = "Android activity unavailable for the download";
            return false;
        }
        // FindClass on a native thread cannot see application classes; the activity's own can.
        auto type = env->GetObjectClass(activity);
        env->DeleteLocalRef(activity);
        auto method =
            env->GetStaticMethodID(type, "httpsGet", "(Ljava/lang/String;Ljava/lang/String;JJ)Ljava/lang/String;");
        if (!method)
        {
            env->ExceptionClear();
            env->DeleteLocalRef(type);
            error = "Blood2Activity.httpsGet is missing";
            return false;
        }
        Transfer transfer{&progress};
        const std::string address(url), path = path_text(file);
        auto jaddress = env->NewStringUTF(address.c_str());
        auto jpath = env->NewStringUTF(path.c_str());
        auto result = static_cast<jstring>(env->CallStaticObjectMethod(
            type, method, jaddress, jpath, jlong(limit), jlong(reinterpret_cast<std::intptr_t>(&transfer))));
        const bool thrown = env->ExceptionCheck();
        env->ExceptionClear();
        std::string text;
        if (result)
        {
            if (const char* chars = env->GetStringUTFChars(result, nullptr))
            {
                text = chars;
                env->ReleaseStringUTFChars(result, chars);
            }
            env->DeleteLocalRef(result);
        }
        env->DeleteLocalRef(jaddress);
        env->DeleteLocalRef(jpath);
        env->DeleteLocalRef(type);
        // "<status>\n<location>\n<bytes>" or "!<error>".
        if (thrown || text.empty() || text.front() == '!')
        {
            error = transfer.cancelled ? "Installation cancelled"
                                       : "Download failed: " + (text.size() > 1 ? text.substr(1) : "no response");
            return false;
        }
        const auto first = text.find('\n'), second = text.find('\n', first == text.npos ? first : first + 1);
        if (first == text.npos || second == text.npos)
        {
            error = "Malformed download result";
            return false;
        }
        response.status = std::strtol(text.c_str(), nullptr, 10);
        response.location = text.substr(first + 1, second - first - 1);
        response.bytes = std::strtoull(text.c_str() + second + 1, nullptr, 10);
        return true;
    }
} // namespace OpenBlood2
