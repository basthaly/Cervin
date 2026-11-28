// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <chrono>
#include <memory>
#include <QBoxLayout>
#include <QCoreApplication>
#include <QDir>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include "common/logging.h"
#include "common/settings.h"
#include "common/settings_enums.h"
#include "core/core.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/util/vk.h"
#include "ui_configure.h"
#include "yuzu/configuration/configure_applets.h"
#include "yuzu/configuration/configure_audio.h"
#include "yuzu/configuration/configure_cpu.h"
#include "yuzu/configuration/configure_debug_tab.h"
#include "yuzu/configuration/configure_dialog.h"
#include "yuzu/configuration/configure_filesystem.h"
#include "yuzu/configuration/configure_general.h"
#include "yuzu/configuration/configure_graphics.h"
#include "yuzu/configuration/configure_graphics_advanced.h"
#include "yuzu/configuration/configure_graphics_extensions.h"
#include "yuzu/configuration/configure_hotkeys.h"
#include "yuzu/configuration/configure_input.h"
#include "yuzu/configuration/configure_input_player.h"
#include "yuzu/configuration/configure_network.h"
#include "yuzu/configuration/configure_profile_manager.h"
#include "yuzu/configuration/configure_system.h"
#include "yuzu/configuration/configure_ui.h"
#include "yuzu/configuration/configure_web.h"
#include "yuzu/hotkeys.h"
#include "yuzu/fritchii/fritchii_ui.h"

namespace {
/// Cervin: how long each Settings tab takes to build (owner 2026-11-28: Settings open slowly).
template <typename Make>
auto TimedTab(const char* name, Make&& make) {
    const auto begin = std::chrono::steady_clock::now();
    auto tab = make();
    LOG_DEBUG(Frontend, "Settings: {} built in {} ms", name,
             std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now() - begin)
                 .count());
    return tab;
}
} // namespace

