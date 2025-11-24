// tinyApp_imgui.cpp - UI Implementation & Testing
#include "tinyApp/tinyApp.hpp"
#include "tinyUI/tinyUI.hpp"


#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <imgui.h>
#include <TextEditor.h>
#include <algorithm>
#include <cstring>

using namespace tinyVk;

// State state tracking
namespace State {
    static tinyHandle sceneHandle;
    static bool isActiveScene(tinyHandle h) { return sceneHandle == h; }

    static std::unordered_set<uint64_t> expanded;

    static tinyHandle selected;
    static tinyHandle dragged;
    static tinyHandle renamed;
    static char renameBuffer[256];

    // Note: tinyHandle is hashed by default

    static bool isExpanded(tinyHandle th) {
        return expanded.find(th.raw()) != expanded.end();
    }

    static void setExpanded(tinyHandle th, bool expand) {
        if (expand) expanded.insert(th.raw());
        else        expanded.erase(th.raw());
    }
}

namespace Editor {
    static tinyHandle selected;
    static const char* what = "None";

    static bool is(const char* typeName) {
        return std::strcmp(what, typeName) == 0;
    }
}

// Code editor state
namespace CodeEditor {
    static TextEditor editor;
    static tinyHandle currentScriptHandle;

    static void Init(TextEditor::LanguageDefinition langDef = TextEditor::LanguageDefinition::Lua()) {
        editor.SetLanguageDefinition(langDef);
        // Set custom palette with transparent background and bluish theme
        TextEditor::Palette palette = TextEditor::GetDarkPalette();
        palette[(int)TextEditor::PaletteIndex::Background] = IM_COL32(0, 0, 0, 0);
        palette[(int)TextEditor::PaletteIndex::CurrentLineFill] = IM_COL32(50, 50, 50, 10);
        palette[(int)TextEditor::PaletteIndex::Selection] = IM_COL32(60, 80, 100, 128);
        palette[(int)TextEditor::PaletteIndex::Keyword] = IM_COL32(240, 70, 70, 255);
        palette[(int)TextEditor::PaletteIndex::String] = IM_COL32(150, 200, 100, 255);
        palette[(int)TextEditor::PaletteIndex::Number] = IM_COL32(200, 150, 100, 255);
        palette[(int)TextEditor::PaletteIndex::Comment] = IM_COL32(80, 80, 80, 155);
        editor.SetPalette(palette);
    }

    static void Render(const char* title) {
        editor.Render(title, ImVec2(-1, -1), false);
    }

    static void SetText(const std::string& text) {
        editor.SetText(text);
    }

    static std::string GetText() {
        return editor.GetText();
    }

    static bool IsTextChanged() {
        return editor.IsTextChanged();
    }
}

static rtScene* sceneRef = nullptr;
static tinyProject* projRef = nullptr;

namespace Texture {

    static UnorderedMap<uint64_t, ImTextureID> textureCache;

    static void Render(tinyTextureVk* texture) {
        uint64_t cacheKey = (uint64_t)texture->view();

        ImTextureID texId;

        auto it = textureCache.find(cacheKey);
        if (it == textureCache.end()) {
            texId = (ImTextureID)ImGui_ImplVulkan_AddTexture(
                texture->sampler(), texture->view(),
                ImageLayout::ShaderReadOnlyOptimal
            );
            textureCache[cacheKey] = texId;
        } else {
            texId = it->second; 
        }


        ImVec2 imgSize;
        float imgRatio = texture->aspectRatio();
        
        ImVec2 availSize = ImGui::GetContentRegionAvail();
        float availRatio = availSize.x / availSize.y;

        if (availRatio < imgRatio) {
            imgSize.x = availSize.x;
            imgSize.y = availSize.x / imgRatio;
        } else {
            imgSize.y = availSize.y;
            imgSize.x = availSize.y * imgRatio;
        }

        ImGui::Image(texId, imgSize);
    }
}

// ============================================================================
// UI INITIALIZATION
// ============================================================================

void tinyApp::initUI() {
    UIBackend = new tinyUI::Backend_Vulkan();

    tinyUI::VulkanBackendData vkData;
    vkData.instance = instanceVk->instance;
    vkData.physicalDevice = dvk->pDevice;
    vkData.device = dvk->device;
    vkData.queueFamily = dvk->queueFamilyIndices.graphicsFamily.value();
    vkData.queue = dvk->graphicsQueue;
    vkData.renderPass = renderer->getMainRenderPass();
    vkData.minImageCount = 2;
    vkData.imageCount = renderer->getSwapChainImageCount();
    vkData.msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    UIBackend->setVulkanData(vkData);

    tinyUI::Init(UIBackend, windowManager->window);

    projRef = project.get();

// ===== Misc =====

    // Set the active scene to main scene by default
    State::sceneHandle = project->mainSceneHandle;

    // Init code editor
    CodeEditor::Init();
}

void tinyApp::updateActiveScene() {
    curScene = project->scene(State::sceneHandle);
    sceneRef = curScene;

    // curScene->setFStart({
    //     renderer->getCurrentFrame(),
    //     fpsManager->deltaTime
    // });
    curScene->update({
        renderer->getCurrentFrame(),
        fpsManager->deltaTime
    });
}

// ===========================================================================
// UTILITIES
// ===========================================================================

template<typename FX>
using Func = std::function<FX>;

template<typename FX>
using CFunc = const Func<FX>;

#define M_CLICKED(M_state) ImGui::IsItemHovered() && ImGui::IsMouseReleased(M_state) && !ImGui::IsMouseDragging(M_state)
#define M_HOVERED ImGui::IsItemHovered() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Right)
#define M_DBCLICKED(M_state) ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(M_state)

#define IMVEC4_COLOR3(col, a) ImVec4(col[0] / 255.0f, col[1] / 255.0f, col[2] / 255.0f, a)



static bool RenderMenuItemToggle(const char* labelPending, const char* labelFinished, bool finished) {
    const char* label = finished ? labelFinished : labelPending;
    return ImGui::MenuItem(label, nullptr, finished, !finished);
}

struct Payload {

    static Payload make(tinyHandle h, std::string name) {
        Payload payload;
        payload.handle = h;
        strncpy(payload.name, name.c_str(), 63);
        payload.name[63] = '\0';
        return payload;
    }

    template<typename T>
    bool is() const {
        return handle.is<T>();
    }

    tinyHandle handle;
    char name[64];
};

template<
    typename FxActive,
    typename FxActiveColor,
    typename FxActiveCondition,
    typename FxDrop,
    typename FxClick,
    typename FxHover
