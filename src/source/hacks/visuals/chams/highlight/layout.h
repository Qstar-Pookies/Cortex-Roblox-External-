#pragma once

#include <cstdint>
#include <game/offsets/offsets.h>

namespace highlightchams::layout
{
    inline constexpr std::uint64_t g_children = Offsets::Instance::ChildrenStart;
    inline constexpr std::uint64_t g_children_end = Offsets::Instance::ChildrenEnd;
    inline constexpr std::uint64_t g_class_descriptor = Offsets::Instance::ClassDescriptor;
    inline constexpr std::uint64_t g_class_name = Offsets::Instance::ClassName;
    inline constexpr std::uint64_t g_visual_engine_pointer = Offsets::VisualEngine::Pointer;
    inline constexpr std::uint64_t g_fake_datamodel_pointer = Offsets::FakeDataModel::Pointer;
    inline constexpr std::uint64_t g_fake_datamodel_to_datamodel = Offsets::FakeDataModel::RealDataModel;
    inline constexpr std::uint64_t g_model_instance = Offsets::Player::ModelInstance;
    inline constexpr std::uint64_t g_parent = Offsets::Instance::Parent;
    inline constexpr std::uint64_t g_local_player = Offsets::Player::LocalPlayer;

    inline constexpr std::uint64_t g_merge_flag = Offsets::HighlightManager::MergeFlag;
    inline constexpr std::uint64_t g_manager = Offsets::HighlightManager::Manager;
    inline constexpr std::uint64_t g_manager_alt = 0xC00;

    inline constexpr std::uint64_t g_cluster_node = Offsets::BasePart::ClusterNode;
    inline constexpr std::uint64_t g_cluster_binding_owner = Offsets::FastClusterBinding::Owner;
    inline constexpr std::uint64_t g_cluster_binding_vtable = Offsets::FastClusterBinding::VTableRva;
    inline constexpr std::uint64_t g_cluster_vtable = Offsets::FastCluster::VTableRva;
    inline constexpr std::uint64_t g_cluster_vtable_sub = Offsets::FastCluster::VTableRvaSub;
    inline constexpr std::uint64_t g_entity_vtable = Offsets::FastClusterEntity::VTableRva;

    inline constexpr std::uint64_t g_entity_highlight = Offsets::FastClusterEntity::Highlight;

    inline constexpr std::uint64_t g_contexts = Offsets::HighlightManager::Contexts;
    inline constexpr std::uint64_t g_context_stride = Offsets::HighlightManager::ContextStride;
    inline constexpr std::uint64_t g_fallback_depth = Offsets::HighlightManager::FallbackDepth;
    inline constexpr std::uint64_t g_fallback_active = Offsets::HighlightManager::FallbackActive;
    inline constexpr std::uint64_t g_fallback_fill = Offsets::HighlightManager::FallbackFill;
    inline constexpr std::uint64_t g_fallback_outline = Offsets::HighlightManager::FallbackOutline;
    inline constexpr std::uint64_t g_context_depth = Offsets::HighlightManager::ContextDepth;
    inline constexpr std::uint64_t g_context_active = Offsets::HighlightManager::ContextActive;
    inline constexpr std::uint64_t g_context_fill = Offsets::HighlightManager::ContextFill;
    inline constexpr std::uint64_t g_context_outline = Offsets::HighlightManager::ContextOutline;
    inline constexpr std::uint64_t g_global_next = Offsets::HighlightManager::GlobalNext;
    inline constexpr std::uint64_t g_global_depth = Offsets::HighlightManager::GlobalDepth;
    inline constexpr std::uint64_t g_global_active = Offsets::HighlightManager::GlobalActive;
    inline constexpr std::uint64_t g_global_fill = Offsets::HighlightManager::GlobalFill;
    inline constexpr std::uint64_t g_global_outline = Offsets::HighlightManager::GlobalOutline;
}
