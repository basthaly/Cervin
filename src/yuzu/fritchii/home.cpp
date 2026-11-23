// SPDX-License-Identifier: GPL-3.0-or-later

#include "yuzu/fritchii/home.h"

#include <algorithm>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include <QApplication>
#include <QBoxLayout>
#include <QComboBox>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QStyle>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTimer>
#include <QToolButton>

#include "common/game_presets.h"
#include "common/settings.h"
#include "qt_common/game_list/game_list_p.h"
#include "yuzu/fritchii/fritchii_ui.h"
#include "yuzu/fritchii/saves_window.h"

namespace FritchiiUi {
namespace {

constexpr u64 OcarinaOfTime = 0x0400486024DEC000ULL;
constexpr int CardCover = 196;
constexpr int CardWidth = 220;
constexpr int PanelWidth = 320;

/// One quick setting of a game: its choices (the first is "as in the main settings") and how it
/// is read from and written to Settings::values while that game's settings are loaded.
struct QuickChoice {
    QString label;
    std::vector<std::pair<QString, u32>> items;
    std::function<std::optional<u32>()> read;          ///< nullopt: uses the main settings
    std::function<void(std::optional<u32>)> write;
};

template <typename T, bool ranged>
QuickChoice MakeChoice(const QString& label, std::vector<std::pair<QString, u32>> items,
                       Settings::SwitchableSetting<T, ranged>& setting) {
    return QuickChoice{
        .label = label,
        .items = std::move(items),
        .read = [&setting]() -> std::optional<u32> {
            if (setting.UsingGlobal()) {
                return std::nullopt;
            }
            return static_cast<u32>(setting.GetValue());
        },
        .write =
            [&setting](std::optional<u32> value) {
                if (!value) {
                    setting.SetGlobal(true);
                    return;
                }
                setting.SetGlobal(false);
                setting.SetValue(static_cast<T>(*value));
            },
    };
}

struct Game {
    QString title;
    QString path;
    u64 program_id = 0;
    QPixmap icon;
};

QPixmap Rounded(const QPixmap& source, int size, int radius) {
    const qreal ratio = qApp->devicePixelRatio();
    QPixmap out(QSize(size, size) * ratio);
    out.setDevicePixelRatio(ratio);
    out.fill(Qt::transparent);
    {
        QPainter painter(&out);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(0, 0, size, size), radius, radius);
        painter.setClipPath(clip);
        if (source.isNull()) {
            painter.fillRect(QRect(0, 0, size, size), QColor(0x24, 0x23, 0x2c));
        } else {
            painter.drawPixmap(QRect(0, 0, size, size), source);
        }
    }
    return out;
}

/// True for a missing or fully transparent icon (some dumps have none).
bool IsBlank(const QPixmap& pixmap) {
    if (pixmap.isNull()) {
        return true;
    }
    const QImage image = pixmap.toImage();
    const int step_x = std::max(1, image.width() / 16);
    const int step_y = std::max(1, image.height() / 16);
    for (int y = 0; y < image.height(); y += step_y) {
        for (int x = 0; x < image.width(); x += step_x) {
            if (qAlpha(image.pixel(x, y)) != 0) {
                return false;
            }
        }
    }
    return true;
}

/// A cover the player put next to the game file: "<file name>.png" or "icon.png" (the same files
/// Game Settings > Detect Icon looks at first). Read only; the game file is never changed.
QPixmap LocalCover(const QString& game_path) {
    const QFileInfo info(game_path);
    const QDir dir = info.absoluteDir();
    for (const QString& base : {info.completeBaseName(), QStringLiteral("icon")}) {
        for (const char* ext : {".png", ".jpg", ".jpeg", ".webp"}) {
            const QFileInfo candidate(dir.filePath(base + QLatin1String(ext)));
            if (!candidate.isFile() || candidate.size() > 16 * 1024 * 1024) {
                continue;
            }
            QPixmap pixmap;
            if (pixmap.load(candidate.filePath())) {
                return pixmap;
            }
        }
    }
    return {};
}

std::vector<Game> CollectGames(QStandardItemModel* model, const QPixmap& fallback) {
    std::vector<Game> games;
    std::set<QString> seen;
    if (!model) {
        return games;
    }
    const auto add = [&](const QStandardItem* item) {
        if (item->data(GameListItem::TypeRole).toInt() !=
            static_cast<int>(GameListItemType::Game)) {
            return;
        }
        const QString path = item->data(GameListItemPath::FullPathRole).toString();
        if (path.isEmpty() || !seen.insert(path).second) {
            return;
        }
        Game game;
        game.path = path;
        game.program_id = item->data(GameListItemPath::ProgramIdRole).toULongLong();
        game.title = item->data(GameListItemPath::TitleRole).toString();
        if (game.title.isEmpty()) {
            game.title = QFileInfo(path).completeBaseName();
        }
        const QByteArray icon_data = item->data(GameListItemPath::IconDataRole).toByteArray();
        if (!game.icon.loadFromData(icon_data)) {
            game.icon = item->data(Qt::DecorationRole).value<QPixmap>();
        }
        if (IsBlank(game.icon)) {
            game.icon = LocalCover(path);
        }
        if (IsBlank(game.icon)) {
            // No icon in the dump and none next to it: Fritchii's own icon, not an empty square.
            game.icon = fallback;
        }
        games.push_back(std::move(game));
    };
    for (int row = 0; row < model->rowCount(); ++row) {
        const QStandardItem* top = model->item(row);
        if (!top) {
            continue;
        }
        if (top->hasChildren()) {
            for (int child = 0; child < top->rowCount(); ++child) {
                add(top->child(child));
            }
        } else {
            add(top);
        }
    }
    return games;
}

} // namespace

