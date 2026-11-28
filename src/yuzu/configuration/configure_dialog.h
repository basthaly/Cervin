// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>
#include <QDialog>
#include "configuration/shared_widget.h"
#include "qt_common/config/shared_translation.h"
#include "qt_common/util/vk.h"
#include "yuzu/configuration/configuration_shared.h"

namespace Core {
class System;
}

class ConfigureApplets;
class ConfigureAudio;
class ConfigureCpu;
class ConfigureDebugTab;
class ConfigureFilesystem;
class ConfigureGeneral;
class ConfigureGraphics;
class ConfigureGraphicsAdvanced;
class ConfigureGraphicsExtensions;
class ConfigureHotkeys;
class ConfigureInput;
class ConfigureProfileManager;
class ConfigureSystem;
class ConfigureNetwork;
class ConfigureUi;
class ConfigureWeb;

class HotkeyRegistry;

namespace InputCommon {
class InputSubsystem;
}

namespace Ui {
class ConfigureDialog;
}

class ConfigureDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConfigureDialog(QWidget* parent, HotkeyRegistry& registry_,
                             InputCommon::InputSubsystem* input_subsystem_,
                             std::vector<VkDeviceInfo::Record>& vk_device_records_,
                             Core::System& system_, bool enable_web_config_ = true);
    ~ConfigureDialog() override;

    void ApplyConfiguration();

private slots:
    void OnLanguageChanged(const QString& locale);

signals:
    void LanguageChanged(const QString& locale);
    void ExternalContentDirsChanged();

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void HandleApplyButtonClicked();

    void SetConfiguration();
    void UpdateVisibleTabs();
    void PopulateSelectionList();

    enum class TabId {
        Applets,
        Audio,
        Cpu,
        Debug,
        Filesystem,
        General,
        GraphicsAdvanced,
        GraphicsExtensions,
        Ui,
        Graphics,
        Hotkeys,
        Input,
        Network,
        Profile,
        System,
        Web,
        TabCount
    };
    static constexpr int TabCount = static_cast<int>(TabId::TabCount);
    /// Builds the tab the first time (Cervin: tabs are built on demand) and returns it.
    QWidget* Tab(TabId id);
    [[nodiscard]] bool IsBuilt(TabId id) const;
    /// The pages of the list's group `group`, built if needed.
    QList<QWidget*> GroupTabs(int group);
    /// Builds the next tab not built yet, then comes back on the next turn of the event loop.
    void BuildNextTab();

    std::unique_ptr<Ui::ConfigureDialog> ui;
    HotkeyRegistry& registry;

    Core::System& system;
    std::unique_ptr<ConfigurationShared::Builder> builder;
    std::vector<ConfigurationShared::Tab*> tab_group;
    InputCommon::InputSubsystem* input_subsystem;
    std::vector<VkDeviceInfo::Record>& vk_device_records;
    bool enable_web_config;
    std::vector<std::vector<TabId>> groups; ///< the tabs of each row of the list

    std::unique_ptr<ConfigureApplets> applets_tab;
    std::unique_ptr<ConfigureAudio> audio_tab;
    std::unique_ptr<ConfigureCpu> cpu_tab;
    std::unique_ptr<ConfigureDebugTab> debug_tab_tab;
    std::unique_ptr<ConfigureFilesystem> filesystem_tab;
    std::unique_ptr<ConfigureGeneral> general_tab;
    std::unique_ptr<ConfigureGraphicsAdvanced> graphics_advanced_tab;
    std::unique_ptr<ConfigureGraphicsExtensions> graphics_extensions_tab;
    std::unique_ptr<ConfigureUi> ui_tab;
    std::unique_ptr<ConfigureGraphics> graphics_tab;
    std::unique_ptr<ConfigureHotkeys> hotkeys_tab;
    std::unique_ptr<ConfigureInput> input_tab;
    std::unique_ptr<ConfigureNetwork> network_tab;
    std::unique_ptr<ConfigureProfileManager> profile_tab;
    std::unique_ptr<ConfigureSystem> system_tab;
    std::unique_ptr<ConfigureWeb> web_tab;
};
