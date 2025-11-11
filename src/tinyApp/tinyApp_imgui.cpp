// tinyApp_imgui.cpp - UI Implementation & Testing
#include "tinyApp/tinyApp.hpp"
#include "tinyEngine/tinyLoader.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <vector>
#include <string>
#include <functional>

using namespace tinyVk;

// ============================================================================
// ACTIVE STATE - Hierarchy window state (kept in cpp to avoid polluting tinyApp)
// ============================================================================

namespace HierarchyState {
    static tinyHandle activeSceneHandle; // Current active scene (invalid = use mainSceneHandle)
    static float splitterPos = 0.5f;     // Splitter position (0-1, default 50/50)
    
    // Expanded nodes tracking
    static std::unordered_set<uint64_t> expandedSceneNodes;
    static std::unordered_set<uint64_t> expandedFileNodes;
    
    // Selection tracking
    static tinyHandle selectedSceneNode;
    static tinyHandle selectedFileNode;
    
    // Drag state
    static tinyHandle draggedSceneNode;
    static tinyHandle draggedFileNode;

    // Note: tinyHandle is hashed by default

    static bool isExpanded(tinyHandle h, bool isScene) {
        auto& set = isScene ? expandedSceneNodes : expandedFileNodes;
        return set.find(h.value) != set.end();
    }

    static void setExpanded(tinyHandle h, bool isScene, bool expanded) {
        auto& set = isScene ? expandedSceneNodes : expandedFileNodes;
        if (expanded) set.insert(h.value);
        else          set.erase(h.value);
    }
}

// ============================================================================
// DRAG-DROP PAYLOAD SYSTEM - Using typeHandle for proper type identification
// ============================================================================

namespace DragDropPayloads {
    struct SceneNodePayload {
        tinyHandle nodeHandle;
        char nodeName[64];
    };
    
    struct FileNodePayload {
        tinyHandle fileHandle;       // File node handle (fnodes_ pool)
        typeHandle dataTypeHandle;   // Registry data handle (if has data)
        char fileName[64];
    };
}

// ============================================================================
// GENERALIZED TREE NODE RENDERING - Abstracts common ImGui tree logic
// ============================================================================

template<typename FuncType>
using Func = std::function<FuncType>;

template<typename FuncType>
using CFunc = const Func<FuncType>;

