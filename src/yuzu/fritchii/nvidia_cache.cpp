// SPDX-License-Identifier: GPL-3.0-or-later

#include "yuzu/fritchii/nvidia_cache.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include <QFile>
#include <QMessageBox>
#include <QString>
#include <QWidget>

#include "common/fs/path_util.h"
#include "common/logging.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace FritchiiUi {

#ifdef _WIN32
namespace {

// NVAPI (github.com/NVIDIA/nvapi, MIT): function ids from nvapi_interface.h, setting id from
// NvApiDriverSettings.h, structures as in nvapi.h (version 1 of each).
constexpr std::uint32_t IdInitialize = 0x0150e828;
constexpr std::uint32_t IdCreateSession = 0x0694d52e;
constexpr std::uint32_t IdDestroySession = 0xdad9cff8;
constexpr std::uint32_t IdLoadSettings = 0x375dbd6b;
constexpr std::uint32_t IdSaveSettings = 0xfcbc7e14;
constexpr std::uint32_t IdFindApplicationByName = 0xeee566b2;
constexpr std::uint32_t IdFindProfileByName = 0x7e4a9a0b;
constexpr std::uint32_t IdCreateProfile = 0xcc176068;
constexpr std::uint32_t IdCreateApplication = 0x4347a9de;
constexpr std::uint32_t IdSetSetting = 0x577dd202;
constexpr std::uint32_t IdGetSetting = 0x73bf8338;

/// "Shader disk cache maximum size" (PS_SHADERDISKCACHE_MAX_SIZE), in MB.
constexpr std::uint32_t ShaderCacheMaxSizeId = 0x00AC8497;
constexpr std::uint32_t WantedMegabytes = 8192; // 8 GB (owner's choice)

constexpr std::size_t UnicodeMax = 2048;
using Unicode = std::uint16_t[UnicodeMax];

struct Profile {
    std::uint32_t version;
    Unicode profileName;
    std::uint32_t gpuSupport;
    std::uint32_t isPredefined;
    std::uint32_t numOfApps;
    std::uint32_t numOfSettings;
};
struct Application {
    std::uint32_t version;
    std::uint32_t isPredefined;
    Unicode appName;
    Unicode userFriendlyName;
    Unicode launcher;
};
struct BinarySetting {
    std::uint32_t valueLength;
    std::uint8_t valueData[4096];
};
struct Setting {
    std::uint32_t version;
    Unicode settingName;
    std::uint32_t settingId;
    std::uint32_t settingType; // 0: DWORD
    std::uint32_t settingLocation;
    std::uint32_t isCurrentPredefined;
    std::uint32_t isPredefinedValid;
    union {
        std::uint32_t u32PredefinedValue;
        BinarySetting binaryPredefinedValue;
        Unicode wszPredefinedValue;
    };
    union {
        std::uint32_t u32CurrentValue;
        BinarySetting binaryCurrentValue;
        Unicode wszCurrentValue;
    };
};

template <typename T>
constexpr std::uint32_t Version() {
    return static_cast<std::uint32_t>(sizeof(T)) | (1u << 16);
}

void CopyName(Unicode& out, const wchar_t* text) {
    std::memset(out, 0, sizeof(out));
    for (std::size_t i = 0; text[i] != 0 && i + 1 < UnicodeMax; ++i) {
        out[i] = static_cast<std::uint16_t>(text[i]);
    }
}

using Session = void*;
using ProfileHandle = void*;

struct Api {
    using QueryInterface = void* (*)(std::uint32_t);
    HMODULE module = nullptr;
    QueryInterface query = nullptr;

