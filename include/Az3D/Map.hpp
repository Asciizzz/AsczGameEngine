#pragma once

#include "Az3D/Az3D.hpp"

#include <queue>
#include <algorithm>

namespace Az3D {


    struct BVHNode {
        glm::vec3 min, max;

        int cl = -1, cr = -1; // Children
        int ll = -1, lr = -1; // Primitives

        int depth = 0; // Not needed, good to have

        static const int NODE_FACES = 3;
        static const int MAX_DEPTH = 10;
        static const int BIN_COUNT = 10;
    };


    // Game map!
    struct Map {
        Map(std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> material) :
            mesh(std::move(mesh)), material(std::move(material)),
            min(glm::vec3(FLT_MAX)), max(glm::vec3(-FLT_MAX)) {

            // Get bouding box from the mesh vertices
            for (const auto& vertex : mesh->vertices) {
                min.x = glm::min(min.x, vertex.pos.x);
                min.y = glm::min(min.y, vertex.pos.y);
                min.z = glm::min(min.z, vertex.pos.z);

                max.x = glm::max(max.x, vertex.pos.x);
                max.y = glm::max(max.y, vertex.pos.y);
                max.z = glm::max(max.z, vertex.pos.z);
            }

            // Get centers of each face
            for (int i = 0; i < mesh->indices.size(); i += 3) {
                glm::vec3 v0 = mesh->vertices[mesh->indices[i]].pos;
                glm::vec3 v1 = mesh->vertices[mesh->indices[i + 1]].pos;
                glm::vec3 v2 = mesh->vertices[mesh->indices[i + 2]].pos;

                glm::vec3 center = (v0 + v1 + v2) / 3.0f;
                centers.push_back(center);
            }
        }

        // Basically a model, but very big and there's only one of them
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Material> material;

        std::vector<glm::vec3> centers; // Centers of each face
        std::vector<BVHNode> nodes;
        glm::vec3 min, max;

