#include "highlight.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <system/memory/memory.h>
#include <game/game/game.h>
#include <game/cache/core/cache.h>
#include <game/offsets/offsets.h>
#include <game/sdk.h>
#include <hacks/system/settings/settings.h>
#include <hacks/system/playerlist/playerlist.h>
#include <hacks/luau/instance.new/creation/instance_create.h>
#include <hacks/luau/instance.new/callgate/call_gate.h>
#include <hacks/visuals/chams/memory/chams_main.h>
#include <system/logger/logger.h>

namespace highlightchams
{
    namespace
    {

        constexpr std::int32_t k_depth_always_on_top = 0;
        constexpr std::int32_t k_depth_occluded = 1;

        struct Style
        {
            std::array<float, 3> fill{};
            std::array<float, 3> line{};
            float fill_t = 0.f;
            float line_t = 1.f;
            std::int32_t depth = k_depth_occluded;

            bool operator==(const Style&) const = default;
        };

        struct Entity
        {
            std::uint64_t inst = 0;
            std::uint64_t character = 0;
            std::uint64_t player = 0;
            int category = 0;
            Style style{};
        };

        inline bool addr_ok(std::uint64_t a)
        {
            return a >= 0x10000 && a < 0x7FFFFFFFFFFFull;
        }

        inline float clamp01(float v)
        {
            return std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
        }

        bool alive(std::uint64_t inst)
        {
            if (!addr_ok(inst) || !memory || !memory->is_attached())
                return false;
            return memory->read<std::uint64_t>(inst) != 0;
        }

        const char* StatusToStr(Status s)
        {
            switch (s)
            {
            case Status::Disabled: return "disabled";
            case Status::WaitingForGame: return "waiting for game";
            case Status::WaitingForCharacters: return "waiting for characters";
            case Status::NoAvailableIds: return "could not create highlight";
            case Status::MemoryUnavailable: return "scene memory unavailable";
            case Status::Active: return "active";
            }
            return "unknown";
        }

        Style style_for(const settings::visuals::priority_settings_t& cfg)
        {
            Style s{};
            for (std::size_t i = 0; i < 3; ++i)
            {
                s.fill[i] = clamp01(cfg.chams_colour[i]);
                s.line[i] = settings::visuals::highlight_outline
                    ? clamp01(cfg.chams_outline_colour[i])
                    : s.fill[i];
            }
            s.fill_t = clamp01(1.0f - cfg.chams_colour[3]);
            s.line_t = settings::visuals::highlight_outline
                ? clamp01(1.0f - cfg.chams_outline_colour[3])
                : 1.0f;
            s.depth = settings::visuals::highlight_through_walls
                ? k_depth_always_on_top
                : k_depth_occluded;
            return s;
        }

        bool write_style(std::uint64_t inst, const Style& s)
        {
            if (!addr_ok(inst) || !memory)
                return false;
            std::uint8_t enabled = 1;
            bool ok = true;
            ok &= memory->write_raw(inst + Offsets::Highlight::FillColor, s.fill.data(), sizeof(float) * 3);
            ok &= memory->write_raw(inst + Offsets::Highlight::OutlineColor, s.line.data(), sizeof(float) * 3);
            ok &= memory->write_raw(inst + Offsets::Highlight::FillTransparency, &s.fill_t, sizeof(float));
            ok &= memory->write_raw(inst + Offsets::Highlight::OutlineTransparency, &s.line_t, sizeof(float));
            ok &= memory->write_raw(inst + Offsets::Highlight::DepthMode, &s.depth, sizeof(std::int32_t));
            ok &= memory->write_raw(inst + Offsets::Highlight::Enabled, &enabled, sizeof(enabled));
            return ok;
        }

