#include "shutdown.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include <hacks/visuals/chams/native/native.h>
#include <hacks/visuals/skyshader/skyshader.h>
#include <system/logger/logger.h>

namespace cheat
{
	namespace shutdown
	{
		namespace
		{
			std::atomic<bool> g_started{ false };
			std::mutex g_mutex;
		}

		bool started()
		{
			return g_started.load(std::memory_order_acquire);
		}

		void run()
		{

			if (g_started.exchange(true, std::memory_order_acq_rel))
			{
				std::lock_guard<std::mutex> lock(g_mutex);
				return;
			}

			std::lock_guard<std::mutex> lock(g_mutex);

			Cheat::Features::SkyShader::Stop();
			nativechams::Stop();

			std::this_thread::sleep_for(std::chrono::milliseconds(120));

			Cheat::Console::Log(Cheat::Console::Color::Gray,
				"shutdown: remote hooks restored");
		}
	}
}
