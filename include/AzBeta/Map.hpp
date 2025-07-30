#pragma once

#include "Az3D/Az3D.hpp"

#include <queue>

namespace AzBeta {

    struct BVHNode {
        glm::vec3 min;
        glm::vec3 max;

        /* Note:
        -1 children means leaf
        Leaf range is [l_leaf, r_leaf)
        */
        int l_child = -1;
        int r_child = -1;
        size_t l_leaf = -1;
        size_t r_leaf = -1;

    };


    class Map {
    public:
        static constexpr size_t MAX_DEPTH = 32;
        static constexpr size_t BIN_COUNT = 11;

        Map(size_t meshIndex=0) : meshIndex(meshIndex) {}
        ~Map() = default;

        size_t meshIndex;
        Az3D::Transform trform;

        std::vector<BVHNode> nodes;

        /* IMPORTANT NOTE:
        
        The indices in sorted indices will be converted to i * 3 + j(j is 0, 1, 2) to get the vertex index
        
        */

        std::vector<size_t> sortedIndices; // For BVH traversal
        
        std::vector<glm::vec3> unsortedABmin;
        std::vector<glm::vec3> unsortedABmax;
        std::vector<glm::vec3> unsortedCenters;
        size_t indexCount = 0; // Number of indices in the mesh

        void createBVH(Az3D::Mesh& mesh) {
            indexCount = mesh.indices.size() / 3;

            BVHNode root = {
                glm::vec3(FLT_MAX),
                glm::vec3(-FLT_MAX),
                -1, -1, 0, indexCount
            };

            sortedIndices.resize(indexCount);

            unsortedCenters.resize(indexCount);
            unsortedABmin.resize(indexCount);
            unsortedABmax.resize(indexCount);

            for (size_t i = 0; i < indexCount; ++i) {
                sortedIndices[i] = i;

                unsortedCenters[i] = glm::vec3(0.0f);
                unsortedABmin[i] = glm::vec3(FLT_MAX);
                unsortedABmax[i] = glm::vec3(-FLT_MAX);

                for (size_t j = 0; j < 3; ++j) {
                    size_t idx = mesh.indices[i * 3 + j];
                    const auto& vertex = mesh.vertices[idx].pos;

                    unsortedCenters[i] += vertex;
                    unsortedABmin[i] = glm::min(unsortedABmin[i], vertex);
                    unsortedABmax[i] = glm::max(unsortedABmax[i], vertex);

                    root.min = glm::min(root.min, vertex);
                    root.max = glm::max(root.max, vertex);
                }

                unsortedCenters[i] /= 3.0f;
            }

            nodes.push_back(root);


            buildBVH(mesh);
        }