static void RenderGenericNodeHierarchy(
    tinyHandle nodeHandle,
    CFunc<bool(tinyHandle)>& isSelected,
    CFunc<void(tinyHandle)>& setSelected,
    CFunc<bool(tinyHandle)>& isDragged,
    CFunc<void(tinyHandle)>& setDragged,
    CFunc<bool(tinyHandle)>& isExpanded,
    CFunc<void(tinyHandle, bool)>& setExpanded,
    CFunc<const char*(tinyHandle)>& getName,
    CFunc<bool(tinyHandle)>& hasChildren,
    CFunc<std::vector<tinyHandle>(tinyHandle)>& getChildren,
    CFunc<void(tinyHandle)>& renderDragSource,
    CFunc<void(tinyHandle)>& renderDropTarget,
    CFunc<void(tinyHandle)>& renderContextMenu,
    CFunc<ImVec4(tinyHandle)>& getNormalColor,
    CFunc<ImVec4(tinyHandle)>& getDraggedColor,
    CFunc<ImVec4(tinyHandle)>& getNormalHoveredColor,
    CFunc<ImVec4(tinyHandle)>& getDraggedHoveredColor,
    CFunc<void()>& clearDragState,
    CFunc<void(tinyHandle)>& clearOtherSelection,
    int depth = 0
) {
    if (!nodeHandle.valid()) return;

    ImGui::PushID(static_cast<int>(nodeHandle.index));

    bool hasChild = hasChildren(nodeHandle);
    bool selected = isSelected(nodeHandle);
    bool dragged = isDragged(nodeHandle);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (!hasChild) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (selected) flags |= ImGuiTreeNodeFlags_Selected;

    ImVec4 headerColor = dragged ? getDraggedColor(nodeHandle) : getNormalColor(nodeHandle);
    ImVec4 headerHoveredColor = dragged ? getDraggedHoveredColor(nodeHandle) : getNormalHoveredColor(nodeHandle);

    ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerHoveredColor);

    bool forceOpen = isExpanded(nodeHandle);
    if (forceOpen) ImGui::SetNextItemOpen(true);

    bool nodeOpen = ImGui::TreeNodeEx(getName(nodeHandle), flags);
    if (hasChild && ImGui::IsItemToggledOpen()) setExpanded(nodeHandle, nodeOpen);

    ImGui::PopStyleColor(2);

    if (ImGui::IsItemClicked()) {
        setSelected(nodeHandle);
        clearOtherSelection(nodeHandle);
    }

    renderDragSource(nodeHandle);
    renderDropTarget(nodeHandle);
    renderContextMenu(nodeHandle);

    if (dragged && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) clearDragState();

    if (nodeOpen && hasChild) {
        std::vector<tinyHandle> children = getChildren(nodeHandle);

        // Sort by having children first, then alphabetically (works for both scene and file nodes)
        std::sort(children.begin(), children.end(), [&](tinyHandle a, tinyHandle b) {
            bool aHasChildren = hasChildren(a);
            bool bHasChildren = hasChildren(b);
            if (aHasChildren != bHasChildren) return aHasChildren > bHasChildren;
            return std::string(getName(a)) < std::string(getName(b));
        });

        for (const auto& child : children) {
            RenderGenericNodeHierarchy(
                child, isSelected, setSelected, isDragged, setDragged, isExpanded, setExpanded,
                getName, hasChildren, getChildren, renderDragSource, renderDropTarget, renderContextMenu,
                getNormalColor, getDraggedColor, getNormalHoveredColor, getDraggedHoveredColor,
                clearDragState, clearOtherSelection, depth + 1
            );
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

// ============================================================================
// HIERARCHY WINDOW - Scene & File Trees
// ============================================================================

static void RenderSceneNodeHierarchy(tinyProject* project, tinySceneRT* scene, tinyHandle nodeHandle, int depth = 0) {
    if (!scene || !nodeHandle.valid()) return;
    
    // Define lambdas for scene-specific logic
    auto isSelected = [](tinyHandle h) { return HierarchyState::selectedSceneNode == h; };
    auto setSelected = [](tinyHandle h) { HierarchyState::selectedSceneNode = h; };
    auto isDragged = [](tinyHandle h) { return HierarchyState::draggedSceneNode == h; };
    auto setDragged = [](tinyHandle h) { HierarchyState::draggedSceneNode = h; };
    auto isExpanded = [](tinyHandle h) { return HierarchyState::isExpanded(h, true); };
    auto setExpanded = [](tinyHandle h, bool expanded) { HierarchyState::setExpanded(h, true, expanded); };
    auto getName = [scene](tinyHandle h) -> const char* {
        const tinyNodeRT* node = scene->node(h);
        return node ? node->name.c_str() : "";
    };
    auto hasChildren = [scene](tinyHandle h) -> bool {
        const tinyNodeRT* node = scene->node(h);
        return node && !node->childrenHandles.empty();
    };
    auto getChildren = [scene](tinyHandle h) -> std::vector<tinyHandle> {
        const tinyNodeRT* node = scene->node(h);
        return node ? node->childrenHandles : std::vector<tinyHandle>();
    };
    auto renderDragSource = [scene](tinyHandle h) {
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            HierarchyState::draggedSceneNode = h;
            if (const tinyNodeRT* node = scene->node(h)) {
                DragDropPayloads::SceneNodePayload payload;
                payload.nodeHandle = h;
                strncpy(payload.nodeName, node->name.c_str(), 63);
                payload.nodeName[63] = '\0';
                
                ImGui::SetDragDropPayload("SCENE_NODE", &payload, sizeof(payload));
                ImGui::Text("Moving: %s", node->name.c_str());
            }
            ImGui::EndDragDropSource();
        }
    };
    auto renderDropTarget = [project, scene](tinyHandle h) {
        if (!HierarchyState::draggedSceneNode.valid() && ImGui::BeginDragDropTarget()) {
            // Accept scene node reparenting
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE")) {
                DragDropPayloads::SceneNodePayload* data = (DragDropPayloads::SceneNodePayload*)payload->Data;
                if (scene->reparentNode(data->nodeHandle, h)) {
                    HierarchyState::setExpanded(h, true, true); // Auto-expand parent
                    HierarchyState::selectedSceneNode = data->nodeHandle; // Keep selection
                }
                HierarchyState::draggedSceneNode = tinyHandle();
            }
            
            // Accept scene file drops (instantiate scene at this node)
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_NODE")) {
                DragDropPayloads::FileNodePayload* data = (DragDropPayloads::FileNodePayload*)payload->Data;

                // Check if this is a scene file
                if (data->dataTypeHandle.isType<tinySceneRT>()) {
                    tinyHandle sceneRegistryHandle = data->dataTypeHandle.handle;
                    tinyHandle activeSceneHandle = HierarchyState::activeSceneHandle.valid() ? 
                        HierarchyState::activeSceneHandle : project->mainSceneHandle;

                    if (sceneRegistryHandle != activeSceneHandle) {
                        project->addSceneInstance(sceneRegistryHandle, activeSceneHandle, h);
                        HierarchyState::setExpanded(h, true, true);
                    }
                }
                HierarchyState::draggedFileNode = tinyHandle();
            }
            
            ImGui::EndDragDropTarget();
        }
    };
    auto renderContextMenu = [scene](tinyHandle h) {
        if (ImGui::BeginPopupContextItem()) {
            const tinyNodeRT* node = scene->node(h);
            if (node) {
                ImGui::Text("Node: %s", node->name.c_str());
                ImGui::Separator();

                if (ImGui::MenuItem("Add Child")) scene->addNode("NewNode", h);
                if (ImGui::MenuItem("Delete"))    scene->removeNode(h); // Already have safeguard
                            if (ImGui::MenuItem("Flatten"))   scene->flattenNode(h);
            ImGui::Separator();
            if (ImGui::MenuItem("Properties")) {
                // TODO: open properties window
            }
            }
            ImGui::EndPopup();
        }
    };
    auto getNormalColor = [](tinyHandle) { return ImVec4(0.26f, 0.59f, 0.98f, 0.4f); };
    auto getDraggedColor = [](tinyHandle) { return ImVec4(0.8f, 0.6f, 0.2f, 0.8f); };
    auto getNormalHoveredColor = [](tinyHandle) { return ImVec4(0.26f, 0.59f, 0.98f, 0.6f); };
    auto getDraggedHoveredColor = [](tinyHandle) { return ImVec4(0.9f, 0.7f, 0.3f, 0.9f); };
    auto clearDragState = []() { HierarchyState::draggedSceneNode = tinyHandle(); };
    auto clearOtherSelection = [](tinyHandle) { HierarchyState::selectedFileNode = tinyHandle(); };

    RenderGenericNodeHierarchy(
        nodeHandle, isSelected, setSelected, isDragged, setDragged, isExpanded, setExpanded,
        getName, hasChildren, getChildren, renderDragSource, renderDropTarget, renderContextMenu,
        getNormalColor, getDraggedColor, getNormalHoveredColor, getDraggedHoveredColor,
        clearDragState, clearOtherSelection, depth
    );
}

