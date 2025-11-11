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
// HIERARCHY WINDOW - Scene & File Trees
// ============================================================================

static void RenderSceneNodeHierarchy(tinyProject* project, tinySceneRT* scene, tinyHandle nodeHandle, int depth = 0) {
    if (!scene || !nodeHandle.valid()) return;
    
    const tinyNodeRT* node = scene->node(nodeHandle);
    if (!node) return;
    
    ImGui::PushID(static_cast<int>(nodeHandle.index));
    
    bool hasChildren = !node->childrenHandles.empty();
    bool isSelected = (HierarchyState::selectedSceneNode == nodeHandle);
    bool isDragged = (HierarchyState::draggedSceneNode == nodeHandle);
    
    // Tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;
    
    // Highlight if being dragged
    if (isDragged) {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.8f, 0.6f, 0.2f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.9f, 0.7f, 0.3f, 0.9f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.26f, 0.59f, 0.98f, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.6f));
    }
    
    // Check if should be expanded
    bool forceOpen = HierarchyState::isExpanded(nodeHandle, true);
    if (forceOpen) ImGui::SetNextItemOpen(true);
    
    bool nodeOpen = ImGui::TreeNodeEx(node->name.c_str(), flags);
    
    // Track expansion state
    if (hasChildren && ImGui::IsItemToggledOpen()) {
        HierarchyState::setExpanded(nodeHandle, true, nodeOpen);
    }
    
    ImGui::PopStyleColor(2);
    
    // Selection on click
    if (ImGui::IsItemClicked()) {
        HierarchyState::selectedSceneNode = nodeHandle;
        HierarchyState::selectedFileNode = tinyHandle(); // Clear file selection
    }
    
    // Drag source
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        HierarchyState::draggedSceneNode = nodeHandle;
        DragDropPayloads::SceneNodePayload payload;
        payload.nodeHandle = nodeHandle;
        strncpy(payload.nodeName, node->name.c_str(), 63);
        payload.nodeName[63] = '\0';
        
        ImGui::SetDragDropPayload("SCENE_NODE", &payload, sizeof(payload));
        ImGui::Text("Moving: %s", node->name.c_str());
        ImGui::EndDragDropSource();
    }
    
    // Drop target - reparent scene nodes or instantiate scenes
    if (!isDragged && ImGui::BeginDragDropTarget()) {
        // Accept scene node reparenting
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE")) {
            DragDropPayloads::SceneNodePayload* data = (DragDropPayloads::SceneNodePayload*)payload->Data;
            if (data->nodeHandle != nodeHandle) {
                // Attempt reparenting
                if (scene->reparentNode(data->nodeHandle, nodeHandle)) {
                    HierarchyState::setExpanded(nodeHandle, true, true); // Auto-expand parent
                    HierarchyState::selectedSceneNode = data->nodeHandle; // Keep selection
                }
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
                
                // Safety: don't drop scene into itself
                if (sceneRegistryHandle != activeSceneHandle) {
                    // Instantiate scene at this node
                    project->addSceneInstance(sceneRegistryHandle, activeSceneHandle, nodeHandle);
                    HierarchyState::setExpanded(nodeHandle, true, true); // Auto-expand
                }
            }
            HierarchyState::draggedFileNode = tinyHandle();
        }
        
        ImGui::EndDragDropTarget();
    }
    
    // Clear drag state on mouse release
    if (HierarchyState::draggedSceneNode.valid() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        HierarchyState::draggedSceneNode = tinyHandle();
    }
    
    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        ImGui::Text("Node: %s", node->name.c_str());
        ImGui::Separator();

        if (ImGui::MenuItem("Add Child Node")) {
            // Would create new empty node
            scene->addNode("NewNode", nodeHandle);
        }
        if (ImGui::MenuItem("Delete") && nodeHandle != scene->rootHandle()) {
            scene->removeNode(nodeHandle);
        }
        if (ImGui::MenuItem("Flatten")) {
            scene->flattenNode(nodeHandle);
        }
        ImGui::EndPopup();
    }
    
    // Render children
    if (nodeOpen && hasChildren) {
        for (const auto& childHandle : node->childrenHandles) {
            RenderSceneNodeHierarchy(project, scene, childHandle, depth + 1);
        }
        ImGui::TreePop();
    }
    
    ImGui::PopID();
}