>
static void RenderDragField(
    FxActive&& labelActive,
    const char* labelInactive,
    FxActiveColor&& activeColor,
    ImVec4 inactiveColor,
    FxActiveCondition&& activeCondition,
    FxDrop&& dropMethod,
    FxClick&& clickMethod,
    FxHover&& hoverMethod
) {
    bool active = activeCondition();
    const char* label = active ? labelActive() : labelInactive;
    ImVec4 borderColor = active ? activeColor() : inactiveColor;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4)); // Adjust padding
    ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // transparent background
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0)); // transparent hover
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0)); // transparent active

    if (!active) ImGui::PushStyleColor(ImGuiCol_Text, inactiveColor);

    if (ImGui::Button(label, ImVec2(-1, 0))) clickMethod();

    if (!active) ImGui::PopStyleColor(); 

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);

    if (M_HOVERED) hoverMethod();

    dropMethod();
}


struct Splitter {
    float splitterSize = 4.0f;
    float directionSize = 0.0f;
    bool horizontal = true;

    void init(size_t splitterCount) {
        if (positions.size() == splitterCount) return;

        positions.clear();
        regionSizes.clear();

        if (splitterCount < 1) return;

        // Create splitterCount positions and splitterCount + 1 heights
        float step = 1.0f / static_cast<float>(splitterCount + 1);

        for (size_t i = 0; i < splitterCount; ++i) {
            positions.push_back(step * static_cast<float>(i + 1));
        }

        for (size_t i = 0; i < splitterCount + 1; ++i) {
            regionSizes.push_back(step);
        }

        calcRegionSizes();
    }

    void calcRegionSizes() {
        for (size_t i = 0; i < regionSizes.size(); ++i) {
            float lowerBound = (i == 0) ? 0.0f : positions[i - 1];
            float upperBound = (i == positions.size()) ? 1.0f : positions[i];
            regionSizes[i] = upperBound - lowerBound;
        }
    }

    float rSize(size_t index) const {
        if (index >= regionSizes.size()) return 0.01f;
        float size = regionSizes[index] * directionSize - splitterSize * tinyUI::Theme().fontScale;
        return size > 0.0f ? size : 0.01f;
    }

    void render(size_t index) {
        if (index >= positions.size()) return;

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));

        ImVec2 size = horizontal ? ImVec2(-1, splitterSize) : ImVec2(splitterSize, -1);

        ImGui::Button(("##Splitter" + std::to_string(index)).c_str(), size);

        if (ImGui::IsItemActive()) {
            float delta = horizontal ? ImGui::GetIO().MouseDelta.y : ImGui::GetIO().MouseDelta.x;

            float lowerLimit = (index == 0) ? 0.0f : positions[index - 1] + 0.05f;
            float upperLimit = (index == positions.size() - 1) ? 1.0f : positions[index + 1] - 0.05f;

            positions[index] += delta / directionSize;
            positions[index] = std::clamp(positions[index], lowerLimit, upperLimit);
        }

        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(horizontal ? ImGuiMouseCursor_ResizeNS : ImGuiMouseCursor_ResizeEW);
        }

        ImGui::PopStyleColor(3);
    }

private:
    // These values are all relative (0.0 -> 1.0)
    std::vector<float> positions;     // n
    std::vector<float> regionSizes; // n + 1
};

// ============================================================================
// Node Tree Rendering Abstraction
// ============================================================================

struct HierarchyStyle {
    ImColor hoverColor = ImColor(50, 100, 150, 40);
    ImColor selectColor = ImColor(50, 100, 200, 90);
};

template<
    typename FxDiv, typename FxDivOpen, typename FxSelected, typename FxChildren,
    typename FxLClick, typename FxRClick, typename FxDbClick, typename FxHover,
    typename FxDrag, typename FxDrop
>
static void RenderGenericNodeHierarchy(
    tinyHandle nodeHandle, int depth,

    FxDiv&& fDiv, FxDivOpen&& fDivOpen, FxSelected&& fSelected, FxChildren&& fChildren, 
    FxLClick&& fLClick, FxRClick&& fRClick, FxDbClick&& fDbClick, FxHover&& fHover,
    FxDrag&& fDrag, FxDrop&& fDrop,

    const HierarchyStyle& style = HierarchyStyle()
) {
    if (!nodeHandle) return;

    ImGui::PushID(static_cast<int>(nodeHandle.index));
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    
    ImGui::BeginGroup();
    fDiv(nodeHandle, depth);
    ImGui::EndGroup();
    
    ImVec2 endPos = ImGui::GetCursorScreenPos();
    float height = endPos.y - startPos.y;
    ImGui::SetCursorScreenPos(startPos);
    ImGui::InvisibleButton("##drag", ImVec2(-1, height));
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (ImGui::IsItemHovered()) {
        drawList->AddRectFilled(startPos, ImVec2(startPos.x + ImGui::GetWindowWidth(), endPos.y), style.hoverColor);
    } else if (fSelected(nodeHandle)) {
        drawList->AddRectFilled(startPos, ImVec2(startPos.x + ImGui::GetWindowWidth(), endPos.y), style.selectColor);
    }

    ImGui::SetCursorScreenPos(endPos);
    ImGui::PopID();
    

    if (M_CLICKED(ImGuiMouseButton_Left)) fLClick(nodeHandle);
    if (M_CLICKED(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup(("Context" + std::to_string(nodeHandle.value)).c_str());
    }
    if (ImGui::BeginPopup(("Context" + std::to_string(nodeHandle.value)).c_str())) {
        fRClick(nodeHandle);
        ImGui::EndPopup();
    }
    
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) fDbClick(nodeHandle);
    if (ImGui::IsItemHovered()) fHover(nodeHandle);

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        fDrag(nodeHandle);
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget()) {
        fDrop(nodeHandle);
        ImGui::EndDragDropTarget();
    }

    std::vector<tinyHandle> children = fChildren(nodeHandle);
    if (fDivOpen(nodeHandle) && !children.empty()) {
        ImGui::Indent();
        for (const auto& child : children) {
            RenderGenericNodeHierarchy(
                child, depth + 1,
                fDiv, fDivOpen, fSelected, fChildren,
                fLClick, fRClick, fDbClick, fHover,
                fDrag, fDrop
            );
        }
        ImGui::Unindent();
    }
}

// ============================================================================
// HIERARCHY WINDOW - Scene & File Trees
// ============================================================================

