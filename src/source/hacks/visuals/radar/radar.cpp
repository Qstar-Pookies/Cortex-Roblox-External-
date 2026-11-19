#include "radar.h"

#include <algorithm>
#include <cmath>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <game/game/game.h>
#include <game/cache/core/cache.h>
#include <game/cache/core/frame.h>
#include <game/math/math.h>
#include <hacks/system/settings/settings.h>
#include <hacks/system/teamcheck/teamcheck.h>
#include <hacks/system/playerlist/playerlist.h>
#include <menu/render/render.h>

namespace radar
{
	static std::vector<radar_point_t> s_points;
	static ImVec2 s_radar_pos = ImVec2(-1, -1);
	static ImVec2 s_minimap_pos = ImVec2(-1, -1);
	static bool s_radar_dragging = false;
	static bool s_minimap_dragging = false;

	static ImVec2 world_to_radar(const math::vector3& world_pos, const math::vector3& local_pos, const math::vector3& local_forward, float radar_range, float radar_size)
	{
		math::vector3 diff = world_pos - local_pos;
		float yaw = atan2f(local_forward.x, local_forward.z);
		
		float cos_yaw = cosf(-yaw);
		float sin_yaw = sinf(-yaw);
		
		float rotated_x = diff.x * cos_yaw - diff.z * sin_yaw;
		float rotated_z = diff.x * sin_yaw + diff.z * cos_yaw;
		
		float scale = radar_size / (radar_range * 2.0f);
		float x = rotated_x * scale;
		float y = rotated_z * scale;
		
		return ImVec2(x, -y);
	}

	static math::vector3 get_local_forward()
	{
		if (game::camera != 0)
		{
			auto camera_cf = memory->read<math::cframe>(game::camera + Offsets::Camera::CFrame);
			return camera_cf.rotation.forward();
		}
		return math::vector3(0, 0, -1);
	}

	static math::vector3 get_local_position()
	{
		cache::entity_t local = cache::get_local_player();
		if (local.humanoid_root_part.address != 0)
		{
			rbx::c_instance hrp(local.humanoid_root_part.address);
			return hrp.get_primitive().get_position();
		}
		if (game::camera != 0)
		{
			return memory->read<math::vector3>(game::camera + Offsets::Camera::Position);
		}
		return math::vector3(0, 0, 0);
	}

	void run()
	{
		if (!settings::visuals::hostile.radar && !settings::visuals::friendly.radar && !settings::visuals::neutral.radar)
			return;

		s_points.clear();

		cache::entity_t local = cache::get_local_player();
		if (local.instance.address == 0)
			return;

		math::vector3 local_pos = get_local_position();
		math::vector3 local_forward = get_local_forward();
		float radar_range = settings::visuals::hostile.radar_range;
		float radar_size = settings::visuals::hostile.radar_size;

		auto local_snap = cache::get_local_snap();
		std::lock_guard<std::mutex> lock(cache::mtx);

		for (const auto& ent : cache::players)
		{
			if (ent.instance.address == local.instance.address)
			{
				if (!settings::visuals::neutral.radar_show_local)
					continue;
				
				math::vector3 ent_pos = ent.humanoid_root_part.address != 0 
					? rbx::c_instance(ent.humanoid_root_part.address).get_primitive().get_position()
					: local_pos;
				
				float dist = (ent_pos - local_pos).length();
				if (dist > radar_range)
					continue;

				ImVec2 radar_pos = world_to_radar(ent_pos, local_pos, local_forward, radar_range, radar_size);
				
				s_points.push_back({
					ent_pos,
					radar_pos,
					dist,
					false,
					false,
					true,
					ent.name,
					ent.instance.address
				});
				continue;
			}

			if (ent.health <= 0.f || ent.knocked)
				continue;

			bool is_friendly = false;
			if (local_snap && local_snap->team != 0 && ent.team != 0 && ent.team == local_snap->team)
			{
				if (game::datamodel && game::datamodel->address != 0 && !game::datamodel->is_mm2() && !game::datamodel->is_arsenal())
					is_friendly = true;
			}
			else if (!ent.name.empty() && playerlist::get_priority(ent.name) == cache::player_priority::friendly)
				is_friendly = true;

			if (is_friendly)
			{
				if (!settings::visuals::friendly.radar_show_friendlies)
					continue;
			}
			else
			{
				if (!settings::visuals::hostile.radar_show_enemies)
					continue;
			}

			math::vector3 ent_pos = ent.humanoid_root_part.address != 0
				? rbx::c_instance(ent.humanoid_root_part.address).get_primitive().get_position()
				: math::vector3(0, 0, 0);

			if (ent_pos == math::vector3(0, 0, 0))
				continue;

			float dist = (ent_pos - local_pos).length();
			if (dist > radar_range)
				continue;

			ImVec2 radar_pos = world_to_radar(ent_pos, local_pos, local_forward, radar_range, radar_size);

			s_points.push_back({
				ent_pos,
				radar_pos,
				dist,
				!is_friendly,
				is_friendly,
				false,
				ent.name,
				ent.instance.address
			});
		}
	}

