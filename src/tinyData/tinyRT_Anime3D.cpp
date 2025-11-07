#include "tinyData/tinyRT_Anime3D.hpp"
#include "tinyData/tinyRT_Scene.hpp"
#include "tinyData/tinyRT_Skeleton3D.hpp"
#include "tinyData/tinyRT_MeshRender3D.hpp"

using namespace tinyRT;

// ============================================================
// HELPER UTILITIES
// ============================================================

// Generate unique name by appending suffix if name already exists in container
template<typename ContainerType>
std::string generateUniqueName(const std::string& baseName, const ContainerType& existingNames, const std::string& defaultName = "Item") {
    std::string name = baseName.empty() ? defaultName : baseName;
    std::string uniqueName = name;
    int suffix = 1;
    
    while (existingNames.find(uniqueName) != existingNames.end()) {
        uniqueName = name + "_" + std::to_string(suffix++);
    }
    
    return uniqueName;
}

// ============================================================
// SAMPLER IMPLEMENTATION
// ============================================================

glm::vec4 Anime3D::Sampler::firstKeyframe() const {
    if (values.empty()) return glm::vec4(0.0f);
    return (interp == Interp::CubicSpline && values.size() >= 3) ? values[1] : values[0];
}

glm::vec4 Anime3D::Sampler::lastKeyframe() const {
    if (values.empty()) return glm::vec4(0.0f);
    return (interp == Interp::CubicSpline && values.size() >= 3) ? values[values.size() - 2] : values.back();
}

glm::vec4 Anime3D::Sampler::evaluate(float time) const {
    if (times.empty() || values.empty()) return glm::vec4(0.0f);

    const float tMin = times.front();
    const float tMax = times.back();

    if (time <= tMin) return firstKeyframe();
    if (time >= tMax) return lastKeyframe();

    // Binary search for keyframe interval
    size_t left = 0;
    size_t right = times.size() - 1;
    while (left < right - 1) {
        size_t mid = left + (right - left) / 2;
        if (time < times[mid]) right = mid;
        else left = mid;
    }
    size_t index = left;

    float t0 = times[index];
    float t1 = times[index + 1];
    float dt = std::max(t1 - t0, 1e-6f);
    const float f = (time - t0) / dt;

    switch (interp) {
        case Interp::Linear: {
            const glm::vec4& v0 = values[index];
            const glm::vec4& v1 = values[index + 1];
            return glm::mix(v0, v1, f);
        }

        case Interp::Step: {
            return values[index];
        }

        case Interp::CubicSpline: {
            const size_t indx0 = index * 3;
            const size_t indx1 = (index + 1) * 3;

            if (indx1 + 2 >= values.size()) return values[indx0 + 1];

            const glm::vec4& v0   = values[indx0 + 1];
            const glm::vec4& out0 = values[indx0 + 2];
            const glm::vec4& in1  = values[indx1];
            const glm::vec4& v1   = values[indx1 + 1];

            float f2 = f * f;
            float f3 = f2 * f;

            float h00 = 2.0f * f3 - 3.0f * f2 + 1.0f;
            float h10 = f3 - 2.0f * f2 + f;
            float h01 = -2.0f * f3 + 3.0f * f2;
            float h11 = f3 - f2;

            glm::vec4 m0 = out0 * dt;
            glm::vec4 m1 = in1 * dt;

            return h00 * v0 + h10 * m0 + h01 * v1 + h11 * m1;
        }

        default:
            return glm::vec4(0.0f);
    }
}

// ============================================================
// CLIP IMPLEMENTATION
// ============================================================