static void RenderSceneNodeHierarchy() {
    rtScene* scene = sceneRef;
    tinyFS& fs = projRef->fs();

    RenderGenericNodeHierarchy(
        scene->root(), 0,
        // Div
        [scene](tinyHandle h, int depth) {
            rtNode* node = scene->node(h);
            if (!node) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "<Invalid>");
                return;
            }

            std::string name = node->name;

            bool hasChildren = node->childrenCount() > 0;
            bool isExpanded = State::isExpanded(h) && hasChildren;

            short state = hasChildren + isExpanded; // 0: no children, 1: collapsed, 2: expanded
            switch (state) {
                case 0: ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "o"); break;
                case 1: ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "+"); break;
                case 2: ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "-"); break;
            }
            ImGui::SameLine();

            if (State::renamed == h) {
                bool enter = ImGui::InputText("##rename", State::renameBuffer, sizeof(State::renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
                if (enter || ImGui::IsItemDeactivatedAfterEdit()) {
                    node->name = std::string(State::renameBuffer);
                    State::renamed = tinyHandle();
                }
            } else {
                // Node name in gray - white
                bool isSelected = State::selected == h;
                ImVec4 colorName = isSelected
                    ? ImVec4(0.9f, 0.9f, 0.5f, 1.0f)
                    : ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
                ImGui::TextColored(colorName, "%s", name.c_str());

                size_t childCount = node->childrenCount();
                if (childCount > 0) {
                    ImGui::SameLine();
                    ImVec4 infoColor = ImVec4(0.6f, 0.8f, 1.0f, 1.0f);
                    ImGui::TextColored(infoColor, "[%zu]", childCount);
                }
            }
        },
        // Div Open
        [](tinyHandle h) -> bool { return State::isExpanded(h); },
        // Selected
        [](tinyHandle h) -> bool { return State::selected == h; },
        // Children
        [](tinyHandle h) -> std::vector<tinyHandle> {
            const rtNode* node = sceneRef->node(h);

            std::vector<tinyHandle> children = node->children;
            std::sort(children.begin(), children.end(), [](tinyHandle a, tinyHandle b) {
                const rtNode* nodeA = sceneRef->node(a);
                const rtNode* nodeB = sceneRef->node(b);

                bool aHasChildren = nodeA->childrenCount() > 0;
                bool bHasChildren = nodeB->childrenCount() > 0;
                if (aHasChildren != bHasChildren) return aHasChildren > bHasChildren;
                return nodeA->name < nodeB->name;
            });
            return children;
        },
        // LClick
        [](tinyHandle h) {

            State::selected = h;
            State::setExpanded(h, !State::isExpanded(h));
        },
        // RClick
        [](tinyHandle h) {
            // rtScene::NWrap wrap = sceneRef->nWrap(h);
            rtNode* node = sceneRef->node(h);
            if (!node) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Invalid Node!");
            }

            State::selected = h;

            ImGui::Text("Node: %s", node->cname());
            ImGui::Separator();
            if (ImGui::MenuItem("Add Child")) {
                sceneRef->nAdd("New Node", h);
                State::setExpanded(h, true);
            }
            ImGui::Separator();

            if (ImGui::MenuItem("Rename")) {
                strcpy(State::renameBuffer, node->cname());
                State::renamed = h;
            }
            ImGui::Separator();

            bool canDelete = h != sceneRef->root();
            if (ImGui::MenuItem("Erase", nullptr, false, canDelete)) sceneRef->nErase(h);
            if (ImGui::MenuItem("Clear", nullptr, false, node->childrenCount() > 0)) {
                std::vector<tinyHandle> children = node->children;
                for (const auto& childHandle : children) sceneRef->nErase(childHandle);
            }

            // if (tinyRT_ANIM3D* anim3D = Wrap.anim3D) {
            //     ImGui::Separator();
            //     for (auto& anime : anim3D->MAL()) {
            //         if (ImGui::MenuItem(anime.first.c_str())) {
            //             anim3D->play(anime.first, true);
            //         }
            //     }
            // }
            // if (tinyRT_SCRIPT* script = Wrap.script) {
            //     // Do nothing
            // }
        },
        // DbClick - Do nothing for now
        [scene](tinyHandle h) {

        },
        // Hover
        [scene](tinyHandle h) {
            if (ImGui::BeginTooltip()) {
                if (const rtNode* node = scene->node(h)) {
                    ImGui::Text("%s", node->cname());
                    ImGui::Separator();
                    std::string compList = "";
                    if (node->has<rtTRANFM3D>()) compList += "[Tranfm 3D] ";
                    if (node->has<rtMESHRD3D>()) compList += "[Mesh Rd 3D] ";
                    compList = compList.empty() ? "[None]" : compList;
                    ImGui::Text("%s", compList.c_str());
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Invalid Node!");
                }
            }
            ImGui::EndTooltip();
        },
        // Drag
        [scene](tinyHandle h) {
            const rtNode* node = scene->node(h);
            if (!node) return;

            Payload payload = Payload::make(h, node->name);
            ImGui::SetDragDropPayload("PAYLOAD", &payload, sizeof(payload));
            ImGui::Text("Dragging: %s", payload.name);
        },
        // Drop
        [scene, &fs](tinyHandle h) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PAYLOAD")) {
                Payload* data = (Payload*)payload->Data;

                // Node = reparenting
                if (data->is<rtNode>()) {
                    scene->nReparent(data->handle, h);
                    State::setExpanded(h, true);
                }

                // File = further handling
                if (data->is<tinyNodeFS>()) {
                    tinyHandle dataHandle = fs.dataHandle(data->handle);

                    // Scene File = instantiate
                    if (dataHandle.is<rtScene>() && dataHandle != State::sceneHandle) {
                        // scene->addScene(dataHandle, h);
                        State::setExpanded(h, true);
                    }

                }

                State::dragged = tinyHandle();
            }
        }
    );
}

