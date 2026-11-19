#pragma once

#include <cstddef>
#include <cstdint>

namespace nativechams {

void Tick();

void Run();

void Stop();
void Dump();

bool Hooked();
const char* Why();
std::uintptr_t Cave();
std::size_t CaveSize();
bool CaveBusy(std::uintptr_t at, std::size_t n);

std::uint32_t Draws();
std::uint32_t Hits();
std::uint32_t Items();
std::uint32_t Entered();
std::uint32_t Creates();
std::uint32_t Stage();
std::uint32_t Ready();

const char* const* ShaderNames();
int ShaderNameCount();

}