ConfigureDialog::ConfigureDialog(QWidget* parent, HotkeyRegistry& registry_,
                                 InputCommon::InputSubsystem* input_subsystem_,
                                 std::vector<VkDeviceInfo::Record>& vk_device_records_,
                                 Core::System& system_, bool enable_web_config_)
    : QDialog(parent), ui{std::make_unique<Ui::ConfigureDialog>()}, registry(registry_),
      system{system_},
      builder{std::make_unique<ConfigurationShared::Builder>(this, !system_.IsPoweredOn())},
      input_subsystem{input_subsystem_}, vk_device_records{vk_device_records_},
      enable_web_config{enable_web_config_} {
    Settings::SetConfiguringGlobal(true);
    const auto timing_begin = std::chrono::steady_clock::now();
    const auto since = [&timing_begin] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - timing_begin)
            .count();
    };

    ui->setupUi(this);

    // Cervin (owner 2026-11-28: Settings took 1.5 s to open): a tab is built when its group is
    // first shown, the others one by one once the page is up (BuildNextTab). Without the
    // Fritchii UI every tab is built here, as before.
    if (!FritchiiUi::Enabled()) {
        for (int id = 0; id < TabCount; ++id) {
            Tab(static_cast<TabId>(id));
        }
        ui->tabWidget->addTab(applets_tab.get(), tr("Applets"));
        ui->tabWidget->addTab(audio_tab.get(), tr("Audio"));
        ui->tabWidget->addTab(cpu_tab.get(), tr("CPU"));
        ui->tabWidget->addTab(debug_tab_tab.get(), tr("Debug"));
        ui->tabWidget->addTab(filesystem_tab.get(), tr("Filesystem"));
        ui->tabWidget->addTab(general_tab.get(), tr("General"));
        ui->tabWidget->addTab(graphics_tab.get(), tr("Graphics"));
        ui->tabWidget->addTab(graphics_advanced_tab.get(), tr("GraphicsAdvanced"));
        ui->tabWidget->addTab(graphics_extensions_tab.get(), tr("GraphicsExtra"));
        ui->tabWidget->addTab(hotkeys_tab.get(), tr("Hotkeys"));
        ui->tabWidget->addTab(input_tab.get(), tr("Controls"));
        ui->tabWidget->addTab(profile_tab.get(), tr("Profiles"));
        ui->tabWidget->addTab(network_tab.get(), tr("Network"));
        ui->tabWidget->addTab(system_tab.get(), tr("System"));
        ui->tabWidget->addTab(ui_tab.get(), tr("Game List"));
        ui->tabWidget->addTab(web_tab.get(), tr("Web"));
    }

    SetConfiguration();
    // The list's rows take their height from the style sheet (padding): polished first, or they
    // pile up on each other (they were sized before the style applied).
    ensurePolished();
    ui->selectorList->ensurePolished();
    PopulateSelectionList();

    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        if (index != -1 && debug_tab_tab) {
            debug_tab_tab->SetCurrentIndex(0);
        }
    });
    connect(ui->selectorList, &QListWidget::itemSelectionChanged, this,
            &ConfigureDialog::UpdateVisibleTabs);

    if (system.IsPoweredOn()) {
        QPushButton* apply_button = ui->buttonBox->addButton(QDialogButtonBox::Apply);
        connect(apply_button, &QAbstractButton::clicked, this,
                &ConfigureDialog::HandleApplyButtonClicked);
    }

    if (!FritchiiUi::Enabled()) {
        adjustSize(); // the Fritchii page takes the main window's size instead
    }
    ui->selectorList->setCurrentRow(0);
    LOG_INFO(Frontend, "Settings: first group shown at {} ms", since());
    if (FritchiiUi::Enabled()) {
        // After the first group is shown: its pages sit in scroll areas by then, so the window
        // can take a normal size.
        setWindowTitle(tr("Settings"));
        ui->label->setText(tr("Some settings can't change while a game runs."));
        ui->label->setVisible(system.IsPoweredOn());
        ui->selectorList->setFixedWidth(180);
        layout()->setSizeConstraint(QLayout::SetNoConstraint);
        setMinimumSize(640, 460);
        // Cervin: Settings open over the whole main window, at its size (maximized with it), not
        // as a small window in its middle (owner 2026-11-24: the cards need the room).
        resize(1080, 700);
        QTimer::singleShot(0, this, [this] {
            if (!isWindow()) {
                return; // a page of the main window (FritchiiUi::EmbedInMainWindow)
            }
            QWidget* const owner = parentWidget() != nullptr ? parentWidget()->window() : nullptr;
            if (owner != nullptr && owner->isMaximized()) {
                setWindowState(windowState() | Qt::WindowMaximized);
                return;
            }
            QScreen* const screen = owner != nullptr && owner->screen() != nullptr ? owner->screen()
                                                                                 : this->screen();
            if (screen == nullptr) {
                return;
            }
            const QRect area = screen->availableGeometry();
            QRect target = owner != nullptr ? owner->geometry() : area;
            target = target.intersected(area);
            if (target.width() < 640 || target.height() < 460) {
                target = QRect(area.topLeft(), QSize(area.width() * 9 / 10, area.height() * 88 / 100));
                target.moveCenter(area.center());
            }
            setGeometry(target);
        });

        // OoT's own values (Configure Game) beat these, the usual "my settings didn't apply".
        if (const QStringList overrides = FritchiiUi::OcarinaGameOverrides();
            !overrides.isEmpty()) {
            auto* const banner = new QWidget(this);
            auto* const row = new QHBoxLayout(banner);
            row->setContentsMargins(0, 0, 0, 8);
            auto* const text = new QLabel(
                tr("Ocarina of Time keeps its own value for %n of these settings, so changes "
                   "here don't reach it.",
                   nullptr, static_cast<int>(overrides.size())),
                banner);
            text->setWordWrap(true);
            text->setToolTip(overrides.join(QLatin1Char('\n')));
            auto* const use_global = new QPushButton(tr("Use these for OoT too"), banner);
            row->addWidget(text, 1);
            row->addWidget(use_global);
            if (auto* const main = qobject_cast<QBoxLayout*>(layout())) {
                main->insertWidget(0, banner);
            }
            connect(use_global, &QPushButton::clicked, this, [banner] {
                if (FritchiiUi::ClearOcarinaGameOverrides()) {
                    banner->hide();
                }
            });
        }
    }

    // Selects the leftmost button on the bottom bar (Cancel as of writing)
    ui->buttonBox->setFocus();
    if (FritchiiUi::Enabled()) {
        QTimer::singleShot(250, this, &ConfigureDialog::BuildNextTab);
        // Once the page sits in the main window (its style may change there): rows sized again.
        QTimer::singleShot(0, this, [this] { ui->selectorList->doItemsLayout(); });
    }
}