static void RenderFileNodeHierarchy() {
    tinyFS& fs = projRef->fs();

    RenderGenericNodeHierarchy(
        fs.root(), 0,
        // Div
        [&fs](tinyHandle h, int depth) {
            const tinyNodeFS* node = fs.fNode(h);
            if (!node) { // Should not happen
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "<Invalid>");
                return;
            }

            std::string name = node->name;
            tinyHandle dHandle = fs.dataHandle(h);

            const tinyFS::TypeInfo* typeInfo = fs.typeInfo(dHandle.tID());

            bool isFolder = node->isFolder();
            bool isExpanded = State::isExpanded(h) && isFolder;

            short state = isFolder + isExpanded; // 0: is file, 1: collapsed, 2: expanded
            switch (state) {
                case 0: ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "o"); break;
                case 1: ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "+"); break;
                case 2: ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "-"); break;
            }
            ImGui::SameLine();

            if (State::renamed == h) {
                bool enter = ImGui::InputText("##rename", State::renameBuffer, sizeof(State::renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
                if (enter || ImGui::IsItemDeactivatedAfterEdit()) {
                    fs.rename(h, State::renameBuffer);
                    State::renamed = tinyHandle();
                }
            } else {
                ImVec4 nameColor = isFolder
                    ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                    : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

                ImGui::TextColored(nameColor, "%s", name.c_str());

                if (!typeInfo->ext.empty()) {
                    ImGui::SameLine();
                    ImVec4 extColor = IMVEC4_COLOR3(typeInfo->color, 1.0f);
                    ImGui::TextColored(extColor, ".%s", typeInfo->c_str());
                }

                if (isFolder) {
                    ImGui::SameLine();

                    size_t childCount = node->children.size();
                    ImVec4 infoColor = childCount > 0 ? ImVec4(0.6f, 0.8f, 1.0f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                    ImGui::TextColored(infoColor, "[%zu]", childCount);
                }
            }
        },
        // Div Open
        [](tinyHandle h) -> bool { return State::isExpanded(h); },
        // Selected
        [](tinyHandle h) -> bool { return State::selected == h; },
        // Children
        [&](tinyHandle h) -> std::vector<tinyHandle> {
            if (const tinyFS::Node* node = fs.fNode(h)) {
                std::vector<tinyHandle> children = node->children;
                std::sort(children.begin(), children.end(), [&](tinyHandle a, tinyHandle b) {
                    tinyFS::TypeInfo* typeA = fs.typeInfo(fs.dataHandle(a).tID());
                    tinyFS::TypeInfo* typeB = fs.typeInfo(fs.dataHandle(b).tID());
                    if (typeA->ext != typeB->ext) return typeA->ext < typeB->ext;
                    return fs.fNode(a)->name < fs.fNode(b)->name;
                });
                return children;
            }
            return std::vector<tinyHandle>();
        },
        // LClick
        [&](tinyHandle h) { 
            State::setExpanded(h, !State::isExpanded(h));

            const tinyNodeFS* node = fs.fNode(h);
            if (!node || node->isFolder()) return;

            State::selected = h;
        },
        // RClick
        [&](tinyHandle h) {
            const tinyFS::Node* node = fs.fNode(h);
            if (!node) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Invalid Node!");
                return;
            }

            State::selected = h;

            const char* name = node->cname();
            ImGui::Text("%s", name);

            if (!node->isFolder()) { // Write colored extension
                ImGui::SameLine();
                tinyFS::TypeInfo* typeInfo = fs.typeInfo(fs.typeID(h));
                ImGui::TextColored(IMVEC4_COLOR3(typeInfo->color, 1.0f), ".%s", typeInfo->c_str());
            }

            ImGui::Separator();

            tinyHandle dHandle = fs.dataHandle(h);
            if (dHandle.is<rtScene>()) {
                rtScene* scene = fs.r().get<rtScene>(dHandle);
                if (RenderMenuItemToggle("Make Active", "Active", State::isActiveScene(dHandle))) {
                    State::sceneHandle = dHandle;
                }
                // if (RenderMenuItemToggle("Cleanse", "Cleansed", scene->isClean())) {
                //     scene->cleanse();
                // }
            }
            if (dHandle.is<tinyScript>()) {
                tinyScript* script = fs.r().get<tinyScript>(dHandle);
                if (ImGui::MenuItem("Compile")) {
                    script->compile();
                }
            }
            if (node->isFolder()) {
                if (ImGui::MenuItem("Add Folder")) {
                    fs.createFolder("New Folder", h);
                    State::setExpanded(h, true);
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Rename", nullptr, nullptr, State::renamed == tinyHandle())) {
                strcpy(State::renameBuffer, node->cname());
                State::renamed = h;
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Delete", nullptr, nullptr)) projRef->fRemove(h);
        },
        // DbClick - Do nothing for now
        [&fs](tinyHandle h) {
            const tinyFS::Node* node = fs.fNode(h);
            if (!node || node->isFolder()) return;

            tinyHandle dHandle = fs.dataHandle(h);
            
            // Scene file -> make active
            if (dHandle.is<rtScene>()) {
                State::sceneHandle = dHandle;
            // Script file -> open code editor
            } else if (dHandle.is<tinyScript>()) {
                Editor::selected = h;
            }
        },
        // Hover
        [&fs](tinyHandle h) {
            if (ImGui::BeginTooltip()) {
                const tinyFS::Node* node = fs.fNode(h);
                if (!node) {
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Invalid Node!");
                    ImGui::EndTooltip();
                    return;
                }

                ImGui::Text("%s", node->cname());
                if (node->isFolder()) {
                    ImGui::Separator();
                    ImGui::Text("Folder (%zu items)", node->children.size());
                } else {
                    ImGui::SameLine();

                    tinyFS::TypeInfo* typeInfo = fs.typeInfo(fs.typeID(h));
                    ImGui::TextColored(IMVEC4_COLOR3(typeInfo->color, 1.0f), ".%s", typeInfo->c_str());
                }
                ImGui::EndTooltip();
            }
        },
        // Drag
        [&fs](tinyHandle h) {
            const tinyFS::Node* node = fs.fNode(h);
            if (!node) return;

            State::dragged = h;
            Payload payload = Payload::make(h, node->name);

            ImGui::SetDragDropPayload("PAYLOAD", &payload, sizeof(payload));
            ImGui::Text("Dragging: %s", node->cname());
            ImGui::Separator();

            const tinyFS::TypeInfo* typeInfo = fs.typeInfo(fs.dataHandle(h).tID());
            ImGui::TextColored(IMVEC4_COLOR3(typeInfo->color, 1.0f), "Type: .%s", typeInfo->c_str());
        },
        // Drop
        [&fs](tinyHandle h) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PAYLOAD")) {
                Payload* data = (Payload*)payload->Data;
                if (data->is<tinyNodeFS>() && fs.move(data->handle, h)) {
                    State::setExpanded(h, true);
                    State::selected = data->handle;
                }
                State::dragged = tinyHandle();
            }
        }
    );
}

// ============================================================================
// INSPECTOR WINDOW
// ============================================================================