Anime3D::Pose Anime3D::Clip::evaluatePose(float time) const {
    Pose pose;

    for (const auto& channel : channels) {
        if (channel.sampler >= samplers.size()) continue;
        
        const auto& sampler = samplers[channel.sampler];

        // Create target ID
        Pose::TargetID targetID;
        targetID.node = channel.node;
        targetID.index = channel.index;

        if (channel.target == Channel::Target::Node)
            targetID.type = Pose::TargetID::Type::Node;
        else if (channel.target == Channel::Target::Bone)
            targetID.type = Pose::TargetID::Type::Bone;
        else if (channel.target == Channel::Target::Morph)
            targetID.type = Pose::TargetID::Type::Morph;

        Pose::Transform& transform = pose.transforms[targetID];

        switch (channel.path) {
            case Channel::Path::T: { // Translation
                glm::vec4 v = sampler.evaluate(time);
                transform.translation = glm::vec3(v.x, v.y, v.z);
                break;
            }

            case Channel::Path::S: { // Scale
                glm::vec4 v = sampler.evaluate(time);
                transform.scale = glm::vec3(v.x, v.y, v.z);
                break;
            }

            case Channel::Path::R: { // Rotation (quaternion slerp)
                if (sampler.times.empty() || sampler.values.empty()) break;

                float tMin = sampler.times.front();
                float tMax = sampler.times.back();

                if (time <= tMin) {
                    glm::vec4 v = sampler.firstKeyframe();
                    transform.rotation = glm::normalize(glm::quat(v.w, v.x, v.y, v.z));
                    break;
                }
                if (time >= tMax) {
                    glm::vec4 v = sampler.lastKeyframe();
                    transform.rotation = glm::normalize(glm::quat(v.w, v.x, v.y, v.z));
                    break;
                }

                // Binary search
                size_t left = 0;
                size_t right = sampler.times.size() - 1;
                while (left < right - 1) {
                    size_t mid = left + (right - left) / 2;
                    if (time < sampler.times[mid]) right = mid;
                    else left = mid;
                }
                size_t index = left;

                float t0 = sampler.times[index];
                float t1 = sampler.times[index + 1];
                float dt = std::max(t1 - t0, 1e-6f);
                float f = (time - t0) / dt;

                const glm::vec4& vv0 = sampler.values[index];
                const glm::vec4& vv1 = sampler.values[index + 1];

                glm::quat q0 = glm::normalize(glm::quat(vv0.w, vv0.x, vv0.y, vv0.z));
                glm::quat q1 = glm::normalize(glm::quat(vv1.w, vv1.x, vv1.y, vv1.z));

                if (sampler.interp == Sampler::Interp::Step) {
                    transform.rotation = q0;
                } else {
                    transform.rotation = glm::normalize(glm::slerp(q0, q1, f));
                }
                break;
            }

            case Channel::Path::W: { // Morph weight
                glm::vec4 v = sampler.evaluate(time);
                transform.morphWeight = glm::clamp(v.x, 0.0f, 1.0f);
                break;
            }
        }
    }

    return pose;
}

// ============================================================
// POSE IMPLEMENTATION
// ============================================================

Anime3D::Pose Anime3D::Pose::blend(const Pose& a, const Pose& b, float t) {
    Pose result;
    t = glm::clamp(t, 0.0f, 1.0f);

    // Blend all targets from both poses
    auto blendTarget = [&](const TargetID& targetID, const Transform* ta, const Transform* tb) {
        Transform& resultTransform = result.transforms[targetID];

        if (ta && tb) {
            // Both poses have this target
            resultTransform.translation = glm::mix(ta->translation, tb->translation, t);
            resultTransform.rotation = glm::normalize(glm::slerp(ta->rotation, tb->rotation, t));
            resultTransform.scale = glm::mix(ta->scale, tb->scale, t);
            resultTransform.morphWeight = glm::mix(ta->morphWeight, tb->morphWeight, t);
        } else if (ta) {
            // Only pose A has this target
            resultTransform = *ta;
        } else if (tb) {
            // Only pose B has this target
            resultTransform = *tb;
        }
    };

    // Process all targets from pose A
    for (const auto& [targetID, transform] : a.transforms) {
        auto itB = b.transforms.find(targetID);
        blendTarget(targetID, &transform, itB != b.transforms.end() ? &itB->second : nullptr);
    }

    // Process targets only in pose B
    for (const auto& [targetID, transform] : b.transforms) {
        if (a.transforms.find(targetID) == a.transforms.end()) {
            blendTarget(targetID, nullptr, &transform);
        }
    }

    return result;
}

