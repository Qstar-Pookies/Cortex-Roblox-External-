#pragma once

#include <game/math/math.h>

#include <d3d11.h>
#include <cstdint>
#include <cstddef>
#include <string>

namespace Cheat {
namespace Visuals {
namespace MeshDxShader {

bool Init(ID3D11Device* device, ID3D11DeviceContext* context);
void Shutdown();
void Resize(unsigned width, unsigned height);
bool Ready();
std::size_t LastDrawn();

void BeginFrame(const Matrix4x4& view, const Vector3& camera, float time);
void QueueMesh(const std::string& mesh_id, const Matrix4x4& world);
void QueueBox(const Matrix4x4& world);
void Flush(ID3D11RenderTargetView* rtv);

const char* const* ModeNames();
int ModeNameCount();

}
}
}