        class HighlightEngine
        {
        public:
            Status status = Status::Disabled;
            Status prev = Status::Disabled;
            std::size_t player_count = 0;
            std::size_t fail_count = 0;
            std::unordered_map<std::uint64_t, Entity> live;
            std::unordered_map<std::uint64_t, std::chrono::steady_clock::time_point> retry;
            std::mutex mtx;
            std::chrono::steady_clock::time_point last_diag{};
            std::chrono::steady_clock::time_point last_apply{};

            void SetStatus(Status s)
            {
                if (s == status)
                    return;
                Cheat::Console::Log(Cheat::Console::Color::Cyan,
                    "[HighlightChams] State: %s -> %s", StatusToStr(status), StatusToStr(s));
                prev = status;
                status = s;
            }

            void Drop(Entity& e)
            {
                if (addr_ok(e.inst) && memory && memory->is_attached())
                    InstanceCreate::SetParent(e.inst, 0);
                e.inst = 0;
            }

            void Stop()
            {
                for (auto& [key, e] : live)
                    Drop(e);
                live.clear();
                retry.clear();
                player_count = 0;
                SetStatus(Status::Disabled);
            }

            bool Spawn(std::uint64_t character, const Style& st, std::uint64_t& out)
            {
                out = 0;
                if (!addr_ok(character))
                    return false;

                const std::uint64_t base = (memory && memory->is_attached()) ? memory->get_base() : 0;
                const std::uint64_t creator = base ? base + Offsets::Highlight::Creator : 0;
                if (!creator)
                    return false;

                std::uint64_t inst = 0;
                if (!InstanceCreate::NewFromCreator(creator, character, &inst) || !addr_ok(inst))
                    return false;

                write_style(inst, st);

                out = inst;
                return true;
            }

            bool Refresh(std::uint64_t inst, std::uint64_t parent, const Style& st)
            {
                if (!alive(inst) || !parent)
                    return false;
                write_style(inst, st);

                InstanceCreate::SetParent(inst, 0);
                return InstanceCreate::SetParent(inst, parent);
            }