// Scene node inspector
static void RenderTRANFM3D(const tinyFS& fs, rtScene* scene, tinyHandle nHandle) {
    rtTRANFM3D* trfm3D = scene->nGetComp<rtTRANFM3D>(nHandle);
    if (!trfm3D) return;

    Tr3D& local = trfm3D->local;

    static glm::quat initialRotation;
    static bool isDraggingRotation = false;
    static glm::vec3 displayEuler;

    if (ImGui::DragFloat3("Translation", &local.T.x, 0.1f)) { }

    if (ImGui::DragFloat3("Rotation", &displayEuler.x, 0.5f)) {
        // Euler angle is a b*tch
        if (!isDraggingRotation) {
            initialRotation = local.R;
            isDraggingRotation = true;
        }
        glm::vec3 initialEuler = glm::eulerAngles(initialRotation) * (180.0f / 3.14159265f);
        glm::vec3 delta = displayEuler - initialEuler;
        local.R = initialRotation * glm::quat(glm::radians(delta));
    }

    if (!ImGui::IsItemActive()) {
        isDraggingRotation = false;
    }

    if (ImGui::DragFloat3("Scale", &local.S.x, 0.1f)) { }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("Reset", ImVec2(-1, 0))) {
        local = Tr3D();
        displayEuler = glm::vec3(0.0f);
    }
    ImGui::PopStyleColor();

    // Display the world transform
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "World Transform:");
    glm::mat4 world = trfm3D->world;
    for (int i = 0; i < 4; ++i) {
        ImGui::Text("[%.3f, %.3f, %.3f, %.3f]", world[i][0], world[i][1], world[i][2], world[i][3]);
    }
}

static void RenderMESHRD3D(const tinyFS& fs, rtScene* scene, tinyHandle nHandle) {
    rtMESHRD3D* meshRD = scene->nGetComp<rtMESHRD3D>(nHandle);
    if (!meshRD) return;

    tinyHandle meshHandle = meshRD->mesh;
    const tinyMeshVk* meshVk = fs.r().get<tinyMeshVk>(meshHandle);

    tinyHandle meshFHandle = fs.rDataToFile(meshHandle);

    RenderDragField(
        [&fs, meshFHandle]() { return fs.nameCStr(meshFHandle); },
        "No Mesh Assigned",
        [&]() { 
            if (!meshVk) return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
            return IMVEC4_COLOR3(fs.typeInfo<tinyMeshVk>()->color, 1.0);
        },
        ImVec4(0.2f, 0.2f, 0.2f, 1.0f),
        [meshVk]() { return meshVk != nullptr; },
        [&fs, meshRD]() {
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PAYLOAD")) {
                    Payload* data = (Payload*)payload->Data;
                    if (!data->is<tinyNodeFS>()) { ImGui::EndDragDropTarget(); return; }

                    tinyHandle fHandle = data->handle;
                    tinyHandle dHandle = fs.dataHandle(fHandle);

                    if (!dHandle.is<tinyMeshVk>()) { ImGui::EndDragDropTarget(); return; }
                    meshRD->mesh = dHandle;
                    ImGui::EndDragDropTarget();
                }
            }
        },
        []() {  },
        [&fs, meshFHandle]() {
            ImGui::BeginTooltip();

            const char* fullPath = fs.path(meshFHandle);
            fullPath = fullPath ? fullPath : "<Invalid Mesh>";

            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "[FS]");
            ImGui::SameLine(); ImGui::Text("%s", fullPath);

            ImGui::EndTooltip();
        }
    );
}

