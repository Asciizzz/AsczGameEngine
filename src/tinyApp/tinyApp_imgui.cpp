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
    static bool isActiveScene(tinyHandle h) {
        return activeSceneHandle == h;
    }

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

template<typename FX>
using Func = std::function<FX>;

template<typename FX>
using CFunc = const Func<FX>;

#define M_CLICKED(M_state) ImGui::IsItemHovered() && ImGui::IsMouseReleased(M_state) && !ImGui::IsMouseDragging(M_state)
#define M_HOVERED(M_state) ImGui::IsItemHovered() && !ImGui::IsMouseDragging(M_state)

#define IMVEC4_COLOR(ext) ImVec4(ext.color[0], ext.color[1], ext.color[2], 1.0f)



static bool RenderMenuItemToggle(const char* labelPending, const char* labelFinished, bool finished) {
    const char* label = finished ? labelFinished : labelPending;
    return ImGui::MenuItem(label, nullptr, finished, !finished);
}

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

template<
    typename LabelActiveFunc,
    typename ActiveColorFunc,
    typename ActiveConditionFunc,
    typename DragMethodFunc,
    typename ClickMethodFunc,
    typename HoverMethodFunc
>
static void RenderDragField(
    LabelActiveFunc&& labelActive,
    const char* labelInactive,
    ActiveColorFunc&& activeColor,
    ImVec4 inactiveColor,
    ActiveConditionFunc&& activeCondition,
    DragMethodFunc&& dragMethod,
    ClickMethodFunc&& clickMethod,
    HoverMethodFunc&& hoverMethod
) {
    bool active = activeCondition();
    const char* label = active ? labelActive() : labelInactive;
    ImVec4 borderColor = active ? activeColor() : inactiveColor;

    ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4)); // Adjust padding
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // transparent background
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0)); // transparent hover
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0)); // transparent active

    if (!active) {
        ImGui::PushStyleColor(ImGuiCol_Text, inactiveColor);
    }

    if (ImGui::Button(label, ImVec2(-1, 0))) {
        clickMethod();
    }

    if (!active) {
        ImGui::PopStyleColor();
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    if (M_HOVERED(ImGuiMouseButton_Left)) hoverMethod();

    dragMethod();
}

// ============================================================================
// Node Tree Rendering Abstraction
// ============================================================================

// HOLY SHIT
template<
    typename GetNameFunc,
    typename IsSelectedFunc, typename SetSelectedFunc,
    typename IsDraggedFunc,  typename ClearDragFunc,
    typename IsExpandedFunc, typename SetExpandedFunc,
    typename HasChildrenFunc,typename GetChildrenFunc,
    typename RenderDragSourceFunc, typename RenderDropTargetFunc,
    typename RenderContextMenuFunc,typename RenderTooltipFunc,
    typename GetNormalColorFunc,        typename GetDraggedColorFunc,
    typename GetNormalHoveredColorFunc, typename GetDraggedHoveredColorFunc