Home::Home(QMainWindow* window_, QBoxLayout* central, QWidget* game_list_, QWidget* placeholder_,
           HomeHooks hooks_)
    : QObject(window_), window{window_}, game_list{game_list_}, placeholder{placeholder_},
      hooks{std::move(hooks_)} {
    // Cervin: the Saves window's copy list shows each game's name and icon from the game list.
    SetSaveGameInfoLookup([model = hooks.model](u64 program_id) -> std::optional<SaveGameInfo> {
        for (const auto& game : CollectGames(model, QPixmap{})) {
            if (game.program_id == program_id) {
                return SaveGameInfo{game.title, game.icon};
            }
        }
        return std::nullopt;
    });

    // The classic menus live behind the rail's menu button. A zero-height bar keeps every
    // shortcut working and stays out of the way when fullscreen shows the bar again.
    window->menuBar()->setFixedHeight(0);

    rail = MakeRail();
    page = MakePage();
    central->insertWidget(0, rail);
    central->addWidget(page, 1);

    game_list->installEventFilter(this);
    placeholder->installEventFilter(this);

    refresh_timer = new QTimer(this);
    refresh_timer->setSingleShot(true);
    refresh_timer->setInterval(150);
    connect(refresh_timer, &QTimer::timeout, this, &Home::Refresh);
    if (hooks.model) {
        const auto later = [this] { refresh_timer->start(); };
        connect(hooks.model, &QAbstractItemModel::rowsInserted, this, later);
        connect(hooks.model, &QAbstractItemModel::rowsRemoved, this, later);
        connect(hooks.model, &QAbstractItemModel::modelReset, this, later);
        connect(hooks.model, &QAbstractItemModel::dataChanged, this, later);
    }
    QTimer::singleShot(0, this, &Home::Restore);
}

QToolButton* Home::RailButton(const QString& icon, const QString& tip) {
    auto* button = new QToolButton(rail);
    button->setObjectName(QStringLiteral("FritchiiRailButton"));
    button->setIcon(Icon(icon, 22));
    button->setIconSize(QSize(22, 22));
    button->setToolTip(tip);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    return button;
}

