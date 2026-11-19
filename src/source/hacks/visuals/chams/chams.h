#pragma once

#include <cmath>
#include <algorithm>
#include <imgui/imgui.h>
#include <game/cache/core/cache.h>
#include <hacks/system/settings/settings.h>

#include "engine/engine.h"
#include "memory/chams_main.h"
#include "memory/chams.h"
#include "memory/shader.h"
#include "memory/cache.h"
#include "memory/parser.h"
#include "memory/shader_chams.h"

namespace chams_utils {

inline void get_chams_color(const cache::entity_t& entity, const settings::visuals::priority_settings_t& cfg, float out_col[4])
{
	(void)entity;
	out_col[0] = cfg.chams_colour[0];
	out_col[1] = cfg.chams_colour[1];
	out_col[2] = cfg.chams_colour[2];
	out_col[3] = cfg.chams_colour[3];
}

}