>
static void RenderGenericNodeHierarchy(
    tinyHandle nodeHandle, int depth,
    GetNameFunc&& getNameFunc,
    IsSelectedFunc&& isSelectedFunc,   SetSelectedFunc&& setSelectedFunc,
    IsDraggedFunc&& isDraggedFunc,     ClearDragFunc&& clearDragFunc,
    IsExpandedFunc&& isExpandedFunc,   SetExpandedFunc&& setExpandedFunc,
    HasChildrenFunc&& hasChildrenFunc, GetChildrenFunc&& getChildrenFunc,
    RenderDragSourceFunc&& renderDragSourceFunc,   RenderDropTargetFunc&& renderDropTargetFunc,
    RenderContextMenuFunc&& renderContextMenuFunc, RenderTooltipFunc&& renderTooltipFunc,
    GetNormalColorFunc&& getNormalColorFunc,       GetDraggedColorFunc&& getDraggedColorFunc,
    GetNormalHoveredColorFunc&& getNormalHoveredColorFunc, GetDraggedHoveredColorFunc&& getDraggedHoveredColorFunc
) {
    if (!nodeHandle.valid()) return;

    ImGui::PushID(static_cast<int>(nodeHandle.index));

    bool hasChild = hasChildrenFunc(nodeHandle);
    bool selected = isSelectedFunc(nodeHandle);
    bool dragged = isDraggedFunc(nodeHandle);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (!hasChild) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (selected) flags |= ImGuiTreeNodeFlags_Selected;

    ImVec4 headerColor = dragged ? getDraggedColorFunc(nodeHandle) : getNormalColorFunc(nodeHandle);
    ImVec4 headerHoveredColor = dragged ? getDraggedHoveredColorFunc(nodeHandle) : getNormalHoveredColorFunc(nodeHandle);

    ImGui::PushStyleColor(ImGuiCol_Header, headerColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, headerHoveredColor);

    bool forceOpen = isExpandedFunc(nodeHandle);
    if (forceOpen) ImGui::SetNextItemOpen(true);

    bool nodeOpen = ImGui::TreeNodeEx(getNameFunc(nodeHandle).c_str(), flags);
    if (hasChild && ImGui::IsItemToggledOpen()) setExpandedFunc(nodeHandle, nodeOpen);

    ImGui::PopStyleColor(2);

    if (M_CLICKED(ImGuiMouseButton_Left)) setSelectedFunc(nodeHandle);
    if (M_HOVERED(ImGuiMouseButton_Left)) renderTooltipFunc(nodeHandle);

    renderDragSourceFunc(nodeHandle);
    renderDropTargetFunc(nodeHandle);
    renderContextMenuFunc(nodeHandle);

    if (dragged && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) clearDragFunc();

    if (nodeOpen && hasChild) {
        for (const auto& child : getChildrenFunc(nodeHandle)) {
            RenderGenericNodeHierarchy(
                child, depth + 1,
                getNameFunc, isSelectedFunc, setSelectedFunc, isDraggedFunc, clearDragFunc, isExpandedFunc, setExpandedFunc,
                hasChildrenFunc, getChildrenFunc, renderDragSourceFunc, renderDropTargetFunc, renderContextMenuFunc, renderTooltipFunc,
                getNormalColorFunc, getDraggedColorFunc, getNormalHoveredColorFunc, getDraggedHoveredColorFunc
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

static void RenderSceneNodeHierarchy(tinyProject* project) {
    if (!project) return;
    tinySceneRT* scene = project->scene(HierarchyState::activeSceneHandle);
    if (!scene) return;

    tinyFS& fs = project->fs();

    RenderGenericNodeHierarchy(
        scene->rootHandle(), 0,
        [scene](tinyHandle h) -> std::string { const tinyNodeRT* node = scene->node(h); return node ? node->name : ""; },
        [](tinyHandle h) { return HierarchyState::selectedNode == MAKE_TH(tinyNodeRT, h); },
        [](tinyHandle h) { HierarchyState::selectedNode = MAKE_TH(tinyNodeRT, h); },
        [](tinyHandle h) { return HierarchyState::draggedNode == MAKE_TH(tinyNodeRT, h); },
        []() { HierarchyState::draggedNode = typeHandle(); },
        [](tinyHandle h) { return HierarchyState::isExpanded(MAKE_TH(tinyNodeRT, h)); },
        [](tinyHandle h, bool expanded) { HierarchyState::setExpanded(MAKE_TH(tinyNodeRT, h), expanded); },
        [scene](tinyHandle h) -> bool { const tinyNodeRT* node = scene->node(h); return node && !node->childrenHandles.empty(); },
        [scene](tinyHandle h) -> std::vector<tinyHandle> {
            if (const tinyNodeRT* node = scene->node(h)) {
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
        },
        [scene](tinyHandle h) {
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                HierarchyState::draggedNode = MAKE_TH(tinyNodeRT, h);
                if (const tinyNodeRT* node = scene->node(h)) {
                    Payload payload = Payload::make<tinyNodeRT>(h, node->name);
                    ImGui::SetDragDropPayload("DRAG_NODE", &payload, sizeof(payload));
                    ImGui::Text("Moving: %s", node->name.c_str());
                }
                ImGui::EndDragDropSource();
            }
        },
        [&fs, scene](tinyHandle h) {
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DRAG_NODE")) {
                    Payload* data = (Payload*)payload->Data;
                    if (data->isType<tinyNodeRT>()) {
                        tinyHandle nodeHandle = data->handle();
                        if (scene->reparentNode(nodeHandle, h)) {
                            HierarchyState::setExpanded(MAKE_TH(tinyNodeRT, h), true);
                            HierarchyState::selectedNode = MAKE_TH(tinyNodeRT, nodeHandle);
                        }
                    } else if (data->isType<tinyNodeFS>()) {
                        tinyHandle fHandle = data->handle();
                        typeHandle fTypeHdl = fs.fTypeHandle(fHandle);
                        tinyHandle handle = fTypeHdl.handle;
                        tinySceneRT::NWrap wrap = scene->Wrap(h);
                        if (fTypeHdl.isType<tinySceneRT>()) {
                            if (HierarchyState::activeSceneHandle != handle) {
                                scene->addScene(handle, h);
                            }
                        } else if (fTypeHdl.isType<tinyScript>()) { 
                            auto* scriptComp = scene->writeComp<tinyNodeRT::SCRIPT>(h);
                            scriptComp->assign(handle);
                        }
                    }
                    HierarchyState::draggedNode = typeHandle();
                }
                ImGui::EndDragDropTarget();
            }
        },
        [scene](tinyHandle h) {
            if (ImGui::BeginPopupContextItem()) {
                if (const tinyNodeRT* node = scene->node(h)) {
                    ImGui::Text("Node: %s", node->name.c_str());
                    ImGui::Separator();
                    if (ImGui::MenuItem("Add Child")) scene->addNode("New Node", h);
                    ImGui::Separator();
                    bool canDelete = h != scene->rootHandle();
                    if (ImGui::MenuItem("Delete", nullptr, false, canDelete))  scene->removeNode(h);
                    if (ImGui::MenuItem("Flatten", nullptr, false, canDelete)) scene->flattenNode(h);
                    if (ImGui::MenuItem("Clear", nullptr, false, !node->childrenHandles.empty())) {
                        std::vector<tinyHandle> children = node->childrenHandles;
                        for (const auto& childHandle : children) scene->removeNode(childHandle);
                    }
                    tinySceneRT::NWrap Wrap = scene->Wrap(h);
                    if (tinyRT_ANIM3D* anim3D = Wrap.anim3D) {
                        ImGui::Separator();
                        for (auto& anime : anim3D->MAL()) {
                            if (ImGui::MenuItem(anime.first.c_str())) {
                                anim3D->play(anime.first, true);
                            }
                        }
                    }
                    if (tinyRT_SCRIPT* script = Wrap.script) {
                        // Do nothing
                    }
                }
                ImGui::EndPopup();
            }
        },
        [scene](tinyHandle h) {
            if (const tinyNodeRT* node = scene->node(h)) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", node->name.c_str());
                ImGui::Separator();
                tinySceneRT::NWrap wrap = scene->Wrap(h);
                std::string compList = "";
                if (wrap.trfm3D) compList += "[TRFM3D] ";
                if (wrap.meshRD) compList += "[MESHRD] ";
                if (wrap.bone3D) compList += "[BONE3D] ";
                if (wrap.skel3D) compList += "[SKEL3D] ";
                if (wrap.anim3D) compList += "[ANIM3D] ";
                if (wrap.script) compList += "[SCRIPT] ";
                if (compList.empty()) compList = "[None]";
                ImGui::Text("%s", compList.c_str());
                ImGui::EndTooltip();
            }
        },
        [](tinyHandle) { return ImVec4(0.26f, 0.59f, 0.98f, 0.4f); },
        [](tinyHandle) { return ImVec4(0.8f, 0.6f, 0.2f, 0.8f); },
        [](tinyHandle) { return ImVec4(0.26f, 0.59f, 0.98f, 0.6f); },
        [](tinyHandle) { return ImVec4(0.9f, 0.7f, 0.3f, 0.9f); }
    );
}

static void RenderFileNodeHierarchy(tinyProject* project) {
    tinyFS& fs = project->fs();

    RenderGenericNodeHierarchy(
        fs.rootHandle(), 0,
        [&fs](tinyHandle h) -> std::string {
            const tinyFS::Node* node = fs.fNode(h);
            if (!node) return "";
            if (node->isFolder()) return std::string(node->name);
            tinyFS::TypeExt typeExt = fs.fTypeExt(h);
            return node->name + "." + typeExt.ext;
        },
        [](tinyHandle h) { return HierarchyState::selectedNode == MAKE_TH(tinyNodeFS, h); },
        [](tinyHandle h) { HierarchyState::selectedNode = MAKE_TH(tinyNodeFS, h); },
        [](tinyHandle h) { return HierarchyState::draggedNode == MAKE_TH(tinyNodeFS, h); },
        []() { HierarchyState::draggedNode = typeHandle(); },
        [](tinyHandle h) { return HierarchyState::isExpanded(MAKE_TH(tinyNodeFS, h)); },
        [](tinyHandle h, bool expanded) { HierarchyState::setExpanded(MAKE_TH(tinyNodeFS, h), expanded); },
        [&fs](tinyHandle h) -> bool {
            const tinyFS::Node* node = fs.fNode(h);
            return node && node->isFolder() && !node->children.empty();
        },
        [&fs](tinyHandle h) -> std::vector<tinyHandle> {
            if (const tinyFS::Node* node = fs.fNode(h)) {
                std::vector<tinyHandle> children = node->children;
                std::sort(children.begin(), children.end(), [&fs](tinyHandle a, tinyHandle b) {
                    tinyFS::TypeExt typeA = fs.fTypeExt(a);
                    tinyFS::TypeExt typeB = fs.fTypeExt(b);
                    if (typeA.ext != typeB.ext) return typeA.ext < typeB.ext;
                    return fs.fNode(a)->name < fs.fNode(b)->name;
                });
                return children;
            }
            return std::vector<tinyHandle>();
        },
        [&fs](tinyHandle h) {
            if (const tinyFS::Node* node = fs.fNode(h)) {
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    HierarchyState::draggedNode = MAKE_TH(tinyNodeFS, h);
                    Payload payload;
                    payload.tHandle = MAKE_TH(tinyNodeFS, h);
                    strncpy(payload.name, node->name.c_str(), 63);
                    payload.name[63] = '\0';
                    ImGui::SetDragDropPayload("DRAG_NODE", &payload, sizeof(payload));
                    ImGui::Text("Dragging: %s", node->name.c_str());
                    tinyFS::TypeExt typeExt = fs.fTypeExt(h);
                    ImGui::Separator();
                    ImGui::TextColored(IMVEC4_COLOR(typeExt), "Type: %s", typeExt.c_str());
                    ImGui::EndDragDropSource();
                }
            }
        },
        [&fs](tinyHandle h) {
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DRAG_NODE")) {
                    Payload* data = (Payload*)payload->Data;
                    if (data->isType<tinyNodeFS>() && fs.fMove(data->handle(), h)) {
                        HierarchyState::setExpanded(MAKE_TH(tinyNodeFS, h), true);
                        HierarchyState::selectedNode = MAKE_TH(tinyNodeFS, data->handle());
                    }
                    HierarchyState::draggedNode = typeHandle();
                }
                ImGui::EndDragDropTarget();
            }
        },
        [&fs](tinyHandle h) {
            if (ImGui::BeginPopupContextItem()) {
                if (const tinyFS::Node* node = fs.fNode(h)) {
                    bool deletable = node->deletable();
                    const char* name = node->name.c_str();
                    ImGui::Text("%s", name);
                    ImGui::Separator();
                    typeHandle dataType = fs.fTypeHandle(h);
                    if (dataType.isType<tinySceneRT>()) {
                        tinySceneRT* scene = fs.rGet<tinySceneRT>(dataType.handle);
                        if (RenderMenuItemToggle("Make Active", "Active", HierarchyState::isActiveScene(dataType.handle))) {
                            HierarchyState::activeSceneHandle = dataType.handle;
                        }
                        if (RenderMenuItemToggle("Cleanse", "Cleansed", scene->isClean())) {
                            scene->cleanse();
                        }
                    }
                    if (dataType.isType<tinyScript>()) {
                        tinyScript* script = fs.rGet<tinyScript>(dataType.handle);
                        if (ImGui::MenuItem("Compile")) {
                            script->compile();
                        }
                    }
                    if (node->isFolder()) {
                        if (ImGui::MenuItem("Add Folder")) fs.addFolder(h, "New Folder");
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete", nullptr, nullptr, deletable)) fs.fRemove(h);
                }
                ImGui::EndPopup();
            }
        },
        [&fs](tinyHandle h) {
            if (const tinyFS::Node* node = fs.fNode(h)) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", node->name.c_str());
                ImGui::Separator();
                if (node->isFolder()) {
                    ImGui::Text("Folder (%zu items)", node->children.size());
                } else {
                    tinyFS::TypeExt typeExt = fs.fTypeExt(h);
                    ImGui::TextColored(IMVEC4_COLOR(typeExt), "Type: %s", typeExt.c_str());
                }
                ImGui::EndTooltip();
            }
        },
        [&fs](tinyHandle h) {
            const tinyFS::Node* node = fs.fNode(h);
            if (node && node->isFolder()) return ImVec4(0.3f, 0.3f, 0.35f, 0.8f);
            tinyFS::TypeExt typeExt = fs.fTypeExt(h);
            typeExt.color[0] *= 0.2f;
            typeExt.color[1] *= 0.2f;
            typeExt.color[2] *= 0.2f;
            return ImVec4(typeExt.color[0], typeExt.color[1], typeExt.color[2], 0.8f);
        },
        [](tinyHandle) { return ImVec4(0.8f, 0.6f, 0.2f, 0.8f); },
        [&fs](tinyHandle h) {
            const tinyFS::Node* node = fs.fNode(h);
            if (node && node->isFolder()) return ImVec4(0.4f, 0.4f, 0.45f, 0.9f);
            tinyFS::TypeExt typeExt = fs.fTypeExt(h);
            typeExt.color[0] *= 0.5f;
            typeExt.color[1] *= 0.5f;
            typeExt.color[2] *= 0.5f;
            return ImVec4(typeExt.color[0], typeExt.color[1], typeExt.color[2], 0.9f);
        },
        [](tinyHandle) { return ImVec4(0.9f, 0.7f, 0.3f, 0.9f); }
    );
}

