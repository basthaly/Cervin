// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-License-Identifier: GPL-2.0-or-later

#include <vector>
#include <QBoxLayout>
#include <QLabel>
#include <qnamespace.h>
#include "common/settings.h"
#include "core/core.h"
#include "qt_common/config/shared_translation.h"
#include "ui_configure_graphics_advanced.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_graphics_advanced.h"
#include "yuzu/configuration/setting_cards.h"
#include "yuzu/configuration/shared_widget.h"
#include "yuzu/fritchii/fritchii_ui.h"
#include "yuzu/fritchii/occlusion_offer.h"

ConfigureGraphicsAdvanced::ConfigureGraphicsAdvanced(
    const Core::System& system_, std::shared_ptr<std::vector<ConfigurationShared::Tab*>> group_,
    const ConfigurationShared::Builder& builder, QWidget* parent)
    : Tab(group_, parent), ui{std::make_unique<Ui::ConfigureGraphicsAdvanced>()}, system{system_} {

    ui->setupUi(this);

    Setup(builder);

    SetConfiguration();

    checkbox_enable_compute_pipelines->setVisible(false);
}

ConfigureGraphicsAdvanced::~ConfigureGraphicsAdvanced() = default;

void ConfigureGraphicsAdvanced::SetConfiguration() {}

void ConfigureGraphicsAdvanced::Setup(const ConfigurationShared::Builder& builder) {
    auto& normal_layout = *ui->normal_target->layout();

    // A map will sort the data for us
    std::map<u32, QWidget*> normal_hold{};

    for (auto setting :
         Settings::values.linkage.by_category[Settings::Category::RendererAdvanced]) {
        // Cervin: Cervin's own bookkeeping, not settings for the player.
        if (setting->Id() == Settings::values.oot_fps_asked.Id() ||
            setting->Id() == Settings::values.frizti_settings_revision.Id()) {
            continue;
        }
        ConfigurationShared::Widget* widget = builder.BuildWidget(setting, apply_funcs);

        if (widget == nullptr) {
            continue;
        }
        if (!widget->Valid()) {
            widget->deleteLater();
            continue;
        }

        const auto id = setting->Id();

        normal_hold.emplace(id, widget);

        // Keep track of enable_compute_pipelines so we can display it when needed
        if (setting->Id() == Settings::values.enable_compute_pipelines.Id()) {
            checkbox_enable_compute_pipelines = widget;
        }
        // Fritchii: Full only where it can act.
        if (setting->Id() == Settings::values.occlusion_speedup.Id()) {
            FritchiiOcclusionOffer::LimitRow(widget);
        }
    }

    for (const auto& [id, widget] : normal_hold) {
        normal_layout.addWidget(widget);
    }
    // Cervin: every setting in a card (what it does, what it changes in game, its cost).
    if (FritchiiUi::Enabled()) {
        ui->verticalLayout_2->insertWidget(
            0, ConfigurationShared::MakeCardsHeader(
                   this, tr("Each setting says what it does and what you gain or lose in game.")));
        ConfigurationShared::MakeCards(qobject_cast<QBoxLayout*>(&normal_layout));
        // Cervin (owner 2026-11-28: too many settings at once): the cards in sections.
        ConfigurationShared::MakeSections(
            qobject_cast<QBoxLayout*>(&normal_layout),
            {{tr("Frame rate"), tr("FPS limits and how frames are paced."),
              {"frame_pacing_mode", "use_video_framerate", "oot_30fps", "oot_high_refresh",
               "oot_high_refresh_cap", "mkw_30fps", "mkw_dynamic_fps"}},
             {tr("Shaders and stutters"),
              tr("What is prepared ahead so the game doesn't stutter."),
              {"use_disk_shader_cache", "use_vulkan_driver_pipeline_cache",
               "use_graphics_pipeline_library", "shader_warm_threads",
               "enable_compute_pipelines"}},
             {tr("Speed"), tr("Shortcuts that give FPS for a little accuracy."),
              {"force_max_clock", "occlusion_speedup", "use_optimized_vertex_buffers",
               "use_reactive_flushing", "vram_usage_mode"}},
             {tr("Textures"), tr("Texture sharpness and how they are decoded."),
              {"max_anisotropy", "accelerate_astc", "astc_recompression"}},
             {tr("Accuracy and compatibility"),
              tr("Leave these as they are unless a game shows glitches."),
              {"gpu_accuracy", "dma_accuracy", "gpu_fence_behavior", "native_compute_write_mark",
               "sync_memory_operations", "barrier_feedback_loops", "enable_buffer_history",
               "nvdec_emulation"}}});
    }
}

void ConfigureGraphicsAdvanced::ApplyConfiguration() {
    const bool is_powered_on = system.IsPoweredOn();
    for (const auto& func : apply_funcs) {
        func(is_powered_on);
    }
}

void ConfigureGraphicsAdvanced::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureGraphicsAdvanced::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureGraphicsAdvanced::ExposeComputeOption() {
    checkbox_enable_compute_pipelines->setVisible(true);
}
