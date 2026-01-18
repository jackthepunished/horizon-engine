#include "animation_blender.hpp"

#include "engine/core/log.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace hz {

namespace {

// Decompose a matrix into translation, rotation, scale
void decompose_transform(const glm::mat4& mat, glm::vec3& pos, glm::quat& rot, glm::vec3& scale) {
    pos = glm::vec3(mat[3]);

    scale.x = glm::length(glm::vec3(mat[0]));
    scale.y = glm::length(glm::vec3(mat[1]));
    scale.z = glm::length(glm::vec3(mat[2]));

    glm::mat3 rot_mat(glm::vec3(mat[0]) / scale.x, glm::vec3(mat[1]) / scale.y,
                      glm::vec3(mat[2]) / scale.z);
    rot = glm::quat_cast(rot_mat);
}

// Compose a matrix from translation, rotation, scale
glm::mat4 compose_transform(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale) {
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), pos);
    glm::mat4 rotation = glm::toMat4(rot);
    glm::mat4 scaling = glm::scale(glm::mat4(1.0f), scale);
    return translation * rotation * scaling;
}

// Blend two transforms
glm::mat4 blend_transforms(const glm::mat4& a, const glm::mat4& b, float t) {
    glm::vec3 pos_a, pos_b, scale_a, scale_b;
    glm::quat rot_a, rot_b;

    decompose_transform(a, pos_a, rot_a, scale_a);
    decompose_transform(b, pos_b, rot_b, scale_b);

    glm::vec3 pos = glm::mix(pos_a, pos_b, t);
    glm::quat rot = glm::slerp(rot_a, rot_b, t);
    glm::vec3 scale = glm::mix(scale_a, scale_b, t);

    return compose_transform(pos, rot, scale);
}

} // namespace

// ============================================================================
// AnimationCrossFade
// ============================================================================

void AnimationCrossFade::blend(const Skeleton& skeleton, const AnimationClip& from,
                               const AnimationClip& to, float time_from, float time_to,
                               float blend_factor, std::vector<glm::mat4>& output) {
    // Calculate transforms for both animations
    skeleton.calculate_bone_transforms(from, time_from, m_from_transforms);
    skeleton.calculate_bone_transforms(to, time_to, m_to_transforms);

    // Resize output
    output.resize(skeleton.bone_count());

    // Blend each bone
    for (u32 i = 0; i < skeleton.bone_count(); ++i) {
        output[i] = blend_transforms(m_from_transforms[i], m_to_transforms[i], blend_factor);
    }
}

// ============================================================================
// BlendTree1D
// ============================================================================

void BlendTree1D::add_clip(std::shared_ptr<AnimationClip> clip, float threshold) {
    BlendTreeNode node;
    node.clip = std::move(clip);
    node.threshold = threshold;
    node.current_time = 0.0f;

    m_nodes.push_back(std::move(node));

    // Sort by threshold
    std::sort(m_nodes.begin(), m_nodes.end(),
              [](const auto& a, const auto& b) { return a.threshold < b.threshold; });
}

void BlendTree1D::update(const Skeleton& skeleton, float parameter, float dt,
                         std::vector<glm::mat4>& output) {
    if (m_nodes.empty()) {
        return;
    }

    // Find the two nodes to blend between
    size_t lower_idx = 0;
    size_t upper_idx = 0;

    for (size_t i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes[i].threshold <= parameter) {
            lower_idx = i;
        }
        if (m_nodes[i].threshold >= parameter && upper_idx == 0) {
            upper_idx = i;
            break;
        }
    }

    if (upper_idx < lower_idx) {
        upper_idx = lower_idx;
    }

    // Update animation times
    for (auto& node : m_nodes) {
        if (node.clip) {
            node.current_time += dt * node.clip->ticks_per_second;
            if (node.current_time >= node.clip->duration) {
                node.current_time = fmod(node.current_time, node.clip->duration);
            }
        }
    }

    auto& lower_node = m_nodes[lower_idx];
    auto& upper_node = m_nodes[upper_idx];

    // Calculate blend factor
    float blend_factor = 0.0f;
    if (upper_idx != lower_idx && upper_node.threshold != lower_node.threshold) {
        blend_factor =
            (parameter - lower_node.threshold) / (upper_node.threshold - lower_node.threshold);
        blend_factor = std::clamp(blend_factor, 0.0f, 1.0f);
    }

    // Get transforms for both
    if (lower_node.clip) {
        skeleton.calculate_bone_transforms(*lower_node.clip, lower_node.current_time,
                                           m_temp_transforms_a);
    }

    if (blend_factor > 0.001f && upper_node.clip && upper_idx != lower_idx) {
        skeleton.calculate_bone_transforms(*upper_node.clip, upper_node.current_time,
                                           m_temp_transforms_b);

        // Blend
        output.resize(skeleton.bone_count());
        for (u32 i = 0; i < skeleton.bone_count(); ++i) {
            output[i] =
                blend_transforms(m_temp_transforms_a[i], m_temp_transforms_b[i], blend_factor);
        }
    } else {
        output = m_temp_transforms_a;
    }
}

// ============================================================================
// LayeredBlend
// ============================================================================