// ============================================================================
// INSPECTOR WINDOW
// ============================================================================

// File inspector

static void RenderFileInspector(tinyProject* project) {
    tinyFS& fs = project->fs();

    typeHandle selectedNode = HierarchyState::selectedNode;
    if (!selectedNode.isType<tinyNodeFS>()) return;

    tinyHandle fHandle = selectedNode.handle;
    const tinyFS::Node* node = fs.fNode(fHandle);
    if (!node) {
        ImGui::Text("Invalid file.");
        return;
    }

    ImGui::Text("%s", node->name.c_str());

    tinyFS::TypeExt typeExt = fs.fTypeExt(fHandle);
    if (!typeExt.ext.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(IMVEC4_COLOR(typeExt), ".%s", typeExt.c_str());
    }
    ImGui::Separator();
}

// Scene node inspector


static void RenderTRFM3D(const tinyFS& fs, tinySceneRT* scene, tinySceneRT::NWrap& wrap) {
    tinyRT_TRFM3D* trfm3D = wrap.trfm3D;
    if (!trfm3D) return;

    glm::vec3 translation, scale, skew;
    glm::quat rotation;
    glm::vec4 perspective;
    glm::decompose(trfm3D->local, scale, rotation, translation, skew, perspective);

    // Helper to recompose after editing
    auto recompose = [&]() {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), translation);
        glm::mat4 r = glm::mat4_cast(rotation);
        glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
        trfm3D->local = t * r * s;
    };

    static glm::quat initialRotation;
    static bool isDraggingRotation = false;
    static glm::vec3 displayEuler;

    if (!isDraggingRotation) {
        displayEuler = glm::eulerAngles(rotation) * (180.0f / 3.14159265f);
    }

    if (ImGui::DragFloat3("Translation", &translation.x, 0.1f)) recompose();
    if (ImGui::DragFloat3("Scale", &scale.x, 0.1f)) recompose();

    if (ImGui::DragFloat3("Rotation", &displayEuler.x, 0.5f)) {
        // Euler angle is a b*tch
        if (!isDraggingRotation) {
            initialRotation = rotation;
            isDraggingRotation = true;
        }
        glm::vec3 initialEuler = glm::eulerAngles(initialRotation) * (180.0f / 3.14159265f);
        glm::vec3 delta = displayEuler - initialEuler;
        rotation = initialRotation * glm::quat(glm::radians(delta));
        recompose();
    }

    if (!ImGui::IsItemActive()) {
        isDraggingRotation = false;
    }
}

