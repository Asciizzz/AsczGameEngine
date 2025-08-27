#include "Az3D/Mesh.hpp"
#include <cfloat>
#include <queue>

namespace Az3D {

// BVH Construction

void Mesh::createBVH() {
    hasBVH = true;

    if (indices.empty()) return;

    indexCount = indices.size() / 3;

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
            size_t idx = indices[i * 3 + j];
            const auto& vertex = vertices[idx].pos;

            unsortedCenters[i] += vertex;
            unsortedABmin[i] = glm::min(unsortedABmin[i], vertex);
            unsortedABmax[i] = glm::max(unsortedABmax[i], vertex);

            root.min = glm::min(root.min, vertex);
            root.max = glm::max(root.max, vertex);
        }

        unsortedCenters[i] /= 3.0f;
    }

    meshMin = root.min;
    meshMax = root.max;

    nodes.clear();
    nodes.push_back(root);

    buildBVH();
}

void Mesh::buildBVH() {
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
        nodes[nIdx].l_child = leftChildIdx;
        nodes[nIdx].r_child = rightChildIdx;

        queue.push(leftChildIdx);
        queue.push(rightChildIdx);
    }
}


// BVH Traversal

HitInfo Mesh::closestHit(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, const glm::mat4& modelMat4) const {
    HitInfo hit;
    if (!hasBVH || nodes.empty()) {
        return hit; // No BVH available
    }
    
    if (glm::length(direction) < 0.0001f) return hit;
    hit.prop.z = maxDistance; // Initialize with max distance

    // Apply reverse transform to origin and direction based on the mesh's transform
    glm::mat4 invModel = glm::inverse(modelMat4);

    glm::vec3 rayOrg = glm::vec3(invModel * glm::vec4(origin, 1.0f));
    glm::vec3 rayDir = glm::normalize(glm::vec3(invModel * glm::vec4(direction, 0.0f)));

    int nstack[MAX_DEPTH] = { 0 };
    int ns_top = 1;

    while (ns_top > 0) {
        int nIdx = nstack[--ns_top];
        const BVHNode& node = nodes[nIdx];

        // Check if the ray is outside the bounding box
        float nodeHitDist = rayIntersectBox(rayOrg, rayDir, node.min, node.max);

        if (nodeHitDist < 0 || nodeHitDist > hit.prop.z) {
            continue; // Ray misses the node
        }

        if (node.l_child > -1 && node.r_child > -1) {
            int leftChildIdx = node.l_child;
            float leftHitDist = rayIntersectBox(rayOrg, rayDir,
                nodes[leftChildIdx].min, nodes[leftChildIdx].max);

            int rightChildIdx = node.r_child;
            float rightHitDist = rayIntersectBox(rayOrg, rayDir,
                nodes[rightChildIdx].min, nodes[rightChildIdx].max);

            bool leftHit = leftHitDist >= 0 && leftHitDist < hit.prop.z;
            bool rightHit = rightHitDist >= 0 && rightHitDist < hit.prop.z;

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
            size_t idx0 = indices[idx * 3 + 0];
            size_t idx1 = indices[idx * 3 + 1];
            size_t idx2 = indices[idx * 3 + 2];

            const glm::vec3& v0 = vertices[idx0].pos;
            const glm::vec3& v1 = vertices[idx1].pos;
            const glm::vec3& v2 = vertices[idx2].pos;

            glm::vec3 curHitProp = rayIntersectTriangle(rayOrg, rayDir, v0, v1, v2);

            if (curHitProp.z >= 0.0f && curHitProp.z < hit.prop.z) {
                hit.prop = curHitProp;
                hit.index = idx;
                hit.triangleIndex = idx;
                hit.hit = true;
                hit.hasHit = true;
            }
        }
    }

    if (!hit.hit) return hit;

    // Retrieve the hit vertex
    glm::vec3 localVertex = rayOrg + rayDir * hit.prop.z;

    // Retrieve the hit normal
    size_t hitIdx = hit.index;
    size_t idx0 = indices[hitIdx * 3 + 0];
    size_t idx1 = indices[hitIdx * 3 + 1];
    size_t idx2 = indices[hitIdx * 3 + 2];
    const glm::vec3& nrml0 = vertices[idx0].nrml;
    const glm::vec3& nrml1 = vertices[idx1].nrml;
    const glm::vec3& nrml2 = vertices[idx2].nrml;

    // Barycentric interpolation for normal
    hit.nrml =  nrml0 * hit.prop.x +
                nrml1 * hit.prop.y +
                nrml2 * (1.0f - hit.prop.x - hit.prop.y);

    // Convert back to world coordinates
    // glm::vec3 worldVertex = localVertex * transform.scl + transform.pos; // Apply scale and translation
    // worldVertex = transform.rot * worldVertex;

    // glm::vec3 worldNormal = glm::normalize(transform.rot * hit.nrml); // Rotate the normal
    
    glm::vec3 worldVertex = glm::vec3(modelMat4 * glm::vec4(localVertex, 1.0f));
    glm::vec3 worldNormal = glm::normalize(glm::vec3(modelMat4 * glm::vec4(hit.nrml, 0.0f))); // Rotate the normal

    hit.vrtx = worldVertex;
    hit.nrml = worldNormal;

    return hit;
}

