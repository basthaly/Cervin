// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <vector>

#include "common/common_types.h"

/// Cervin (owner 2026-11-28): which graphics pipelines a player meets, when, across sessions, so
/// the shader warm-up builds first what this player uses and, while they play, what came right
/// after the pipelines they just met (the next area, not one across the map). One file per game
/// beside its warm-up file: the last 12 sessions, each a list of (pipeline key hash, seconds
/// since the session started) in first-use order.
namespace Vulkan::PlayHistory {

/// Loads `file` (a missing or broken one starts empty) and starts the session clock.
void Begin(const std::filesystem::path& file);
/// A graphics pipeline key met for the first time this session (any thread). Queues the keys
/// that came within 90 s after it in earlier sessions (PopHot).
void NoteUsed(u64 hash);
/// Saves this session (when it met any pipeline) as the newest of the file.
void End();
/// How much this player used the key: sessions it appeared in, newer ones weighing more; 0 for
/// a key never met.
[[nodiscard]] double Score(u64 hash);
/// The next queued key to warm first, false when none.
[[nodiscard]] bool PopHot(u64& hash);
/// True when the history holds at least one session.
[[nodiscard]] bool HasHistory();

} // namespace Vulkan::PlayHistory
