#include "components.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace hz {

glm::mat4 TransformComponent::get_transform() const {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);

    // Rotation ZYX order is standard
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0, 0, 1));
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1, 0, 0));

    model = glm::scale(model, scale);
    return model;
}

} // namespace hz