Anime3D::Pose Anime3D::Pose::blendAdditive(const Pose& base, const Pose& additive, const Pose& reference, float weight) {
    Pose result = base;
    weight = glm::clamp(weight, 0.0f, 1.0f);

    if (weight <= 0.0f) return result;

    for (const auto& [targetID, additiveTransform] : additive.transforms) {
        auto itRef = reference.transforms.find(targetID);
        if (itRef == reference.transforms.end()) continue;

        const Transform& refTransform = itRef->second;

        // Compute delta
        glm::vec3 deltaT = additiveTransform.translation - refTransform.translation;
        glm::quat deltaR = additiveTransform.rotation * glm::inverse(refTransform.rotation);
        glm::vec3 deltaS = additiveTransform.scale / refTransform.scale;
        float deltaW = additiveTransform.morphWeight - refTransform.morphWeight;

        // Apply delta to base
        Transform& resultTransform = result.transforms[targetID];
        resultTransform.translation += deltaT * weight;
        resultTransform.rotation = glm::normalize(resultTransform.rotation * glm::slerp(glm::quat(1, 0, 0, 0), deltaR, weight));
        resultTransform.scale *= glm::mix(glm::vec3(1.0f), deltaS, weight);
        resultTransform.morphWeight += deltaW * weight;
    }

    return result;
}

void Anime3D::Pose::applyToScene(Scene* scene) const {
    if (!scene) return;

    for (const auto& [targetID, transform] : transforms) {
        switch (targetID.type) {
            case TargetID::Type::Node: {
                tinyNodeRT::TRFM3D* nodeTransform = scene->rtComp<tinyNodeRT::TRFM3D>(targetID.node);
                if (nodeTransform) {
                    glm::mat4 mat = glm::translate(glm::mat4(1.0f), transform.translation) *
                                    glm::mat4_cast(transform.rotation) *
                                    glm::scale(glm::mat4(1.0f), transform.scale);
                    nodeTransform->set(mat);
                }
                break;
            }

            case TargetID::Type::Bone: {
                tinyRT_SKEL3D* skeleton = scene->rtComp<tinyNodeRT::SKEL3D>(targetID.node);
                if (skeleton && skeleton->boneValid(targetID.index)) {
                    glm::mat4 mat = glm::translate(glm::mat4(1.0f), transform.translation) *
                                    glm::mat4_cast(transform.rotation) *
                                    glm::scale(glm::mat4(1.0f), transform.scale);
                    skeleton->setLocalPose(targetID.index, mat);
                }
                break;
            }

            case TargetID::Type::Morph: {
                tinyRT_MESHRD* mesh = scene->rtComp<tinyNodeRT::MESHRD>(targetID.node);
                if (mesh) {
                    mesh->setMrphWeight(targetID.index, transform.morphWeight);
                }
                break;
            }
        }
    }
}

// ============================================================
// STATE MACHINE IMPLEMENTATION
// ============================================================

tinyHandle Anime3D::StateMachine::addState(State&& state) {
    state.name = generateUniqueName(state.name, nameToHandle_, "State");
    std::string stateName = state.name; // Cache before move

    tinyHandle handle = states_.add(std::move(state));
    nameToHandle_[stateName] = handle;

    return handle;
}

tinyHandle Anime3D::StateMachine::addState(const std::string& name, const tinyHandle& clipHandle) {
    State state;
    state.name = name;
    state.clipHandle = clipHandle;
    return addState(std::move(state));
}

void Anime3D::StateMachine::removeState(const tinyHandle& handle) {
    // Remove from name map
    for (auto it = nameToHandle_.begin(); it != nameToHandle_.end(); ) {
        if (it->second == handle) {
            it = nameToHandle_.erase(it);
        } else {
            ++it;
        }
    }
    
    // Remove transitions involving this state
    transitions_.erase(
        std::remove_if(transitions_.begin(), transitions_.end(),
            [&handle](const Transition& t) {
                return t.fromState == handle || t.toState == handle;
            }),
        transitions_.end()
    );
    
    // Clear current state if it's being removed
    if (currentState_ == handle) {
        currentState_ = tinyHandle();
        currentTime_ = 0.0f;
        activeTransition_ = nullptr;
    }
    
    // Remove from pool
    states_.remove(handle);
}

