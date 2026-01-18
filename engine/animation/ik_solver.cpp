#include "ik_solver.hpp"

#include "engine/core/log.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace hz {

// ============================================================================
// IKChain
// ============================================================================

void IKChain::calculate_length(const Skeleton& skeleton) {
    total_length = 0.0f;

    for (size_t i = 0; i + 1 < bone_ids.size(); ++i) {
        const Bone* current = skeleton.get_bone(bone_ids[i]);
        const Bone* next = skeleton.get_bone(bone_ids[i + 1]);

        if (current && next) {
            // Calculate bone length from offset matrices
            glm::vec3 current_pos = glm::vec3(glm::inverse(current->offset_matrix)[3]);
            glm::vec3 next_pos = glm::vec3(glm::inverse(next->offset_matrix)[3]);
            total_length += glm::length(next_pos - current_pos);
        }
    }
}

// ============================================================================
// TwoBoneIK
// ============================================================================

float TwoBoneIK::law_of_cosines(float a, float b, float c) {
    // c² = a² + b² - 2ab*cos(C)
    // cos(C) = (a² + b² - c²) / (2ab)
    float numerator = a * a + b * b - c * c;
    float denominator = 2.0f * a * b;

    if (std::abs(denominator) < 0.0001f) {
        return 0.0f;
    }

    float cos_angle = std::clamp(numerator / denominator, -1.0f, 1.0f);
    return std::acos(cos_angle);
}

void TwoBoneIK::solve(Skeleton& skeleton, const IKChain& chain, const glm::vec3& target,
                      std::vector<glm::mat4>& bone_transforms) {
    // Two-bone IK requires exactly 3 bones: root, middle, end
    if (chain.bone_ids.size() != 3) {
        HZ_ENGINE_WARN("TwoBoneIK requires exactly 3 bones in chain, got {}",
                       chain.bone_ids.size());
        return;
    }

    const i32 root_id = chain.bone_ids[0];
    const i32 mid_id = chain.bone_ids[1];
    const i32 end_id = chain.bone_ids[2];

    Bone* root_bone = skeleton.get_bone(root_id);
    Bone* mid_bone = skeleton.get_bone(mid_id);
    Bone* end_bone = skeleton.get_bone(end_id);

    if (!root_bone || !mid_bone || !end_bone) {
        return;
    }

    // Get current joint positions from transforms
    // Extract positions from the final bone transforms
    const glm::mat4& global_inverse = skeleton.global_inverse_transform();
    const glm::mat4 global_transform = glm::inverse(global_inverse);

    auto get_bone_world_pos = [&](i32 bone_id) -> glm::vec3 {
        const Bone* bone = skeleton.get_bone(bone_id);
        if (!bone || bone_id < 0 || static_cast<size_t>(bone_id) >= bone_transforms.size()) {
            return glm::vec3(0.0f);
        }
        glm::mat4 offset_inv = glm::inverse(bone->offset_matrix);
        glm::mat4 world =
            global_transform * bone_transforms[static_cast<size_t>(bone_id)] * offset_inv;
        return glm::vec3(world[3]);
    };

    glm::vec3 root_pos = get_bone_world_pos(root_id);
    glm::vec3 mid_pos = get_bone_world_pos(mid_id);
    glm::vec3 end_pos = get_bone_world_pos(end_id);

    // Calculate bone lengths
    float upper_length = glm::length(mid_pos - root_pos);
    float lower_length = glm::length(end_pos - mid_pos);
    float total_length = upper_length + lower_length;

    // Direction and distance to target
    glm::vec3 to_target = target - root_pos;
    float target_distance = glm::length(to_target);

    // Clamp target distance to reachable range
    if (target_distance < 0.001f) {
        target_distance = 0.001f;
    }
    target_distance = std::min(target_distance, total_length * 0.9999f);
    target_distance = std::max(target_distance, std::abs(upper_length - lower_length) * 1.0001f);

    // Calculate angles using law of cosines
    float angle_at_root = law_of_cosines(upper_length, target_distance, lower_length);
    float angle_at_mid = law_of_cosines(upper_length, lower_length, target_distance);

    // Direction to target (normalized)
    glm::vec3 target_dir = glm::normalize(to_target);

    // Calculate the bend plane using pole vector
    glm::vec3 pole_dir = glm::normalize(pole_vector - root_pos);
    glm::vec3 bend_normal = glm::normalize(glm::cross(target_dir, pole_dir));

    if (glm::length(bend_normal) < 0.001f) {
        // Target and pole are aligned, use a default perpendicular
        bend_normal = glm::vec3(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(target_dir, bend_normal)) > 0.9f) {
            bend_normal = glm::vec3(1.0f, 0.0f, 0.0f);
        }
        bend_normal = glm::normalize(glm::cross(target_dir, bend_normal));
    }

    // Rotate target direction by root angle to get upper bone direction
    glm::quat root_rotation = glm::angleAxis(-angle_at_root, bend_normal);
    glm::vec3 upper_dir = glm::normalize(root_rotation * target_dir);

    // New middle position
    glm::vec3 new_mid_pos = root_pos + upper_dir * upper_length;

    // Lower bone direction (from mid to target)
    glm::vec3 lower_dir = glm::normalize(target - new_mid_pos);

    // Update bone rotations
    // This is a simplified approach - in a full implementation, we'd update
    // the bone's local rotation to achieve these world-space directions

    // Store the calculated positions for debug visualization
    mid_bone->position = new_mid_pos - root_pos; // Local to parent

    HZ_UNUSED(angle_at_mid);
    HZ_UNUSED(lower_dir);
    HZ_UNUSED(end_bone);
}

