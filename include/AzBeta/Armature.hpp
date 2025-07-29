#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "Az3D/Model.hpp"
#include "AzVulk/Buffer.hpp"

namespace AzBeta {


    struct Bone {
        ~Bone() {
            for (Bone* child : children) {
                delete child;
            }
        }

        Az3D::Model model = {2, 0};

        // Rel: relative to parent bone
        Az3D::Transform relTrform;

        // Local: local space position (final world position will be determined by the armature struct)
        Az3D::Transform localTrform;

        std::vector<Bone*> children; // Child bones
    };


    // Armature class to represent a skeletal structure for animations
    class Armature {
    public:
        Bone* root;

        Az3D::Transform trform;

        ~Armature() { delete root; }

        Armature() {
            root = new Bone();

            Bone* spine = new Bone(); root->children.push_back(spine);
            spine->relTrform.pos = {0.0f, 1.0f, 0.0f};

            Bone* pelvis = new Bone(); root->children.push_back(pelvis);
            pelvis->relTrform.pos = {0.0f, -1.0f, 0.0f};
        };

        // Recursive update of the bone transforms
        void updateTransforms(  std::vector<AzVulk::ModelInstance>& instances,
                                std::vector<Az3D::Model>& models,
                                Bone* bone, const Az3D::Transform& parentTransform) {
            // Calculate the local transform
            bone->localTrform.pos = parentTransform.pos + bone->relTrform.pos;
            bone->localTrform.rot = parentTransform.rot * bone->relTrform.rot;
            bone->localTrform.scl = 0.3f;

            // Update the model transform
            bone->model.trform = bone->localTrform;

            AzVulk::ModelInstance instance{};
            instance.modelMatrix = bone->localTrform.modelMatrix();
            instances.push_back(instance);

            models.push_back(bone->model);

            // Recursively update children
            for (Bone* child : bone->children) {
                updateTransforms(instances, models, child, bone->localTrform);
            }
        }
    };

}; // namespace AzBeta