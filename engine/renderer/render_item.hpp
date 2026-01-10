#pragma once

/**
 * @file render_item.hpp
 * @brief RenderItem struct for submission-based rendering
 *
 * Following Hell2025's RenderDataManager pattern - entities submit RenderItems
 * that encapsulate all data needed for rendering.
 */

#include "engine/assets/asset_handle.hpp"
#include "engine/assets/material.hpp"

#include <glm/glm.hpp>

namespace hz {

// Forward declarations
class Mesh;

/**
 * @brief Encapsulates all data needed to render a single mesh
 *
 * Used with the submission-based rendering pattern where game code submits
 * RenderItems and the renderer batches/draws them during render passes.
 */
struct RenderItem {
    // Transform
    glm::mat4 transform{1.0f};

    // Mesh to draw (for primitives)
    const Mesh* mesh{nullptr};

    // Material properties
    const Material* material{nullptr};

    // Alternative: use handles instead of pointers for model-based rendering
    ModelHandle model{};

    // ==========================================================================
    // Factory Methods
    // ==========================================================================

    /**
     * @brief Create a RenderItem from a mesh and material
     */
    static RenderItem from_mesh(const Mesh* mesh, const glm::mat4& transform,
                                const Material* material = nullptr) {
        RenderItem item;
        item.mesh = mesh;
        item.transform = transform;
        item.material = material;
        return item;
    }

    /**
     * @brief Create a RenderItem from a model handle
     */
    static RenderItem from_model(ModelHandle model, const glm::mat4& transform,
                                 const Material* material = nullptr) {
        RenderItem item;
        item.model = model;
        item.transform = transform;
        item.material = material;
        return item;
    }

    /**
     * @brief Check if this item uses a mesh or a model
     */
    [[nodiscard]] bool uses_mesh() const { return mesh != nullptr; }
    [[nodiscard]] bool uses_model() const { return model.is_valid(); }
    [[nodiscard]] bool is_valid() const { return uses_mesh() || uses_model(); }
};

} // namespace hz
