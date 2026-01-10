#pragma once

/**
 * @file skeleton.hpp
 * @brief Skeletal animation data structures
 *
 * Defines Bone, Skeleton, AnimationClip, and Keyframe types for
 * skeletal animation following LearnOpenGL patterns.
 */

#include "engine/core/types.hpp"

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace hz {

// Maximum bones per vertex (matches shader)
constexpr u32 MAX_BONE_INFLUENCE = 4;
// Maximum bones per skeleton (matches shader uniform array size)
constexpr u32 MAX_BONES = 100;

/**
 * @brief Bone vertex data for skinning
 */
struct BoneVertexData {
    i32 bone_ids[MAX_BONE_INFLUENCE] = {-1, -1, -1, -1};
    float weights[MAX_BONE_INFLUENCE] = {0.0f, 0.0f, 0.0f, 0.0f};

    void add_bone(i32 bone_id, float weight) {
        for (u32 i = 0; i < MAX_BONE_INFLUENCE; ++i) {
            if (bone_ids[i] < 0) {
                bone_ids[i] = bone_id;
                weights[i] = weight;
                return;
            }
        }
        // All slots full - could replace lowest weight
    }
};

/**
 * @brief Single bone in skeleton hierarchy
 */
struct Bone {
    std::string name;
    i32 id{-1};
    i32 parent_id{-1};

    // Bind pose: transforms from bone space to model space at rest
    glm::mat4 offset_matrix{1.0f};

    // Local transform relative to parent (animated)
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};

    [[nodiscard]] glm::mat4 get_local_transform() const;
};

/**
 * @brief Keyframe for a single property
 */
template <typename T>
struct Keyframe {
    float time{0.0f};
    T value{};
};

using PositionKey = Keyframe<glm::vec3>;
using RotationKey = Keyframe<glm::quat>;
using ScaleKey = Keyframe<glm::vec3>;

/**
 * @brief Animation channel for a single bone
 */
struct BoneAnimation {
    std::string bone_name;
    i32 bone_id{-1};

    std::vector<PositionKey> position_keys;
    std::vector<RotationKey> rotation_keys;
    std::vector<ScaleKey> scale_keys;

    /**
     * @brief Interpolate position at given time
     */
    [[nodiscard]] glm::vec3 interpolate_position(float time) const;

    /**
     * @brief Interpolate rotation at given time
     */
    [[nodiscard]] glm::quat interpolate_rotation(float time) const;

    /**
     * @brief Interpolate scale at given time
     */
    [[nodiscard]] glm::vec3 interpolate_scale(float time) const;
};

/**
 * @brief Animation clip containing keyframes for multiple bones
 */
struct AnimationClip {
    std::string name;
    float duration{0.0f};
    float ticks_per_second{25.0f};

    std::vector<BoneAnimation> channels;

    /**
     * @brief Get channel by bone name
     */
    [[nodiscard]] const BoneAnimation* get_channel(const std::string& bone_name) const;
};

/**
 * @brief Complete skeleton with bone hierarchy
 */
class Skeleton {
public:
    Skeleton() = default;
    ~Skeleton() = default;

    /**
     * @brief Add a bone to the skeleton
     */
    i32 add_bone(const std::string& name, i32 parent_id, const glm::mat4& offset);

    /**
     * @brief Get bone by ID
     */
    [[nodiscard]] Bone* get_bone(i32 id);
    [[nodiscard]] const Bone* get_bone(i32 id) const;

    /**
     * @brief Get bone ID by name
     */
    [[nodiscard]] i32 get_bone_id(const std::string& name) const;

    /**
     * @brief Get number of bones
     */
    [[nodiscard]] u32 bone_count() const { return static_cast<u32>(m_bones.size()); }

    /**
     * @brief Calculate final bone transforms for given animation time
     */
    void calculate_bone_transforms(const AnimationClip& clip, float time,
                                   std::vector<glm::mat4>& out_transforms) const;

    /**
     * @brief Get global inverse transform
     */
    [[nodiscard]] const glm::mat4& global_inverse_transform() const {
        return m_global_inverse_transform;
    }

    void set_global_inverse_transform(const glm::mat4& mat) { m_global_inverse_transform = mat; }

private:
    std::vector<Bone> m_bones;
    std::unordered_map<std::string, i32> m_bone_name_to_id;
    glm::mat4 m_global_inverse_transform{1.0f};

    void calculate_bone_transform_recursive(i32 bone_id, const AnimationClip& clip, float time,
                                            const glm::mat4& parent_transform,
                                            std::vector<glm::mat4>& out_transforms) const;
};

} // namespace hz
