#ifndef CAMERA_H_
#define CAMERA_H_

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class Camera {
public:
    Camera(glm::vec3 vPosition);
    Camera(float x, float y, float z);
   ~Camera();

    void Update();

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetProjectionMatrix() const;

    glm::vec3 GetPosition() const;

    void SetPosition(glm::vec3 vPosition);

private:
    glm::vec3 position  = { 0.0f, 0.0f, 0.0f };
    glm::vec3 direction = { 0.0f, 0.0f, 1.0f };
    glm::vec3 up        = { 0.0f, 1.0f, 0.0f };

    float fov           = 45.0f;
    float aspectRatio   = 16.0f / 9.0f;
    float near          = 0.1f;
    float far           = 1000.0f;

    glm::mat4 viewMatrix;
    glm::mat4 projectionMatrix;

};

#endif /* CAMERA_H_ */