void ConfigureDialog::SaveScreenshots(const QString& folder) {
    QDir().mkpath(folder);
    grab().save(folder + QStringLiteral("/dialog.png"));
    for (int row = 0; row < ui->selectorList->count(); ++row) {
        ui->selectorList->setCurrentRow(row);
        QCoreApplication::processEvents();
        for (int index = 0; index < ui->tabWidget->count(); ++index) {
            ui->tabWidget->setCurrentIndex(index);
            QCoreApplication::processEvents();
            QWidget* page = ui->tabWidget->widget(index);
            if (auto* const area = qobject_cast<QScrollArea*>(page); area && area->widget()) {
                page = area->widget();
            }
            page->grab().save(
                QStringLiteral("%1/%2-%3.png").arg(folder).arg(row).arg(index));
        }
    }
    ui->selectorList->setCurrentRow(0);
}

QWidget* ConfigureDialog::Tab(TabId id) {
    const auto make = [this](auto& tab, const char* name, auto&& create) -> QWidget* {
        if (!tab) {
            Settings::SetConfiguringGlobal(true);
            tab = TimedTab(name, create);
        }
        return tab.get();
    };
    Core::System& system_ = system;
    switch (id) {
    case TabId::Applets:
        return make(applets_tab, "applets", [&] {
            return std::make_unique<ConfigureApplets>(system_, nullptr, *builder, this);
        });
    case TabId::Audio:
        return make(audio_tab, "audio", [&] {
            return std::make_unique<ConfigureAudio>(system_, nullptr, *builder, this);
        });
    case TabId::Cpu:
        return make(cpu_tab, "cpu", [&] {
            return std::make_unique<ConfigureCpu>(system_, nullptr, *builder, this);
        });
    case TabId::Debug:
        return make(debug_tab_tab, "debug",
                    [&] { return std::make_unique<ConfigureDebugTab>(system_, this); });
    case TabId::Filesystem:
        return make(filesystem_tab, "filesystem",
                    [&] { return std::make_unique<ConfigureFilesystem>(this); });
    case TabId::General:
        return make(general_tab, "general", [&] {
            auto tab = std::make_unique<ConfigureGeneral>(system_, nullptr, *builder, this);
            tab->SetResetCallback([this] { this->close(); });
            connect(tab.get(), &ConfigureGeneral::ExternalContentDirsChanged, this,
                    &ConfigureDialog::ExternalContentDirsChanged);
            return tab;
        });
    case TabId::GraphicsAdvanced:
        return make(graphics_advanced_tab, "graphics advanced", [&] {
            return std::make_unique<ConfigureGraphicsAdvanced>(system_, nullptr, *builder, this);
        });
    case TabId::GraphicsExtensions:
        return make(graphics_extensions_tab, "graphics extensions", [&] {
            return std::make_unique<ConfigureGraphicsExtensions>(system_, nullptr, *builder,
                                                                 this);
        });
    case TabId::Ui:
        return make(ui_tab, "interface", [&] {
            auto tab = std::make_unique<ConfigureUi>(system_, this);
            connect(tab.get(), &ConfigureUi::LanguageChanged, this,
                    &ConfigureDialog::OnLanguageChanged);
            if (FritchiiUi::Enabled()) {
                tab->setAccessibleName(tr("Interface"));
            }
            return tab;
        });
    case TabId::Graphics:
        // It tells these two about its changes, so they come first.
        Tab(TabId::GraphicsAdvanced);
        Tab(TabId::Ui);
        return make(graphics_tab, "graphics", [&] {
            return std::make_unique<ConfigureGraphics>(
                system_, vk_device_records,
                [this]() {
                    if (graphics_advanced_tab) {
                        graphics_advanced_tab->ExposeComputeOption();
                    }
                },
                [this](Settings::AspectRatio ratio, Settings::ResolutionSetup setup) {
                    if (ui_tab) {
                        ui_tab->UpdateScreenshotInfo(ratio, setup);
                    }
                },
                nullptr, *builder, this);
        });
    case TabId::Hotkeys:
        return make(hotkeys_tab, "hotkeys", [&] {
            auto tab = std::make_unique<ConfigureHotkeys>(system_.HIDCore(), this);
            tab->Populate(registry);
            if (FritchiiUi::Enabled()) {
                tab->setAccessibleName(tr("Shortcuts"));
            }
            return tab;
        });
    case TabId::Input:
        return make(input_tab, "controls", [&] {
            auto tab = std::make_unique<ConfigureInput>(system_, this);
            tab->Initialize(input_subsystem);
            return tab;
        });
    case TabId::Network:
        return make(network_tab, "network",
                    [&] { return std::make_unique<ConfigureNetwork>(system_, this); });
    case TabId::Profile:
        return make(profile_tab, "users", [&] {
            auto tab = std::make_unique<ConfigureProfileManager>(system_, this);
            if (FritchiiUi::Enabled()) {
                tab->setAccessibleName(tr("Users"));
            }
            return tab;
        });
    case TabId::System:
        return make(system_tab, "system", [&] {
            return std::make_unique<ConfigureSystem>(system_, nullptr, *builder, this);
        });
    case TabId::Web:
        return make(web_tab, "web", [&] {
            auto tab = std::make_unique<ConfigureWeb>(this);
            tab->SetWebServiceConfigEnabled(enable_web_config);
            return tab;
        });
    case TabId::TabCount:
        break;
    }
    return nullptr;
}