        void buildBVH() {
            std::queue<int> queue;
            queue.push(0);

            /*
            while (!queue.empty()) {
                int nIdx = queue.front();
                queue.pop();

                AzNode &nd = nodes[nIdx];

                int nF = nd.lr - nd.ll;
                if (nF <= NODE_FACES || nd.depth >= MAX_DEPTH) {
                    nodes[nIdx].cl = -1;
                    nodes[nIdx].cr = -1;
                    continue;
                }

                float nLn_x = nd.max_x - nd.min_x;
                float nLn_y = nd.max_y - nd.min_y;
                float nLn_z = nd.max_z - nd.min_z;

                int bestAxis = -1;
                int bestSplit = -1;

                float bestLab_min_x, bestLab_min_y, bestLab_min_z;
                float bestLab_max_x, bestLab_max_y, bestLab_max_z;

                float bestRab_min_x, bestRab_min_y, bestRab_min_z;
                float bestRab_max_x, bestRab_max_y, bestRab_max_z;

                float bestCost = (nLn_x * nLn_x + nLn_y * nLn_y + nLn_z * nLn_z) * nF;

                #pragma omp parallel
                for (int i = 0; i < (BIN_COUNT - 1) * 3; ++i) {
                    float lmin_x =  INFINITY, lmin_y =  INFINITY, lmin_z =  INFINITY;
                    float lmax_x = -INFINITY, lmax_y = -INFINITY, lmax_z = -INFINITY;

                    float rmin_x =  INFINITY, rmin_y =  INFINITY, rmin_z =  INFINITY;
                    float rmax_x = -INFINITY, rmax_y = -INFINITY, rmax_z = -INFINITY;

                    bool ax = i % 3 == 0, ay = i % 3 == 1, az = i % 3 == 2;
                    int a = i % 3;
                    int b = i / 3;

                    float s1 = nd.min_x * ax + nd.min_y * ay + nd.min_z * az;
                    float s2 = nLn_x    * ax + nLn_y    * ay + nLn_z    * az;
                    float splitPoint = s1 + s2 * (b + 1) / BIN_COUNT;

                    int splitIdx = nd.ll;

                    for (int g = nd.ll; g < nd.lr; ++g) {
                        int i = fIdxs[g];

                        float cent = c_x[i] * ax + c_y[i] * ay + c_z[i] * az;

                        if (cent < splitPoint) {
                            lmin_x = lmin_x < min_x[i] ? lmin_x : min_x[i];
                            lmin_y = lmin_y < min_y[i] ? lmin_y : min_y[i];
                            lmin_z = lmin_z < min_z[i] ? lmin_z : min_z[i];

                            lmax_x = lmax_x > max_x[i] ? lmax_x : max_x[i];
                            lmax_y = lmax_y > max_y[i] ? lmax_y : max_y[i];
                            lmax_z = lmax_z > max_z[i] ? lmax_z : max_z[i];

                            splitIdx++;
                        }
                        else {
                            rmin_x = rmin_x < min_x[i] ? rmin_x : min_x[i];
                            rmin_y = rmin_y < min_y[i] ? rmin_y : min_y[i];
                            rmin_z = rmin_z < min_z[i] ? rmin_z : min_z[i];

                            rmax_x = rmax_x > max_x[i] ? rmax_x : max_x[i];
                            rmax_y = rmax_y > max_y[i] ? rmax_y : max_y[i];
                            rmax_z = rmax_z > max_z[i] ? rmax_z : max_z[i];
                        }
                    }

                    float lN_x = lmax_x - lmin_x, rN_x = rmax_x - rmin_x;
                    float lN_y = lmax_y - lmin_y, rN_y = rmax_y - rmin_y;
                    float lN_z = lmax_z - lmin_z, rN_z = rmax_z - rmin_z;

                    float lCost = (lN_x * lN_x + lN_y * lN_y + lN_z * lN_z) * (splitIdx - nd.ll);
                    float rCost = (rN_x * rN_x + rN_y * rN_y + rN_z * rN_z) * (nd.lr - splitIdx);

                    float cost = lCost + rCost;

                    #pragma omp critical
                    if (cost < bestCost) {
                        bestCost = cost;
                        bestAxis = a;
                        bestSplit = splitIdx;

                        bestLab_min_x = lmin_x; bestRab_min_x = rmin_x;
                        bestLab_min_y = lmin_y; bestRab_min_y = rmin_y;
                        bestLab_min_z = lmin_z; bestRab_min_z = rmin_z;

                        bestLab_max_x = lmax_x; bestRab_max_x = rmax_x;
                        bestLab_max_y = lmax_y; bestRab_max_y = rmax_y;
                        bestLab_max_z = lmax_z; bestRab_max_z = rmax_z;
                    }
                }

                if (bestAxis == -1) {
                    nodes[nIdx].cl = -1;
                    nodes[nIdx].cr = -1;
                    continue;
                }

                std::sort(fIdxs.begin() + nd.ll, fIdxs.begin() + nd.lr,
                [&](int i1, int i2) {
                    return (c_x[i1] < c_x[i2]) * (bestAxis == 0) +
                        (c_y[i1] < c_y[i2]) * (bestAxis == 1) +
                        (c_z[i1] < c_z[i2]) * (bestAxis == 2);
                });

                // Create left and right node
                AzNode nl = {
                    bestLab_min_x, bestLab_min_y, bestLab_min_z,
                    bestLab_max_x, bestLab_max_y, bestLab_max_z,
                    -1, -1, nd.ll, bestSplit, nd.depth + 1
                };

                AzNode nr = {
                    bestRab_min_x, bestRab_min_y, bestRab_min_z,
                    bestRab_max_x, bestRab_max_y, bestRab_max_z,
                    -1, -1, bestSplit, nd.lr, nd.depth + 1
                };

                int lIdx = nodes.size();
                nodes.push_back(nl);

                int rIdx = nodes.size();
                nodes.push_back(nr);

                nodes[nIdx].cl = lIdx;
                nodes[nIdx].cr = rIdx;

                queue.push(lIdx);
                queue.push(rIdx);
            }*/

            while (!queue.empty()) {
                int nIdx = queue.front();
                queue.pop();

                BVHNode& node = nodes[nIdx];

                int nF = node.lr - node.ll;
                if (nF <= BVHNode::NODE_FACES || node.depth >= BVHNode::MAX_DEPTH) {
                    nodes[nIdx].cl = -1;
                    nodes[nIdx].cr = -1;
                    continue;
                }

                glm::vec3 nLength = node.max - node.min;

                int bestAxis = -1;
                int bestSplit = -1;

                glm::vec3 bestLab_min, bestLab_max;
                glm::vec3 bestRab_min, bestRab_max;

                float bestCost = glm::dot(nLength, nLength) * nF;

                for (int i = 0; i < (BVHNode::BIN_COUNT - 1) * 3; ++i) {
                    glm::vec3 lmin(INFINITY), lmax(-INFINITY);
                    glm::vec3 rmin(INFINITY), rmax(-INFINITY);

                    int axis = i % 3;
                    int bin = i / 3;

                    float splitPoint = node.min[axis] + nLength[axis] * (bin + 1) / BVHNode::BIN_COUNT;

                    int splitIdx = node.ll;

                    for (int g = node.ll; g < node.lr; ++g) {
                        const auto& vertex = mesh->vertices[g];

                        float cent = vertex.pos[axis];

                        if (cent < splitPoint) {
                            lmin = glm::min(lmin, vertex.pos);
                            lmax = glm::max(lmax, vertex.pos);
                            splitIdx++;
                        } else {
                            rmin = glm::min(rmin, vertex.pos);
                            rmax = glm::max(rmax, vertex.pos);
                        }
                    }

                    glm::vec3 lN = lmax - lmin;
                    glm::vec3 rN = rmax - rmin;

                    float lCost = glm::dot(lN, lN) * (splitIdx - node.ll);
                    float rCost = glm::dot(rN, rN) * (node.lr - splitIdx);

                    float cost = lCost + rCost;

                    if (cost < bestCost) {
                        bestCost = cost;
                        bestAxis = axis;
                        bestSplit = splitIdx;

                        bestLab_min = lmin; bestRab_min = rmin;
                        bestLab_max = lmax; bestRab_max = rmax;
                    }
                }

                if (bestAxis == -1) {
                    nodes[nIdx].cl = -1;
                    nodes[nIdx].cr = -1;
                    continue;
                }

                std::sort(mesh->indices.begin() + node.ll, mesh->indices.begin() + node.lr,
                [&](uint32_t i1, uint32_t i2) {
                    return (centers[i1][bestAxis] < centers[i2][bestAxis]);
                });

                // Create left and right node

                BVHNode leftNode = {
                    bestLab_min, bestLab_max,
                    -1, -1, node.ll, bestSplit, node.depth + 1
                };
                BVHNode rightNode = {
                    bestRab_min, bestRab_max,
                    -1, -1, bestSplit, node.lr, node.depth + 1
                };

                int lIdx = nodes.size();
                nodes.push_back(leftNode);

                int rIdx = nodes.size();
                nodes.push_back(rightNode);

                nodes[nIdx].cl = lIdx;
                nodes[nIdx].cr = rIdx;

                queue.push(lIdx);
                queue.push(rIdx);
            }
        }
    };

}