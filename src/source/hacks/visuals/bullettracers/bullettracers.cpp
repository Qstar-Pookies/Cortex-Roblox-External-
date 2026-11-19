#include "bullettracers.h"

#include <vector>
#include <algorithm>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <game/game/game.h>
#include <game/cache/core/frame.h>
#include <game/math/math.h>
#include <hacks/system/settings/settings.h>
#include <menu/render/render.h>

namespace bullettracers
{
	static std::vector<tracer_t> s_tracers;

	void add_tracer(const math::vector3& start, const math::vector3& end, bool is_enemy, bool is_local)
	{
		if (!settings::visuals::hostile.bullet_tracers)
			return;

		if (is_enemy && !settings::visuals::hostile.bullet_tracer_show_enemy)
			return;
		if (is_local && !settings::visuals::hostile.bullet_tracer_show_local)
			return;

		tracer_t t;
		t.start = start;
		t.end = end;
		t.time_created = (float)ImGui::GetTime();
		t.duration = settings::visuals::hostile.bullet_tracer_duration;
		t.is_enemy = is_enemy;
		t.is_local = is_local;

		s_tracers.push_back(t);

		if (s_tracers.size() > 200)
			s_tracers.erase(s_tracers.begin());
	}

	void run()
	{
		if (!settings::visuals::hostile.bullet_tracers)
		{
			s_tracers.clear();
			return;
		}

		float current_time = (float)ImGui::GetTime();
		s_tracers.erase(
			std::remove_if(s_tracers.begin(), s_tracers.end(),
				[current_time](const tracer_t& t) {
					return (current_time - t.time_created) > t.duration;
				}),
			s_tracers.end()
		);
	}

	void render()
	{
		if (!settings::visuals::hostile.bullet_tracers)
			return;

		if (s_tracers.empty())
			return;

		if (!game::visualengine || game::visualengine->address == 0)
			return;

		const auto fc = frame_cache::get_for_thread();
		const math::matrix4& view = fc.view;
		const math::vector2& dims = fc.dims;

		if (dims.x <= 0.0f || dims.y <= 0.0f)
			return;

		ImDrawList* draw = ImGui::GetBackgroundDrawList();
		if (!draw)
			return;

		float current_time = (float)ImGui::GetTime();

		for (const auto& t : s_tracers)
		{
			math::vector2 start_ndc{}, end_ndc{};
			if (!game::visualengine->world_to_screen(view, dims, t.start, start_ndc))
				continue;
			if (!game::visualengine->world_to_screen(view, dims, t.end, end_ndc))
				continue;

			const ImVec2 screen_start(start_ndc.x, start_ndc.y);
			const ImVec2 screen_end(end_ndc.x, end_ndc.y);

			float progress = (current_time - t.time_created) / t.duration;
			if (progress < 0.f) progress = 0.f;
			if (progress > 1.f) progress = 1.f;

			int alpha = (int)(255 * (1.f - progress));
			if (alpha < 10) continue;

			ImU32 colour;
			if (t.is_local)
			{
				colour = IM_COL32(
					(int)(settings::visuals::hostile.bullet_tracer_colour[0] * 255),
					(int)(settings::visuals::hostile.bullet_tracer_colour[1] * 255),
					(int)(settings::visuals::hostile.bullet_tracer_colour[2] * 255),
					alpha
				);
			}
			else
			{
				colour = IM_COL32(
					(int)(settings::visuals::hostile.bullet_tracer_enemy_colour[0] * 255),
					(int)(settings::visuals::hostile.bullet_tracer_enemy_colour[1] * 255),
					(int)(settings::visuals::hostile.bullet_tracer_enemy_colour[2] * 255),
					alpha
				);
			}

			float thickness = settings::visuals::hostile.bullet_tracer_thickness * (1.f - progress * 0.5f);

			if (settings::visuals::hostile.bullet_tracer_style == 0)
			{
				draw->AddLine(screen_start, screen_end, colour, thickness);
			}
			else if (settings::visuals::hostile.bullet_tracer_style == 1)
			{
				draw->AddLine(screen_start, screen_end, IM_COL32(0, 0, 0, alpha / 2), thickness + 2);
				draw->AddLine(screen_start, screen_end, colour, thickness);
			}
			else if (settings::visuals::hostile.bullet_tracer_style == 2)
			{
				ImVec2 mid = ImVec2(
					(screen_start.x + screen_end.x) * 0.5f,
					(screen_start.y + screen_end.y) * 0.5f
				);
				draw->AddLine(screen_start, mid, colour, thickness);
				draw->AddLine(mid, screen_end, IM_COL32(
					(int)(colour >> 16 & 0xFF),
					(int)(colour >> 8 & 0xFF),
					(int)(colour & 0xFF),
					alpha / 3
				), thickness);
			}
		}
	}
}