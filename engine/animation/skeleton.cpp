#include "skeleton.hpp"

#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace hz {

// ============================================================================
// Bone
// ============================================================================

glm::mat4 Bone::get_local_transform() const {
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 rot = glm::toMat4(rotation);
    glm::mat4 scl = glm::scale(glm::mat4(1.0f), scale);
    return translation * rot * scl;
}

// ============================================================================
// BoneAnimation
// ============================================================================

namespace {

template <typename T>
size_t find_key_index(const std::vector<Keyframe<T>>& keys, float time) {
    for (size_t i = 0; i < keys.size() - 1; ++i) {
        if (time < keys[i + 1].time) {
            return i;
        }
    }
    return keys.size() - 1;
}

float get_scale_factor(float prev_time, float next_time, float time) {
    float mid_way = time - prev_time;
    float frame_diff = next_time - prev_time;
    if (frame_diff <= 0.0f)
        return 0.0f;
    return mid_way / frame_diff;
}

} // namespace

glm::vec3 BoneAnimation::interpolate_position(float time) const {
    if (position_keys.size() == 1) {
        return position_keys[0].value;
    }

    size_t p0_idx = find_key_index(position_keys, time);
    size_t p1_idx = p0_idx + 1;
    if (p1_idx >= position_keys.size()) {
        return position_keys[p0_idx].value;
    }

    float factor = get_scale_factor(position_keys[p0_idx].time, position_keys[p1_idx].time, time);
    return glm::mix(position_keys[p0_idx].value, position_keys[p1_idx].value, factor);
}

glm::quat BoneAnimation::interpolate_rotation(float time) const {
    if (rotation_keys.size() == 1) {
        return glm::normalize(rotation_keys[0].value);
    }

    size_t r0_idx = find_key_index(rotation_keys, time);
    size_t r1_idx = r0_idx + 1;
    if (r1_idx >= rotation_keys.size()) {
        return glm::normalize(rotation_keys[r0_idx].value);
    }

    float factor = get_scale_factor(rotation_keys[r0_idx].time, rotation_keys[r1_idx].time, time);
    return glm::normalize(
        glm::slerp(rotation_keys[r0_idx].value, rotation_keys[r1_idx].value, factor));
}

glm::vec3 BoneAnimation::interpolate_scale(float time) const {
    if (scale_keys.size() == 1) {
        return scale_keys[0].value;
    }

    size_t s0_idx = find_key_index(scale_keys, time);
    size_t s1_idx = s0_idx + 1;
    if (s1_idx >= scale_keys.size()) {
        return scale_keys[s0_idx].value;
    }

    float factor = get_scale_factor(scale_keys[s0_idx].time, scale_keys[s1_idx].time, time);
    return glm::mix(scale_keys[s0_idx].value, scale_keys[s1_idx].value, factor);
}

// ============================================================================
// AnimationClip
// ============================================================================

const BoneAnimation* AnimationClip::get_channel(const std::string& bone_name) const {
    auto it = std::find_if(channels.begin(), channels.end(),
                           [&](const BoneAnimation& ch) { return ch.bone_name == bone_name; });
    return it != channels.end() ? &(*it) : nullptr;
}

// ============================================================================
// Skeleton
// ============================================================================

i32 Skeleton::add_bone(const std::string& name, i32 parent_id, const glm::mat4& offset) {
    i32 id = static_cast<i32>(m_bones.size());

    Bone bone;
    bone.name = name;
    bone.id = id;
    bone.parent_id = parent_id;
    bone.offset_matrix = offset;

    m_bones.push_back(std::move(bone));
    m_bone_name_to_id[name] = id;

    return id;
}

Bone* Skeleton::get_bone(i32 id) {
    if (id < 0 || static_cast<size_t>(id) >= m_bones.size())
        return nullptr;
    return &m_bones[static_cast<size_t>(id)];
}

const Bone* Skeleton::get_bone(i32 id) const {
    if (id < 0 || static_cast<size_t>(id) >= m_bones.size())
        return nullptr;
    return &m_bones[static_cast<size_t>(id)];
}

i32 Skeleton::get_bone_id(const std::string& name) const {
    auto it = m_bone_name_to_id.find(name);
    return it != m_bone_name_to_id.end() ? it->second : -1;
}

void Skeleton::calculate_bone_transforms(const AnimationClip& clip, float time,
                                         std::vector<glm::mat4>& out_transforms) const {
    out_transforms.resize(m_bones.size(), glm::mat4(1.0f));

    // Find root bones (parent_id == -1) and start recursion
    for (const auto& bone : m_bones) {
        if (bone.parent_id == -1) {
            calculate_bone_transform_recursive(bone.id, clip, time, glm::mat4(1.0f),
                                               out_transforms);
        }
    }
}

void Skeleton::calculate_bone_transform_recursive(i32 bone_id, const AnimationClip& clip,
                                                  float time, const glm::mat4& parent_transform,
                                                  std::vector<glm::mat4>& out_transforms) const {
    const Bone* bone = get_bone(bone_id);
    if (!bone)
        return;

    glm::mat4 local_transform = bone->get_local_transform();

    // Check if this bone has animation
    if (const BoneAnimation* channel = clip.get_channel(bone->name)) {
        glm::vec3 pos = channel->interpolate_position(time);
        glm::quat rot = channel->interpolate_rotation(time);
        glm::vec3 scl = channel->interpolate_scale(time);

        glm::mat4 translation = glm::translate(glm::mat4(1.0f), pos);
        glm::mat4 rotation = glm::toMat4(rot);
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), scl);

        local_transform = translation * rotation * scale;
    }

    glm::mat4 global_transform = parent_transform * local_transform;

    // Final transform = global_inverse * global_transform * offset
    out_transforms[static_cast<size_t>(bone_id)] =
        m_global_inverse_transform * global_transform * bone->offset_matrix;

    // Recurse to children
    for (i32 child_id : bone->children) {
        calculate_bone_transform_recursive(child_id, clip, time, global_transform, out_transforms);
    }
}

} // namespace hz
