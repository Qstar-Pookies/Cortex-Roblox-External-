#pragma once
#include <vector>
#include <game/math/math.h>

namespace bullettracers
{
	struct tracer_t
	{
		math::vector3 start;
		math::vector3 end;
		float time_created;
		float duration;
		bool is_enemy;
		bool is_local;
	};

	void add_tracer(const math::vector3& start, const math::vector3& end, bool is_enemy, bool is_local);
	void run();
	void render();
}