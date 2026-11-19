#pragma once

#include <cstdint>
#include <vector>

#include <imgui/imgui.h>
#include <d3d11.h>

#include <game/math/math.h>
#include <game/cache/core/cache.h>

namespace nazichams {

void ensure_init(ID3D11Device* device, ID3D11DeviceContext* ctx);
void on_resize(unsigned width, unsigned height);
void shutdown();

std::uint64_t character_address(const cache::entity_t& e);

struct EntityBucket {
    const std::vector<const cache::entity_t*>* entities{ nullptr };
    const float* fill{ nullptr };
    const float* line{ nullptr };
};

void render_buckets(
    ImDrawList* dl,
    ID3D11Device* device,
    ID3D11DeviceContext* ctx,
    ID3D11RenderTargetView* rtv,
    const Matrix4x4& view,
    const Vector3& camera,
    const Vector2& viewport,
    const EntityBucket* buckets,
    std::size_t bucket_count);

}