void Anime3D::StateMachine::addTransition(Transition&& transition) {
    transitions_.push_back(std::move(transition));
}

void Anime3D::StateMachine::setCurrentState(const tinyHandle& stateHandle, bool resetTime) {
    const State* state = states_.get(stateHandle);
    if (!state) return;

    currentState_ = stateHandle;
    if (resetTime) {
        currentTime_ = state->timeOffset;
    }
    activeTransition_ = nullptr;
}

Anime3D::State* Anime3D::StateMachine::getState(const tinyHandle& handle) {
    return states_.get(handle);
}

const Anime3D::State* Anime3D::StateMachine::getState(const tinyHandle& handle) const {
    return states_.get(handle);
}

Anime3D::State* Anime3D::StateMachine::getState(const std::string& name) {
    auto it = nameToHandle_.find(name);
    if (it != nameToHandle_.end()) {
        return states_.get(it->second);
    }
    return nullptr;
}

const Anime3D::State* Anime3D::StateMachine::getState(const std::string& name) const {
    return const_cast<StateMachine*>(this)->getState(name);
}

tinyHandle Anime3D::StateMachine::getStateHandle(const std::string& name) const {
    auto it = nameToHandle_.find(name);
    if (it == nameToHandle_.end()) return tinyHandle();
    return it->second;
}

Anime3D::Pose Anime3D::StateMachine::evaluate(Anime3D* animData, float deltaTime) {
    if (!animData || !isPlaying_) return Pose();

    // Handle transitions
    if (activeTransition_) {
        transitionProgress_ += deltaTime;
        float t = glm::clamp(transitionProgress_ / activeTransition_->duration, 0.0f, 1.0f);

        const State* fromState = states_.get(activeTransition_->fromState);
        const State* toState = states_.get(activeTransition_->toState);

        if (!fromState || !toState) {
            activeTransition_ = nullptr;
            return Pose();
        }

        const Clip* fromClip = animData->getClip(fromState->clipHandle);
        const Clip* toClip = animData->getClip(toState->clipHandle);

        if (!fromClip || !toClip) {
            activeTransition_ = nullptr;
            return Pose();
        }

        Pose fromPose, toPose;

        switch (activeTransition_->type) {
            case Transition::Type::Smooth: {
                // Continue advancing both animations
                fromPose = fromClip->evaluatePose(currentTime_);
                
                State* mutableToState = states_.get(activeTransition_->toState);
                float toTime = mutableToState->timeOffset;
                toTime += deltaTime * toState->speed;
                if (toClip->duration > 0.0f && toState->loop) {
                    toTime = fmod(toTime, toClip->duration);
                }
                mutableToState->timeOffset = toTime;
                
                toPose = toClip->evaluatePose(toTime);
                break;
            }

            case Transition::Type::Frozen: {
                // Hold source pose, blend to target
                fromPose = fromClip->evaluatePose(currentTime_);
                toPose = toClip->evaluatePose(toState->timeOffset);
                break;
            }

            case Transition::Type::Synchronized: {
                // Match normalized time
                float normalizedTime = fromClip->duration > 0.0f ? (currentTime_ / fromClip->duration) : 0.0f;
                float toTime = normalizedTime * toClip->duration;
                fromPose = fromClip->evaluatePose(currentTime_);
                toPose = toClip->evaluatePose(toTime);
                break;
            }
        }

        Pose blended = Pose::blend(fromPose, toPose, t);

        // Complete transition
        if (t >= 1.0f) {
            currentState_ = activeTransition_->toState;
            currentTime_ = states_.get(currentState_)->timeOffset;
            activeTransition_ = nullptr;
        }

        return blended;
    }

    // Normal state playback
    const State* state = states_.get(currentState_);
    if (!state) return Pose();

    const Clip* clip = animData->getClip(state->clipHandle);
    if (!clip || !clip->valid()) return Pose();

    // Update time
    currentTime_ += deltaTime * state->speed;

    if (clip->duration > 0.0f) {
        if (state->loop) {
            currentTime_ = fmod(currentTime_, clip->duration);
            if (currentTime_ < 0.0f) currentTime_ += clip->duration;
        } else {
            currentTime_ = glm::clamp(currentTime_, 0.0f, clip->duration);
        }
    }

    return clip->evaluatePose(currentTime_);
}

