#pragma once
#include "imgui.h"

class c_colors
{
public:
	ImColor accent{ 211, 34, 44 };
	ImColor dark_accent{ 158, 25, 33 };

	struct
	{
		ImColor background_one{ 29, 29, 29 };
		ImColor background_two{ 33, 33, 33 };
		ImColor stroke{ 46, 46, 46 };
	} window;

	struct
	{
		ImColor stroke_two{ 19, 19, 19 };
		ImColor text{ 137, 137, 137 };
		ImColor text_inactive{ 67, 67, 67 };
		ImColor unsafe_text{ 186, 76, 76 };
	} widgets;
};

inline c_colors* clr = new c_colors();