HitInfo Mesh::closestHit(const glm::vec3& sphere_origin, float sphere_radius, const glm::mat4& modelMat4) const {
    HitInfo hit;
    if (!hasBVH || nodes.empty()) { return hit; }
    
    hit.prop.z = sphere_radius;

    glm::mat4 invModel = glm::inverse(modelMat4);

    glm::vec3 sphereOrg = glm::vec3(invModel * glm::vec4(sphere_origin, 1.0f));

    // IMPORTANT: Keep in mind that bvh assume uniform scaling
    float sphereRad = sphere_radius / glm::vec3(modelMat4[0][0], modelMat4[1][1], modelMat4[2][2]).x;

    int nstack[MAX_DEPTH] = { 0 };
    int ns_top = 1;

    while (ns_top > 0) {
        int nIdx = nstack[--ns_top];
        const BVHNode& node = nodes[nIdx];

        float nodeHitDist = sphereIntersectBox(sphereOrg, sphereRad, node.min, node.max);

        if (nodeHitDist < 0 || nodeHitDist > hit.prop.z) {
            continue; // Ray misses the node
        }

        if (node.l_child > -1 && node.r_child > -1) {
            int leftChildIdx = node.l_child;
            float leftHitDist = sphereIntersectBox(sphereOrg, sphereRad,
                nodes[leftChildIdx].min, nodes[leftChildIdx].max);

            int rightChildIdx = node.r_child;
            float rightHitDist = sphereIntersectBox(sphereOrg, sphereRad,
                nodes[rightChildIdx].min, nodes[rightChildIdx].max);

            bool leftHit = leftHitDist >= 0 && leftHitDist < hit.prop.z;
            bool rightHit = rightHitDist >= 0 && rightHitDist < hit.prop.z;

            bool leftCloser = leftHitDist < rightHitDist;

            nstack[ns_top] = rightChildIdx * leftCloser + leftChildIdx * !leftCloser;
            ns_top += rightHit * leftCloser + leftHit * !leftCloser;

            nstack[ns_top] = leftChildIdx * leftCloser + rightChildIdx * !leftCloser;
            ns_top += leftHit * leftCloser + rightHit * !leftCloser;

            continue;
        }

        for (int i = node.l_leaf; i < node.r_leaf; ++i) {
            size_t idx = sortedIndices[i];
            // Retrieve the actual indices
            size_t idx0 = indices[idx * 3 + 0];
            size_t idx1 = indices[idx * 3 + 1];
            size_t idx2 = indices[idx * 3 + 2];

            const glm::vec3& v0 = vertices[idx0].pos;
            const glm::vec3& v1 = vertices[idx1].pos;
            const glm::vec3& v2 = vertices[idx2].pos;

            glm::vec3 curHitProp = sphereIntersectTriangle(sphereOrg, sphereRad, v0, v1, v2);

            if (curHitProp.z >= 0.0f && curHitProp.z <= hit.prop.z) {
                hit.prop = curHitProp;
                hit.index = idx;
                hit.triangleIndex = idx;
                hit.hit = true;
                hit.hasHit = true;
            }
        }
    }

    if (!hit.hit) return hit;

    size_t hitIdx = hit.index;
    size_t idx0 = indices[hitIdx * 3 + 0];
    size_t idx1 = indices[hitIdx * 3 + 1];
    size_t idx2 = indices[hitIdx * 3 + 2];
    
    // Retrieve the hit normal
    const VertexStatic& vrtx0 = vertices[idx0];
    const VertexStatic& vrtx1 = vertices[idx1];
    const VertexStatic& vrtx2 = vertices[idx2];
    hit.nrml =  vrtx0.nrml * hit.prop.x +
                vrtx1.nrml * hit.prop.y +
                vrtx2.nrml * (1.0f - hit.prop.x - hit.prop.y);

    // Retrieve the hit vertex (require barycentric coordinates)
    hit.vrtx = vrtx0.pos * hit.prop.x +
                vrtx1.pos * hit.prop.y +
                vrtx2.pos * (1.0f - hit.prop.x - hit.prop.y);

    // Convert back to world coordinates
    glm::vec3 worldVertex = glm::vec3(modelMat4 * glm::vec4(hit.vrtx, 1.0f));
    glm::vec3 worldNormal = glm::normalize(glm::vec3(modelMat4 * glm::vec4(hit.nrml, 0.0f))); // Rotate the normal

    hit.vrtx = worldVertex;
    hit.nrml = worldNormal;
    hit.hit = true; // Mark as hit

    return hit;
}