/*

static void RenderBONE3D(const tinyFS& fs, rtScene* scene, rtScene::NWrap& wrap) {
    tinyRT_BONE3D* bone3D = wrap.bone3D;
    if (!bone3D) return;

    tinyHandle skeleNodeHandle = bone3D->skeleNodeHandle;
}


static void RenderSKEL3D(const tinyFS& fs, rtScene* scene, rtScene::NWrap& wrap) {
    tinyRT_SKEL3D* skel3D = wrap.skel3D;
    if (!skel3D) return;

    RenderDragField(
        [scene, wrap]() { return scene->nodeName(wrap.handle); },
        "Skeleton Component",
        []() { return ImVec4(0.8f, 0.6f, 0.6f, 1.0f); },
        ImVec4(0.2f, 0.2f, 0.2f, 1.0f),
        []() { return true; },
        []() {  },
        []() {  },
        [&]() {
            ImGui::BeginTooltip();

            if (skel3D->hasSkeleton()) {
                tinyHandle skeleHandle = skel3D->skeleHandle();

                tinyHandle fHandle = fs.rDataToFile(skeleHandle);
                const char* fullPath = fs.path(fHandle);
                fullPath = fullPath ? fullPath : "<Invalid Skeleton>";

                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "[FS]");
                ImGui::SameLine(); ImGui::Text("%s", fullPath);
            }

            ImGui::EndTooltip();
        }
    );

    // For the time being just open the editor
    if (ImGui::Button("Open Runtime Skeleton Editor", ImVec2(-1, 0))) {
        Editor::selected = wrap.handle;
        Editor::what = "SKEL3D";
    }
}

static void RenderSCRIPT(const tinyFS& fs, rtScene* scene, rtScene::NWrap& wrap) {
    tinyRT_SCRIPT* script = wrap.script;
    if (!script) return;

    tinyHandle scriptHandle = script->scriptHandle();
    tinyHandle scriptFHandle = fs.rDataToFile(scriptHandle);

    const tinyScript* scriptPtr = fs.r().get<tinyScript>(scriptHandle);

    // Render the drag field for the script file
    RenderDragField(
        [&fs, scriptFHandle]() { return fs.name(scriptFHandle).c_str(); },
        "No Script Assigned",
        [&]() { return ImVec4(0.4f, 0.6f, 0.9f, 1.0f); },
        ImVec4(0.2f, 0.2f, 0.2f, 1.0f),
        [&]() { return scriptPtr != nullptr; },
        [&fs, script]() {
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PAYLOAD")) {
                    Payload* data = (Payload*)payload->Data;
                    if (!data->is<tinyNodeFS>()) { ImGui::EndDragDropTarget(); return; }
                    
                    tinyHandle fHandle = data->handle;
                    tinyHandle dHandle = fs.dataHandle(fHandle);
                    if (!dHandle.is<tinyScript>()) { ImGui::EndDragDropTarget(); return; }

                    script->assign(dHandle);
                    ImGui::EndDragDropTarget();
                }
            }
        },
        []() {  },
        [&fs, scriptFHandle]() {
            ImGui::BeginTooltip();

            const char* fullPath = fs.path(scriptFHandle);
            fullPath = fullPath ? fullPath : "<Invalid Script>";

            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "[FS]");
            ImGui::SameLine(); ImGui::Text("%s", fullPath);

            ImGui::EndTooltip();
        }
    );

    ImGui::Separator();

    ImVec4 varColor = ImVec4(0.2f, 0.5f, 0.8f, 0.2f);
    ImGui::PushStyleColor(ImGuiCol_Header, varColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.6f, 0.9f, 0.6f));

    if (ImGui::CollapsingHeader("Variables", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, varColor);

        ImGui::BeginChild("VariablesContent", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY, 0);

        tinyVarsMap& vMap = script->vMap();
        std::vector<std::pair<std::string, tinyVarsMap::mapped_type>> sortedItems(vMap.begin(), vMap.end());
        std::sort(sortedItems.begin(), sortedItems.end(), [](const auto& a, const auto& b) {
            auto getOrder = [](const auto& v) -> int {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, bool>) return 0;
                if constexpr (std::is_same_v<T, int>) return 1;
                if constexpr (std::is_same_v<T, float>) return 2;
                if constexpr (std::is_same_v<T, glm::vec2>) return 3;
                if constexpr (std::is_same_v<T, glm::vec3>) return 4;
                if constexpr (std::is_same_v<T, glm::vec4>) return 5;
                if constexpr (std::is_same_v<T, std::string>) return 6;
                if constexpr (std::is_same_v<T, tinyHandle>) return 7;
                return 8;
            };
            int orderA = std::visit(getOrder, a.second);
            int orderB = std::visit(getOrder, b.second);
            if (orderA != orderB) return orderA < orderB;
            return a.first < b.first;
        });
        for (auto& [name, value] : sortedItems) {
            auto& realValue = vMap[name];

            std::visit([&](auto&& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, float>) ImGui::DragFloat(name.c_str(), &val, 0.1f); else
                if constexpr (std::is_same_v<T, int>)   ImGui::DragInt(  name.c_str(), &val);       else
                if constexpr (std::is_same_v<T, bool>)  ImGui::Checkbox( name.c_str(), &val);       else
                if constexpr (std::is_same_v<T, glm::vec2>) ImGui::DragFloat2(name.c_str(), &val.x, 0.1f); else
                if constexpr (std::is_same_v<T, glm::vec3>) ImGui::DragFloat3(name.c_str(), &val.x, 0.1f); else
                if constexpr (std::is_same_v<T, glm::vec4>) ImGui::DragFloat4(name.c_str(), &val.x, 0.1f); else
                if constexpr (std::is_same_v<T, std::string>) {
                    static std::map<std::string, std::string> buffers;
                    if (buffers.find(name) == buffers.end()) {
                        buffers[name] = val;
                    }
                    char buf[256];
                    strcpy(buf, buffers[name].c_str());
                    if (ImGui::InputText(name.c_str(), buf, sizeof(buf))) {
                        buffers[name] = buf;
                        val = buf;
                    }
                } else if constexpr (std::is_same_v<T, tinyHandle>) {
                    ImGui::PushID(name.c_str());
                    static std::string labelBuffer;

                    std::string type = "Unknown";
                    if (val.is<tinyNodeRT>())  type = "Node"; else
                    if (val.is<rtScene>()) type = "Scene"; else
                    if (val.is<tinyScript>())  type = "Script"; else
                    if (val.is<tinyMeshVk>())  type = "Mesh";

                    RenderDragField(
                        [&]() {
                            std::string info = " [" + type + ", " + std::to_string(val.idx()) + ", " + std::to_string(val.ver()) + "]";

                            labelBuffer = name + info;
                            return labelBuffer.c_str();
                        },
                        name.c_str(),
                        [&]() {
                            return ImVec4(0.6f, 0.4f, 0.9f, 1.0f);
                        },
                        ImVec4(0.2f, 0.2f, 0.2f, 1.0f),
                        [&]() { return static_cast<bool>(val); },
                        [&fs, &val]() {
                            if (ImGui::BeginDragDropTarget()) {
                                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PAYLOAD")) {
                                    Payload* data = (Payload*)payload->Data;

                                    // Scene node
                                    if (data->is<tinyNodeRT>() && val.is<tinyNodeRT>()) {
                                        val = data->handle;
                                    }
                                    // File need a bit more checking
                                    else if (data->handle.is<tinyNodeFS>()) {
                                        tinyHandle dHandle = fs.dataHandle(data->handle);
                                        if (val.is(dHandle.tID())) val = dHandle;
                                    }

                                    ImGui::EndDragDropTarget();
                                }
                            }
                        },
                        []() {  },
                        [name]() {
                            ImGui::BeginTooltip();
                            ImGui::Text("%s", name.c_str());
                            ImGui::EndTooltip();
                        }
                    );
                    ImGui::PopID();
                }
            }, realValue);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleColor(2);
}
*/

struct CompInfo {
    std::string name;
    bool active;
    std::function<void()> renderFunc;
    std::function<void()> addFunc;
    std::function<void()> removeFunc;
};

const static ImVec4 compAddColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // green
const static ImVec4 compRemoveColor = ImVec4(0.8f, 0.2f, 0.2f, 1.0f); // red

static void RenderCOMP(const CompInfo& comp) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
    if (!comp.active) {
        flags |= ImGuiTreeNodeFlags_Leaf;
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 0.8f)); // darker background
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.15f, 0.15f, 0.15f, 0.9f)); // darker hover
    } else {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.5f, 0.8f, 0.4f)); // normal background
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.6f, 0.9f, 0.6f)); // normal hover
    }

    bool open = ImGui::CollapsingHeader(comp.name.c_str(), flags);
    ImGui::PopStyleColor(2);

    if (ImGui::BeginPopupContextItem(comp.name.c_str())) {
        ImGui::PushStyleColor(ImGuiCol_Text, comp.active ? compRemoveColor : compAddColor);

        if (!comp.active) { if (ImGui::MenuItem("Add"))    comp.addFunc();    }
        else              { if (ImGui::MenuItem("Remove")) comp.removeFunc(); }

        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }

    if (open && comp.active) {
        ImGui::Indent();
        comp.renderFunc();
        ImGui::Unindent();
    }
}