static void RenderMESHRD(const tinyFS& fs, tinySceneRT* scene, tinySceneRT::NWrap& wrap) {
    tinyRT_MESHRD* meshRD = wrap.meshRD;
    if (!meshRD) return;

    tinyHandle meshHandle = meshRD->meshHandle();

    tinyHandle meshFHandle = fs.dataToFileHandle(MAKE_TH(tinyMeshVk, meshHandle));

    tinyHandle rtSkeleHandle = meshRD->skeleNodeHandle();
    tinySceneRT::CNWrap skeleCWrap = scene->CWrap(rtSkeleHandle);

    const auto* meshVk = fs.rGet<tinyMeshVk>(meshHandle);

    RenderDragField(
        [&fs, meshFHandle]() { return fs.fName(meshFHandle); },
        "No Mesh Assigned",
        [&]() { if (meshVk) return IMVEC4_COLOR(fs.typeExt<tinyMeshVk>()); return ImVec4(0.5f, 0.5f, 0.5f, 1.0f); },
        ImVec4(0.2f, 0.2f, 0.2f, 1.0f),
        [meshVk]() { return meshVk != nullptr; },
        [&fs, meshRD]() {
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DRAG_NODE")) {
                    Payload* data = (Payload*)payload->Data;
                    if (!data->isType<tinyNodeFS>()) { ImGui::EndDragDropTarget(); return; }
                    tinyHandle fHandle = data->handle();
                    typeHandle fTypeHdl = fs.fTypeHandle(fHandle);
                    if (!fTypeHdl.isType<tinyMeshVk>()) { ImGui::EndDragDropTarget(); return; }
                    meshRD->setMesh(fTypeHdl.handle);
                    ImGui::EndDragDropTarget();
                }
            }
        },
        []() { /* Do nothing for now */ },
        [&fs, meshFHandle]() {
            ImGui::BeginTooltip();

            const char* fullPath = fs.fName(meshFHandle, true, true);
            fullPath = fullPath ? fullPath : "<Invalid Mesh>";

            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "[FS]");
            ImGui::SameLine(); ImGui::Text("%s", fullPath);

            ImGui::EndTooltip();
        }
    );

    RenderDragField(
        [scene, skeleCWrap]() {
            if (skeleCWrap.skel3D) return scene->nodeName(skeleCWrap.handle);
            return "Invalid Skeleton Node";
        },
        "No Skeleton Node Assigned",
        [&]() { if (skeleCWrap.skel3D) return ImVec4(0.8f, 0.6f, 0.6f, 1.0f); return ImVec4(0.5f, 0.5f, 0.5f, 1.0f); },
        ImVec4(0.2f, 0.2f, 0.2f, 1.0f),
        [skeleCWrap]() { return skeleCWrap.skel3D != nullptr; },
        [&fs, scene, meshRD]() {
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DRAG_NODE")) {
                    Payload* data = (Payload*)payload->Data;
                    if (!data->isType<tinyNodeRT>()) { ImGui::EndDragDropTarget(); return; }
                    tinyHandle nodeHandle = data->handle();
                    tinySceneRT::CNWrap cWrap = scene->CWrap(nodeHandle);
                    if (!cWrap.skel3D) { ImGui::EndDragDropTarget(); return; }
                    meshRD->setSkeleNode(nodeHandle);
                    ImGui::EndDragDropTarget();
                }
            }
        },
        []() { /* Do nothing for now */ },
        [scene, rtSkeleHandle]() {
            ImGui::BeginTooltip();
            const char* fullPath = scene->nodeName(rtSkeleHandle, true);
            fullPath = fullPath ? fullPath : "<Invalid Node>";

            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "[Scene]");
            ImGui::SameLine(); ImGui::Text("%s", fullPath);

            ImGui::EndTooltip();
        }
    );
}

