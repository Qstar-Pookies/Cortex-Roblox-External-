#pragma once

#include <vector>
#include <mutex>
#include <memory>
#include <algorithm>

#include <game/math/math.h>

namespace rbx
{
	
	struct obb
	{
		math::vector3 center{};
		math::vector3 half_size{};
		math::vector3 axes[3]{ { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 0.f, 1.f } };

		
		
		bool intersects(const math::vector3& origin, const math::vector3& dir, float max_dist) const
		{
			const math::vector3 p = center - origin;
			float t_min = 0.0f;
			float t_max = max_dist;

			for (int i = 0; i < 3; ++i)
			{
				const float e = axes[i].dot(p);          
				const float f = axes[i].dot(dir);         
				const float h = half_size[i];

				if (f > 1e-6f || f < -1e-6f)
				{
					float t1 = (e - h) / f;
					float t2 = (e + h) / f;
					if (t1 > t2) std::swap(t1, t2);
					if (t1 > t_min) t_min = t1;
					if (t2 < t_max) t_max = t2;
					if (t_min > t_max) return false;
				}
				else if (-e - h > 0.0f || -e + h < 0.0f)
				{
					
					return false;
				}
			}
			return true;
		}
	};
}

class c_wallcheck
{
public:
	
	
	void cache_workspace()
	{
		std::vector<rbx::obb> fresh;

		
		
		
		
		
		
		
		
		
		

		std::lock_guard<std::mutex> lock(m_mutex);
		m_obstacles.swap(fresh);
	}

	
	bool is_visible(const math::vector3& from, const math::vector3& to) const
	{
		math::vector3 delta = to - from;
		float dist = delta.length();
		if (dist <= 1e-4f)
			return true;

		const math::vector3 dir = delta / dist;

		std::lock_guard<std::mutex> lock(m_mutex);
		for (const rbx::obb& box : m_obstacles)
		{
			if (box.intersects(from, dir, dist))
				return false;
		}
		return true;
	}

	
	
	
	const std::vector<rbx::obb>& get_obstacles() const
	{
		return m_obstacles;
	}

private:
	mutable std::mutex m_mutex;
	std::vector<rbx::obb> m_obstacles;
};

inline std::unique_ptr<c_wallcheck> wallcheck = std::make_unique<c_wallcheck>();
