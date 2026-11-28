// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/renderer_vulkan/vk_play_history.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "common/logging.h"

namespace Vulkan::PlayHistory {

namespace {

constexpr std::array<char, 4> Magic{'C', 'V', 'P', 'H'};
constexpr u32 FileVersion = 1;
constexpr size_t KeptSessions = 12;
constexpr size_t MaxEntriesPerSession = 40000;
constexpr u32 NeighbourWindowSeconds = 90;
constexpr size_t MaxHotQueue = 20000;

struct Entry {
    u64 hash;
    u32 seconds;
};
using SessionList = std::vector<Entry>; // in first-use order, seconds rising

struct State {
    std::mutex mutex;
    std::filesystem::path file;
    std::vector<SessionList> sessions; // [0] newest
    /// hash -> (session index, position in it) of every earlier appearance.
    std::unordered_map<u64, std::vector<std::pair<u32, u32>>> where;
    std::unordered_map<u64, double> scores;
    SessionList current;
    std::unordered_set<u64> current_seen;
    std::unordered_set<u64> queued;
    std::deque<u64> hot;
    std::chrono::steady_clock::time_point begin{};
    bool active = false;
};

State& Get() {
    static State state;
    return state;
}

bool Load(const std::filesystem::path& file, std::vector<SessionList>& out) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        return false;
    }
    std::array<char, 4> magic{};
    u32 version{};
    u32 count{};
    in.read(magic.data(), magic.size());
    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!in || magic != Magic || version != FileVersion || count > KeptSessions) {
        return false;
    }
    for (u32 s = 0; s < count; ++s) {
        u32 size{};
        in.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (!in || size > MaxEntriesPerSession) {
            return false;
        }
        SessionList list(size);
        in.read(reinterpret_cast<char*>(list.data()),
                static_cast<std::streamsize>(size * sizeof(Entry)));
        if (!in) {
            return false;
        }
        out.push_back(std::move(list));
    }
    return true;
}

} // namespace

void Begin(const std::filesystem::path& file) {
    State& state = Get();
    std::scoped_lock lock{state.mutex};
    state.sessions.clear();
    state.where.clear();
    state.scores.clear();
    state.current.clear();
    state.current_seen.clear();
    state.queued.clear();
    state.hot.clear();
    state.file = file;
    if (!Load(file, state.sessions)) {
        state.sessions.clear();
    }
    for (u32 s = 0; s < state.sessions.size(); ++s) {
        const double weight = 1.0 / (1.0 + s); // the newest session weighs most
        const SessionList& list = state.sessions[s];
        for (u32 i = 0; i < list.size(); ++i) {
            state.where[list[i].hash].emplace_back(s, i);
            state.scores[list[i].hash] += weight;
        }
    }
    state.begin = std::chrono::steady_clock::now();
    state.active = true;
    LOG_INFO(Render_Vulkan, "Play history: {} earlier sessions, {} pipeline keys met ({})",
             state.sessions.size(), state.scores.size(), file.filename().string());
}

void NoteUsed(u64 hash) {
    State& state = Get();
    std::scoped_lock lock{state.mutex};
    if (!state.active || !state.current_seen.insert(hash).second) {
        return;
    }
    if (state.current.size() < MaxEntriesPerSession) {
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
                                 std::chrono::steady_clock::now() - state.begin)
                                 .count();
        state.current.push_back({hash, static_cast<u32>(seconds)});
    }
    // What came next in earlier sessions: the keys met within the window after this one.
    const auto found = state.where.find(hash);
    if (found == state.where.end()) {
        return;
    }
    for (const auto& [session, position] : found->second) {
        const SessionList& list = state.sessions[session];
        const u32 until = list[position].seconds + NeighbourWindowSeconds;
        for (size_t i = position + 1; i < list.size() && list[i].seconds <= until; ++i) {
            const u64 next = list[i].hash;
            if (state.hot.size() >= MaxHotQueue) {
                return;
            }
            if (!state.current_seen.contains(next) && state.queued.insert(next).second) {
                state.hot.push_back(next);
            }
        }
    }
}

void End() {
    State& state = Get();
    std::scoped_lock lock{state.mutex};
    if (!state.active) {
        return;
    }
    state.active = false;
    if (state.current.empty() || state.file.empty()) {
        return;
    }
    state.sessions.insert(state.sessions.begin(), std::move(state.current));
    if (state.sessions.size() > KeptSessions) {
        state.sessions.resize(KeptSessions);
    }
    const std::filesystem::path temp = state.file.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            return;
        }
        const u32 count = static_cast<u32>(state.sessions.size());
        out.write(Magic.data(), Magic.size());
        out.write(reinterpret_cast<const char*>(&FileVersion), sizeof(FileVersion));
        out.write(reinterpret_cast<const char*>(&count), sizeof(count));
        for (const SessionList& list : state.sessions) {
            const u32 size = static_cast<u32>(list.size());
            out.write(reinterpret_cast<const char*>(&size), sizeof(size));
            out.write(reinterpret_cast<const char*>(list.data()),
                      static_cast<std::streamsize>(size * sizeof(Entry)));
        }
        if (!out) {
            return;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, state.file, ec);
    LOG_INFO(Render_Vulkan, "Play history saved: {} pipeline keys met this session",
             state.sessions.front().size());
}

double Score(u64 hash) {
    State& state = Get();
    std::scoped_lock lock{state.mutex};
    const auto found = state.scores.find(hash);
    return found == state.scores.end() ? 0.0 : found->second;
}

bool PopHot(u64& hash) {
    State& state = Get();
    std::scoped_lock lock{state.mutex};
    if (state.hot.empty()) {
        return false;
    }
    hash = state.hot.front();
    state.hot.pop_front();
    return true;
}

bool HasHistory() {
    State& state = Get();
    std::scoped_lock lock{state.mutex};
    return !state.sessions.empty();
}

} // namespace Vulkan::PlayHistory
