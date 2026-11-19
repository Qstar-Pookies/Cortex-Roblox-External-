#pragma once

#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <game/sdk.h>
#include <game/math/math.h>

namespace frame_cache
{
    inline std::atomic<uint64_t> frame_counter{ 0 };

    struct frame_data_t
    {
        math::matrix4 view{};
        math::vector2 dims{};
    };

    namespace detail
    {

        inline frame_data_t      g_data{};
        inline std::mutex        g_mtx;
        inline std::atomic<bool> g_have_data{ false };

        inline bool usable(const math::matrix4& m, const math::vector2& d)
        {
            if (!std::isfinite(d.x) || !std::isfinite(d.y) || !(d.x > 0.0f) || !(d.y > 0.0f))
                return false;

            for (int row = 0; row < 4; ++row)
                for (int col = 0; col < 4; ++col)
                    if (!std::isfinite(m.m[row][col]))
                        return false;

            if (std::abs(m.m[3][3]) < 1e-6f)
                return false;
            if (m.m[3][0] == 0.0f && m.m[3][1] == 0.0f && m.m[3][2] == 0.0f)
                return false;

            return true;
        }
    }

    inline void tick(rbx::c_visualengine* ve)
    {
        if (!ve || ve->address == 0)
            return;

        const math::matrix4 view = ve->get_viewmatrix();
        const math::vector2 dims = ve->get_dimensions();
        if (!detail::usable(view, dims))
            return;

        {
            std::lock_guard<std::mutex> lock(detail::g_mtx);
            detail::g_data.view = view;
            detail::g_data.dims = dims;
        }
        detail::g_have_data.store(true, std::memory_order_release);
        frame_counter.fetch_add(1, std::memory_order_relaxed);
    }

    inline frame_data_t get_for_thread()
    {
        thread_local frame_data_t shadow{};
        thread_local uint64_t     last_frame = UINT64_MAX;
        thread_local auto         last_ts    = std::chrono::steady_clock::time_point{};

        if (!detail::g_have_data.load(std::memory_order_acquire))
            return shadow;

        const uint64_t cur = frame_counter.load(std::memory_order_relaxed);
        const auto     now = std::chrono::steady_clock::now();
        const auto     age = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_ts).count();

        if (cur != last_frame || age >= 4)
        {
            std::lock_guard<std::mutex> lock(detail::g_mtx);
            shadow     = detail::g_data;
            last_frame = cur;
            last_ts    = now;
        }
        return shadow;
    }
}