static void RenderSceneNodeInspector(tinyProject* project) {
    rtScene* scene = project->scene(State::sceneHandle);
    if (!scene) return;

    tinyHandle handle = State::selected;
    if (!handle.is<rtNode>()) return;

    // rtScene::NWrap wrap = scene->nWrap(handle);
    const rtNode* node = scene->node(handle);

    ImGui::Text("%s", node->cname());
    ImGui::Separator();

    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.9f, 1.0f), "[HDL: %zu, %zu]", handle.index, handle.version);

    const tinyFS& fs = project->fs();

    // Collect components
    std::vector<CompInfo> components;
    components.push_back({
        "Transform 3D", node->has<rtTRANFM3D>(),
        [&]() { RenderTRANFM3D(fs, scene, handle); },
        [&]() { scene->nAddComp<rtTRANFM3D>(handle); },
        [&]() { scene->nEraseComp<rtTRANFM3D>(handle); }
    });
    components.push_back({
        "Mesh Render 3D", node->has<rtMESHRD3D>(),
        [&]() { RenderMESHRD3D(fs, scene, handle); },
        [&]() { scene->nAddComp<rtMESHRD3D>(handle); },
        [&]() { scene->nEraseComp<rtMESHRD3D>(handle); }
    });
    // components.push_back({
    //     "Bone 3D", wrap.bone3D != nullptr,
    //     [&]() { RenderBONE3D(fs, scene, wrap); },
    //     [&scene, handle]() { scene->writeComp<tinyNodeRT::BONE3D>(handle); },
    //     [&scene, handle]() { scene->removeComp<tinyNodeRT::BONE3D>(handle); }
    // });
    // components.push_back({
    //     "Skeleton 3D", wrap.skel3D != nullptr,
    //     [&]() { RenderSKEL3D(fs, scene, wrap); },
    //     [&scene, handle]() { scene->writeComp<tinyNodeRT::SKEL3D>(handle); },
    //     [&scene, handle]() { scene->removeComp<tinyNodeRT::SKEL3D>(handle); }
    // });
    // components.push_back({
    //     "Runtime Script", wrap.script != nullptr,
    //     [&]() { RenderSCRIPT(fs, scene, wrap); },
    //     [&scene, handle]() { scene->writeComp<tinyNodeRT::SCRIPT>(handle); },
    //     [&scene, handle]() { scene->removeComp<tinyNodeRT::SCRIPT>(handle); }
    // });

    // Sort: active first
    std::sort(components.begin(), components.end(), [](const CompInfo& a, const CompInfo& b) {
        return a.active > b.active;
    });

    // Render components
    for (const auto& comp : components) {
        RenderCOMP(comp);
    }
}

// File inspector
static void RenderFileInspector(tinyProject* project) {
    tinyFS& fs = project->fs();

    tinyHandle handle = State::selected;
    if (!handle.is<tinyNodeFS>()) return;

    const tinyFS::Node* node = fs.fNode(handle);
    if (!node) {
        ImGui::Text("Invalid file.");
        return;
    }

    // typeHandle typeHdl = fs.fTypeHandle(fHandle);
    tinyHandle dHandle = fs.dataHandle(handle);

    ImGui::BeginGroup();
    ImGui::Text("%s", node->cname());

    const tinyFS::TypeInfo* typeInfo = fs.typeInfo(handle);
    if (!typeInfo->ext.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(IMVEC4_COLOR3(typeInfo->color, 1.0), ".%s", typeInfo->c_str());
    }
    ImGui::EndGroup();

    if (M_HOVERED) {
        ImGui::BeginTooltip();

        // ".root" + path + ".ext"
        ImGui::Text("%s", fs.path(handle)); ImGui::SameLine();
        // tinyFS::TypeExt typeExt = fs.fTypeExt(handle); 
        const auto* typeInfo = fs.typeInfo(handle);
        ImGui::TextColored(IMVEC4_COLOR3(typeInfo->color, 1.0), ".%s", typeInfo->c_str());

        ImGui::EndTooltip();
    }

    ImGui::Separator();

    if (node->isFolder()) {
        // Display folder info
        ImGui::Text("Folder (%zu items)", node->children.size());

        ImGui::Indent();

        // Display all types of children in the format Type (with color): count
        std::map<tinyType::ID, int> typeCounts;
        for (const auto& childHdl : node->children) {
            tinyHandle childDataHandle = fs.dataHandle(childHdl);
            typeCounts[childDataHandle.typeID]++;
        }
        for (const auto& [typeID, count] : typeCounts) {
            const auto* typeInfo = fs.typeInfo(typeID);

            const std::string& extStr = typeInfo->ext.empty() ? "folder" : typeInfo->ext;
            ImGui::TextColored(IMVEC4_COLOR3(typeInfo->color, 1.0f), "%s: %d", extStr.c_str(), count);
        }

        return;
    }

    if (dHandle.is<tinyScript>()) {
        tinyScript* script = fs.r().get<tinyScript>(dHandle);

        // Log the version, and compilation status
        if (script->valid())
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Compiled [%u.0]", script->version());
        else if (script->debug().empty())
            ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.2f, 1.0f), "Not Compiled ");
        else
            ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "Compilation Error ");

        // A button to open editor (overrides the editorSelection)

        if (ImGui::Button("Open in Editor", ImVec2(-1, 0))) {
            Editor::selected = handle;
        }
    } else if (dHandle.is<tinyTextureVk>()) {
        tinyTextureVk* texture = fs.r().get<tinyTextureVk>(dHandle);

        Texture::Render(texture);
    }
}

static void RenderInspector(tinyProject* project) {
    RenderSceneNodeInspector(project);
    RenderFileInspector(project);
}


// ============================================================================
// EDITOR WINDOWS RENDERING
// ============================================================================