static void RenderBONE3D(const tinyFS& fs, tinySceneRT* scene, tinySceneRT::NWrap& wrap) {
    tinyRT_BONE3D* bone3D = wrap.bone3D;
    if (!bone3D) return;
}

struct CompInfo {
    std::string name;
    bool active;
    std::function<void()> renderFunc;
    std::function<void()> addFunc;
    std::function<void()> removeFunc;
};

static void RenderCOMP(const CompInfo& comp) {
    bool open = ImGui::CollapsingHeader(comp.name.c_str());

    if (ImGui::BeginPopupContextItem(comp.name.c_str())) {
        if (!comp.active) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f)); // green
            if (ImGui::MenuItem("Add")) {
                comp.addFunc();
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // red
            if (ImGui::MenuItem("Remove")) {
                comp.removeFunc();
            }
            ImGui::PopStyleColor();
        }
        ImGui::EndPopup();
    }

    if (open && comp.active) {
        ImGui::Indent();
        comp.renderFunc();
        ImGui::Unindent();
    }
}

static void RenderSceneNodeInspector(tinyProject* project) {
    tinySceneRT* scene = project->scene(HierarchyState::activeSceneHandle);
    if (!scene) return;

    typeHandle selectedNode = HierarchyState::selectedNode;
    if (!selectedNode.isType<tinyNodeRT>()) return;

    tinyHandle handle = selectedNode.handle;

    const tinyNodeRT* node = scene->node(handle);
    if (!node) {
        ImGui::Text("Invalid node.");
        return;
    }
    
    ImGui::Text("%s", node->name.c_str());
    ImGui::Separator();

    tinySceneRT::NWrap wrap = scene->Wrap(handle);

    const tinyFS& fs = project->fs();

    // Collect components
    std::vector<CompInfo> components;
    components.push_back({
        "Transform 3D",
        wrap.trfm3D != nullptr,
        [&]() { RenderTRFM3D(fs, scene, wrap); },
        [&scene, handle]() { scene->writeComp<tinyNodeRT::TRFM3D>(handle); },
        [&scene, handle]() { scene->removeComp<tinyNodeRT::TRFM3D>(handle); }
    });
    components.push_back({
        "Mesh Renderer 3D",
        wrap.meshRD != nullptr,
        [&]() { RenderMESHRD(fs, scene, wrap); },
        [&scene, handle]() { scene->writeComp<tinyNodeRT::MESHRD>(handle); },
        [&scene, handle]() { scene->removeComp<tinyNodeRT::MESHRD>(handle); }
    });
    components.push_back({
        "Bone 3D",
        wrap.bone3D != nullptr,
        [&]() { RenderBONE3D(fs, scene, wrap); },
        [&scene, handle]() { scene->writeComp<tinyNodeRT::BONE3D>(handle); },
        [&scene, handle]() { scene->removeComp<tinyNodeRT::BONE3D>(handle); }
    });

    // Sort: active first
    std::sort(components.begin(), components.end(), [](const CompInfo& a, const CompInfo& b) {
        return a.active > b.active;
    });

    // Render components
    for (const auto& comp : components) {
        RenderCOMP(comp);
    }
}