QWidget* Home::MakeRail() {
    rail = new QWidget;
    rail->setObjectName(QStringLiteral("FritchiiRail"));
    rail->setAttribute(Qt::WA_StyledBackground);
    rail->setFixedWidth(64);
    auto* layout = new QVBoxLayout(rail);
    layout->setContentsMargins(11, 16, 11, 16);
    layout->setSpacing(8);

    library_button = RailButton(QStringLiteral("library"), tr("Library"));
    library_button->setCheckable(true);
    connect(library_button, &QToolButton::clicked, this, &Home::ShowHome);

    QToolButton* settings = RailButton(QStringLiteral("settings"), tr("Settings"));
    connect(settings, &QToolButton::clicked, this, [this] { hooks.open_settings(); });

    QToolButton* help = RailButton(QStringLiteral("help"), tr("Help"));
    connect(help, &QToolButton::clicked, this, [this, help] {
        if (hooks.help_menu) {
            hooks.help_menu->exec(help->mapToGlobal(QPoint(help->width() + 8, 0)));
        }
    });

    auto* classic = new QMenu(rail);
    for (QMenu* menu : hooks.classic_menus) {
        classic->addMenu(menu);
    }
    QToolButton* menu_button = RailButton(QStringLiteral("menu"), tr("Menu"));
    connect(menu_button, &QToolButton::clicked, this, [classic, menu_button] {
        const QSize size = classic->sizeHint();
        classic->exec(menu_button->mapToGlobal(
            QPoint(menu_button->width() + 8, menu_button->height() - size.height())));
    });

    // Discord and Donate are on the home page itself (MakePage), with their names.
    layout->addWidget(library_button);
    layout->addWidget(settings);
    layout->addWidget(help);
    layout->addStretch(1);
    layout->addWidget(menu_button);
    return rail;
}

QWidget* Home::MakePage() {
    page = new QWidget;
    page->setObjectName(QStringLiteral("FritchiiHome"));
    page->setAttribute(Qt::WA_StyledBackground);
    page->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(page, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        if (hero_path.isEmpty()) {
            return;
        }
        // The game list's full menu (modders use its folders and dumps), then Delete Shader Cache.
        QMenu menu;
        if (hooks.game_menu) {
            hooks.game_menu(menu, hero_id, hero_path.toStdString());
        } else {
            menu.addAction(tr("Game Settings..."), &menu, [this] {
                hooks.game_settings(hero_id, hero_path.toStdString());
            });
        }
        menu.addSeparator();
        menu.addAction(tr("Delete Shader Cache..."), &menu,
                       [this] { hooks.delete_shader_cache(hero_id); });
        menu.exec(page->mapToGlobal(pos));
    });

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(56, 24, 56, 36);
    layout->setSpacing(0);

    // Discord and Donate in plain sight at the top right, with their names (2026-11-10: the
    // rail's bare icons were easy to miss).
    auto* links = new QHBoxLayout;
    links->setSpacing(10);
    links->addStretch(1);
    // v5.8.3: the Saves window (export, import, copy between Fritchii, Eden and Lichii).
    auto* saves = new QPushButton(Icon(QStringLiteral("list"), 18), tr("Saves"), page);
    saves->setObjectName(QStringLiteral("FritchiiLink"));
    saves->setIconSize(QSize(18, 18));
    saves->setCursor(Qt::PointingHandCursor);
    saves->setFocusPolicy(Qt::NoFocus);
    connect(saves, &QPushButton::clicked, this, [this] {
        ShowSavesWindow(window, hooks.emulation_running);
    });
    links->addWidget(saves);
    // Cervin: no Discord or donation buttons here. Fritchii's pointed to its own Discord server and
    // to Frizti's donation page, which are not Cervin's.
    layout->addLayout(links);
    layout->addSpacing(28);

    // Cervin: the library as a grid of cards, the selected game's quick settings beside it.
    auto* heading = new QLabel(tr("Library"), page);
    heading->setObjectName(QStringLiteral("FritchiiHeroTitle"));
    layout->addWidget(heading);
    layout->addSpacing(18);

    auto* body = new QHBoxLayout;
    body->setSpacing(28);

    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    grid_box = new QWidget;
    grid_box->setObjectName(QStringLiteral("CervinLibraryGrid"));
    grid = new QGridLayout(grid_box);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(18);
    grid->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    scroll->setWidget(grid_box);
    // The number of columns follows the width (eventFilter).
    scroll->viewport()->installEventFilter(this);
    body->addWidget(scroll, 1);

    panel = new QFrame(page);
    panel->setObjectName(QStringLiteral("CervinQuickPanel"));
    panel->setFixedWidth(PanelWidth);
    panel->setFrameShape(QFrame::StyledPanel);
    panel_layout = new QVBoxLayout(panel);
    panel_layout->setContentsMargins(18, 18, 18, 18);
    panel_layout->setSpacing(10);
    body->addWidget(panel, 0, Qt::AlignTop);

    layout->addLayout(body, 1);

    page->setStyleSheet(QStringLiteral(
        "#CervinGameCard { border: 1px solid palette(mid); border-radius: 14px; }"
        "#CervinGameCard[selected=\"true\"] { border: 2px solid palette(highlight); }"
        "#CervinQuickPanel { border-radius: 14px; }"
        "#CervinBadge { color: #F2B45A; font-weight: 600; }"));
    return page;
}