void Anime3D::StateMachine::evaluateTransitions() {
    if (activeTransition_) return; // Already transitioning

    const State* currentState = states_.get(currentState_);
    if (!currentState) return;

    for (auto& transition : transitions_) {
        if (transition.fromState != currentState_) continue;

        bool shouldTransition = false;

        // Check exit time
        if (transition.exitTime && currentState) {
            const Clip* clip = nullptr; // Would need access to animData here
            // This is a simplification - in real use, you'd pass clip duration
            shouldTransition = true;
        }

        // Check condition
        if (transition.condition && transition.condition()) {
            shouldTransition = true;
        }

        if (shouldTransition) {
            activeTransition_ = &transition;
            transitionProgress_ = 0.0f;
            nextState_ = transition.toState;
            break;
        }
    }
}

// ============================================================
// LAYER & CONTROLLER IMPLEMENTATION
// ============================================================

Anime3D::Layer& Anime3D::Controller::addLayer(const std::string& name) {
    Layer layer;
    layer.name = generateUniqueName(name, layerNameToIndex_, "Layer");
    
    size_t newIndex = layers_.size();
    layers_.push_back(std::move(layer));
    layerNameToIndex_[layers_.back().name] = newIndex;
    
    return layers_.back();
}

void Anime3D::Controller::removeLayer(const std::string& name) {
    layers_.erase(
        std::remove_if(layers_.begin(), layers_.end(),
            [&name](const Layer& layer) { return layer.name == name; }),
        layers_.end()
    );
    rebuildLayerNameMap();
}

void Anime3D::Controller::removeLayer(size_t index) {
    if (index < layers_.size()) {
        layers_.erase(layers_.begin() + index);
        rebuildLayerNameMap();
    }
}

void Anime3D::Controller::rebuildLayerNameMap() {
    layerNameToIndex_.clear();
    for (size_t i = 0; i < layers_.size(); ++i) {
        layerNameToIndex_[layers_[i].name] = i;
    }
}

void Anime3D::Controller::setLayerWeight(const std::string& name, float weight) {
    for (auto& layer : layers_) {
        if (layer.name == name) {
            layer.weight = glm::clamp(weight, 0.0f, 1.0f);
            return;
        }
    }
}

float Anime3D::Controller::getLayerWeight(const std::string& name) const {
    for (const auto& layer : layers_) {
        if (layer.name == name) {
            return layer.weight;
        }
    }
    return 0.0f;
}

Anime3D::Layer* Anime3D::Controller::getLayer(const std::string& name) {
    for (auto& layer : layers_) {
        if (layer.name == name) {
            return &layer;
        }
    }
    return nullptr;
}

const Anime3D::Layer* Anime3D::Controller::getLayer(const std::string& name) const {
    return const_cast<Controller*>(this)->getLayer(name);
}

Anime3D::Layer* Anime3D::Controller::getLayer(size_t index) {
    return (index < layers_.size()) ? &layers_[index] : nullptr;
}

const Anime3D::Layer* Anime3D::Controller::getLayer(size_t index) const {
    return (index < layers_.size()) ? &layers_[index] : nullptr;
}

Anime3D::StateMachine* Anime3D::Controller::getStateMachine(const std::string& layerName) {
    Layer* layer = getLayer(layerName);
    return layer ? &layer->stateMachine : nullptr;
}

const Anime3D::StateMachine* Anime3D::Controller::getStateMachine(const std::string& layerName) const {
    return const_cast<Controller*>(this)->getStateMachine(layerName);
}

void Anime3D::Controller::update(Anime3D* animData, Scene* scene, float deltaTime) {
    if (!animData || !scene) return;

    Pose finalPose = evaluateFinal(animData, deltaTime);
    finalPose.applyToScene(scene);
}