void LayeredBlend::blend(const Skeleton& skeleton, const std::vector<glm::mat4>& base,
                         const AnimationClip& overlay, float overlay_time,
                         const std::vector<i32>& overlay_bones, float weight,
                         std::vector<glm::mat4>& output) {
    // Calculate overlay transforms
    skeleton.calculate_bone_transforms(overlay, overlay_time, m_overlay_transforms);

    // Build affected bones mask
    m_affected_bones.resize(skeleton.bone_count(), false);
    std::fill(m_affected_bones.begin(), m_affected_bones.end(), false);

    // Mark overlay bones and their children
    for (i32 bone_id : overlay_bones) {
        if (bone_id >= 0 && static_cast<u32>(bone_id) < skeleton.bone_count()) {
            m_affected_bones[static_cast<size_t>(bone_id)] = true;

            // Mark children (simple approach - assumes bones are ordered parent-first)
            for (u32 i = static_cast<u32>(bone_id) + 1; i < skeleton.bone_count(); ++i) {
                const Bone* bone = skeleton.get_bone(static_cast<i32>(i));
                if (bone && m_affected_bones[static_cast<size_t>(bone->parent_id)]) {
                    m_affected_bones[i] = true;
                }
            }
        }
    }

    // Copy base or blend
    output = base;
    for (u32 i = 0; i < skeleton.bone_count(); ++i) {
        if (m_affected_bones[i]) {
            output[i] = blend_transforms(base[i], m_overlay_transforms[i], weight);
        }
    }
}

// ============================================================================
// AnimationStateMachine
// ============================================================================

void AnimationStateMachine::add_state(const std::string& name, std::shared_ptr<AnimationClip> clip,
                                      bool loop, float speed) {
    AnimationState state;
    state.name = name;
    state.clip = std::move(clip);
    state.loop = loop;
    state.speed = speed;
    state.current_time = 0.0f;

    m_states[name] = std::move(state);

    // Set as current if first state
    if (m_current_state.empty()) {
        m_current_state = name;
    }
}

void AnimationStateMachine::add_transition(const std::string& from, const std::string& to,
                                           float duration, std::function<bool()> condition) {
    AnimationTransition trans;
    trans.from_state = from;
    trans.to_state = to;
    trans.duration = duration;
    trans.condition = std::move(condition);
    m_transitions.push_back(std::move(trans));
}

void AnimationStateMachine::set_state(const std::string& name) {
    if (m_states.find(name) != m_states.end()) {
        m_current_state = name;
        m_states[name].current_time = 0.0f;
        m_transitioning = false;
    }
}

void AnimationStateMachine::transition_to(const std::string& name, float duration) {
    if (m_states.find(name) == m_states.end() || name == m_current_state) {
        return;
    }

    m_next_state = name;
    m_transitioning = true;
    m_transition_time = 0.0f;

    // Use provided duration or find from transitions
    if (duration >= 0.0f) {
        m_transition_duration = duration;
    } else {
        m_transition_duration = 0.2f; // Default
        for (const auto& trans : m_transitions) {
            if (trans.from_state == m_current_state && trans.to_state == name) {
                m_transition_duration = trans.duration;
                break;
            }
        }
    }
}

void AnimationStateMachine::update(const Skeleton& skeleton, float dt,
                                   std::vector<glm::mat4>& output) {
    if (m_current_state.empty()) {
        return;
    }

    auto current_it = m_states.find(m_current_state);
    if (current_it == m_states.end()) {
        return;
    }

    auto& current = current_it->second;

    // Check automatic transitions
    if (!m_transitioning) {
        for (const auto& trans : m_transitions) {
            if (trans.from_state == m_current_state && trans.condition && trans.condition()) {
                transition_to(trans.to_state, trans.duration);
                break;
            }
        }
    }

    // Update current animation
    if (current.clip) {
        current.current_time += dt * current.speed * current.clip->ticks_per_second;
        if (current.current_time >= current.clip->duration) {
            if (current.loop) {
                current.current_time = fmod(current.current_time, current.clip->duration);
            } else {
                current.current_time = current.clip->duration;
            }
        }
    }

    if (m_transitioning) {
        auto next_it = m_states.find(m_next_state);
        if (next_it != m_states.end()) {
            auto& next = next_it->second;

            // Update next animation time
            if (next.clip) {
                next.current_time += dt * next.speed * next.clip->ticks_per_second;
                if (next.current_time >= next.clip->duration) {
                    if (next.loop) {
                        next.current_time = fmod(next.current_time, next.clip->duration);
                    } else {
                        next.current_time = next.clip->duration;
                    }
                }
            }

            m_transition_time += dt;
            float blend = std::clamp(m_transition_time / m_transition_duration, 0.0f, 1.0f);

            if (current.clip && next.clip) {
                m_cross_fader.blend(skeleton, *current.clip, *next.clip, current.current_time,
                                    next.current_time, blend, output);
            }

            // Transition complete
            if (m_transition_time >= m_transition_duration) {
                m_current_state = m_next_state;
                m_next_state.clear();
                m_transitioning = false;
            }
        }
    } else {
        // Just calculate current state
        if (current.clip) {
            skeleton.calculate_bone_transforms(*current.clip, current.current_time, output);
        }
    }
}

} // namespace hz
