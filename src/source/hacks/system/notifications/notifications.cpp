#include "notifications.h"
#include <hacks/system/settings/settings.h>
#include <menu/menu/settings/functions.h>
#include <menu/cortex/cortex_ui.h>
#include <vector>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace notifications
{
    struct notification_item_t
    {
        std::string text;
        std::chrono::high_resolution_clock::time_point spawn_time;
        float total_duration = 4.0f;
        float current_y_offset = -100.0f;
    };

    std::vector<notification_item_t> g_notifications;
    std::mutex g_notify_mutex;

    void add(const std::string& text, float duration)
    {
        if (!settings::notifications::enabled)
            return;

        cx::Toast(text.c_str());

        std::lock_guard<std::mutex> lock(g_notify_mutex);
        notification_item_t item;
        item.text = text;
        item.spawn_time = std::chrono::high_resolution_clock::now();
        item.total_duration = (duration > 0.1f) ? duration : settings::notifications::duration;
        item.current_y_offset = -100.0f;
        g_notifications.push_back(item);

        if (g_notifications.size() > 8)
        {
            g_notifications.erase(g_notifications.begin());
        }
    }

    void add_welcome()
    {
        if (!settings::notifications::enabled)
            return;

        if (settings::notifications::welcome)
        {
            add("Welcome to Cortex! ^-^", 5.0f);
            add("Thank you for Buying Cortex have fun!", 5.5f);
        }
    }

    void add_hit(const std::string& player_name, float damage)
    {
        if (!settings::notifications::enabled || !settings::notifications::hit)
            return;

        char buf[128];
        snprintf(buf, sizeof(buf), "Hit %s for %.0f dmg", player_name.c_str(), damage);
        add(buf, 3.5f);
    }

    void add_kill(const std::string& player_name, const std::string& weapon)
    {
        if (!settings::notifications::enabled || !settings::notifications::kill)
            return;

        char buf[128];
        if (!weapon.empty())
            snprintf(buf, sizeof(buf), "Killed %s with %s", player_name.c_str(), weapon.c_str());
        else
            snprintf(buf, sizeof(buf), "Killed %s", player_name.c_str());
        add(buf, 4.0f);
    }

    void render()
    {
        if (!settings::notifications::enabled)
            return;

        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        if (!dl) return;

        ImGuiIO& io = ImGui::GetIO();
        auto now = std::chrono::high_resolution_clock::now();

        std::lock_guard<std::mutex> lock(g_notify_mutex);
        if (g_notifications.empty())
            return;

        for (auto it = g_notifications.begin(); it != g_notifications.end();)
        {
            float elapsed = std::chrono::duration<float>(now - it->spawn_time).count();
            if (elapsed >= it->total_duration)
            {
                it = g_notifications.erase(it);
            }
            else
            {
                ++it;
            }
        }

        if (g_notifications.empty())
            return;

        float target_y = 0.0f;
        const float pad_x = 12.0f;
        const float pad_y = 6.0f;
        const float spacing = 6.0f;

        for (size_t i = 0; i < g_notifications.size(); ++i)
        {
            auto& item = g_notifications[i];
            float elapsed = std::chrono::duration<float>(now - item.spawn_time).count();

            float alpha = 1.0f;
            float fade_in_dur = 0.20f;
            float fade_out_dur = 0.40f;

            if (elapsed < fade_in_dur)
            {
                alpha = elapsed / fade_in_dur;
            }
            else if (elapsed > (item.total_duration - fade_out_dur))
            {
                alpha = (item.total_duration - elapsed) / fade_out_dur;
            }
            alpha = std::clamp(alpha, 0.0f, 1.0f);

            const char* tag = "^_^";
            std::string tag_prefix = tag;
            std::string body_text = " | " + item.text;

            ImVec2 tag_size = ImGui::CalcTextSize(tag_prefix.c_str());
            ImVec2 body_size = ImGui::CalcTextSize(body_text.c_str());
            float box_w = tag_size.x + body_size.x + pad_x * 2.0f;
            float box_h = (std::max)(tag_size.y, body_size.y) + pad_y * 2.0f;

            float base_x = 15.0f;
            float base_y = 15.0f;

            int pos_type = settings::notifications::position;
            if (pos_type == 0)
            {
                base_x = io.DisplaySize.x - box_w - 15.0f;
                base_y = 15.0f + target_y;
            }
            else if (pos_type == 1)
            {
                base_x = 15.0f;
                base_y = 60.0f + target_y;
            }
            else if (pos_type == 2)
            {
                base_x = io.DisplaySize.x - box_w - 15.0f;
                base_y = io.DisplaySize.y - box_h - 15.0f - target_y;
            }
            else if (pos_type == 3)
            {
                base_x = 15.0f;
                base_y = io.DisplaySize.y - box_h - 15.0f - target_y;
            }

            target_y += box_h + spacing;

            if (item.current_y_offset < -50.0f)
                item.current_y_offset = base_y;
            else
                item.current_y_offset += (base_y - item.current_y_offset) * 0.25f;

            float pos_x = base_x;
            float pos_y = item.current_y_offset;

            ImVec2 wmin = ImVec2(pos_x, pos_y);
            ImVec2 wmax = ImVec2(pos_x + box_w, pos_y + box_h);

            ImU32 bg_col = ImColor(clr->window.background_one.Value.x, clr->window.background_one.Value.y, clr->window.background_one.Value.z, alpha);
            ImU32 border_col = ImColor(clr->window.stroke.Value.x, clr->window.stroke.Value.y, clr->window.stroke.Value.z, alpha);
            ImU32 border_out_col = ImColor(clr->widgets.stroke_two.Value.x, clr->widgets.stroke_two.Value.y, clr->widgets.stroke_two.Value.z, alpha);
            ImU32 accent_col = ImColor(clr->accent.Value.x, clr->accent.Value.y, clr->accent.Value.z, alpha);
            ImU32 text_col = ImColor(clr->widgets.text.Value.x, clr->widgets.text.Value.y, clr->widgets.text.Value.z, alpha);

            dl->AddRectFilled(wmin, wmax, bg_col, 0.0f);
            dl->AddRect(wmin - ImVec2(1, 1), wmax + ImVec2(1, 1), border_out_col, 0.0f, 0, 1.0f);
            dl->AddRect(wmin, wmax, border_col, 0.0f, 0, 1.0f);
            dl->AddLine(ImVec2(wmin.x, wmin.y), ImVec2(wmax.x, wmin.y), accent_col, 1.5f);

            dl->AddText(ImVec2(pos_x + pad_x, pos_y + pad_y), accent_col, tag_prefix.c_str());
            dl->AddText(ImVec2(pos_x + pad_x + tag_size.x, pos_y + pad_y), text_col, body_text.c_str());
        }
    }
}
