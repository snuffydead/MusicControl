#include <jni.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Foundation.h>
#include <string>
#include <vector>
#include <future>
#include <stdexcept>
#include <windows.h>

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Storage::Streams;
using namespace Windows::Foundation;

void log_debug(const char* message) {
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

template <typename T>
T await(IAsyncOperation<T> const& op, const char* context) {
    try {
        std::promise<T> p;
        auto f = p.get_future();
        op.Completed([&p](auto&& asyncOp, auto&& status) {
            if (status == AsyncStatus::Completed) {
                p.set_value(asyncOp.GetResults());
            }
            else {
                p.set_exception(std::make_exception_ptr(std::runtime_error("Async operation failed")));
            }
            });
        return f.get();
    }
    catch (const std::exception& e) {
        std::string msg = std::string(context) + ": " + e.what();
        log_debug(msg.c_str());
        throw;
    }
}

std::string to_base64(const std::vector<uint8_t>& data) {
    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    size_t i = 0;
    uint8_t buf3[3];
    uint8_t buf4[4];

    for (uint8_t byte : data) {
        buf3[i++] = byte;
        if (i == 3) {
            buf4[0] = (buf3[0] & 0xfc) >> 2;
            buf4[1] = ((buf3[0] & 0x03) << 4) | ((buf3[1] & 0xf0) >> 4);
            buf4[2] = ((buf3[1] & 0x0f) << 2) | ((buf3[2] & 0xc0) >> 6);
            buf4[3] = buf3[2] & 0x3f;
            for (int j = 0; j < 4; ++j) {
                result += chars[buf4[j]];
            }
            i = 0;
        }
    }

    if (i > 0) {
        for (size_t j = i; j < 3; ++j) {
            buf3[j] = 0;
        }
        buf4[0] = (buf3[0] & 0xfc) >> 2;
        buf4[1] = ((buf3[0] & 0x03) << 4) | ((buf3[1] & 0xf0) >> 4);
        buf4[2] = ((buf3[1] & 0x0f) << 2) | ((buf3[2] & 0xc0) >> 6);
        for (size_t j = 0; j < i + 1; ++j) {
            result += chars[buf4[j]];
        }
        while (i++ < 3) {
            result += '=';
        }
    }
    return result;
}

GlobalSystemMediaTransportControlsSession get_session() {
    try {
        auto manager = await(GlobalSystemMediaTransportControlsSessionManager::RequestAsync(), "RequestAsync");
        if (!manager) {
            log_debug("No media transport controls manager available");
            return nullptr;
        }
        auto session = manager.GetCurrentSession();
        if (!session) {
            log_debug("No active media session found");
        }
        return session;
    }
    catch (...) {
        log_debug("Failed to get media session");
        return nullptr;
    }
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    try {
        init_apartment(winrt::apartment_type::multi_threaded);
        log_debug("JNI initialized successfully");
        return JNI_VERSION_1_6;
    }
    catch (...) {
        log_debug("JNI initialization failed");
        return -1;
    }
}

extern "C" JNIEXPORT jstring JNICALL Java_ru_snuffy_music_MusicController_getCurrentTrackTitle(JNIEnv* env, jobject) {
    try {
        auto session = get_session();
        if (session) {
            auto info = await(session.TryGetMediaPropertiesAsync(), "TryGetMediaPropertiesAsync (title)");
            if (info) {
                return env->NewStringUTF(to_string(info.Title()).c_str());
            }
            log_debug("No media properties available for title");
        }
    }
    catch (...) {
        log_debug("Error in getCurrentTrackTitle");
    }
    return nullptr;
}

extern "C" JNIEXPORT jstring JNICALL Java_ru_snuffy_music_MusicController_getCurrentTrackAlbum(JNIEnv* env, jobject) {
    try {
        auto session = get_session();
        if (session) {
            auto info = await(session.TryGetMediaPropertiesAsync(), "TryGetMediaPropertiesAsync (album)");
            if (info) {
                return env->NewStringUTF(to_string(info.AlbumTitle()).c_str());
            }
            log_debug("No media properties available for album");
        }
    }
    catch (...) {
        log_debug("Error in getCurrentTrackAlbum");
    }
    return nullptr;
}

extern "C" JNIEXPORT jstring JNICALL Java_ru_snuffy_music_MusicController_getCurrentTrackArtist(JNIEnv* env, jobject) {
    try {
        auto session = get_session();
        if (session) {
            auto info = await(session.TryGetMediaPropertiesAsync(), "TryGetMediaPropertiesAsync (artist)");
            if (info) {
                return env->NewStringUTF(to_string(info.Artist()).c_str());
            }
            log_debug("No media properties available for artist");
        }
    }
    catch (...) {
        log_debug("Error in getCurrentTrackArtist");
    }
    return nullptr;
}

extern "C" JNIEXPORT jstring JNICALL Java_ru_snuffy_music_MusicController_getCurrentTrackCoverBase64(JNIEnv* env, jobject) {
    try {
        auto session = get_session();
        if (session) {
            auto info = await(session.TryGetMediaPropertiesAsync(), "TryGetMediaPropertiesAsync (cover)");
            if (info && info.Thumbnail()) {
                auto stream = await(info.Thumbnail().OpenReadAsync(), "OpenReadAsync (cover)");
                DataReader reader(stream.GetInputStreamAt(0));
                auto bytes = std::vector<uint8_t>(static_cast<size_t>(stream.Size()));
                await(reader.LoadAsync(static_cast<uint32_t>(stream.Size())), "LoadAsync (cover)");
                reader.ReadBytes(bytes);
                return env->NewStringUTF(to_base64(bytes).c_str());
            }
            log_debug("No thumbnail available for cover");
        }
    }
    catch (...) {
        log_debug("Error in getCurrentTrackCoverBase64");
    }
    return nullptr;
}

extern "C" JNIEXPORT jint JNICALL Java_ru_snuffy_music_MusicController_getCurrentTrackProgress(JNIEnv* env, jobject) {
    try {
        auto session = get_session();
        if (session) {
            auto timeline = session.GetTimelineProperties();
            return static_cast<jint>(timeline.Position().count() / 10000000);
        }
    }
    catch (...) {
        log_debug("Error in getCurrentTrackProgress");
    }
    return -1;
}

extern "C" JNIEXPORT jint JNICALL Java_ru_snuffy_music_MusicController_getCurrentTrackDuration(JNIEnv* env, jobject) {
    try {
        auto session = get_session();
        if (session) {
            auto timeline = session.GetTimelineProperties();
            return static_cast<jint>(timeline.EndTime().count() / 10000000);
        }
    }
    catch (...) {
        log_debug("Error in getCurrentTrackDuration");
    }
    return -1;
}

extern "C" JNIEXPORT jboolean JNICALL Java_ru_snuffy_music_MusicController_isPlaying(JNIEnv* env, jobject) {
    try {
        auto session = get_session();
        if (session) {
            auto status = session.GetPlaybackInfo().PlaybackStatus();
            return status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing ? JNI_TRUE : JNI_FALSE;
        }
    }
    catch (...) {
        log_debug("Error in isPlaying");
    }
    return JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL Java_ru_snuffy_music_MusicController_setPlaying(JNIEnv* env, jobject, jboolean play) {
    try {
        auto session = get_session();
        if (session) {
            if (play) {
                await(session.TryPlayAsync(), "TryPlayAsync");
            }
            else {
                await(session.TryPauseAsync(), "TryPauseAsync");
            }
        }
    }
    catch (...) {
        log_debug("Error in setPlaying");
    }
}

extern "C" JNIEXPORT void JNICALL Java_ru_snuffy_music_MusicController_seekTo(JNIEnv* env, jobject, jint seconds) {
    try {
        auto session = get_session();
        if (session) {
            int64_t position = static_cast<int64_t>(seconds) * 10000000;
            await(session.TryChangePlaybackPositionAsync(position), "TryChangePlaybackPositionAsync");
        }
    }
    catch (...) {
        log_debug("Error in seekTo");
    }
}

extern "C" JNIEXPORT void JNICALL Java_ru_snuffy_music_MusicController_nextTrack(JNIEnv* env, jobject) {
    try {
        auto session = get_session();
        if (session) {
            await(session.TrySkipNextAsync(), "TrySkipNextAsync");
        }
    }
    catch (...) {
        log_debug("Error in nextTrack");
    }
}

extern "C" JNIEXPORT void JNICALL Java_ru_snuffy_music_MusicController_previousTrack(JNIEnv* env, jobject) {
    try {
        auto session = get_session();
        if (session) {
            await(session.TrySkipPreviousAsync(), "TrySkipPreviousAsync");
        }
    }
    catch (...) {
        log_debug("Error in previousTrack");
    }
}

extern "C" JNIEXPORT jint JNICALL Java_ru_snuffy_music_MusicController_getLastKnownPosition(JNIEnv* env, jobject) {
    try {
        auto session = get_session();
        if (session) {
            auto timeline = session.GetTimelineProperties();
            return static_cast<jint>(timeline.Position().count() / 10'000'000);
        }
    }
    catch (...) {
        log_debug("Error in getLastKnownPosition");
    }
    return -1;
}

extern "C" JNIEXPORT jlong JNICALL Java_ru_snuffy_music_MusicController_getLastUpdatedTime(JNIEnv* env, jobject) {
    try {
        auto session = get_session();
        if (session) {
            auto timeline = session.GetTimelineProperties();
            auto timestamp = timeline.LastUpdatedTime();
            int64_t unixTimeMs = (timestamp.time_since_epoch().count() - 116444736000000000LL) / 10'000;
            return static_cast<jlong>(unixTimeMs);
        }
    }
    catch (...) {
        log_debug("Error in getLastUpdatedTime");
    }
    return -1;
}