            void Tick()
            {
                std::lock_guard<std::mutex> lock(mtx);

                const bool want[3] = {
                    settings::visuals::hostile.chams && settings::visuals::hostile.chams_type == 4,
                    settings::visuals::friendly.chams && settings::visuals::friendly.chams_type == 4,
                    settings::visuals::neutral.chams && settings::visuals::neutral.chams_type == 4,
                };

                if (!want[0] && !want[1] && !want[2])
                {
                    if (status != Status::Disabled)
                        Stop();
                    return;
                }

                if (!memory || !memory->is_attached())
                {
                    Stop();
                    SetStatus(Status::WaitingForGame);
                    return;
                }

                std::vector<cache::entity_t> fallback;
                auto snap = cache::get_players_snap();
                if (!snap || snap->empty())
                {
                    std::lock_guard<std::mutex> cache_lock(cache::mtx);
                    fallback = cache::players;
                }
                const auto* players = (snap && !snap->empty()) ? snap.get() : &fallback;
                if (!players || players->empty())
                {
                    Stop();
                    SetStatus(Status::WaitingForCharacters);
                    return;
                }

                auto local = cache::get_local_snap();
                const std::uint64_t local_addr = local ? local->instance.address : 0;
                const std::uint64_t local_team = local ? local->team : 0;

                const settings::visuals::priority_settings_t* cfgs[3] = {
                    &settings::visuals::hostile,
                    &settings::visuals::friendly,
                    &settings::visuals::neutral,
                };

                std::unordered_set<std::uint64_t> seen;
                std::size_t fails = 0;
                const auto now = std::chrono::steady_clock::now();
                const bool may_apply = last_apply.time_since_epoch().count() == 0 ||
                    now - last_apply >= std::chrono::milliseconds(150);

                auto process = [&](const cache::entity_t& e, int category)
                {
                    if (category < 0 || category > 2 || !want[category])
                        return;
                    if (!e.instance.address)
                        return;
                    if (e.health <= 0.f && e.max_health > 0.f)
                        return;

                    const auto& cfg = *cfgs[category];
                    if (!cfg.chams || cfg.chams_type != 4)
                        return;

                    const std::uint64_t key = e.instance.address;
                    seen.insert(key);

                    const std::uint64_t character = nazichams::character_address(e);
                    if (!character)
                        return;

                    const Style st = style_for(cfg);

                    auto it = live.find(key);
                    if (it != live.end())
                    {
                        Entity& rec = it->second;
                        if (rec.character == character && alive(rec.inst))
                        {
                            if (may_apply && !(rec.style == st))
                            {
                                if (Refresh(rec.inst, rec.character, st))
                                    rec.style = st;
                                else
                                {
                                    Drop(rec);
                                    live.erase(it);
                                    ++fails;
                                }
                            }
                            return;
                        }
                        Drop(rec);
                        live.erase(it);
                    }

                    auto wait = retry.find(key);
                    if (wait != retry.end() && now < wait->second)
                        return;

                    std::uint64_t inst = 0;
                    if (!Spawn(character, st, inst))
                    {
                        retry[key] = now + std::chrono::milliseconds(1500);
                        ++fails;
                        return;
                    }
                    retry.erase(key);

                    Entity rec{};
                    rec.inst = inst;
                    rec.character = character;
                    rec.player = key;
                    rec.category = category;
                    rec.style = st;
                    live[key] = rec;
                };

                for (const auto& e : *players)
                {
                    int category = 0;
                    const bool is_local = local_addr != 0 && e.instance.address == local_addr;
                    if (is_local)
                    {
                        category = 2;
                    }
                    else
                    {
                        bool friendly = false;
                        if (e.priority == cache::player_priority::friendly)
                            friendly = true;
                        else if (e.priority == cache::player_priority::hostile)
                            friendly = false;
                        else if (local_team != 0 && e.team != 0 && e.team == local_team)
                        {
                            if (game::datamodel && !game::datamodel->is_mm2() && !game::datamodel->is_arsenal())
                                friendly = true;
                        }
                        else if (!e.name.empty() && playerlist::get_priority(e.name) == cache::player_priority::friendly)
                            friendly = true;
                        category = friendly ? 1 : 0;
                    }
                    process(e, category);
                }

                if (want[2] && local && local->instance.address)
                    process(*local, 2);

                for (auto it = live.begin(); it != live.end();)
                {
                    if (seen.find(it->first) == seen.end())
                    {
                        Drop(it->second);
                        retry.erase(it->first);
                        it = live.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }

                player_count = live.size();
                fail_count = fails;
                if (may_apply)
                    last_apply = now;

                SetStatus(live.empty() ? Status::WaitingForCharacters : Status::Active);

                if (last_diag.time_since_epoch().count() == 0 || now - last_diag >= std::chrono::seconds(4))
                {
                    last_diag = now;
                    Cheat::Console::Log(Cheat::Console::Color::White,
                        "[HighlightChams] %s: %zu highlight(s), %zu scanned, %zu failed (create fail %d)",
                        StatusToStr(status), live.size(), players->size(), fails,
                        InstanceCreate::LastFail());
                }
            }
        };

        HighlightEngine g_engine{};
    }

    void Tick()
    {
        g_engine.Tick();
    }

    void Run()
    {
        for (;;)
        {
            try
            {
                Tick();
            }
            catch (...) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    }

    void Stop()
    {
        std::lock_guard<std::mutex> lock(g_engine.mtx);
        g_engine.Stop();
    }

    bool Active()
    {
        return g_engine.status == Status::Active;
    }

    Status GetStatus()
    {
        return g_engine.status;
    }

    std::size_t GetEntityCount()
    {
        std::lock_guard<std::mutex> lock(g_engine.mtx);
        return g_engine.live.size();
    }

    std::size_t GetPlayerCount()
    {
        std::lock_guard<std::mutex> lock(g_engine.mtx);
        return g_engine.player_count;
    }

    const char* StatusString()
    {
        return StatusToStr(g_engine.status);
    }
}