    template <typename F>
    F Get(std::uint32_t id) const {
        return reinterpret_cast<F>(query(id));
    }
    bool Open() {
        module = LoadLibraryW(L"nvapi64.dll");
        if (module == nullptr) {
            return false;
        }
        query = reinterpret_cast<QueryInterface>(GetProcAddress(module, "nvapi_QueryInterface"));
        if (query == nullptr) {
            return false;
        }
        const auto init = Get<int (*)()>(IdInitialize);
        return init != nullptr && init() == 0;
    }
};

enum class State { NoNvidia, AlreadyEnough, Missing, Failed };

struct Result {
    State state = State::NoNvidia;
    std::uint32_t current_mb = 0;
    int error = 0;
};

/// Reads (and with `apply`, writes) the setting in Cervin.exe's profile.
Result Run(bool apply) {
    Result result;
    Api api;
    if (!api.Open()) {
        return result;
    }
    const auto create_session = api.Get<int (*)(Session*)>(IdCreateSession);
    const auto destroy_session = api.Get<int (*)(Session)>(IdDestroySession);
    const auto load = api.Get<int (*)(Session)>(IdLoadSettings);
    const auto save = api.Get<int (*)(Session)>(IdSaveSettings);
    const auto find_app =
        api.Get<int (*)(Session, std::uint16_t*, ProfileHandle*, Application*)>(
            IdFindApplicationByName);
    const auto find_profile =
        api.Get<int (*)(Session, std::uint16_t*, ProfileHandle*)>(IdFindProfileByName);
    const auto create_profile =
        api.Get<int (*)(Session, Profile*, ProfileHandle*)>(IdCreateProfile);
    const auto create_app = api.Get<int (*)(Session, ProfileHandle, Application*)>(
        IdCreateApplication);
    const auto set_setting = api.Get<int (*)(Session, ProfileHandle, Setting*)>(IdSetSetting);
    const auto get_setting =
        api.Get<int (*)(Session, ProfileHandle, std::uint32_t, Setting*)>(IdGetSetting);
    if (!create_session || !destroy_session || !load || !save || !find_app || !find_profile ||
        !create_profile || !create_app || !set_setting || !get_setting) {
        return result;
    }
    Session session = nullptr;
    if (create_session(&session) != 0) {
        return result;
    }
    result.state = State::Failed;
    auto* const app = new Application{};
    auto* const setting = new Setting{};
    auto* const profile = new Profile{};
    do {
        if ((result.error = load(session)) != 0) {
            break;
        }
        Unicode exe{};
        CopyName(exe, L"cervin.exe");
        ProfileHandle handle = nullptr;
        app->version = Version<Application>();
        const bool app_found = find_app(session, exe, &handle, app) == 0 && handle != nullptr;
        if (app_found) {
            setting->version = Version<Setting>();
            if (get_setting(session, handle, ShaderCacheMaxSizeId, setting) == 0) {
                result.current_mb = setting->u32CurrentValue;
                if (result.current_mb >= WantedMegabytes) { // 0xFFFFFFFF: unlimited
                    result.state = State::AlreadyEnough;
                    break;
                }
            }
        }
        if (!apply) {
            result.state = State::Missing;
            break;
        }
        if (!app_found) {
            Unicode name{};
            CopyName(name, L"Cervin");
            if (find_profile(session, name, &handle) != 0 || handle == nullptr) {
                profile->version = Version<Profile>();
                CopyName(profile->profileName, L"Cervin");
                if ((result.error = create_profile(session, profile, &handle)) != 0) {
                    break;
                }
            }
            std::memset(app, 0, sizeof(*app));
            app->version = Version<Application>();
            CopyName(app->appName, L"cervin.exe");
            CopyName(app->userFriendlyName, L"Cervin");
            if ((result.error = create_app(session, handle, app)) != 0) {
                break;
            }
        }
        std::memset(setting, 0, sizeof(*setting));
        setting->version = Version<Setting>();
        setting->settingId = ShaderCacheMaxSizeId;
        setting->settingType = 0;
        setting->u32CurrentValue = WantedMegabytes;
        if ((result.error = set_setting(session, handle, setting)) != 0) {
            break;
        }
        if ((result.error = save(session)) != 0) {
            break;
        }
        result.current_mb = WantedMegabytes;
        result.state = State::AlreadyEnough;
    } while (false);
    delete profile;
    delete setting;
    delete app;
    destroy_session(session);
    return result;
}

QString AnswerPath() {
    return QString::fromStdString(
        Common::FS::PathToUTF8String(Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir) /
                                     "cervin_nvidia_cache.txt"));
}

} // namespace
#endif

void OfferNvidiaShaderCache([[maybe_unused]] QWidget* parent) {
#ifdef _WIN32
    if (const char* value = std::getenv("CERVIN_NVIDIA_CACHE"); value && value[0] == '0') {
        return;
    }
    const Result found = Run(false);
    if (found.state == State::NoNvidia) {
        return;
    }
    if (found.state == State::AlreadyEnough) {
        LOG_INFO(Frontend, "NVIDIA shader cache for Cervin.exe: {} MB, enough", found.current_mb);
        return;
    }
    QFile answer_file(AnswerPath());
    if (answer_file.exists()) {
        LOG_INFO(Frontend, "NVIDIA shader cache for Cervin.exe: left as is (answered before)");
        return;
    }
    const auto choice = QMessageBox::question(
        parent, QObject::tr("NVIDIA shader cache"),
        QObject::tr("Your NVIDIA driver keeps only about 1 GB of compiled shaders for Cervin, "
                    "too little for Ocarina of Time: shaders already prepared get compiled again "
                    "while you play, which makes it stutter.\n\nLet Cervin raise this cache to "
                    "8 GB, for Cervin only? Your other games and NVIDIA settings don't change; "
                    "it only uses up to 8 GB of disk space."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (choice != QMessageBox::Yes) {
        if (answer_file.open(QFile::WriteOnly)) {
            answer_file.write("no\n");
        }
        LOG_INFO(Frontend, "NVIDIA shader cache for Cervin.exe: player declined");
        return;
    }
    const Result applied = Run(true);
    if (applied.state == State::AlreadyEnough) {
        if (answer_file.open(QFile::WriteOnly)) {
            answer_file.write("yes\n");
        }
        LOG_INFO(Frontend, "NVIDIA shader cache for Cervin.exe set to {} MB", applied.current_mb);
        QMessageBox::information(
            parent, QObject::tr("NVIDIA shader cache"),
            QObject::tr("Done: Cervin's NVIDIA shader cache is now 8 GB. Run Emulation > Prepare "
                        "Shaders Now once in Ocarina of Time so the driver keeps them all."));
        return;
    }
    LOG_WARNING(Frontend, "NVIDIA shader cache for Cervin.exe: not changed (NVAPI error {})",
                applied.error);
    QMessageBox::warning(
        parent, QObject::tr("NVIDIA shader cache"),
        QObject::tr("Cervin could not change the setting (NVIDIA error %1; Windows may require "
                    "administrator rights). You can do it yourself: in the NVIDIA app or NVIDIA "
                    "Control Panel > Manage 3D settings, set Shader Cache Size to 10 GB or "
                    "Unlimited.")
            .arg(applied.error));
#endif
}

} // namespace FritchiiUi