void Anime3D::Controller::applyPose(Scene* scene, const Pose& pose) {
    if (!scene) return;
    pose.applyToScene(scene);
}

Anime3D::Pose Anime3D::Controller::evaluateFinal(Anime3D* animData, float deltaTime) {
    if (layers_.empty()) return Pose();

    std::vector<Pose> layerPoses;
    layerPoses.reserve(layers_.size());

    for (auto& layer : layers_) {
        if (layer.weight <= 0.0f) continue;
        
        Pose pose = layer.stateMachine.evaluate(animData, deltaTime);
        layerPoses.push_back(std::move(pose));
    }

    return blendLayers(layerPoses);
}

Anime3D::Pose Anime3D::Controller::blendLayers(const std::vector<Pose>& layerPoses) {
    if (layerPoses.empty()) return Pose();
    if (layerPoses.size() == 1) return layerPoses[0];

    Pose result;

    for (size_t i = 0; i < layerPoses.size() && i < layers_.size(); ++i) {
        const Layer& layer = layers_[i];
        const Pose& layerPose = layerPoses[i];

        if (layer.weight <= 0.0f) continue;

        if (i == 0) {
            // First layer (base)
            for (const auto& [targetID, transform] : layerPose.transforms) {
                if (layer.mask.affects(targetID)) {
                    result.transforms[targetID] = transform;
                }
            }
        } else {
            // Subsequent layers
            if (layer.blendMode == Layer::BlendMode::Override) {
                for (const auto& [targetID, transform] : layerPose.transforms) {
                    if (layer.mask.affects(targetID)) {
                        auto it = result.transforms.find(targetID);
                        if (it != result.transforms.end()) {
                            // Blend with weight
                            Pose tempA, tempB;
                            tempA.transforms[targetID] = it->second;
                            tempB.transforms[targetID] = transform;
                            Pose blended = Pose::blend(tempA, tempB, layer.weight);
                            result.transforms[targetID] = blended.transforms[targetID];
                        } else {
                            result.transforms[targetID] = transform;
                        }
                    }
                }
            } else { // Additive
                result = Pose::blendAdditive(result, layerPose, layer.referencePose, layer.weight);
            }
        }
    }

    return result;
}

// ============================================================
// CLIP MANAGEMENT IMPLEMENTATION
// ============================================================

tinyHandle Anime3D::addClip(Clip&& clip) {
    if (!clip.valid()) return tinyHandle();

    clip.name = generateUniqueName(clip.name, clipNameToHandle_, "Clip");

    // Cache duration
    for (const auto& sampler : clip.samplers) {
        if (sampler.times.empty()) continue;
        clip.duration = std::max(clip.duration, sampler.times.back());
    }

    tinyHandle handle = clips_.add(std::move(clip));
    clipNameToHandle_[clips_.get(handle)->name] = handle;

    return handle;
}

Anime3D::Clip* Anime3D::getClip(const tinyHandle& handle) {
    return clips_.get(handle);
}

const Anime3D::Clip* Anime3D::getClip(const tinyHandle& handle) const {
    return clips_.get(handle);
}

Anime3D::Clip* Anime3D::getClip(const std::string& name) {
    auto it = clipNameToHandle_.find(name);
    if (it != clipNameToHandle_.end()) {
        return clips_.get(it->second);
    }
    return nullptr;
}

const Anime3D::Clip* Anime3D::getClip(const std::string& name) const {
    return const_cast<Anime3D*>(this)->getClip(name);
}

tinyHandle Anime3D::getClipHandle(const std::string& name) const {
    auto it = clipNameToHandle_.find(name);
    if (it == clipNameToHandle_.end()) return tinyHandle();
    return it->second;
}

float Anime3D::clipDuration(const tinyHandle& handle) const {
    const Clip* clip = clips_.get(handle);
    return clip ? clip->duration : 0.0f;
}

float Anime3D::clipDuration(const std::string& name) const {
    auto it = clipNameToHandle_.find(name);
    if (it == clipNameToHandle_.end()) return 0.0f;
    return clipDuration(it->second);
}