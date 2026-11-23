// SPDX-License-Identifier: GPL-3.0-or-later

#include "yuzu/fritchii/emulator_saves.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace Cervin::EmulatorSaves {

namespace {

constexpr const char* SaveSpace = "0000000000000000";
constexpr const char* DeviceUser = "00000000000000000000000000000000";

std::string Utf8(const fs::path& path) {
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

bool IsDir(const fs::path& path) {
    std::error_code ec;
    return fs::is_directory(path, ec);
}

bool IsHex(const std::string& text, std::size_t length) {
    return text.size() == length && std::ranges::all_of(text, [](char c) {
               return std::isxdigit(static_cast<unsigned char>(c)) != 0;
           });
}

std::optional<u64> ParseTitle(const std::string& name) {
    if (!IsHex(name, 16)) {
        return std::nullopt;
    }
    u64 value{};
    const auto [end, error] = std::from_chars(name.data(), name.data() + name.size(), value, 16);
    if (error != std::errc{} || value == 0) {
        return std::nullopt;
    }
    return value;
}

std::string TitleFolder(u64 title_id) {
    std::array<char, 17> text{};
    std::snprintf(text.data(), text.size(), "%016llX", static_cast<unsigned long long>(title_id));
    return text.data();
}

/// The user folders (32 hex digits) of a yuzu-family save root.
std::vector<fs::path> UserFolders(const fs::path& root) {
    std::vector<fs::path> result;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (entry.is_directory(ec) && IsHex(Utf8(entry.path().filename()), 32)) {
            result.push_back(entry.path());
        }
    }
    return result;
}

/// The users an install has, from <nand>/system/save/8000000000000010/su/avators/profiles.dat
/// (0x10 header, then 8 users of 0xC8 bytes; a user id is written high half then low half).
std::vector<std::string> InstallUsers(const fs::path& root) {
    const fs::path nand = root.parent_path().parent_path().parent_path();
    std::ifstream file(nand / "system" / "save" / "8000000000000010" / "su" / "avators" /
                           "profiles.dat",
                       std::ios::binary);
    const std::vector<char> data((std::istreambuf_iterator<char>(file)), {});
    std::vector<std::string> ids;
    for (std::size_t offset = 0x10; offset + 0xC8 <= data.size(); offset += 0xC8) {
        u64 low{};
        u64 high{};
        std::memcpy(&low, data.data() + offset, sizeof(low));
        std::memcpy(&high, data.data() + offset + 8, sizeof(high));
        if (low != 0 || high != 0) {
            std::array<char, 33> text{};
            std::snprintf(text.data(), text.size(), "%016llX%016llX",
                          static_cast<unsigned long long>(high),
                          static_cast<unsigned long long>(low));
            ids.emplace_back(text.data());
        }
    }
    return ids;
}

bool HasFiles(const fs::path& dir) {
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(dir, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (it->is_regular_file(ec)) {
            return true;
        }
    }
    return false;
}

std::vector<GameSave> ListYuzu(const fs::path& root) {
    const auto users = InstallUsers(root);
    const std::string first_user = users.empty() ? std::string{} : users.front();
    struct Pick {
        GameSave save;
        bool first_user;
    };
    std::map<std::pair<u64, bool>, Pick> picks;
    for (const auto& user : UserFolders(root)) {
        const std::string user_name = Utf8(user.filename());
        const bool device = user_name == DeviceUser;
        const bool is_first = !first_user.empty() && user_name == first_user;
        std::error_code ec;
        for (const auto& game : fs::directory_iterator(user, ec)) {
            const auto title = ParseTitle(Utf8(game.path().filename()));
            if (!title || !game.is_directory(ec) || !HasFiles(game.path())) {
                continue;
            }
            GameSave save{*title, device, game.path(), LastChange(game.path())};
            const auto key = std::make_pair(*title, device);
            const auto it = picks.find(key);
            const bool better = it == picks.end() || (is_first && !it->second.first_user) ||
                                (is_first == it->second.first_user &&
                                 save.modified > it->second.save.modified);
            if (better) {
                picks[key] = Pick{std::move(save), is_first};
            }
        }
    }
    std::vector<GameSave> result;
    for (auto& [key, pick] : picks) {
        result.push_back(std::move(pick.save));
    }
    return result;
}

std::vector<GameSave> ListRyujinx(const fs::path& root) {
    std::map<std::pair<u64, bool>, GameSave> picks;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (!entry.is_directory(ec)) {
            continue;
        }
        auto save = ReadRyujinxSave(entry.path());
        if (!save || !HasFiles(save->dir)) {
            continue;
        }
        const auto key = std::make_pair(save->title_id, save->device);
        const auto it = picks.find(key);
        if (it == picks.end() || save->modified > it->second.modified) {
            picks[key] = std::move(*save);
        }
    }
    std::vector<GameSave> result;
    for (auto& [key, save] : picks) {
        result.push_back(std::move(save));
    }
    return result;
}