// ============================================================================
// FABRIKSolver
// ============================================================================

void FABRIKSolver::solve(Skeleton& skeleton, const IKChain& chain, const glm::vec3& target,
                         std::vector<glm::mat4>& bone_transforms) {
    if (chain.bone_ids.size() < 2) {
        return;
    }

    const size_t num_joints = chain.bone_ids.size();

    // Initialize positions and lengths
    m_positions.resize(num_joints);
    m_lengths.resize(num_joints - 1);

    const glm::mat4& global_inverse = skeleton.global_inverse_transform();
    const glm::mat4 global_transform = glm::inverse(global_inverse);

    // Extract current joint positions
    for (size_t i = 0; i < num_joints; ++i) {
        const Bone* bone = skeleton.get_bone(chain.bone_ids[i]);
        if (!bone)
            return;

        size_t bone_idx = static_cast<size_t>(chain.bone_ids[i]);
        if (bone_idx >= bone_transforms.size())
            return;

        glm::mat4 offset_inv = glm::inverse(bone->offset_matrix);
        glm::mat4 world = global_transform * bone_transforms[bone_idx] * offset_inv;
        m_positions[i] = glm::vec3(world[3]);
    }

    // Calculate bone lengths
    for (size_t i = 0; i < num_joints - 1; ++i) {
        m_lengths[i] = glm::length(m_positions[i + 1] - m_positions[i]);
    }

    // Check if target is reachable
    float total_length = 0.0f;
    for (float len : m_lengths) {
        total_length += len;
    }

    glm::vec3 root_pos = m_positions[0];
    float dist_to_target = glm::length(target - root_pos);

    if (dist_to_target > total_length) {
        // Target unreachable - stretch toward it
        glm::vec3 dir = glm::normalize(target - root_pos);
        for (size_t i = 0; i < num_joints - 1; ++i) {
            m_positions[i + 1] = m_positions[i] + dir * m_lengths[i];
        }
    } else {
        // FABRIK iterations
        for (u32 iter = 0; iter < max_iterations; ++iter) {
            // Check if we're close enough
            float error = glm::length(m_positions[num_joints - 1] - target);
            if (error < tolerance) {
                break;
            }

            // Forward reaching
            forward_reach(target);

            // Backward reaching
            backward_reach(root_pos);
        }
    }

    // Update bone transforms (simplified - just update positions for now)
    // A full implementation would calculate proper rotations
    HZ_UNUSED(bone_transforms);
}

void FABRIKSolver::forward_reach(const glm::vec3& target) {
    size_t n = m_positions.size();
    m_positions[n - 1] = target;

    for (size_t i = n - 2; i < n; --i) { // size_t underflow-safe loop
        glm::vec3 dir = glm::normalize(m_positions[i] - m_positions[i + 1]);
        m_positions[i] = m_positions[i + 1] + dir * m_lengths[i];

        if (i == 0)
            break;
    }
}

void FABRIKSolver::backward_reach(const glm::vec3& root) {
    m_positions[0] = root;

    for (size_t i = 0; i < m_positions.size() - 1; ++i) {
        glm::vec3 dir = glm::normalize(m_positions[i + 1] - m_positions[i]);
        m_positions[i + 1] = m_positions[i] + dir * m_lengths[i];
    }
}

} // namespace hz
