#pragma once

/**
 * @file ik_solver.hpp
 * @brief Inverse Kinematics solvers for skeletal animation
 */

#include "skeleton.hpp"

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace hz {

/**
 * @brief A chain of bones for IK solving
 */
struct IKChain {
    std::vector<i32> bone_ids; // Bone IDs from root to end effector
    float total_length{0.0f};  // Sum of bone lengths (calculated)

    /**
     * @brief Calculate total chain length from skeleton
     */
    void calculate_length(const Skeleton& skeleton);
};

/**
 * @brief Abstract base class for IK solvers
 */
class IKSolver {
public:
    virtual ~IKSolver() = default;

    /**
     * @brief Solve IK for a chain to reach target position
     *
     * @param skeleton The skeleton containing the bones
     * @param chain The IK chain to solve
     * @param target Target position in model space
     * @param bone_transforms Current bone transforms (will be modified)
     */
    virtual void solve(Skeleton& skeleton, const IKChain& chain, const glm::vec3& target,
                       std::vector<glm::mat4>& bone_transforms) = 0;
};

/**
 * @brief Two-Bone IK solver
 *
 * Perfect for arms (shoulder-elbow-hand) and legs (hip-knee-foot).
 * Uses law of cosines for exact analytical solution.
 */
class TwoBoneIK : public IKSolver {
public:
    /**
     * @brief Pole vector for controlling bend direction (e.g., elbow direction)
     */
    glm::vec3 pole_vector{0.0f, 0.0f, -1.0f};

    void solve(Skeleton& skeleton, const IKChain& chain, const glm::vec3& target,
               std::vector<glm::mat4>& bone_transforms) override;

private:
    /**
     * @brief Calculate angle using law of cosines
     */
    [[nodiscard]] static float law_of_cosines(float a, float b, float c);
};

/**
 * @brief FABRIK (Forward And Backward Reaching Inverse Kinematics) solver
 *
 * Iterative solver for chains of any length. Good for spines, tails, tentacles.
 */
class FABRIKSolver : public IKSolver {
public:
    u32 max_iterations{10};
    float tolerance{0.001f};

    void solve(Skeleton& skeleton, const IKChain& chain, const glm::vec3& target,
               std::vector<glm::mat4>& bone_transforms) override;

private:
    // Temporary storage for joint positions during solving
    std::vector<glm::vec3> m_positions;
    std::vector<float> m_lengths;

    void forward_reach(const glm::vec3& target);
    void backward_reach(const glm::vec3& root);
};

/**
 * @brief Foot IK data for ground placement
 */
struct FootIKData {
    i32 hip_bone_id{-1};
    i32 knee_bone_id{-1};
    i32 foot_bone_id{-1};

    glm::vec3 target_position{0.0f};
    glm::vec3 pole_vector{0.0f, 0.0f, 1.0f}; // Knee bend direction

    float ground_offset{0.0f}; // Offset from ground hit point
    bool grounded{false};
};

} // namespace hz