std::optional<Source> Probe(const std::string& name, const fs::path& folder) {
    // Ryujinx: its data folder, or a portable install (portable/ next to the exe).
    for (const fs::path candidate : {folder / "bis" / "user" / "save",
                                     folder / "portable" / "bis" / "user" / "save"}) {
        if (IsDir(candidate)) {
            return Source{name, Kind::Ryujinx, candidate};
        }
    }
    // Yuzu family: the data folder, a portable user/ folder, a nand folder, or a save folder.
    for (const fs::path candidate :
         {folder / "nand" / "user" / "save" / SaveSpace,
          folder / "user" / "nand" / "user" / "save" / SaveSpace, folder / "user" / "save" / SaveSpace,
          folder / "save" / SaveSpace, folder / SaveSpace}) {
        if (IsDir(candidate)) {
            return Source{name, Kind::YuzuFamily, candidate};
        }
    }
    if (!UserFolders(folder).empty()) {
        return Source{name, Kind::YuzuFamily, folder};
    }
    return std::nullopt;
}

bool SamePlace(const fs::path& a, const fs::path& b) {
    std::error_code ec;
    return !b.empty() && fs::equivalent(a, b, ec);
}

} // namespace

fs::file_time_type LastChange(const fs::path& dir) {
    std::error_code ec;
    fs::file_time_type newest = fs::last_write_time(dir, ec);
    for (auto it = fs::recursive_directory_iterator(dir, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        std::error_code time_ec;
        const auto time = it->last_write_time(time_ec);
        if (!time_ec && time > newest) {
            newest = time;
        }
    }
    return newest;
}

std::vector<Source> Detect(const fs::path& data_home, const fs::path& config_home,
                           const fs::path& exclude_root) {
    struct Known {
        const char* name;
        const char* folder;
    };
    // The folder each one keeps its data in. Trudy uses Eden's; Orchid has its own.
    static constexpr std::array<Known, 8> yuzu_family{{{"Eden / Trudy", "eden"},
                                                       {"Lichii", "lichii"},
                                                       {"Eglantine", "eglantine"},
                                                       {"Orchid", "orchid"},
                                                       {"yuzu", "yuzu"},
                                                       {"suyu", "suyu"},
                                                       {"Sudachi", "sudachi"},
                                                       {"Citron", "citron"}}};
    std::vector<Source> sources;
    if (!config_home.empty()) {
        const fs::path ryujinx = config_home / "Ryujinx" / "bis" / "user" / "save";
        if (IsDir(ryujinx)) {
            sources.push_back({"Ryujinx", Kind::Ryujinx, ryujinx});
        }
    }
    if (!data_home.empty()) {
        for (const auto& known : yuzu_family) {
            const fs::path root = data_home / known.folder / "nand" / "user" / "save" / SaveSpace;
            if (IsDir(root) && !SamePlace(root, exclude_root)) {
                sources.push_back({known.name, Kind::YuzuFamily, root});
            }
        }
    }
    return sources;
}

std::optional<Source> FromFolder(const fs::path& folder) {
    std::string name = Utf8(folder.filename());
    if (name.empty()) {
        name = Utf8(folder.parent_path().filename());
    }
    return Probe(name, folder);
}

std::vector<GameSave> List(const Source& source) {
    return source.kind == Kind::Ryujinx ? ListRyujinx(source.root) : ListYuzu(source.root);
}

std::optional<GameSave> ReadRyujinxSave(const fs::path& save_dir) {
    std::array<unsigned char, 0x40> extra{};
    bool read = false;
    for (const char* name : {"ExtraData0", "ExtraData1"}) {
        std::ifstream file(save_dir / name, std::ios::binary);
        if (file.read(reinterpret_cast<char*>(extra.data()), extra.size()) &&
            file.gcount() == static_cast<std::streamsize>(extra.size())) {
            read = true;
            break;
        }
    }
    if (!read) {
        return std::nullopt;
    }
    u64 program_id{};
    std::memcpy(&program_id, extra.data(), sizeof(program_id));
    const unsigned char type = extra[0x20];
    constexpr unsigned char AccountSave = 1;
    constexpr unsigned char DeviceSave = 3;
    if (program_id == 0 || (type != AccountSave && type != DeviceSave)) {
        return std::nullopt;
    }
    // 0/ holds the committed files; 1/ is the working copy some versions leave alone.
    fs::path files = save_dir / "0";
    if (!IsDir(files)) {
        files = save_dir / "1";
        if (!IsDir(files)) {
            return std::nullopt;
        }
    }
    return GameSave{program_id, type == DeviceSave, files, LastChange(files)};
}

std::string TargetUser(const fs::path& root) {
    const auto users = InstallUsers(root);
    for (const auto& id : users) {
        if (IsDir(root / id)) {
            return id;
        }
    }
    if (!users.empty()) {
        return users.front();
    }
    // No profiles.dat: the one user folder with saves, if there is exactly one.
    std::vector<fs::path> folders;
    for (const auto& folder : UserFolders(root)) {
        if (Utf8(folder.filename()) != DeviceUser) {
            folders.push_back(folder);
        }
    }
    return folders.size() == 1 ? Utf8(folders.front().filename()) : std::string{};
}

int CopyInto(const std::vector<GameSave>& saves, const fs::path& root) {
    const std::string user = TargetUser(root);
    int copied = 0;
    for (const auto& save : saves) {
        if (!save.device && user.empty()) {
            throw std::runtime_error("Cervin has no user profile yet");
        }
        const fs::path target = root / (save.device ? std::string{DeviceUser} : user) /
                                TitleFolder(save.title_id);
        fs::remove_all(target);
        fs::create_directories(target);
        fs::copy(save.dir, target, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        ++copied;
    }
    return copied;
}

} // namespace Cervin::EmulatorSaves