static void RenderInspector(tinyProject* project) {
    RenderFileInspector(project);
    RenderSceneNodeInspector(project);
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

    tinyFS& fs = project->fs();

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

    // ===== HIERARCHY WINDOW - Scene & File System =====
    if (tinyUI::Exec::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse)) {
        tinyHandle activeSceneHandle = HierarchyState::activeSceneHandle;

        tinyHandle sceneFHandle = fs.dataToFileHandle(MAKE_TH(tinySceneRT, activeSceneHandle));
        const char* sceneName = fs.fName(sceneFHandle);

        if (!activeScene) {
            ImGui::Text("No active scene");
        } else {
            float fontScale = tinyUI::Exec::GetTheme().fontScale;

            // Get available height for split view
            float availHeight = ImGui::GetContentRegionAvail().y;
            float splitterHeight = 4.0f;
            
            float topHeight = availHeight * HierarchyState::splitterPos - splitterHeight;
            float bottomHeight = availHeight * (1.0f - HierarchyState::splitterPos) - splitterHeight;

            // ===== TOP: SCENE HIERARCHY =====
            ImGui::Text("%s [HDL %u.%u]", sceneName, activeSceneHandle.index, activeSceneHandle.version);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0)); // Transparent background
            ImGui::BeginChild("SceneHierarchy", ImVec2(0, topHeight), true);
            RenderSceneNodeHierarchy(project.get());
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


    if (tinyUI::Exec::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse)) {

        RenderInspector(project.get());

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