static void RenderFileNodeHierarchy(tinyProject* project, tinyHandle fileHandle, int depth = 0) {
    tinyFS& fs = project->fs();

    const tinyFS::Node* node = fs.fNode(fileHandle);
    if (!node) return;
    
    // Define lambdas for file-specific logic
    auto isSelected = [](tinyHandle h) { return HierarchyState::selectedFileNode == h; };
    auto setSelected = [](tinyHandle h) { HierarchyState::selectedFileNode = h; };
    auto isDragged = [](tinyHandle h) { return HierarchyState::draggedFileNode == h; };
    auto setDragged = [](tinyHandle h) { HierarchyState::draggedFileNode = h; };
    auto isExpanded = [](tinyHandle h) { return HierarchyState::isExpanded(h, false); };
    auto setExpanded = [](tinyHandle h, bool expanded) { HierarchyState::setExpanded(h, false, expanded); };
    auto getName = [&fs](tinyHandle h) -> const char* {
        const tinyFS::Node* node = fs.fNode(h);
        return node ? node->name.c_str() : "";
    };
    auto hasChildren = [&fs](tinyHandle h) -> bool {
        const tinyFS::Node* node = fs.fNode(h);
        return node && node->isFolder() && !node->children.empty();
    };
    auto getChildren = [&fs](tinyHandle h) -> std::vector<tinyHandle> {
        const tinyFS::Node* node = fs.fNode(h);
        return node && node->isFolder() ? node->children : std::vector<tinyHandle>();
    };
    auto renderDragSource = [&fs](tinyHandle h) {
        const tinyFS::Node* node = fs.fNode(h);
        if (node && !node->isFolder()) {  // Only files can be dragged
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                HierarchyState::draggedFileNode = h;
                
                DragDropPayloads::FileNodePayload payload;
                payload.fileHandle = h;
                payload.dataTypeHandle = fs.fTypeHandle(h); // Get typeHandle from file
                strncpy(payload.fileName, node->name.c_str(), 63);
                payload.fileName[63] = '\0';
                
                ImGui::SetDragDropPayload("FILE_NODE", &payload, sizeof(payload));
                ImGui::Text("Dragging: %s", node->name.c_str());
                
                // Show type info if has data
                if (payload.dataTypeHandle.valid()) {
                    if (payload.dataTypeHandle.isType<tinySceneRT>()) {
                        ImGui::Text("[Scene]");
                    }
                }
                
                ImGui::EndDragDropSource();
            }
        }
    };
    auto renderDropTarget = [](tinyHandle) {};  // No drop target for files
    auto renderContextMenu = [&fs](tinyHandle h) {
        if (ImGui::BeginPopupContextItem()) {
            if (const tinyFS::Node* node = fs.fNode(h)) {
                bool deletable = node->deletable();
                const char* name = node->name.c_str();

                ImGui::Text("%s", name);

                // Special types methods
                ImGui::Separator();

                typeHandle dataType = fs.fTypeHandle(h);
                if (dataType.isType<tinySceneRT>()) {
                    if (ImGui::MenuItem("Make Active Scene")) {
                        HierarchyState::activeSceneHandle = dataType.handle;
                    }
                }

                // Folder methods
                if (node->isFolder()) {

                    ImGui::Separator();
                    if (ImGui::MenuItem("Flatten", nullptr, nullptr, deletable)) fs.fRemove(h, false);
                }

                // Delete methods
                if (ImGui::MenuItem("Delete", nullptr, nullptr, deletable)) fs.fRemove(h);
            }
            ImGui::EndPopup();
        }
    };
    auto getNormalColor = [project](tinyHandle h) {
        const tinyFS& fs = project->fs();
        const tinyFS::Node* node = fs.fNode(h);
        if (node && node->isFolder()) return ImVec4(0.3f, 0.3f, 0.35f, 0.8f);
        else return ImVec4(0.2f, 0.5f, 0.3f, 0.6f);
    };
    auto getDraggedColor = [](tinyHandle) { return ImVec4(0.8f, 0.6f, 0.2f, 0.8f); };
    auto getNormalHoveredColor = [project](tinyHandle h) {
        const tinyFS& fs = project->fs();
        const tinyFS::Node* node = fs.fNode(h);
        if (node && node->isFolder()) return ImVec4(0.4f, 0.4f, 0.45f, 0.9f);
        else return ImVec4(0.3f, 0.6f, 0.4f, 0.8f);
    };
    auto getDraggedHoveredColor = [](tinyHandle) { return ImVec4(0.9f, 0.7f, 0.3f, 0.9f); };
    auto clearDragState = []() { HierarchyState::draggedFileNode = tinyHandle(); };
    auto clearOtherSelection = [](tinyHandle) { HierarchyState::selectedSceneNode = tinyHandle(); };

    RenderGenericNodeHierarchy(
        fileHandle, isSelected, setSelected, isDragged, setDragged, isExpanded, setExpanded,
        getName, hasChildren, getChildren, renderDragSource, renderDropTarget, renderContextMenu,
        getNormalColor, getDraggedColor, getNormalHoveredColor, getDraggedHoveredColor,
        clearDragState, clearOtherSelection, depth
    );
}

