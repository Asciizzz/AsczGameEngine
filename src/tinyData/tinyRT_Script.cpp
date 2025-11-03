#include "tinyData/tinyRT_Script.hpp"
#include "tinyData/tinyRT_Scene.hpp"

// SDL for input detection lmao
#include "SDL2/SDL.h"

#define GLM_ENABLE_EXPERIMENTAL

// GLM for matrix decomposition
#include <glm/gtx/matrix_decompose.hpp>

using namespace tinyRT;

void Script::haveFun(Scene* scene, tinyHandle nodeHandle, float dTime) {
    const Scene::NComp comps = scene->nComp(nodeHandle);

    // Use arrow keys to move the object in the OXZ plane
    const Uint8* state = SDL_GetKeyboardState(NULL);

    bool k_up = state[SDL_SCANCODE_UP];
    bool k_down = state[SDL_SCANCODE_DOWN];
    bool k_left = state[SDL_SCANCODE_LEFT];
    bool k_right = state[SDL_SCANCODE_RIGHT];

    bool k_shift = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
    float moveSpeed = k_shift ? 3.0f : 1.0f; // units per second

    int vz = k_up - k_down;
    int vx = k_left - k_right;

    if (comps.trfm3D) {
        // Calculate movement direction
        glm::vec3 moveDir = glm::vec3(static_cast<float>(vx), 0.0f, static_cast<float>(vz));
        
        // Check if there's any movement input
        bool isMoving = (vx != 0) || (vz != 0);
        
        if (isMoving) {
            // Normalize the direction for diagonal movement
            moveDir = glm::normalize(moveDir);
            
            // Calculate target orientation (rotation around Y axis)
            // atan2 gives us the angle in radians from the X and Z components
            float targetYaw = std::atan2(moveDir.x, moveDir.z);
            
            // Decompose the current transform to preserve position and scale
            glm::vec3 currentPos;
            glm::quat currentRot;
            glm::vec3 currentScale;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(comps.trfm3D->local, currentScale, currentRot, currentPos, skew, perspective);
            
            // Create rotation quaternion from the yaw angle (rotation around Y axis)
            glm::quat targetRot = glm::angleAxis(targetYaw, glm::vec3(0.0f, 1.0f, 0.0f));
            
            // Apply translation
            currentPos += moveDir * moveSpeed * dTime;
            
            // Reconstruct the transform matrix with new rotation and position
            comps.trfm3D->local = glm::translate(glm::mat4(1.0f), currentPos) 
                                * glm::mat4_cast(targetRot) 
                                * glm::scale(glm::mat4(1.0f), currentScale);
        }
    }

    if (comps.anim3D) {
        auto* anim3D = comps.anim3D;

        // comps.anim3D->update(dTime);

        std::string walkLoop = "Walk_Loop";
        std::string idleLoop = "Idle_Loop";

        tinyHandle walkHandle = anim3D->getHandle(walkLoop);
        tinyHandle idleHandle = anim3D->getHandle(idleLoop);
        tinyHandle curHandle = anim3D->curHandle();

        bool isMoving = (vx != 0) || (vz != 0);

        anim3D->setSpeed(isMoving ? moveSpeed : 1.0f);

        // Only reset in the case of a change
        if (isMoving) {
            anim3D->play(walkHandle, (curHandle != walkHandle));
        } else {
            anim3D->play(idleHandle, (curHandle != idleHandle));
        }
    }
}