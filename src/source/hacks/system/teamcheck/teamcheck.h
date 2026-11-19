#pragma once

#include <cstdint>

#include <game/cache/core/cache.h>
#include <hacks/system/settings/settings.h>

namespace teamcheck
{
	inline bool universal()
	{
		return settings::teamcheck;
	}

	inline bool is_teammate(const cache::entity_t& entity, std::uint64_t local_team,
		bool use_friend_list)
	{
		if (local_team != 0 && entity.team != 0 && entity.team == local_team)
			return true;
		if (use_friend_list && entity.priority == cache::player_priority::friendly)
			return true;
		return false;
	}

	inline bool skip(const cache::entity_t& entity, std::uint64_t local_team,
		bool feature_toggle)
	{
		const bool use_universal = universal();
		if (!use_universal && !feature_toggle)
			return false;
		return is_teammate(entity, local_team, use_universal);
	}
}