static void RenderScriptEditor() {
    tinyHandle fHandle = Editor::selected;

    tinyFS& fs = projRef->fs();
    const tinyFS::Node* node = fs.fNode(fHandle);
    if (!node) return;

    tinyHandle dHandle = fs.dataHandle(fHandle);
    if (!dHandle.is<tinyScript>()) return;

    tinyScript* script = fs.r().get<tinyScript>(dHandle);

    if (script && !script->code.empty() && dHandle != CodeEditor::currentScriptHandle) {
        CodeEditor::currentScriptHandle = dHandle;
        CodeEditor::SetText(script->code);
    }

    static Splitter split;
    split.init(1);
    split.directionSize = ImGui::GetContentRegionAvail().y;
    split.calcRegionSizes();

    // Code editor
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("CodeEditor", ImVec2(0, split.rSize(0)), true);
    
    tinyDebug& debug = script->debug();

    ImGui::TextColored(ImVec4(0.6f, 0.6f, 1.0f, 1.0f), "Script: %s", node->cname());
    ImGui::SameLine();
    if (script->valid()) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "[Compiled v%u]", script->version());
    } else if (debug.empty()) {
        ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.2f, 1.0f), "[Not Compiled]");
    } else {
        ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "[Compilation Error]");
    }
    ImGui::SameLine();

    if (ImGui::Button("Compile")) script->compile();

    ImGui::Separator();

    if (CodeEditor::IsTextChanged() && script) {
        script->code = CodeEditor::GetText();
    }
    CodeEditor::Render(node->cname());

    ImGui::EndChild();
    ImGui::PopStyleColor();

    split.render(0);

    // Debug console
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("DebugTerminal", ImVec2(0, split.rSize(1)), true);

    ImGui::TextColored(ImVec4(0.6f, 0.6f, 1.0f, 1.0f), "Debug Console");
    ImGui::SameLine();
    if (ImGui::Button("Clear")) debug.clear();
    ImGui::Separator();

    for (const auto& line : debug.logs()) {
        ImGui::TextColored(ImVec4(line.color[0], line.color[1], line.color[2], 1.0f), "%s", line.c_str());
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

/*
static void RenderSkeleNodeEditor() {
    if (Editor::what != "SKEL3D") return;

    rtScene* scene = sceneRef;

    tinyHandle nHandle = Editor::selected;
    tinyRT_SKEL3D* skel3D = scene->rtComp<tinyNodeRT::SKEL3D>(nHandle);
    if (!skel3D) return;

    const tinyFS& fs = projRef->fs();

    const tinySkeleton* skeleton = skel3D->rSkeleton();
    if (!skeleton) return;

    // Static variables for bone selection
    static int selectedBoneIndex = -1;
    static tinyHandle lastSkeletonHandle;

    // Reset selection if skeleton changed
    if (lastSkeletonHandle != nHandle) {
        selectedBoneIndex = -1;
        lastSkeletonHandle = nHandle;
    }

    static Splitter split; // Vertical splitter (trying out something new!)
    split.init(1);
    split.horizontal = false;
    split.directionSize = ImGui::GetContentRegionAvail().x;
    split.calcRegionSizes();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("BoneHierarchy", ImVec2(split.rSize(0), 0), true);

    std::function<void(int)> renderBoneTree = [&](int boneIndex) {
        if (boneIndex < 0 || boneIndex >= static_cast<int>(skeleton->bones.size())) return;

        const tinyBone& bone = skeleton->bones[boneIndex];

        ImGui::PushID(boneIndex);

        bool hasChildren = !bone.children.empty();
        bool isSelected = (selectedBoneIndex == boneIndex);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (!hasChildren) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        if (isSelected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        std::string boneLabel = std::to_string(boneIndex) + ": " + bone.name;
        bool nodeOpen = ImGui::TreeNodeEx(boneLabel.c_str(), flags);

        // Handle selection
        if (ImGui::IsItemClicked()) {
            selectedBoneIndex = boneIndex;
        }

        if (nodeOpen && hasChildren) {
            for (int childIndex : bone.children) {
                renderBoneTree(childIndex);
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    };

    for (int i = 0; i < static_cast<int>(skeleton->bones.size()); ++i) {
        if (skeleton->bones[i].parent == -1) {
            renderBoneTree(i);
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine();

    split.render(0);

    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("BoneTransformEditor", ImVec2(split.rSize(1), 0), true);

    if (selectedBoneIndex >= 0 && selectedBoneIndex < static_cast<int>(skeleton->bones.size())) {
        ImGui::Separator();
        const tinyBone& selectedBone = skeleton->bones[selectedBoneIndex];
        ImGui::Text("Selected Bone: %d - %s", selectedBoneIndex, selectedBone.name.c_str());

        glm::mat4 boneLocal = skel3D->localPose(selectedBoneIndex);

        glm::vec3 translation, scale, skew;
        glm::quat rotation;
        glm::vec4 perspective;
        glm::decompose(boneLocal, scale, rotation, translation, skew, perspective);

        // Helper to recompose after editing
        auto recompose = [&]() {
            glm::mat4 t = glm::translate(glm::mat4(1.0f), translation);
            glm::mat4 r = glm::mat4_cast(rotation);
            glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
            skel3D->setLocalPose(selectedBoneIndex, t * r * s);
        };

        static glm::quat initialRotation;
        static bool isDraggingRotation = false;
        static glm::vec3 displayEuler;

        if (!isDraggingRotation) {
            displayEuler = glm::eulerAngles(rotation) * (180.0f / 3.14159265f);
        }

        if (ImGui::DragFloat3("Translation", &translation.x, 0.1f)) recompose();

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

        if (ImGui::DragFloat3("Scale", &scale.x, 0.1f)) recompose();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("Refresh", ImVec2(-1, 0))) skel3D->refresh(selectedBoneIndex);
        if (ImGui::Button("Refresh All", ImVec2(-1, 0))) skel3D->refreshAll(selectedBoneIndex);
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}
*/

// ============================================================================
// MAIN UI RENDERING FUNCTION
// ============================================================================

void tinyApp::renderUI() {
    auto& fpsRef = *fpsManager;
    auto& camRef = *project->camera();

    sceneRef = project->scene(State::sceneHandle);

    static float frameTime = 1.0f;
    static const float printInterval = 1.0f; // Print fps every second
    static float currentFps = 0.0f;

    float deltaTime = fpsRef.deltaTime;

    tinyFS& fs = project->fs();

    // ===== DEBUG PANEL WINDOW =====
    static bool showThemeEditor = false;
    if (tinyUI::Begin("Debug Panel", nullptr, ImGuiWindowFlags_NoCollapse)) {
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

        tinyUI::End();
    }

    // ===== THEME EDITOR WINDOW =====
    if (showThemeEditor) {
        if (tinyUI::Begin("Theme Editor", &showThemeEditor)) {
            tinyUI::ThemeStruct& theme = tinyUI::Theme();

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
                theme.apply();
            }

            tinyUI::End();
        }
    }

    // ===== HIERARCHY WINDOW - Scene & File System =====
    if (tinyUI::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse)) {
        tinyHandle sceneHandle = State::sceneHandle;

        if (!curScene) {
            ImGui::Text("No active scene");
        } else {
            static Splitter split;
            split.init(1);
            split.directionSize = ImGui::GetContentRegionAvail().y;
            split.calcRegionSizes();

            // ===== TOP: SCENE HIERARCHY =====
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0)); // Transparent background
            ImGui::BeginChild("SceneHierarchy", ImVec2(0, split.rSize(0)), true);

            ImGui::Text("[HDL %u.%u]", sceneHandle.index, sceneHandle.version);
            ImGui::Separator();
            RenderSceneNodeHierarchy();

            ImGui::EndChild();
            ImGui::PopStyleColor();
            
            // ===== HORIZONTAL SPLITTER =====
            split.render(0);
            
            // ===== BOTTOM: FILE SYSTEM HIERARCHY =====
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0)); // Transparent background
            ImGui::BeginChild("FileHierarchy", ImVec2(0, split.rSize(1)), true);
            
            ImGui::Text("File System");
            ImGui::Separator();
            RenderFileNodeHierarchy();

            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        tinyUI::End();
    }

    if (tinyUI::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse)) {
        RenderInspector(project.get());
        tinyUI::End();
    }

    if (tinyUI::Begin("Editor", nullptr, ImGuiWindowFlags_NoCollapse)) {
        RenderScriptEditor();
        // RenderSkeleNodeEditor();
        tinyUI::End();
    }
}