QList<QWidget*> ConfigureDialog::GroupTabs(int group) {
    QList<QWidget*> tabs;
    if (group < 0 || static_cast<size_t>(group) >= groups.size()) {
        return tabs;
    }
    for (const TabId id : groups[group]) {
        QWidget* const tab = Tab(id);
        if (id == TabId::Input) {
            tabs.append(input_tab->GetSubTabs());
        } else if (tab != nullptr) {
            tabs.append(tab);
        }
    }
    return tabs;
}

void ConfigureDialog::BuildNextTab() {
    // One tab per turn of the event loop, in the order of the list, so the page stays usable.
    for (const auto& group : groups) {
        for (const TabId id : group) {
            if (!IsBuilt(id)) {
                Tab(id);
                QTimer::singleShot(0, this, &ConfigureDialog::BuildNextTab);
                return;
            }
        }
    }
    LOG_INFO(Frontend, "Settings: every tab built");
}

bool ConfigureDialog::IsBuilt(TabId id) const {
    switch (id) {
    case TabId::Applets:
        return applets_tab != nullptr;
    case TabId::Audio:
        return audio_tab != nullptr;
    case TabId::Cpu:
        return cpu_tab != nullptr;
    case TabId::Debug:
        return debug_tab_tab != nullptr;
    case TabId::Filesystem:
        return filesystem_tab != nullptr;
    case TabId::General:
        return general_tab != nullptr;
    case TabId::GraphicsAdvanced:
        return graphics_advanced_tab != nullptr;
    case TabId::GraphicsExtensions:
        return graphics_extensions_tab != nullptr;
    case TabId::Ui:
        return ui_tab != nullptr;
    case TabId::Graphics:
        return graphics_tab != nullptr;
    case TabId::Hotkeys:
        return hotkeys_tab != nullptr;
    case TabId::Input:
        return input_tab != nullptr;
    case TabId::Network:
        return network_tab != nullptr;
    case TabId::Profile:
        return profile_tab != nullptr;
    case TabId::System:
        return system_tab != nullptr;
    case TabId::Web:
        return web_tab != nullptr;
    case TabId::TabCount:
        break;
    }
    return true;
}

ConfigureDialog::~ConfigureDialog() = default;

void ConfigureDialog::SetConfiguration() {}