void Home::Refresh() {
    const std::vector<Game> games = CollectGames(hooks.model, window->windowIcon().pixmap(256));

    // The selected game: the one picked last, else Ocarina of Time, else the first game.
    const Game* hero = nullptr;
    for (const Game& game : games) {
        if (!hero_path.isEmpty() && game.path == hero_path) {
            hero = &game;
            break;
        }
    }
    if (!hero) {
        for (const Game& game : games) {
            if (game.program_id == OcarinaOfTime) {
                hero = &game;
                break;
            }
        }
    }
    if (!hero && !games.empty()) {
        hero = &games.front();
    }
    hero_path = hero ? hero->path : QString{};
    hero_title = hero ? hero->title : QString{};
    hero_id = hero ? hero->program_id : 0;

    // Mario Kart World shows a badge while its recommended settings are not chosen.
    bool mkw_recommended = true;
    if (hooks.with_game_settings && !hooks.emulation_running()) {
        hooks.with_game_settings(Settings::MarioKartWorldProgramId, [&mkw_recommended] {
            mkw_recommended = Settings::values.mkw_recommended.GetValue();
            return false;
        });
    }

    // deleteLater: a card's own click can land here.
    while (QLayoutItem* item = grid->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    const QWidget* const viewport = grid_box->parentWidget();
    const int available = std::max(CardWidth, viewport != nullptr ? viewport->width() : 0);
    const int columns = std::max(1, (available + 18) / (CardWidth + 18));

    int index = 0;
    const auto place = [&](QWidget* widget) {
        grid->addWidget(widget, index / columns, index % columns);
        ++index;
    };
    for (const Game& game : games) {
        auto* card = new QFrame(grid_box);
        card->setObjectName(QStringLiteral("CervinGameCard"));
        card->setProperty("selected", game.path == hero_path);
        card->setFixedWidth(CardWidth);
        auto* card_layout = new QVBoxLayout(card);
        card_layout->setContentsMargins(12, 12, 12, 12);
        card_layout->setSpacing(8);

        // The cover selects the game (its quick settings show beside the grid).
        auto* cover_button = new QToolButton(card);
        cover_button->setIcon(QIcon(Rounded(game.icon, CardCover, 14)));
        cover_button->setIconSize(QSize(CardCover, CardCover));
        cover_button->setAutoRaise(true);
        cover_button->setCursor(Qt::PointingHandCursor);
        cover_button->setToolTip(tr("Show its quick settings"));
        connect(cover_button, &QToolButton::clicked, this,
                [this, path = game.path, id = game.program_id] {
                    hero_path = path;
                    hero_id = id;
                    Refresh();
                });
        card_layout->addWidget(cover_button, 0, Qt::AlignHCenter);

        auto* name = new QLabel(card);
        const QFontMetrics metrics(name->font());
        name->setText(metrics.elidedText(game.title, Qt::ElideRight, CardWidth - 24));
        name->setToolTip(game.title);
        QFont name_font = name->font();
        name_font.setBold(true);
        name->setFont(name_font);
        card_layout->addWidget(name);

        if (game.program_id == Settings::MarioKartWorldProgramId && !mkw_recommended) {
            auto* badge = new QLabel(tr("Recommended settings available"), card);
            badge->setObjectName(QStringLiteral("CervinBadge"));
            badge->setWordWrap(true);
            card_layout->addWidget(badge);
        }

        auto* buttons = new QHBoxLayout;
        buttons->setSpacing(8);
        auto* play_button = new QPushButton(Glyph(QStringLiteral("play"), 0x1c1433, 16), tr("Play"),
                                            card);
        play_button->setObjectName(QStringLiteral("FritchiiPlay"));
        play_button->setCursor(Qt::PointingHandCursor);
        connect(play_button, &QPushButton::clicked, this,
                [this, path = game.path, id = game.program_id] { hooks.play(path, id); });
        auto* settings_button = new QToolButton(card);
        settings_button->setIcon(Icon(QStringLiteral("settings"), 18));
        settings_button->setToolTip(tr("Game settings"));
        settings_button->setCursor(Qt::PointingHandCursor);
        connect(settings_button, &QToolButton::clicked, this,
                [this, path = game.path, id = game.program_id] {
                    hooks.game_settings(id, path.toStdString());
                    Refresh();
                });
        buttons->addWidget(play_button, 1);
        buttons->addWidget(settings_button);
        card_layout->addLayout(buttons);
        place(card);
    }

    // "All games" (the classic list) and "Add folder" close the grid.
    const auto tile = [&](const QString& icon, const QString& text) {
        auto* button = new QToolButton(grid_box);
        button->setObjectName(QStringLiteral("FritchiiTile"));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIcon(Icon(icon, 40));
        button->setIconSize(QSize(40, 40));
        button->setText(text);
        button->setFixedWidth(CardWidth);
        button->setMinimumHeight(CardCover / 2);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        place(button);
        return button;
    };
    if (!games.empty()) {
        connect(tile(QStringLiteral("list"), tr("All games")), &QToolButton::clicked, this,
                &Home::ShowList);
    }
    connect(tile(QStringLiteral("plus"), tr("Add folder")), &QToolButton::clicked, this,
            [this] { hooks.add_folder(); });

    RefreshPanel();
}

void Home::RefreshPanel() {
    while (QLayoutItem* item = panel_layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    panel->setVisible(hero_id != 0);
    if (hero_id == 0) {
        return;
    }

    auto* kicker = new QLabel(tr("Quick settings"), panel);
    kicker->setForegroundRole(QPalette::PlaceholderText);
    panel_layout->addWidget(kicker);
    auto* name = new QLabel(hero_title, panel);
    name->setWordWrap(true);
    QFont name_font = name->font();
    name_font.setPointSizeF(name_font.pointSizeF() * 1.3);
    name_font.setBold(true);
    name->setFont(name_font);
    panel_layout->addWidget(name);

    const bool running = hooks.emulation_running();
    auto* note = new QLabel(running ? tr("Close the game to change them here.")
                                    : tr("Only this game. \"As in Settings\" follows your main "
                                         "settings."),
                            panel);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    panel_layout->addWidget(note);

    using Settings::values;
    std::vector<QuickChoice> choices;
    choices.push_back(MakeChoice(
        tr("DLSS priority"),
        {{tr("Picture (base at 2/3)"), static_cast<u32>(Settings::NvidiaDlssQuality::Quality)},
         {tr("Balanced (base at 58%)"), static_cast<u32>(Settings::NvidiaDlssQuality::Balanced)},
         {tr("FPS (base at half)"), static_cast<u32>(Settings::NvidiaDlssQuality::Performance)},
         {tr("Max FPS (base at a third)"),
          static_cast<u32>(Settings::NvidiaDlssQuality::UltraPerformance)},
         {tr("Edges only (DLAA): no FPS gain"),
          static_cast<u32>(Settings::NvidiaDlssQuality::Native)},
         {tr("Auto (from the game's size)"),
          static_cast<u32>(Settings::NvidiaDlssQuality::Automatic)}},
        values.nvidia_dlss_quality));
    choices.push_back(MakeChoice(
        tr("DLSS model"),
        {{tr("Automatic (recommended)"), static_cast<u32>(Settings::DlssPreset::Auto)},
         {tr("K: the all-rounder"), static_cast<u32>(Settings::DlssPreset::K)},
         {tr("M: for a small base (FPS)"), static_cast<u32>(Settings::DlssPreset::M)},
         {tr("L: 4K from a very small base"), static_cast<u32>(Settings::DlssPreset::L)},
         {tr("J: a bit sharper than K"), static_cast<u32>(Settings::DlssPreset::J)}},
        values.nvidia_dlss_preset));
    choices.push_back(MakeChoice(
        tr("Console mode"),
        {{tr("TV (sharper)"), static_cast<u32>(Settings::ConsoleMode::Docked)},
         {tr("Handheld (smaller picture, more FPS)"),
          static_cast<u32>(Settings::ConsoleMode::Handheld)}},
        values.use_docked_mode));
    choices.push_back(MakeChoice(tr("Speed limit"),
                                 {{tr("Normal speed"), 1U}, {tr("No limit (fast forward)"), 0U}},
                                 values.use_speed_limit));

    // The game's own values: read once with its settings loaded.
    std::vector<std::optional<u32>> current(choices.size());
    if (hooks.with_game_settings && !running) {
        hooks.with_game_settings(hero_id, [&] {
            for (size_t i = 0; i < choices.size(); ++i) {
                current[i] = choices[i].read();
            }
            return false;
        });
    }

    for (size_t i = 0; i < choices.size(); ++i) {
        auto* label = new QLabel(choices[i].label, panel);
        QFont label_font = label->font();
        label_font.setBold(true);
        label->setFont(label_font);
        panel_layout->addWidget(label);
        auto* combo = new QComboBox(panel);
        combo->addItem(tr("As in Settings"));
        for (const auto& [text, value] : choices[i].items) {
            combo->addItem(text, value);
        }
        if (current[i]) {
            const int row = combo->findData(*current[i]);
            combo->setCurrentIndex(row >= 0 ? row : 0);
        }
        combo->setEnabled(!running && static_cast<bool>(hooks.with_game_settings));
        connect(combo, QOverload<int>::of(&QComboBox::activated), this,
                [this, combo, write = choices[i].write](int row) {
                    const std::optional<u32> value =
                        row <= 0 ? std::nullopt
                                 : std::optional<u32>{combo->itemData(row).toUInt()};
                    hooks.with_game_settings(hero_id, [&] {
                        write(value);
                        return true;
                    });
                });
        panel_layout->addWidget(combo);
    }

    if (hero_id == Settings::MarioKartWorldProgramId) {
        auto* reco = new QPushButton(tr("Recommended settings for this game..."), panel);
        connect(reco, &QPushButton::clicked, this, [this] {
            hooks.game_settings(hero_id, hero_path.toStdString());
            Refresh();
        });
        panel_layout->addWidget(reco);
    }
    auto* all = new QPushButton(tr("All the game's settings"), panel);
    connect(all, &QPushButton::clicked, this, [this] {
        hooks.game_settings(hero_id, hero_path.toStdString());
        Refresh();
    });
    panel_layout->addSpacing(6);
    panel_layout->addWidget(all);
    panel_layout->addStretch(1);
}

void Home::ShowHome() {
    list_mode = false;
    game_list->hide();
    placeholder->hide();
    rail->show();
    page->show();
    library_button->setChecked(true);
    Refresh();
}

void Home::ShowList() {
    list_mode = true;
    page->hide();
    rail->show();
    library_button->setChecked(false);
    if (hooks.game_list_empty()) {
        placeholder->show();
    } else {
        game_list->show();
    }
}

void Home::Restore() {
    // A restart shows the list for a moment, and nothing may cover a running game.
    if (hooks.single_window() && hooks.emulation_running()) {
        return;
    }
    if (list_mode) {
        rail->show();
        page->hide();
    } else {
        ShowHome();
    }
}

void Home::OnEmulationStarting() {
    if (!hooks.single_window()) {
        return;
    }
    rail->hide();
    page->hide();
}

bool Home::eventFilter(QObject* watched, QEvent* event) {
    // The main window shows its game list (start up, a game stopped, the list filled): show the
    // home page instead, unless the player asked for the full list.
    if (event->type() == QEvent::Show && (watched == game_list || watched == placeholder)) {
        QTimer::singleShot(0, this, &Home::Restore);
    }
    // Cervin: the grid's width changed: lay the cards out again (once the resizing settles).
    if (event->type() == QEvent::Resize && grid_box != nullptr &&
        watched == grid_box->parentWidget() && refresh_timer != nullptr) {
        refresh_timer->start();
    }
    return QObject::eventFilter(watched, event);
}

} // namespace FritchiiUi
