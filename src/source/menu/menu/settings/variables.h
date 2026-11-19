#pragma once
#include <string>
#include <vector>
#include "imgui.h"

class c_variables
{
public:
	struct
	{
		ImGuiWindowFlags main_flags{ ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_Tooltip };
		ImGuiWindowFlags flags{ ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground };
		ImVec2 padding{ 0, 0 };
		ImVec2 spacing{ 8, 8 };
		float shadow_size{ 40 };
		float shadow_alpha{ 0.4f };
		float border_size{ 1 };
		float rounding{ 8 };
		float child_rounding{ 6 };
		float frame_rounding{ 4 };
		float grab_rounding{ 4 };
		float width{ 0 };
		float titlebar{ 28 };
		float scrollbar_size{ 6 };
		bool hover_hightlight{ true };
		bool lerp_animations{ true };
		bool window_gradients{ true };
		bool window_glow{ true };
	} window;

	struct
	{
		bool current_section[10] = { true, false, false, false, false, false, false, false, false, false };
		const char* section_icons[IM_ARRAYSIZE(current_section)] = {"A", "B", "C", "D", "E", "F", "G", "{/}", "SVR", "NPC"};

		float menu_alpha{ 0 };
		bool menu_opened{ false };
		int menu_key{ 46 };
		bool custom_cursor{ true };
		bool viewer_open{ false };
	} gui;

	struct
	{
		ImFont* icons[2];
		ImFont* tahoma;
	} font;
};

inline c_variables* var = new c_variables();