void ConfigureDialog::ApplyConfiguration() {
    // A tab never built was never shown: its settings are untouched.
    if (general_tab) {
        general_tab->ApplyConfiguration();
    }
    if (ui_tab) {
        ui_tab->ApplyConfiguration();
    }
    if (system_tab) {
        system_tab->ApplyConfiguration();
    }
    if (profile_tab) {
        profile_tab->ApplyConfiguration();
    }
    if (filesystem_tab) {
        filesystem_tab->ApplyConfiguration();
    }
    if (input_tab) {
        input_tab->ApplyConfiguration();
    }
    if (hotkeys_tab) {
        hotkeys_tab->ApplyConfiguration(registry);
    }
    if (cpu_tab) {
        cpu_tab->ApplyConfiguration();
    }
    if (graphics_tab) {
        graphics_tab->ApplyConfiguration();
    }
    if (graphics_advanced_tab) {
        graphics_advanced_tab->ApplyConfiguration();
    }
    if (graphics_extensions_tab) {
        graphics_extensions_tab->ApplyConfiguration();
    }
    if (audio_tab) {
        audio_tab->ApplyConfiguration();
    }
    if (debug_tab_tab) {
        debug_tab_tab->ApplyConfiguration();
    }
    if (web_tab) {
        web_tab->ApplyConfiguration();
    }
    if (network_tab) {
        network_tab->ApplyConfiguration();
    }
    if (applets_tab) {
        applets_tab->ApplyConfiguration();
    }
    system.ApplySettings();
    Settings::LogSettings();
}

void ConfigureDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QDialog::changeEvent(event);
}

void ConfigureDialog::RetranslateUI() {
    const int old_row = ui->selectorList->currentRow();
    const int old_index = ui->tabWidget->currentIndex();

    ui->retranslateUi(this);

    PopulateSelectionList();
    ui->selectorList->setCurrentRow(old_row);

    UpdateVisibleTabs();
    ui->tabWidget->setCurrentIndex(old_index);
}

void ConfigureDialog::HandleApplyButtonClicked() {
    UISettings::values.configuration_applied = true;
    ApplyConfiguration();
}

void ConfigureDialog::PopulateSelectionList() {
    using enum TabId;
    std::vector<std::pair<QString, std::vector<TabId>>> items;
    if (FritchiiUi::Enabled()) {
        // Fritchii: everyday settings first, everything a player rarely needs under Advanced.
        items = {{tr("General"), {General, Ui, Hotkeys}},
                 {tr("Graphics"), {Graphics, GraphicsAdvanced, GraphicsExtensions}},
                 {tr("Controls"), {Input}},
                 {tr("Audio"), {Audio}},
                 {tr("System"), {System, Profile}},
                 {tr("Advanced"), {Cpu, Network, Filesystem, Applets, Web, Debug}}};
    } else {
        items = {{tr("General"), {General, Hotkeys, Ui, Web, Debug}},
                 {tr("System"), {System, Profile, Network, Filesystem, Applets}},
                 {tr("CPU"), {Cpu}},
                 {tr("Graphics"), {Graphics, GraphicsAdvanced, GraphicsExtensions}},
                 {tr("Audio"), {Audio}},
                 {tr("Controls"), {Input}}};
    }

    [[maybe_unused]] const QSignalBlocker blocker(ui->selectorList);

    ui->selectorList->clear();
    groups.clear();
    for (auto& entry : items) {
        auto* const item = new QListWidgetItem(entry.first);
        item->setData(Qt::UserRole, static_cast<int>(groups.size()));
        groups.push_back(std::move(entry.second));
        ui->selectorList->addItem(item);
    }
}

void ConfigureDialog::OnLanguageChanged(const QString& locale) {
    emit LanguageChanged(locale);
    //  Reloading the game list is needed to force retranslation.
    UISettings::values.is_game_list_reload_pending = true;
    // first apply the configuration, and then restore the display
    ApplyConfiguration();
    RetranslateUI();
    SetConfiguration();
}

void ConfigureDialog::UpdateVisibleTabs() {
    const auto items = ui->selectorList->selectedItems();
    if (items.isEmpty()) {
        return;
    }

    [[maybe_unused]] const QSignalBlocker blocker(ui->tabWidget);

    ui->tabWidget->clear();

    const auto tabs = GroupTabs(items[0]->data(Qt::UserRole).toInt());

    for (auto* const tab : tabs) {
        LOG_DEBUG(Frontend, "{}", tab->accessibleName().toStdString());
        ui->tabWidget->addTab(FritchiiUi::SettingsPage(tab), tab->accessibleName());
    }
}
