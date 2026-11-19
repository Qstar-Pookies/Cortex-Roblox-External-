#pragma once
#include <vector>
#include <string>
#include <imgui/imgui.h>
#include <game/math/math.h>

namespace radar
{
	struct radar_point_t
	{
		math::vector3 world_pos;
		ImVec2 screen_pos;
		float distance;
		bool is_enemy;
		bool is_friendly;
		bool is_local;
		std::string name;
		uint64_t address;
	};

	void run();
	void render_radar();
	void render_minimap();
}