// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <string>
#include <QWidget>
#include "common/common_types.h"

class QBoxLayout;
class QFrame;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QMainWindow;
class QMenu;
class QPushButton;
class QStandardItemModel;
class QTimer;
class QToolButton;
class QVBoxLayout;

namespace FritchiiUi {

/// What the home screen asks the main window to do.
struct HomeHooks {
    std::function<void(const QString& path, u64 program_id)> play;
    std::function<void()> add_folder;
    std::function<void()> open_settings;
    std::function<void(u64 program_id, const std::string& path)> game_settings;
    std::function<void(u64 program_id)> delete_shader_cache; ///< after a confirmation
    /// Fills a menu with the game list's own right-click entries for the game (mod folders,
    /// dumps, Remove, Verify Integrity, Configure Game...).
    std::function<void(QMenu& menu, u64 program_id, const std::string& path)> game_menu;
    std::function<bool()> game_list_empty;
    std::function<bool()> single_window;
    std::function<bool()> emulation_running;
    QStandardItemModel* model = nullptr;
    QMenu* help_menu = nullptr;
    QMenu* network_menu = nullptr; ///< Cervin: Multiplayer, behind its own rail button
    QList<QMenu*> classic_menus;  ///< File, Game, View, Tools, Help behind the rail's menu button
    /// Cervin: runs `change` with the game's own settings (its per-game config) loaded into
    /// Settings::values, and saves them when it returns true. Does nothing while a game runs.
    std::function<void(u64 program_id, const std::function<bool()>& change)> with_game_settings;
};

/// The main screen: a left icon rail (Library, Settings, Help, and the classic menus behind a
/// button) and a home page. Cervin: every game as a card in a grid (cover, name, Play and its
/// settings), and beside it the selected game's quick settings (DLSS priority and model, console
/// mode, speed limit), written to that game's own settings. The classic game list stays one click
/// away ("All games"); the main window keeps managing it as before.
class Home : public QObject {
    Q_OBJECT
public:
    Home(QMainWindow* window, QBoxLayout* central, QWidget* game_list, QWidget* placeholder,
         HomeHooks hooks);

    /// A game is starting in the main window: the render view takes the space.
    void OnEmulationStarting();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void ShowHome();
    void ShowList();
    void Restore();
    void Refresh();
    /// The quick settings beside the grid, for the selected game.
    void RefreshPanel();

    QWidget* MakeRail();
    QWidget* MakePage();
    QToolButton* RailButton(const QString& icon, const QString& tip);

    QMainWindow* window;
    QWidget* game_list;
    QWidget* placeholder;
    HomeHooks hooks;

    QWidget* rail = nullptr;
    QWidget* page = nullptr;
    QToolButton* library_button = nullptr;
    QWidget* grid_box = nullptr;      ///< The game cards
    QGridLayout* grid = nullptr;
    QFrame* panel = nullptr;          ///< The selected game's quick settings
    QVBoxLayout* panel_layout = nullptr;
    QTimer* refresh_timer = nullptr;

    bool list_mode = false;
    QString hero_path;                ///< The selected game
    QString hero_title;
    u64 hero_id = 0;
};

} // namespace FritchiiUi
