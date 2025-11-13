// tinyApp_imgui.cpp - UI Implementation & Testing
#include "tinyApp/tinyApp.hpp"
#include "tinyEngine/tinyLoader.hpp"
#include "tinySystem/tinyUI.hpp"

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

// Hierarchy state tracking
namespace HierarchyState {
    static tinyHandle activeSceneHandle;
    static float splitterPos = 0.5f;
    
    // Expanded nodes tracking
    static std::unordered_set<uint64_t> expandedNodes;
    
    // Selection tracking
    static typeHandle selectedNode;
    
    // Drag state
    static typeHandle draggedNode;

    // Note: tinyHandle is hashed by default

    static bool isExpanded(typeHandle th) {
        return expandedNodes.find(th.hash()) != expandedNodes.end();
    }

    static void setExpanded(typeHandle th, bool expanded) {
        if (expanded) expandedNodes.insert(th.hash());
        else          expandedNodes.erase(th.hash());
    }
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
    vkData.renderPass = renderer->getMainRenderPass();
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

// ===========================================================================
// UTILITIES
// ===========================================================================

static bool renderMenuItemToggle(const char* labelPending, const char* labelFinished, bool finished) {
    const char* label = finished ? labelFinished : labelPending;
    return ImGui::MenuItem(label, nullptr, finished, !finished);
}


// Payload data
struct Payload {

    template<typename T>
    static Payload make(tinyHandle h, std::string name) {
        Payload payload;
        payload.tHandle = typeHandle::make<T>(h);
        strncpy(payload.name, name.c_str(), 63);
        payload.name[63] = '\0';
        return payload;
    }

    template<typename T>
    bool isType() const {
        return tHandle.isType<T>();
    }

    tinyHandle handle() const {
        return tHandle.handle;
    }

    typeHandle tHandle;
    char name[64];
};

// ============================================================================
// GENERALIZED TREE NODE RENDERING - Abstracts common ImGui tree logic
// ============================================================================

template<typename FuncType>
using Func = std::function<FuncType>;

template<typename FuncType>
using CFunc = const Func<FuncType>;

static void RenderGenericNodeHierarchy(
    tinyHandle nodeHandle, int depth,
    // Lambdas for node state management
    CFunc<std::string(tinyHandle)>& getName,
    CFunc<bool(tinyHandle)>& isSelected,  CFunc<void(tinyHandle)>& setSelected,
    CFunc<bool(tinyHandle)>& isDragged,   CFunc<void(tinyHandle)>& setDragged,  CFunc<void()>&           clearDragState,
    CFunc<bool(tinyHandle)>& isExpanded,  CFunc<void(tinyHandle, bool)>& setExpanded,
    CFunc<bool(tinyHandle)>& hasChildren, CFunc<std::vector<tinyHandle>(tinyHandle)>& getChildren,
    CFunc<void(tinyHandle)>& renderDragSource, CFunc<void(tinyHandle)>& renderDropTarget, CFunc<void(tinyHandle)>& renderContextMenu,
    CFunc<ImVec4(tinyHandle)>& getNormalColor,        CFunc<ImVec4(tinyHandle)>& getDraggedColor,
    CFunc<ImVec4(tinyHandle)>& getNormalHoveredColor, CFunc<ImVec4(tinyHandle)>& getDraggedHoveredColor
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

    bool nodeOpen = ImGui::TreeNodeEx(getName(nodeHandle).c_str(), flags);
    if (hasChild && ImGui::IsItemToggledOpen()) setExpanded(nodeHandle, nodeOpen);

    ImGui::PopStyleColor(2);

    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        setSelected(nodeHandle);
    }

    renderDragSource(nodeHandle);
    renderDropTarget(nodeHandle);
    renderContextMenu(nodeHandle);

    if (dragged && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) clearDragState();

