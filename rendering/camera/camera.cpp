#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(glm::vec3 vPosition) : position(vPosition)
{
    Update();
}

Camera::Camera(float x, float y, float z)
{
    this->position = glm::vec3(x, y, z);
}

Camera::~Camera()
{
    /* do nothing... */
}

void Camera::Update()
{
    viewMatrix = glm::lookAt(position, position + direction, up);
    projectionMatrix = glm::perspective(glm::radians(fov), aspectRatio, near, far);
}

glm::mat4 Camera::GetViewMatrix() const
{
    return viewMatrix;
}

glm::mat4 Camera::GetProjectionMatrix() const
{
    return projectionMatrix;
}

glm::vec3 Camera::GetPosition() const
{
    return position;
}

void Camera::SetPosition(glm::vec3 vPosition)
{
    this->position = vPosition;
}