// Helper functions - exact copies from original Map.hpp
float Mesh::rayIntersectBox(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                            const glm::vec3& boxMin, const glm::vec3& boxMax) {
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
        rayOrigin.z > boxMin.z && rayOrigin.z < boxMax.z)
        return 0.0f;

    // Ray misses the box
    if (tMax < tMin || tMin < 0)
        return -1.0f;

    return tMin;
}

glm::vec3 Mesh::rayIntersectTriangle(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                                    const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2) {
    glm::vec3 e1 = v1 - v0;
    glm::vec3 e2 = v2 - v0;
    glm::vec3 h = glm::cross(rayDirection, e2);
    float a = glm::dot(e1, h);

    if (a == 0.0f) return glm::vec3(-1.0f); // Ray is parallel to the triangle

    float f = 1.0f / a;
    glm::vec3 s = rayOrigin - v0;
    float u = f * glm::dot(s, h);

    if (u < 0.0f || u > 1.0f) return glm::vec3(-1.0f); // Outside the triangle

    glm::vec3 q = glm::cross(s, e1);
    float v = f * glm::dot(rayDirection, q);
    if (v < 0.0f || u + v > 1.0f) return glm::vec3(-1.0f); // Outside the triangle

    float t = f * glm::dot(e2, q);
    return t > 0.0f ? glm::vec3(u, v, t) : glm::vec3(-1.0f);
}

float Mesh::sphereIntersectBox( const glm::vec3& sphereOrigin, float sphereRadius,
                                const glm::vec3& boxMin, const glm::vec3& boxMax) {
    glm::vec3 closestPoint = glm::clamp(sphereOrigin, boxMin, boxMax);

    glm::vec3 delta = closestPoint - sphereOrigin;
    float distSqr = glm::dot(delta, delta);
    if (distSqr == 0.0f) return 0.0f;

    bool intersect = distSqr < sphereRadius * sphereRadius;
    return intersect ? glm::sqrt(distSqr) : -1.0f;
}

glm::vec3 Mesh::sphereIntersectTriangle(const glm::vec3& sphereOrigin, float sphereRadius,
                                        const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2) {
    // Compute triangle normal
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

    // Compute perpendicular distance from sphere center to triangle plane
    float distance = glm::dot(sphereOrigin - v0, normal);

    // Distance > Radius = No intersection
    if (std::abs(distance) > sphereRadius) return glm::vec3(-1.0f);

    // Project sphere origin onto triangle plane
    glm::vec3 projectedPoint = sphereOrigin - distance * normal;

    // Compute barycentric coordinates
    glm::vec3 v0v1 = v1 - v0;
    glm::vec3 v0v2 = v2 - v0;
    glm::vec3 v0p  = projectedPoint - v0;
    
    float d00 = glm::dot(v0v1, v0v1);
    float d01 = glm::dot(v0v1, v0v2);
    float d11 = glm::dot(v0v2, v0v2);
    float d20 = glm::dot(v0p,  v0v1);
    float d21 = glm::dot(v0p,  v0v2);

    float denom = d00 * d11 - d01 * d01;
    if (denom == 0.0f) return glm::vec3(-1.0f); // Degenerate triangle

    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;

    // Check if point is inside triangle
    if (u >= 0 && v >= 0 && w >= 0) {
        return glm::vec3(u, v, std::abs(distance));
    }

    return glm::vec3(-1.0f); // Outside triangle
}

}
