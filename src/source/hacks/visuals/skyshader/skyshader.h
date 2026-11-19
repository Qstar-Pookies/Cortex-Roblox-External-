#pragma once

#include <cstddef>
#include <cstdint>

namespace Cheat
{
	namespace Features
	{
		namespace SkyShader
		{

			void Run();

			void Tick();

			void Stop();
			void Dump();

			const char* Status();

			bool CaveBusy(std::uintptr_t at, std::size_t n);
		}
	}
}