	void render_radar()
	{
		if (!settings::visuals::hostile.radar && !settings::visuals::friendly.radar && !settings::visuals::neutral.radar)
			return;

		if (s_points.empty())
			return;

		ImGuiIO& io = ImGui::GetIO();
		ImDrawList* draw = ImGui::GetBackgroundDrawList();
		if (!draw)
			return;

		float radar_size = settings::visuals::hostile.radar_size;
		const char* positions[] = { "Top Left", "Top Right", "Bottom Left", "Bottom Right", "Custom" };
		ImVec2 pos;

		switch (settings::visuals::hostile.radar_position)
		{
		case 0: pos = ImVec2(20, 20); break;
		case 1: pos = ImVec2(io.DisplaySize.x - radar_size - 20, 20); break;
		case 2: pos = ImVec2(20, io.DisplaySize.y - radar_size - 20); break;
		case 3: pos = ImVec2(io.DisplaySize.x - radar_size - 20, io.DisplaySize.y - radar_size - 20); break;
		case 4: pos = (s_radar_pos.x >= 0 && s_radar_pos.y >= 0) ? s_radar_pos : ImVec2(20, 20); break;
		default: pos = ImVec2(20, 20);
		}

		if (settings::visuals::hostile.radar_position == 4)
		{
			ImGui::SetNextWindowPos(pos, ImGuiCond_Once);
			ImGui::Begin("Radar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);
			s_radar_pos = ImGui::GetWindowPos();
			ImGui::End();
		}

		ImVec2 center = pos + ImVec2(radar_size * 0.5f, radar_size * 0.5f);
		ImVec2 min = pos;
		ImVec2 max = pos + ImVec2(radar_size, radar_size);

		ImU32 bg_col = IM_COL32(
			(int)(settings::visuals::hostile.radar_background_colour[0] * 255),
			(int)(settings::visuals::hostile.radar_background_colour[1] * 255),
			(int)(settings::visuals::hostile.radar_background_colour[2] * 255),
			(int)(settings::visuals::hostile.radar_background_colour[3] * 255)
		);
		ImU32 border_col = IM_COL32(
			(int)(settings::visuals::hostile.radar_border_colour[0] * 255),
			(int)(settings::visuals::hostile.radar_border_colour[1] * 255),
			(int)(settings::visuals::hostile.radar_border_colour[2] * 255),
			(int)(settings::visuals::hostile.radar_border_colour[3] * 255)
		);

		draw->AddRectFilled(min, max, bg_col, 6.0f);
		draw->AddRect(min, max, border_col, 6.0f, 0, 1.5f);

		if (settings::visuals::hostile.radar_crosshair)
		{
			ImU32 cross_col = IM_COL32(
				(int)(settings::visuals::hostile.radar_crosshair_colour[0] * 255),
				(int)(settings::visuals::hostile.radar_crosshair_colour[1] * 255),
				(int)(settings::visuals::hostile.radar_crosshair_colour[2] * 255),
				(int)(settings::visuals::hostile.radar_crosshair_colour[3] * 255)
			);
			draw->AddLine(ImVec2(center.x - 8, center.y), ImVec2(center.x + 8, center.y), cross_col, 1.5f);
			draw->AddLine(ImVec2(center.x, center.y - 8), ImVec2(center.x, center.y + 8), cross_col, 1.5f);
			draw->AddCircle(center, radar_size * 0.5f - 2, cross_col, 32, 0.5f);
		}

		for (const auto& point : s_points)
		{
			ImVec2 screen_pos = center + point.screen_pos;
			
			if (screen_pos.x < min.x + 2 || screen_pos.x > max.x - 2 ||
				screen_pos.y < min.y + 2 || screen_pos.y > max.y - 2)
				continue;

			ImU32 point_col;
			if (point.is_local)
				point_col = IM_COL32(
					(int)(settings::visuals::neutral.radar_local_colour[0] * 255),
					(int)(settings::visuals::neutral.radar_local_colour[1] * 255),
					(int)(settings::visuals::neutral.radar_local_colour[2] * 255),
					(int)(settings::visuals::neutral.radar_local_colour[3] * 255)
				);
			else if (point.is_enemy)
				point_col = IM_COL32(
					(int)(settings::visuals::hostile.radar_enemy_colour[0] * 255),
					(int)(settings::visuals::hostile.radar_enemy_colour[1] * 255),
					(int)(settings::visuals::hostile.radar_enemy_colour[2] * 255),
					(int)(settings::visuals::hostile.radar_enemy_colour[3] * 255)
				);
			else
				point_col = IM_COL32(
					(int)(settings::visuals::friendly.radar_friendly_colour[0] * 255),
					(int)(settings::visuals::friendly.radar_friendly_colour[1] * 255),
					(int)(settings::visuals::friendly.radar_friendly_colour[2] * 255),
					(int)(settings::visuals::friendly.radar_friendly_colour[3] * 255)
				);

			draw->AddCircleFilled(screen_pos, 4.0f, point_col);
			draw->AddCircle(screen_pos, 4.0f, IM_COL32(0, 0, 0, 200), 12, 1.0f);

			if (settings::visuals::hostile.radar_names && !point.name.empty())
			{
				ImVec2 text_size = ImGui::CalcTextSize(point.name.c_str());
				ImVec2 text_pos = screen_pos + ImVec2(6, -text_size.y * 0.5f);
				draw->AddText(text_pos, IM_COL32(
					(int)(settings::visuals::hostile.radar_name_colour[0] * 255),
					(int)(settings::visuals::hostile.radar_name_colour[1] * 255),
					(int)(settings::visuals::hostile.radar_name_colour[2] * 255),
					(int)(settings::visuals::hostile.radar_name_colour[3] * 255)
				), point.name.c_str());
			}
		}

		char range_text[32];
		sprintf_s(range_text, "%.0fm", settings::visuals::hostile.radar_range);
		draw->AddText(ImVec2(min.x + 5, min.y + 5), IM_COL32(200, 200, 200, 200), range_text);
	}

	void render_minimap()
	{
		if (!settings::visuals::hostile.radar)
			return;

		ImGuiIO& io = ImGui::GetIO();
		ImDrawList* draw = ImGui::GetBackgroundDrawList();
		if (!draw)
			return;

		float minimap_size = 200.f;
		ImVec2 pos = ImVec2(io.DisplaySize.x - minimap_size - 20, io.DisplaySize.y - minimap_size - 20);

		ImVec2 center = pos + ImVec2(minimap_size * 0.5f, minimap_size * 0.5f);
		ImVec2 min = pos;
		ImVec2 max = pos + ImVec2(minimap_size, minimap_size);

		ImU32 bg_col = IM_COL32(10, 10, 15, 200);
		ImU32 border_col = IM_COL32(60, 60, 70, 255);

		draw->AddRectFilled(min, max, bg_col, 8.0f);
		draw->AddRect(min, max, border_col, 8.0f, 0, 2.0f);

		draw->AddCircleFilled(center, 5.0f, IM_COL32(100, 100, 255, 255));
		draw->AddCircle(center, 5.0f, IM_COL32(0, 0, 0, 200), 12, 1.5f);

		for (const auto& point : s_points)
		{
			float scale = minimap_size / (settings::visuals::hostile.radar_range * 2.0f);
			ImVec2 screen_pos = center + point.screen_pos * (minimap_size / settings::visuals::hostile.radar_size);
			
			if (screen_pos.x < min.x + 3 || screen_pos.x > max.x - 3 ||
				screen_pos.y < min.y + 3 || screen_pos.y > max.y - 3)
				continue;

			ImU32 point_col = point.is_enemy 
				? IM_COL32(255, 60, 60, 255)
				: (point.is_friendly ? IM_COL32(60, 255, 60, 255) : IM_COL32(60, 100, 255, 255));

			draw->AddCircleFilled(screen_pos, 3.0f, point_col);
		}
	}
}