static void RenderFileNodeHierarchy(tinyProject* project, tinyHandle fileHandle, int depth = 0) {
    const tinyFS& fs = project->fs();
    const tinyFS::Node* node = fs.fNode(fileHandle);
    if (!node) return;
    
    ImGui::PushID(static_cast<int>(fileHandle.index));
    
    bool isFolder = node->isFolder();
    bool hasChildren = !node->children.empty();
    bool isSelected = (HierarchyState::selectedFileNode == fileHandle);
    bool isDragged = (HierarchyState::draggedFileNode == fileHandle);
    
    if (isFolder) {
        // Folder rendering
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
        if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;
        
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.3f, 0.35f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.4f, 0.4f, 0.45f, 0.9f));
        
        bool forceOpen = HierarchyState::isExpanded(fileHandle, false);
        if (forceOpen) ImGui::SetNextItemOpen(true);
        
        bool nodeOpen = ImGui::TreeNodeEx(node->name.c_str(), flags);
        
        if (hasChildren && ImGui::IsItemToggledOpen()) {
            HierarchyState::setExpanded(fileHandle, false, nodeOpen);
        }
        
        ImGui::PopStyleColor(2);
        
        if (ImGui::IsItemClicked()) {
            HierarchyState::selectedFileNode = fileHandle;
            HierarchyState::selectedSceneNode = tinyHandle();
        }
        
        if (nodeOpen && hasChildren) {
            for (const auto& childHandle : node->children) {
                RenderFileNodeHierarchy(project, childHandle, depth + 1);
            }
            ImGui::TreePop();
        }
    } else {
        // File rendering
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;
        
        if (isDragged) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.8f, 0.6f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.9f, 0.7f, 0.3f, 0.9f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.5f, 0.3f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.6f, 0.4f, 0.8f));
        }
        
        ImGui::TreeNodeEx(node->name.c_str(), flags);
        
        ImGui::PopStyleColor(2);
        
        if (ImGui::IsItemClicked()) {
            HierarchyState::selectedFileNode = fileHandle;
            HierarchyState::selectedSceneNode = tinyHandle();
        }
        
        // Drag source for files
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            HierarchyState::draggedFileNode = fileHandle;
            
            DragDropPayloads::FileNodePayload payload;
            payload.fileHandle = fileHandle;
            payload.dataTypeHandle = fs.fTypeHandle(fileHandle); // Get typeHandle from file
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
        
        // Clear drag state on release
        if (HierarchyState::draggedFileNode.valid() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            HierarchyState::draggedFileNode = tinyHandle();
        }
    }
    
    ImGui::PopID();
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
        
        // Camera info - Display only (not editable)
        if (ImGui::CollapsingHeader("Camera")) {
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", camRef.pos.x, camRef.pos.y, camRef.pos.z);
            ImGui::Text("Forward: (%.2f, %.2f, %.2f)", camRef.forward.x, camRef.forward.y, camRef.forward.z);
            ImGui::Text("Right: (%.2f, %.2f, %.2f)", camRef.right.x, camRef.right.y, camRef.right.z);
            ImGui::Text("Up: (%.2f, %.2f, %.2f)", camRef.up.x, camRef.up.y, camRef.up.z);
        }
        tinyUI::Exec::End();
    }

    // ===== HIERARCHY WINDOW - Scene & File System =====
    static bool showHierarchy = true;
    if (tinyUI::Exec::Begin("Hierarchy", &showHierarchy)) {
        // Get active scene
        tinyHandle activeSceneHandle = HierarchyState::activeSceneHandle.valid() ? 
            HierarchyState::activeSceneHandle : project->mainSceneHandle;
        tinySceneRT* activeScene = project->scene(activeSceneHandle);
        
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
            ImGui::BeginChild("SceneHierarchy", ImVec2(0, topHeight), true);
            RenderSceneNodeHierarchy(project.get(), activeScene, activeScene->rootHandle());
            ImGui::EndChild();
            
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
            ImGui::BeginChild("FileHierarchy", ImVec2(0, bottomHeight), true);
            RenderFileNodeHierarchy(project.get(), project->fs().rootHandle());
            ImGui::EndChild();
        }
        tinyUI::Exec::End();
    }
}