    if (nodeOpen && hasChild) {
        for (const auto& child : getChildren(nodeHandle)) {
            RenderGenericNodeHierarchy(
                child, depth + 1,
                getName, isSelected, setSelected,
                isDragged, setDragged, clearDragState, isExpanded, setExpanded,
                hasChildren, getChildren, renderDragSource, renderDropTarget, renderContextMenu,
                getNormalColor, getDraggedColor, getNormalHoveredColor, getDraggedHoveredColor
            );
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

// ============================================================================
// HIERARCHY WINDOW - Scene & File Trees
// ============================================================================

#define MAKE_TH(T, h) typeHandle::make<T>(h)
#define MAKE_TH_DEF(T) typeHandle::make<T>()

static void RenderSceneNodeHierarchy(tinyProject* project, tinySceneRT* scene) {
    if (!scene) return;

    tinyFS& fs = project->fs();

    auto getName = [scene](tinyHandle h) -> std::string {
        const tinyNodeRT* node = scene->node(h);
        return node ? node->name : "";
    };

    auto isSelected  = [](tinyHandle h) { return HierarchyState::selectedNode == MAKE_TH(tinyNodeRT, h); };
    auto setSelected = [](tinyHandle h) { HierarchyState::selectedNode = MAKE_TH(tinyNodeRT, h); };

    auto isDragged   = [](tinyHandle h) { return HierarchyState::draggedNode == MAKE_TH(tinyNodeRT, h); };
    auto setDragged  = [](tinyHandle h) { HierarchyState::draggedNode = MAKE_TH(tinyNodeRT, h); };
    auto clearDragState = []() { HierarchyState::draggedNode = typeHandle(); };

    auto isExpanded  = [](tinyHandle h) { return HierarchyState::isExpanded(MAKE_TH(tinyNodeRT, h)); };
    auto setExpanded = [](tinyHandle h, bool expanded) { HierarchyState::setExpanded(MAKE_TH(tinyNodeRT, h), expanded); };

    auto hasChildren = [scene](tinyHandle h) -> bool {
        const tinyNodeRT* node = scene->node(h);
        return node && !node->childrenHandles.empty();
    };
    auto getChildren = [scene](tinyHandle h) -> std::vector<tinyHandle> {
        if (const tinyNodeRT* node = scene->node(h)) {
            // Sort children by presence of children then alphabetically
            std::vector<tinyHandle> children = node->childrenHandles;
            std::sort(children.begin(), children.end(), [scene](tinyHandle a, tinyHandle b) {
                const tinyNodeRT* nodeA = scene->node(a);
                const tinyNodeRT* nodeB = scene->node(b);
                bool aHasChildren = nodeA && !nodeA->childrenHandles.empty();
                bool bHasChildren = nodeB && !nodeB->childrenHandles.empty();
                if (aHasChildren != bHasChildren) return aHasChildren > bHasChildren;
                return nodeA->name < nodeB->name;
            });
            return children;
        }
        return std::vector<tinyHandle>();
    };

    auto renderDragSource = [scene](tinyHandle h) {
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            HierarchyState::draggedNode = MAKE_TH(tinyNodeRT, h);

            if (const tinyNodeRT* node = scene->node(h)) {
                Payload payload = Payload::make<tinyNodeRT>(h, node->name);

                ImGui::SetDragDropPayload("DRAG_NODE", &payload, sizeof(payload));
                ImGui::Text("Moving: %s", node->name.c_str());
            }
            ImGui::EndDragDropSource();
        }
    };
    auto renderDropTarget = [project, &fs, scene](tinyHandle h) {
        if (ImGui::BeginDragDropTarget()) {
            // Accept scene node reparenting
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DRAG_NODE")) {
                Payload* data = (Payload*)payload->Data;

                if (data->isType<tinyNodeRT>()) {
                    tinyHandle nodeHandle = data->handle();

                    if (scene->reparentNode(nodeHandle, h)) {
                        HierarchyState::setExpanded(MAKE_TH(tinyNodeRT, h), true); // Auto-expand parent
                        HierarchyState::selectedNode = MAKE_TH(tinyNodeRT, nodeHandle); // Keep selection
                    }
                } else if (data->isType<tinyNodeFS>()) {
                    tinyHandle fileHandle = data->handle();

                    typeHandle fTypeHdl = fs.fTypeHandle(fileHandle);

                    if (fTypeHdl.isType<tinySceneRT>()) {
                        tinyHandle sceneRegistryHandle = fTypeHdl.handle;

                        tinyHandle activeSceneHandle = HierarchyState::activeSceneHandle;

                        project->addSceneInstance(sceneRegistryHandle, activeSceneHandle, h);
                        HierarchyState::setExpanded(MAKE_TH(tinyNodeRT, h), true);
                    }
                }

                HierarchyState::draggedNode = typeHandle();
            }
            
            ImGui::EndDragDropTarget();
        }
    };
    auto renderContextMenu = [scene](tinyHandle h) {
        if (ImGui::BeginPopupContextItem()) {
            if (const tinyNodeRT* node = scene->node(h)) {
                ImGui::Text("Node: %s", node->name.c_str());
                ImGui::Separator();

                if (ImGui::MenuItem("Add Child")) scene->addNode("New Node", h);
                ImGui::Separator();

                bool canDelete = h != scene->rootHandle();
                if (ImGui::MenuItem("Delete", nullptr, false, canDelete))  scene->removeNode(h);
                if (ImGui::MenuItem("Flatten", nullptr, false, canDelete)) scene->flattenNode(h);
                ImGui::Separator();

                if (ImGui::MenuItem("Clear", nullptr, false, !node->childrenHandles.empty())) {
                    for (const auto& childHandle : node->childrenHandles) scene->removeNode(childHandle);
                }
                ImGui::Separator();

                // Special functions
                tinySceneRT::NWrap nWrap = scene->nWrap(h);

                if (tinyRT_ANIM3D* anim3D = nWrap.anim3D) {
                    for (auto& anime : anim3D->MAL()) {
                        if (ImGui::MenuItem(anime.first.c_str())) {
                            anim3D->play(anime.first, true);
                        }
                    }
                }

                if (tinyRT_SCRIPT* script = nWrap.script) {
                    // Do nothing
                }
            }
            ImGui::EndPopup();
        }
    };

    auto getNormalColor  = [](tinyHandle) { return ImVec4(0.26f, 0.59f, 0.98f, 0.4f); };
    auto getDraggedColor = [](tinyHandle) { return ImVec4(0.8f, 0.6f, 0.2f, 0.8f); };
    auto getNormalHoveredColor  = [](tinyHandle) { return ImVec4(0.26f, 0.59f, 0.98f, 0.6f); };
    auto getDraggedHoveredColor = [](tinyHandle) { return ImVec4(0.9f, 0.7f, 0.3f, 0.9f); };

    RenderGenericNodeHierarchy(
        scene->rootHandle(), 0,
        getName, isSelected, setSelected,
        isDragged, setDragged, clearDragState, isExpanded, setExpanded,
        hasChildren, getChildren, renderDragSource, renderDropTarget, renderContextMenu,
        getNormalColor, getDraggedColor, getNormalHoveredColor, getDraggedHoveredColor
    );
}

static void RenderFileNodeHierarchy(tinyProject* project) {
    tinyFS& fs = project->fs();

    auto getName = [&fs](tinyHandle h) -> std::string {
        const tinyFS::Node* node = fs.fNode(h);
        if (!node) return "";

        if (node->isFolder()) return std::string(node->name);

        tinyFS::TypeExt typeExt = fs.fTypeExt(h);
        return node->name + "." + typeExt.ext;
    };

    auto isSelected  = [](tinyHandle h) { return HierarchyState::selectedNode == MAKE_TH(tinyNodeFS, h); };
    auto setSelected = [](tinyHandle h) { HierarchyState::selectedNode = MAKE_TH(tinyNodeFS, h); };

    auto isDragged   = [](tinyHandle h) { return HierarchyState::draggedNode == MAKE_TH(tinyNodeFS, h); };
    auto setDragged  = [](tinyHandle h) { HierarchyState::draggedNode = MAKE_TH(tinyNodeFS, h); };
    auto clearDragState = []() { HierarchyState::draggedNode = typeHandle(); };

    auto isExpanded  = [](tinyHandle h) { return HierarchyState::isExpanded(MAKE_TH(tinyNodeFS, h)); };
    auto setExpanded = [](tinyHandle h, bool expanded) { HierarchyState::setExpanded(MAKE_TH(tinyNodeFS, h), expanded); };

    auto hasChildren = [&fs](tinyHandle h) -> bool {
        const tinyFS::Node* node = fs.fNode(h);
        return node && node->isFolder() && !node->children.empty();
    };
    auto getChildren = [&fs](tinyHandle h) -> std::vector<tinyHandle> {
        if (const tinyFS::Node* node = fs.fNode(h)) {
            std::vector<tinyHandle> children = node->children;

            // Sort by folder first, then extension type then name (both alphabetically)
            std::sort(children.begin(), children.end(), [&fs](tinyHandle a, tinyHandle b) {
                tinyFS::TypeExt typeA = fs.fTypeExt(a);
                tinyFS::TypeExt typeB = fs.fTypeExt(b);

                if (typeA.ext != typeB.ext) return typeA.ext < typeB.ext;
                return fs.fNode(a)->name < fs.fNode(b)->name;
            });
            return children;
        }
        return std::vector<tinyHandle>();
    };

    auto renderDragSource = [&fs](tinyHandle h) {
        if (const tinyFS::Node* node = fs.fNode(h)) {
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                HierarchyState::draggedNode = MAKE_TH(tinyNodeFS, h);

                Payload payload;
                payload.tHandle = MAKE_TH(tinyNodeFS, h);
                strncpy(payload.name, node->name.c_str(), 63);
                payload.name[63] = '\0';

                ImGui::SetDragDropPayload("DRAG_NODE", &payload, sizeof(payload));
                ImGui::Text("Dragging: %s", node->name.c_str());

                // Show type info if has data
                tinyFS::TypeExt typeExt = fs.fTypeExt(h);
                ImGui::Separator();
                ImGui::TextColored(ImVec4(typeExt.color[0], typeExt.color[1], typeExt.color[2], 1.0f),
                                   "Type: %s", typeExt.c_str());

                ImGui::EndDragDropSource();
            }
        }
    };
    auto renderDropTarget  = [&fs](tinyHandle h) {
        // Perform move operation within the file system

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DRAG_NODE")) {
                Payload* data = (Payload*)payload->Data;

                // Move the dragged node into this folder
                if (data->isType<tinyNodeFS>() && fs.fMove(data->handle(), h)) {
                    HierarchyState::setExpanded(MAKE_TH(tinyNodeFS, h), true); // Auto-expand parent
                    HierarchyState::selectedNode = MAKE_TH(tinyNodeFS, data->handle()); // Keep selection
                }

                HierarchyState::draggedNode = typeHandle();
            }
            ImGui::EndDragDropTarget();
        }
    };

    auto renderContextMenu = [&fs](tinyHandle h) {
        if (ImGui::BeginPopupContextItem()) {
            if (const tinyFS::Node* node = fs.fNode(h)) {
                bool deletable = node->deletable();
                const char* name = node->name.c_str();

                ImGui::Text("%s", name);
                ImGui::Separator();

                // Special types methods
                typeHandle dataType = fs.fTypeHandle(h);

                if (dataType.isType<tinySceneRT>()) {
                    tinySceneRT* scene = fs.rGet<tinySceneRT>(dataType.handle);

                    if (renderMenuItemToggle("Make Active", "Active", HierarchyState::activeSceneHandle == dataType.handle)) {
                        HierarchyState::activeSceneHandle = dataType.handle;
                    }

                    if (renderMenuItemToggle("Cleanse", "Cleansed", scene->isClean())) {
                        scene->cleanse();
                    }
                }

                
                if (dataType.isType<tinyScript>()) {
                    tinyScript* script = fs.rGet<tinyScript>(dataType.handle);

                    if (ImGui::MenuItem("Compile")) {
                        script->compile();
                    }
                }

                // Folder methods
                if (node->isFolder()) {
                    if (ImGui::MenuItem("Add Folder")) fs.addFolder(h, "New Folder");
                }

                // Delete methods
                ImGui::Separator();
                if (ImGui::MenuItem("Delete", nullptr, nullptr, deletable)) fs.fRemove(h);
            }
            ImGui::EndPopup();
        }
    };
    auto getNormalColor = [&fs](tinyHandle h) {
        const tinyFS::Node* node = fs.fNode(h);
        if (node && node->isFolder()) return ImVec4(0.3f, 0.3f, 0.35f, 0.8f);

        tinyFS::TypeExt typeExt = fs.fTypeExt(h);
        typeExt.color[0] *= 0.2f;
        typeExt.color[1] *= 0.2f;
        typeExt.color[2] *= 0.2f;
        return ImVec4(typeExt.color[0], typeExt.color[1], typeExt.color[2], 0.8f);
    };
    auto getDraggedColor = [](tinyHandle) { return ImVec4(0.8f, 0.6f, 0.2f, 0.8f); };
    auto getNormalHoveredColor = [&fs](tinyHandle h) {
        const tinyFS::Node* node = fs.fNode(h);
        if (node && node->isFolder()) return ImVec4(0.4f, 0.4f, 0.45f, 0.9f);

        tinyFS::TypeExt typeExt = fs.fTypeExt(h);
        typeExt.color[0] *= 0.5f;
        typeExt.color[1] *= 0.5f;
        typeExt.color[2] *= 0.5f;
        return ImVec4(typeExt.color[0], typeExt.color[1], typeExt.color[2], 0.9f);
    };
    auto getDraggedHoveredColor = [](tinyHandle) { return ImVec4(0.9f, 0.7f, 0.3f, 0.9f); };

    RenderGenericNodeHierarchy(
        fs.rootHandle(), 0,
        getName, isSelected, setSelected,
        isDragged, setDragged, clearDragState, isExpanded, setExpanded,
        hasChildren, getChildren, renderDragSource, renderDropTarget, renderContextMenu,
        getNormalColor, getDraggedColor, getNormalHoveredColor, getDraggedHoveredColor
    );
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

    float deltaTime = fpsRef.deltaTime;

    // ===== DEBUG PANEL WINDOW =====
    static bool showThemeEditor = false;
    if (tinyUI::Exec::Begin("Debug Panel")) {
        // FPS Info (once every printInterval)
        frameTime += deltaTime;
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

        ImGui::Separator();
        if (ImGui::Button("Theme Editor")) {
            showThemeEditor = !showThemeEditor;
        }

        tinyUI::Exec::End();
    }
    
    // ===== HIERARCHY WINDOW - Scene & File System =====
    if (tinyUI::Exec::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse)) {
        tinyHandle activeSceneHandle = HierarchyState::activeSceneHandle;

        if (!activeScene) {
            ImGui::Text("No active scene");
        } else {
            // Header with scene selector
            ImGui::Text("Active Scene: [Handle: %u.%u]", activeSceneHandle.index, activeSceneHandle.version);
            ImGui::Separator();

            float fontScale = tinyUI::Exec::GetTheme().fontScale;

            // Get available height for split view
            float availHeight = ImGui::GetContentRegionAvail().y;
            float splitterHeight = 4.0f;
            
            float topHeight = availHeight * HierarchyState::splitterPos - splitterHeight;
            float bottomHeight = availHeight * (1.0f - HierarchyState::splitterPos) - splitterHeight;

            // ===== TOP: SCENE HIERARCHY =====
            ImGui::Text("Scene Hierarchy");
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0)); // Transparent background
            ImGui::BeginChild("SceneHierarchy", ImVec2(0, topHeight), true);
            RenderSceneNodeHierarchy(project.get(), activeScene);
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
            RenderFileNodeHierarchy(project.get());
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        tinyUI::Exec::End();
    }

    // ===== THEME EDITOR WINDOW =====
    if (showThemeEditor) {
        if (tinyUI::Exec::Begin("Theme Editor", &showThemeEditor)) {
            tinyUI::Theme& theme = tinyUI::Exec::GetTheme();

            if (ImGui::CollapsingHeader("Colors")) {
                ImGui::ColorEdit4("Text", &theme.text.x);
                ImGui::ColorEdit4("Text Disabled", &theme.textDisabled.x);
                ImGui::ColorEdit4("Window Background", &theme.windowBg.x);
                ImGui::ColorEdit4("Child Background", &theme.childBg.x);
                ImGui::ColorEdit4("Border", &theme.border.x);
                ImGui::ColorEdit4("Title Background", &theme.titleBg.x);
                ImGui::ColorEdit4("Title Background Active", &theme.titleBgActive.x);
                ImGui::ColorEdit4("Title Background Collapsed", &theme.titleBgCollapsed.x);
                ImGui::ColorEdit4("Button", &theme.button.x);
                ImGui::ColorEdit4("Button Hovered", &theme.buttonHovered.x);
                ImGui::ColorEdit4("Button Active", &theme.buttonActive.x);
                ImGui::ColorEdit4("Header", &theme.header.x);
                ImGui::ColorEdit4("Header Hovered", &theme.headerHovered.x);
                ImGui::ColorEdit4("Header Active", &theme.headerActive.x);
                ImGui::ColorEdit4("Frame Background", &theme.frameBg.x);
                ImGui::ColorEdit4("Frame Background Hovered", &theme.frameBgHovered.x);
                ImGui::ColorEdit4("Frame Background Active", &theme.frameBgActive.x);
                ImGui::ColorEdit4("Scrollbar Background", &theme.scrollbarBg.x);
                ImGui::ColorEdit4("Scrollbar Grab", &theme.scrollbarGrab.x);
                ImGui::ColorEdit4("Scrollbar Grab Hovered", &theme.scrollbarGrabHovered.x);
                ImGui::ColorEdit4("Scrollbar Grab Active", &theme.scrollbarGrabActive.x);
            }

            if (ImGui::CollapsingHeader("Sizes & Rounding")) {
                ImGui::DragFloat("Scrollbar Size", &theme.scrollbarSize, 0.01f, 0.0f, 20.0f);
                ImGui::DragFloat("Scrollbar Rounding", &theme.scrollbarRounding, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Frame Rounding", &theme.frameRounding, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Child Rounding", &theme.childRounding, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Button Rounding", &theme.buttonRounding, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Window Rounding", &theme.windowRounding, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Window Border Size", &theme.windowBorderSize, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("Font Scale", &theme.fontScale, 0.01f, 0.5f, 2.0f);
            }

            if (ImGui::Button("Apply")) {
                tinyUI::Exec::ApplyTheme();
            }

            tinyUI::Exec::End();
        }
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