        void buildBVH(Az3D::Mesh const& mesh) {
            std::queue<size_t> queue;
            queue.push(0);

            while (!queue.empty()) {
                int nIdx = queue.front();
                queue.pop();

                int faceCount = nodes[nIdx].r_leaf - nodes[nIdx].l_leaf;
                if (faceCount <= 2) {
                    nodes[nIdx].l_child = -1;
                    nodes[nIdx].r_child = -1;
                    continue; // Leaf node
                }

                glm::vec3 boxSize = nodes[nIdx].max - nodes[nIdx].min;

                short bestAxis = -1;
                size_t bestSplit = -1;

                glm::vec3 bestLeftMin;
                glm::vec3 bestLeftMax;

                glm::vec3 bestRightMin;
                glm::vec3 bestRightMax;

                float bestCost = (boxSize.x * boxSize.x +
                                  boxSize.y * boxSize.y +
                                  boxSize.z * boxSize.z) * faceCount;

                for (int i = 0; i < (BIN_COUNT - 1) * 3; ++i) {
                    glm::vec3 leftMin = glm::vec3(FLT_MAX);
                    glm::vec3 leftMax = glm::vec3(-FLT_MAX);

                    glm::vec3 rightMin = glm::vec3(FLT_MAX);
                    glm::vec3 rightMax = glm::vec3(-FLT_MAX);

                    short axis = i % 3; // 0: x, 1: y, 2: z

                    int b = i / 3; // Bin index

                    float splitPoint = nodes[nIdx].min[axis] + boxSize[axis] * (b + 1) / BIN_COUNT;

                    size_t splitIndex = nodes[nIdx].l_leaf;

                    for (size_t j = nodes[nIdx].l_leaf; j < nodes[nIdx].r_leaf; ++j) {
                        size_t idx = sortedIndices[j];
                        
                        const glm::vec3& center = unsortedCenters[idx];

                        if (center[axis] < splitPoint) {
                            leftMin = glm::min(leftMin, unsortedABmin[idx]);
                            leftMax = glm::max(leftMax, unsortedABmax[idx]);
                            splitIndex++;
                        } else {
                            rightMin = glm::min(rightMin, unsortedABmin[idx]);
                            rightMax = glm::max(rightMax, unsortedABmax[idx]);
                        }
                    }

                    glm::vec3 leftSize = leftMax - leftMin;
                    glm::vec3 rightSize = rightMax - rightMin;

                    float leftCost =   (leftSize.x * leftSize.x +
                                        leftSize.y * leftSize.y +
                                        leftSize.z * leftSize.z) * (splitIndex - nodes[nIdx].l_leaf);
                    float rightCost =  (rightSize.x * rightSize.x +
                                        rightSize.y * rightSize.y +
                                        rightSize.z * rightSize.z) * (nodes[nIdx].r_leaf - splitIndex);

                    float totalCost = leftCost + rightCost;

                    if (totalCost < bestCost) {
                        bestCost = totalCost;
                        bestAxis = axis;
                        bestSplit = splitIndex;

                        bestLeftMin = leftMin;
                        bestLeftMax = leftMax;

                        bestRightMin = rightMin;
                        bestRightMax = rightMax;
                    }
                }

                if (bestAxis == -1) {
                    nodes[nIdx].l_child = -1;
                    nodes[nIdx].r_child = -1;
                    continue; // No valid split found
                }

                std::sort(sortedIndices.begin() + nodes[nIdx].l_leaf, sortedIndices.begin() + nodes[nIdx].r_leaf,
                [&](size_t i1, size_t i2) {
                    return (unsortedCenters[i1][bestAxis] < unsortedCenters[i2][bestAxis]);
                });

                // Create left and right children
                size_t leftChildIdx = nodes.size();
                BVHNode leftNode = {
                    bestLeftMin,
                    bestLeftMax,
                    -1, -1, nodes[nIdx].l_leaf, bestSplit
                };

                nodes.push_back(leftNode);

                size_t rightChildIdx = nodes.size();
                BVHNode rightNode = {
                    bestRightMin,
                    bestRightMax,
                    -1, -1, bestSplit, nodes[nIdx].r_leaf
                };
                nodes.push_back(rightNode);

                // Update current node
                // Ps, I have no idea why but for some reason the &node up there just doesn't work at all
                nodes[nIdx].l_child = leftChildIdx;
                nodes[nIdx].r_child = rightChildIdx;

                queue.push(leftChildIdx);
                queue.push(rightChildIdx);
            }
        }