// ============================================================================
// UI INITIALIZATION
// ============================================================================

void tinyApp::initUI() {
    uiBackend = new tinyUI::UIBackend_Vulkan();

    tinyUI::VulkanBackendData vkData;
    vkData.instance = instanceVk->instance;
    vkData.physicalDevice = deviceVk->pDevice;
    vkData.device = deviceVk->device;
    vkData.queueFamily = deviceVk->queueFamilyIndices.graphicsFamily.value();
    vkData.queue = deviceVk->graphicsQueue;
    vkData.renderPass = renderer->getOffscreenRenderPass();  // Use offscreen for UI overlay
    vkData.minImageCount = 2;
    vkData.imageCount = renderer->getSwapChainImageCount();
    vkData.msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    
    uiBackend->setVulkanData(vkData);
    tinyUI::Exec::Init(uiBackend, windowManager->window);

// ===== Misc =====

    // Set the active scene to main scene by default
    HierarchyState::activeSceneHandle = project->mainSceneHandle;
    activeScene = project->scene(HierarchyState::activeSceneHandle);
}

// ============================================================================
// MAIN UI RENDERING FUNCTION
// ============================================================================

void tinyApp::renderUI() {
    auto& fpsRef = *fpsManager;
    auto& camRef = *project->camera();

    static float frameTime = 1.0f;
    static const float printInterval = 1.0f; // Print fps every second
    static float currentFps = 0.0f;

    float dTime = fpsRef.deltaTime;

    // ===== DEBUG PANEL WINDOW =====
    if (tinyUI::Exec::Begin("Debug Panel", nullptr, 0)) {
        // FPS Info (once every printInterval)
        frameTime += dTime;
        if (frameTime >= printInterval) {
            currentFps = fpsRef.currentFPS;
            frameTime = 0.0f;
        }

        ImGui::Text("FPS: %.1f", currentFps);
        ImGui::Separator();
        
        ImGui::Text("Camera");
        ImGui::Text("Pos: (%.2f, %.2f, %.2f)", camRef.pos.x, camRef.pos.y, camRef.pos.z);
        ImGui::Text("Fwd: (%.2f, %.2f, %.2f)", camRef.forward.x, camRef.forward.y, camRef.forward.z);
        ImGui::Text("Rg:  (%.2f, %.2f, %.2f)", camRef.right.x, camRef.right.y, camRef.right.z);
        ImGui::Text("Up:  (%.2f, %.2f, %.2f)", camRef.up.x, camRef.up.y, camRef.up.z);

        tinyUI::Exec::End();
    }

    // ===== HIERARCHY WINDOW - Scene & File System =====
    static bool showHierarchy = true;
    if (tinyUI::Exec::Begin("Hierarchy", &showHierarchy)) {
        // Get active scene
        tinyHandle activeSceneHandle = HierarchyState::activeSceneHandle;

        if (!activeScene) {
            ImGui::Text("No active scene");
        } else {
            // Header with scene selector
            ImGui::Text("Active Scene: [Handle: %u.%u]", activeSceneHandle.index, activeSceneHandle.version);
            ImGui::Separator();

            // Get available height for split view
            float availHeight = ImGui::GetContentRegionAvail().y;
            float splitterHeight = 4.0f;
            
            float topHeight = availHeight * HierarchyState::splitterPos - splitterHeight * 0.5f;
            float bottomHeight = availHeight * (1.0f - HierarchyState::splitterPos) - splitterHeight * 0.5f;
            
            // ===== TOP: SCENE HIERARCHY =====
            ImGui::Text("Scene Hierarchy");
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0)); // Transparent background
            ImGui::BeginChild("SceneHierarchy", ImVec2(0, topHeight), true);
            RenderSceneNodeHierarchy(project.get(), activeScene, activeScene->rootHandle());
            ImGui::EndChild();
            ImGui::PopStyleColor();
            
            // ===== HORIZONTAL SPLITTER =====
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));

            ImGui::Button("##HierarchySplitter", ImVec2(-1, splitterHeight));

            if (ImGui::IsItemActive()) {
                float delta = ImGui::GetIO().MouseDelta.y;
                HierarchyState::splitterPos += delta / availHeight;
                HierarchyState::splitterPos = std::clamp(HierarchyState::splitterPos, 0.1f, 0.9f);
            }
            
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            }
            
            ImGui::PopStyleColor(3);
            
            // ===== BOTTOM: FILE SYSTEM HIERARCHY =====
            ImGui::Text("File System");
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0)); // Transparent background
            ImGui::BeginChild("FileHierarchy", ImVec2(0, bottomHeight), true);
            RenderFileNodeHierarchy(project.get(), project->fs().rootHandle());
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        tinyUI::Exec::End();
    }
}


void tinyApp::updateActiveScene() {
    activeScene = project->scene(HierarchyState::activeSceneHandle);

    activeScene->setFStart({
        renderer->getCurrentFrame(),
        fpsManager->deltaTime
    });
    activeScene->update();
}
