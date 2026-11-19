#pragma once

#include <game/math/math.h>
#include <imgui/imgui.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Cheat {
namespace Visuals {
namespace MeshChams {

void Draw(
	ImDrawList* dl,
	std::uint64_t character,
	const Matrix4x4& view,
	const Vector2& viewport,
	float scale_x,
	float scale_y,
	ImU32 fill_col);

const char* const* OutlineStyleNames();
int OutlineStyleNameCount();

bool ExpandBounds(
	std::uint64_t character,
	const Matrix4x4& view,
	const Vector2& viewport,
	float scale_x,
	float scale_y,
	float& min_x, float& max_x,
	float& min_y, float& max_y,
	Vector3& wmin, Vector3& wmax);

std::size_t BakedPartCount(std::uint64_t character);

std::vector<std::uint64_t> BakedParts(std::uint64_t character);

struct FallbackBox {
    Vector3 pos{};
    Matrix4x4 rot{};
    Vector3 sz{};
};
void GetFallbackBoxes(std::uint64_t character, std::vector<FallbackBox>& out);

}
}
}
