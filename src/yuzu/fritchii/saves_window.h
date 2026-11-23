// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <optional>

#include <QPixmap>
#include <QString>

#include "common/common_types.h"

class QWidget;

namespace FritchiiUi {

/// The Saves window (Fritchii v5.8.3): export the saves to a .zip, import them from one, or copy
/// them between Fritchii, Eden (%APPDATA%\eden) and Lichii 150+ (%APPDATA%\lichii). Saves that
/// get replaced are backed up first. `emulation_running` blocks it while a game runs.
/// Cervin adds "From another emulator": pick games and copy their saves into Cervin from Ryujinx
/// or any yuzu-family emulator (yuzu, suyu, Sudachi, Citron, Eden, Trudy, Lichii, Eglantine,
/// Orchid), installed or portable.
void ShowSavesWindow(QWidget* parent, const std::function<bool()>& emulation_running);

/// A game's name and icon, for the copy list (Cervin). Set by the home page from the game list.
struct SaveGameInfo {
    QString title;
    QPixmap icon;
};
void SetSaveGameInfoLookup(std::function<std::optional<SaveGameInfo>(u64 program_id)> lookup);

} // namespace FritchiiUi