        // Basically the same version but return the distance instead
        float closestDistance(
            const Az3D::Mesh& mesh,
            glm::vec3 origin,
            glm::vec3 direction,
            float maxDistance) const {
            glm::vec3 hit = closestHit(mesh, origin, direction, maxDistance);
            return glm::distance(hit, origin);
        };
        glm::vec3 closestHit(
            const Az3D::Mesh& mesh,
            glm::vec3 origin,
            glm::vec3 direction,
            float maxDistance)
        const {
            float closestDistance = maxDistance;

            // Apply reverse transform to origin and direction based on the map's transform
            glm::mat4 invTransform = glm::inverse(trform.modelMatrix());

            glm::vec4 origin4 = invTransform * glm::vec4(origin, 1.0f);
            glm::vec4 direction4 = invTransform * glm::vec4(direction, 0.0f);

            origin = glm::vec3(origin4);
            direction = glm::normalize(glm::vec3(direction4));

            int nstack[64] = { 0 };
            int ns_top = 1;

            while (ns_top > 0) {
                int nIdx = nstack[--ns_top];
                const BVHNode& node = nodes[nIdx];

                // Check if the ray is outside the bounding box
                float nodeHit = rayIntersectsBox(origin, direction, node.min, node.max);

                if (nodeHit < 0 || nodeHit > closestDistance) {
                    continue; // Ray misses the node
                }

                if (node.l_child > -1 && node.r_child > -1) {
                    int leftChildIdx = node.l_child;

                    float leftHitDist = rayIntersectsBox(origin, direction,
                        nodes[leftChildIdx].min, nodes[leftChildIdx].max);

                    int rightChildIdx = node.r_child;
                    float rightHitDist = rayIntersectsBox(origin, direction,
                        nodes[rightChildIdx].min, nodes[rightChildIdx].max);

                    bool leftHit = leftHitDist >= 0 && leftHitDist < closestDistance;
                    bool rightHit = rightHitDist >= 0 && rightHitDist < closestDistance;

                    // Child ordering for closer intersection and early exit
                    bool leftCloser = leftHitDist < rightHitDist;

                    // Some cool branchless logic!
                    nstack[ns_top] = rightChildIdx * leftCloser + leftChildIdx * !leftCloser;
                    ns_top += rightHit * leftCloser + leftHit * !leftCloser;

                    nstack[ns_top] = leftChildIdx * leftCloser + rightChildIdx * !leftCloser;
                    ns_top += leftHit * leftCloser + rightHit * !leftCloser;

                    continue;
                }

                for (int i = node.l_leaf; i < node.r_leaf; ++i) {
                    size_t idx = sortedIndices[i];
                    // Retrieve the actual indices
                    size_t index0 = mesh.indices[idx * 3 + 0];
                    size_t index1 = mesh.indices[idx * 3 + 1];
                    size_t index2 = mesh.indices[idx * 3 + 2];

                    const glm::vec3& v0 = mesh.vertices[index0].pos;
                    const glm::vec3& v1 = mesh.vertices[index1].pos;
                    const glm::vec3& v2 = mesh.vertices[index2].pos;

                    glm::vec3 e1 = v1 - v0;
                    glm::vec3 e2 = v2 - v0;

                    glm::vec3 h = glm::cross(direction, e2);

                    float a = glm::dot(e1, h);

                    if (a == 0.0f) continue; // Ray is parallel to the triangle

                    float f = 1.0f / a;
                    glm::vec3 s = origin - v0;

                    float u = f * glm::dot(s, h);

                    if (u < 0.0f || u > 1.0f) continue; // Outside the triangle

                    glm::vec3 q = glm::cross(s, e1);

                    float v = f * glm::dot(direction, q);
                    if (v < 0.0f || u + v > 1.0f) continue; // Outside the triangle

                    float t = f * glm::dot(e2, q);
                    if (t <= 0.0f || t >= closestDistance) continue; // Not a valid hit

                    closestDistance = t;
                }
            }

            // Return new closest point if found, otherwise return the original point
            // And since by logic if there's no hit, closestDistance will be maxDistance
            // Automatically return the original point
            return origin + direction * closestDistance;
        }

        // This function will return the closest distance (0 if ray is inside box, -1 if ray misses, anything else is the distance)
        static inline float rayIntersectsBox(glm::vec3 const& rayOrigin, glm::vec3 const& rayDirection,
                                            glm::vec3 const& boxMin, glm::vec3 const& boxMax) {
            glm::vec3 invDir = 1.0f / rayDirection;
            glm::vec3 t0 = (boxMin - rayOrigin) * invDir;
            glm::vec3 t1 = (boxMax - rayOrigin) * invDir;

            float tMin = glm::min(t0.x, t1.x);
            float tMax = glm::max(t0.x, t1.x);

            for (int i = 1; i < 3; ++i) {
                float t0i = glm::min(t0[i], t1[i]);
                float t1i = glm::max(t0[i], t1[i]);

                tMin = glm::max(tMin, t0i);
                tMax = glm::min(tMax, t1i);
            }

            // Ray is inside the box
            if (rayOrigin.x > boxMin.x && rayOrigin.x < boxMax.x &&
                rayOrigin.y > boxMin.y && rayOrigin.y < boxMax.y &&
                rayOrigin.z > boxMin.z && rayOrigin.z < boxMax.z) {
                return 0;
            }

            if (tMax < tMin || tMin < 0) {
                return -1.0f; // Ray misses the box
            }

            return tMin;
        }
    };
}