// SPDX-License-Identifier: GPL-3.0-or-later

// Cervin: saves from other Switch emulators. Finds Ryujinx and the yuzu-family emulators (yuzu,
// suyu, Sudachi, Citron, Eden, Trudy, Lichii, Eglantine, Orchid), lists their saves per game and
// copies the chosen ones into Cervin's save folder. No Qt here, so it can be tested on its own.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "common/common_types.h"

namespace Cervin::EmulatorSaves {

namespace fs = std::filesystem;

enum class Kind { YuzuFamily, Ryujinx };

struct Source {
    std::string name; ///< the emulator's own name, shown as is
    Kind kind{};
    /// Yuzu family: <nand>/user/save/0000000000000000 (a folder per user, one per game inside).
    /// Ryujinx: <data>/bis/user/save (one numbered folder per save, its files in 0/).
    fs::path root;
};

struct GameSave {
    u64 title_id{};
    bool device{}; ///< a device save (shared by every user), else a user's
    fs::path dir;  ///< the folder holding the save's own files
    fs::file_time_type modified{};
};

/// The emulators with saves in the usual folders: `data_home` holds the yuzu family's folders
/// (%APPDATA% on Windows, ~/.local/share on Linux), `config_home` Ryujinx's (%APPDATA%,
/// ~/.config). `exclude_root` is Cervin's own save root, never listed.
std::vector<Source> Detect(const fs::path& data_home, const fs::path& config_home,
                           const fs::path& exclude_root);

/// A folder the player picked: an emulator's data folder, a portable install (yuzu's user/ next
/// to the exe, Ryujinx's portable/), a nand folder or a save folder. nullopt when none matches.
std::optional<Source> FromFolder(const fs::path& folder);

/// The saves in `source`, one per game and kind (device or user). With several users, the
/// source's first user wins (yuzu family), else the most recently changed save.
std::vector<GameSave> List(const Source& source);

/// Reads a Ryujinx save folder (bis/user/save/<id>) from its ExtraData0/ExtraData1 (LibHac
/// SaveDataExtraData: program id at 0, save type at 0x20). nullopt unless it is a game's user
/// (type 1) or device (type 3) save with files in 0/ (or 1/).
std::optional<GameSave> ReadRyujinxSave(const fs::path& save_dir);

/// The user folder (32 hex digits) saves go to in a yuzu-family save root: the user with saves
/// already, else the install's first user (its profiles.dat). Empty when it has no user yet.
std::string TargetUser(const fs::path& root);

/// Copies `saves` into the yuzu-family save root `root` (Cervin), replacing those games' saves
/// there. Throws std::runtime_error when a user save needs a user and `root` has none. Returns
/// how many saves were copied.
int CopyInto(const std::vector<GameSave>& saves, const fs::path& root);

/// The newest change time of a folder's files (the folder's own time when empty).
fs::file_time_type LastChange(const fs::path& dir);

} // namespace Cervin::EmulatorSaves
