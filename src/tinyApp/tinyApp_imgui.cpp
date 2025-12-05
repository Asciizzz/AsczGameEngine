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
#include <any>
#include <functional>

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
    struct Tab {
        std::string title;
        ImVec4 color = ImVec4(0.2f, 0.5f, 0.8f, 1.0f); // Default blue color
        
        // Dynamic state storage - each tab can store any type of data
        std::unordered_map<std::string, std::any> state;

        std::function<void(Tab&)> renderFunc;
        std::function<void(Tab&)> selectFunc;
        std::function<void(Tab&)> contextMenuFunc;
        std::function<void(Tab&)> hoverFunc;

        bool operator==(const Tab& o) const {
            return title == o.title;
        }

        template<typename T>
        T* getState(const std::string& key) {
            auto it = state.find(key);
            if (it == state.end()) return nullptr;
            try {
                return std::any_cast<T>(&it->second);
            } catch (const std::bad_any_cast&) {
                return nullptr;
            }
        }
        
        template<typename T>
        void setState(const std::string& key, const T& value) {
            state[key] = value;
        }
        
        template<typename T>
        T getStateOr(const std::string& key, const T& defaultValue) {
            T* ptr = getState<T>(key);
            return ptr ? *ptr : defaultValue;
        }
    };

    static std::vector<Tab> tabs;
    static size_t selectedTab = 0;
    void selectTab(size_t index) {
        if (index >= tabs.size()) return;

        selectedTab = index;
        if (tabs[index].selectFunc) {
            tabs[index].selectFunc(tabs[index]);
        }
    }

    size_t addTab(Tab tab, bool select = true) {
        for (size_t i = 0; i < tabs.size(); ++i) {
            if (tabs[i] == tab) {
                if (select) selectTab(i);
                return i;
            }
        }

        tabs.push_back(tab);
        if (select) selectTab(tabs.size() - 1);
        return tabs.size() - 1;
    }

    Tab* currentTab() {
        if (tabs.empty() || selectedTab >= tabs.size()) return nullptr;
        return &tabs[selectedTab];
    }
    
    void closeTab(size_t index) {
        if (index >= tabs.size()) return;
        
        tabs.erase(tabs.begin() + index);

        if (tabs.empty()) {
            selectedTab = 0;
        } else if (selectedTab >= tabs.size()) {
            selectedTab = tabs.size() - 1;
        }
        else if (index < selectedTab) {
            selectedTab--;
        }
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

    static void Render(const tinyTexture* texture, VkSampler sampler, ImVec2 drawSize = ImVec2(128, 128)) {
        uint64_t cacheKey = (uint64_t)texture->view();

        ImTextureID texId;

        auto it = textureCache.find(cacheKey);
        if (it == textureCache.end()) {
            texId = (ImTextureID)ImGui_ImplVulkan_AddTexture(
                sampler, texture->view(),
                ImageLayout::ShaderReadOnlyOptimal
            );
            textureCache[cacheKey] = texId;
        } else {
            texId = it->second; 
        }


        ImVec2 imgSize;
        float imgRatio = texture->aspectRatio();

        float drawRatio = drawSize.x / drawSize.y;

        if (drawRatio < imgRatio) {
            imgSize.x = drawSize.x;
            imgSize.y = drawSize.x / imgRatio;
        } else {
            imgSize.y = drawSize.y;
            imgSize.x = drawSize.y * imgRatio;
        }

        ImGui::Image(texId, imgSize);
    }
}

// ============================================================================
// UI INITIALIZATION
// ============================================================================

static const tinyVk::SamplerVk* imguiSampler;

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

    uiSampler = tinyVk::SamplerVk();
    SamplerConfig sampConfig;
    sampConfig.device = dvk->device;
    sampConfig.addressModes(VK_SAMPLER_ADDRESS_MODE_REPEAT);

    uiSampler.create(sampConfig);

    imguiSampler = &uiSampler;

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
        float size = regionSizes[index] * directionSize - splitterSize * ImGui::GetIO().FontGlobalScale;
        return size > 0.0f ? size : 0.01f;
    }

    void render(size_t index, const std::string& id = "Splitter") {
        if (index >= positions.size()) return;

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));

        ImVec2 size = horizontal ? ImVec2(-1, splitterSize) : ImVec2(splitterSize, -1);

        std::string splitterID = "##" + id + "_Splitter_" + std::to_string(index);
        ImGui::Button(splitterID.c_str(), size);

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

        // Double click to put it at the middle between neighbors
        if (M_DBCLICKED(ImGuiMouseButton_Left)) {
            float lowerBound = (index == 0) ? 0.0f : positions[index - 1];
            float upperBound = (index == positions.size() - 1) ? 1.0f : positions[index + 1];
            positions[index] = (lowerBound + upperBound) / 2.0f;
        }

        ImGui::PopStyleColor(3);
    }

    // These values are all relative (0.0 -> 1.0)
    std::vector<float> positions;   // n
    std::vector<float> regionSizes; // n + 1
};

// ============================================================================
// TAB CREATION HELPERS
// ============================================================================

static Editor::Tab CreateScriptEditorTab(const std::string& title, tinyHandle fHandle) {
    Editor::Tab tab;
    tab.title = title;
    tab.color = ImVec4(0.4f, 0.6f, 0.3f, 0.4f); // Green color for scripts

    // Initialize tab-specific state
    tab.setState<tinyHandle>("handle", fHandle);
    tab.setState<tinyType::ID>("type", tinyType::TypeID<tinyScript>());
    tab.setState<Splitter>("splitter", Splitter());
    tab.setState<tinyHandle>("lastScriptHandle", tinyHandle());
    tab.setState<bool>("horizontal", false);
    
    // Define select function - called when tab becomes active
    tab.selectFunc = [](Editor::Tab& self) {
        tinyHandle* fHandlePtr = self.getState<tinyHandle>("handle");
        if (!fHandlePtr) return;
        tinyHandle fHandle = *fHandlePtr;
        
        tinyFS& fs = projRef->fs();
        const tinyFS::Node* node = fs.fNode(fHandle);
        if (!node) return;
        
        tinyHandle dHandle = fs.dataHandle(fHandle);
        if (!dHandle.is<tinyScript>()) return;
        
        tinyScript* script = fs.rGet<tinyScript>(dHandle);
        if (script && !script->code.empty()) {
            CodeEditor::SetText(script->code);
        }
    };
    
    // Define context menu for script tab
    tab.contextMenuFunc = [](Editor::Tab& self) {
        tinyHandle* fHandlePtr = self.getState<tinyHandle>("handle");
        if (!fHandlePtr) return;
        tinyHandle fHandle = *fHandlePtr;
        tinyFS& fs = projRef->fs();
        const tinyFS::Node* node = fs.fNode(fHandle);
        if (!node) return;
        
        tinyHandle dHandle = fs.dataHandle(fHandle);
        tinyScript* script = fs.rGet<tinyScript>(dHandle);
        if (!script) return;
        
        if (ImGui::MenuItem("Compile")) {
            script->compile();
        }
        
        ImGui::Separator();
        
        bool* horizontal = self.getState<bool>("horizontal");
        if (horizontal) {
            const char* layoutLabel = *horizontal ? "Switch to Vertical" : "Switch to Horizontal";
            if (ImGui::MenuItem(layoutLabel)) {
                *horizontal = !*horizontal;
            }
        }
        
        ImGui::Separator();
        
        if (script->valid()) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Compiled v%u", script->version());
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.2f, 1.0f), "Not Compiled");
        }
    };
    
    // Define hover tooltip for script tab
    tab.hoverFunc = [](Editor::Tab& self) {
        tinyHandle* fHandlePtr = self.getState<tinyHandle>("handle");
        if (!fHandlePtr) return;
        tinyHandle fHandle = *fHandlePtr;
        
        tinyFS& fs = projRef->fs();
        const tinyFS::Node* node = fs.fNode(fHandle);
        if (!node) return;
        
        ImGui::BeginTooltip();
        ImGui::Text("Script: %s", node->cname());

        ImGui::Separator();
        
        // Display file path
        const char* fullPath = fs.path(fHandle);
        if (fullPath) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Path: %s", fullPath);
        }
        
        ImGui::Separator();
        
        tinyHandle dHandle = fs.dataHandle(fHandle);
        tinyScript* script = fs.rGet<tinyScript>(dHandle);
        if (script) {
            if (script->valid()) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Compiled v%u", script->version());
            } else if (script->debug().empty()) {
                ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.2f, 1.0f), "Not Compiled");
            } else {
                ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "Compilation Error");
            }
        }
        ImGui::EndTooltip();
    };
    
    // Define the custom render function for script editing
    tab.renderFunc = [](Editor::Tab& self) {
        tinyHandle* fHandlePtr = self.getState<tinyHandle>("handle");
        if (!fHandlePtr) return;
        tinyHandle fHandle = *fHandlePtr;
        
        tinyFS& fs = projRef->fs();
        const tinyFS::Node* node = fs.fNode(fHandle);
        if (!node) return;
        
        tinyHandle dHandle = fs.dataHandle(fHandle);
        if (!dHandle.is<tinyScript>()) return;
        
        tinyScript* script = fs.rGet<tinyScript>(dHandle);
        
        Splitter* split = self.getState<Splitter>("splitter");
        if (!split) return;
        
        bool* horizontal = self.getState<bool>("horizontal");
        if (!horizontal) return;
        
        split->init(1);
        split->horizontal = *horizontal;
        split->directionSize = *horizontal ? ImGui::GetContentRegionAvail().y : ImGui::GetContentRegionAvail().x;
        split->calcRegionSizes();
        
        // Code editor
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImVec2 codeEditorSize = *horizontal ? ImVec2(0, split->rSize(0)) : ImVec2(split->rSize(0), 0);
        ImGui::BeginChild("CodeEditor", codeEditorSize, true);
        
        tinyDebug& debug = script->debug();
        
        if (CodeEditor::IsTextChanged() && script) {
            script->code = CodeEditor::GetText();
        }
        CodeEditor::Render(node->cname());
        
        ImGui::EndChild();
        ImGui::PopStyleColor();
        
        if (!*horizontal) ImGui::SameLine();
        
        split->render(0);
        
        if (!*horizontal) ImGui::SameLine();
        
        // Debug console
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImVec2 debugConsoleSize = *horizontal ? ImVec2(0, split->rSize(1)) : ImVec2(split->rSize(1), 0);
        ImGui::BeginChild("DebugTerminal", debugConsoleSize, true);
        
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 1.0f, 1.0f), "Debug Console");
        ImGui::SameLine();
        if (ImGui::Button("Clear")) debug.clear();
        ImGui::Separator();
        
        for (const auto& line : debug.logs()) {
            ImGui::TextColored(ImVec4(line.color[0], line.color[1], line.color[2], 1.0f), "%s", line.c_str());
        }
        
        ImGui::EndChild();
        ImGui::PopStyleColor();
    };
    
    return tab;
}

static Editor::Tab CreateRtSkeletonEditorTab(const std::string& title, tinyHandle nHandle) {
    Editor::Tab tab;
    tab.title = title;
    tab.color = ImVec4(0.6f, 0.3f, 0.7f, 0.4f); // Purple color for skeletons
    
    // Initialize tab-specific state
    tab.setState<tinyHandle>("handle", nHandle);
    tab.setState<tinyType::ID>("type", tinyType::TypeID<rtSKELE3D>());
    tab.setState<Splitter>("splitter", Splitter());
    tab.setState<int>("selectedBoneIndex", -1);
    tab.setState<tinyHandle>("lastSkeletonHandle", tinyHandle());
    tab.setState<glm::quat>("initialRotation", glm::quat());
    tab.setState<bool>("isDraggingRotation", false);
    tab.setState<glm::vec3>("displayEuler", glm::vec3(0.0f));
    
    // Define context menu for skeleton tab
    tab.contextMenuFunc = [](Editor::Tab& self) {
        tinyHandle* nHandlePtr = self.getState<tinyHandle>("handle");
        if (!nHandlePtr) return;
        tinyHandle nHandle = *nHandlePtr;
        
        rtScene* scene = sceneRef;
        rtSKELE3D* skel3D = scene->nGetComp<rtSKELE3D>(nHandle);
        if (!skel3D) return;
        
        const tinySkeleton* skeleton = skel3D->rSkeleton();
        if (!skeleton) return;
        
        int* selectedBoneIndex = self.getState<int>("selectedBoneIndex");
        if (!selectedBoneIndex) return;
        
        if (*selectedBoneIndex >= 0) {
            if (ImGui::MenuItem("Refresh Selected Bone")) {
                skel3D->refresh(*selectedBoneIndex);
            }
            if (ImGui::MenuItem("Refresh All from Selected")) {
                skel3D->refresh(*selectedBoneIndex, true);
            }
            ImGui::Separator();
        }
        
        ImGui::Text("Bone Count: %zu", skeleton->bones.size());
    };
    
    // Define hover tooltip for skeleton tab
    tab.hoverFunc = [](Editor::Tab& self) {
        tinyHandle* nHandlePtr = self.getState<tinyHandle>("handle");
        if (!nHandlePtr) return;
        tinyHandle nHandle = *nHandlePtr;
        
        rtScene* scene = sceneRef;
        rtSKELE3D* skel3D = scene->nGetComp<rtSKELE3D>(nHandle);
        if (!skel3D) return;
        
        const tinySkeleton* skeleton = skel3D->rSkeleton();
        if (!skeleton) return;
        
        int* selectedBoneIndex = self.getState<int>("selectedBoneIndex");
        
        ImGui::BeginTooltip();
        ImGui::Text("Skeleton Editor");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Node: [%u, %u]", nHandle.idx(), nHandle.ver());
        ImGui::Separator();
        ImGui::Text("Bones: %zu", skeleton->bones.size());
        if (selectedBoneIndex && *selectedBoneIndex >= 0) {
            ImGui::Text("Selected: %d - %s", *selectedBoneIndex, skeleton->bones[*selectedBoneIndex].name.c_str());
        }
        ImGui::EndTooltip();
    };
    
    // Define the custom render function for skeleton editing
    tab.renderFunc = [&](Editor::Tab& self) {
        tinyHandle* nHandlePtr = self.getState<tinyHandle>("handle");
        if (!nHandlePtr) return;
        tinyHandle nHandle = *nHandlePtr;
        
        rtScene* scene = sceneRef;
        rtSKELE3D* skel3D = scene->nGetComp<rtSKELE3D>(nHandle);
        if (!skel3D) return;
        
        const tinyFS& fs = projRef->fs();
        
        const tinySkeleton* skeleton = skel3D->rSkeleton();
        if (!skeleton) return;
        
        // Get state from tab
        int* selectedBoneIndex = self.getState<int>("selectedBoneIndex");
        tinyHandle* lastSkeletonHandle = self.getState<tinyHandle>("lastSkeletonHandle");
        if (!selectedBoneIndex || !lastSkeletonHandle) return;
        
        // Reset selection if skeleton changed
        if (*lastSkeletonHandle != nHandle) {
            *selectedBoneIndex = -1;
            *lastSkeletonHandle = nHandle;
        }
        
        Splitter* split = self.getState<Splitter>("splitter");
        if (!split) return;
        
        split->init(1);
        split->horizontal = false;
        split->directionSize = ImGui::GetContentRegionAvail().x;
        split->calcRegionSizes();
        
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::BeginChild("BoneHierarchy", ImVec2(split->rSize(0), 0), true);
        
        std::function<void(int)> renderBoneTree = [&](int boneIndex) {
            if (boneIndex < 0 || boneIndex >= static_cast<int>(skeleton->bones.size())) return;
            
            const tinyBone& bone = skeleton->bones[boneIndex];
            
            ImGui::PushID(boneIndex);
            
            bool hasChildren = !bone.children.empty();
            bool isSelected = (*selectedBoneIndex == boneIndex);
            
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
                *selectedBoneIndex = boneIndex;
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
        
        split->render(0, "SkeletonEditorSplitter");
        
        ImGui::SameLine();
        
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::BeginChild("BoneTransformEditor", ImVec2(split->rSize(1), 0), true);
        
        if (*selectedBoneIndex >= 0 && *selectedBoneIndex < static_cast<int>(skeleton->bones.size())) {
            const tinyBone& selectedBone = skeleton->bones[*selectedBoneIndex];
            ImGui::Text("Bone: %d - %s", *selectedBoneIndex, selectedBone.name.c_str());
            ImGui::Separator();
            
            glm::mat4& boneLocal = skel3D->localPose(*selectedBoneIndex);
            
            glm::vec3 translation, scale, skew;
            glm::quat rotation;
            glm::vec4 perspective;
            glm::decompose(boneLocal, scale, rotation, translation, skew, perspective);
            
            // Helper to recompose after editing
            auto recompose = [&]() {
                glm::mat4 t = glm::translate(glm::mat4(1.0f), translation);
                glm::mat4 r = glm::mat4_cast(rotation);
                glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
                boneLocal = t * r * s;
            };
            
            glm::quat* initialRotation = self.getState<glm::quat>("initialRotation");
            bool* isDraggingRotation = self.getState<bool>("isDraggingRotation");
            glm::vec3* displayEuler = self.getState<glm::vec3>("displayEuler");
            
            if (!initialRotation || !isDraggingRotation || !displayEuler) return;
            
            if (!*isDraggingRotation) {
                *displayEuler = glm::eulerAngles(rotation) * (180.0f / 3.14159265f);
            }
            
            if (ImGui::DragFloat3("Translation", &translation.x, 0.1f)) recompose();
            
            if (ImGui::DragFloat3("Rotation", &displayEuler->x, 0.5f)) {
                // Euler angle is a b*tch
                if (!*isDraggingRotation) {
                    *initialRotation = rotation;
                    *isDraggingRotation = true;
                }
                glm::vec3 initialEuler = glm::eulerAngles(*initialRotation) * (180.0f / 3.14159265f);
                glm::vec3 delta = *displayEuler - initialEuler;
                rotation = *initialRotation * glm::quat(glm::radians(delta));
                recompose();
            }
            
            if (!ImGui::IsItemActive()) {
                *isDraggingRotation = false;
            }
            
            if (ImGui::DragFloat3("Scale", &scale.x, 0.1f)) recompose();
            
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Refresh", ImVec2(-1, 0))) skel3D->refresh(*selectedBoneIndex);
            if (ImGui::Button("Refresh All", ImVec2(-1, 0))) skel3D->refresh(*selectedBoneIndex, true);
            ImGui::PopStyleColor();
        }
        
        ImGui::EndChild();
        ImGui::PopStyleColor();
    };
    
    return tab;
}

static Editor::Tab CreateRtMorphTargetEditorTab(const std::string& title, tinyHandle nHandle) {
    Editor::Tab tab;
    tab.title = title;
    tab.color = ImVec4(0.8f, 0.5f, 0.3f, 0.4f); // Orange color for morph editor
    
    // Initialize tab-specific state
    tab.setState<tinyHandle>("handle", nHandle);
    tab.setState<tinyType::ID>("type", tinyType::TypeID<rtMESHRD3D>());
    tab.setState<Splitter>("splitter", Splitter());
    tab.setState<std::string>("searchFilter", std::string());
    tab.setState<float>("weightMin", -1.0f);
    tab.setState<float>("weightMax", 1.0f);
    tab.setState<bool>("enableWeightFilter", false);
    
    // Define select function
    tab.selectFunc = [](Editor::Tab& self) {
        // Nothing special needed for morph editor on select
    };
    
    // Define context menu
    tab.contextMenuFunc = [](Editor::Tab& self) {
        tinyHandle* nHandlePtr = self.getState<tinyHandle>("handle");
        if (!nHandlePtr) return;
        tinyHandle nHandle = *nHandlePtr;
        
        rtScene* scene = sceneRef;
        rtMESHRD3D* meshRD = scene->nGetComp<rtMESHRD3D>(nHandle);
        if (!meshRD) return;
        
        if (ImGui::MenuItem("Reset All Weights")) {
            std::vector<float>& weights = meshRD->mrphWeights();
            for (float& w : weights) w = 0.0f;
        }
        
        ImGui::Separator();
        ImGui::Text("Morph Count: %zu", meshRD->mrphWeights().size());
    };
    
    // Define hover tooltip
    tab.hoverFunc = [](Editor::Tab& self) {
        tinyHandle* nHandlePtr = self.getState<tinyHandle>("handle");
        if (!nHandlePtr) return;
        tinyHandle nHandle = *nHandlePtr;
        
        rtScene* scene = sceneRef;
        rtMESHRD3D* meshRD = scene->nGetComp<rtMESHRD3D>(nHandle);
        if (!meshRD) return;
        
        ImGui::BeginTooltip();
        ImGui::Text("Morph Target Editor");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Node: [%u, %u]", nHandle.idx(), nHandle.ver());
        ImGui::Separator();
        ImGui::Text("Targets: %zu", meshRD->mrphWeights().size());
        ImGui::EndTooltip();
    };
    
    // Define render function
    tab.renderFunc = [](Editor::Tab& self) {
        tinyHandle* nHandlePtr = self.getState<tinyHandle>("handle");
        if (!nHandlePtr) return;
        tinyHandle nHandle = *nHandlePtr;
        
        rtScene* scene = sceneRef;
        rtMESHRD3D* meshRD = scene->nGetComp<rtMESHRD3D>(nHandle);
        if (!meshRD) {
            ImGui::Text("Mesh Render component not found");
            return;
        }
        
        const tinyFS& fs = projRef->fs();
        tinyHandle meshHandle = meshRD->meshHandle();
        const tinyMesh* mesh = fs.rGet<tinyMesh>(meshHandle);
        if (!mesh) {
            ImGui::Text("No mesh assigned");
            return;
        }
        
        std::vector<float>& mrphWeights = meshRD->mrphWeights();
        const auto& morphTargetNames = mesh->mrphTargetNames();
        
        if (morphTargetNames.empty()) {
            ImGui::Text("This mesh has no morph targets");
            return;
        }
        
        // Get state
        Splitter* split = self.getState<Splitter>("splitter");
        std::string* searchFilter = self.getState<std::string>("searchFilter");
        float* weightMin = self.getState<float>("weightMin");
        float* weightMax = self.getState<float>("weightMax");
        bool* enableWeightFilter = self.getState<bool>("enableWeightFilter");
        
        if (!split || !searchFilter || !weightMin || !weightMax || !enableWeightFilter) return;
        
        split->init(1);
        split->horizontal = false;
        split->directionSize = ImGui::GetContentRegionAvail().x;
        split->calcRegionSizes();
        
        // Left panel: Search and filter controls
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::BeginChild("FilterPanel", ImVec2(split->rSize(0), 0), true);
        
        ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.4f, 1.0f), "Search & Filter");
        ImGui::Separator();
        
        // Search by name
        char searchBuf[256];
        strncpy(searchBuf, searchFilter->c_str(), 255);
        searchBuf[255] = '\0';
        
        ImGui::Text("Search Name:");

        // Make input full width
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##search", searchBuf, 256)) {
            *searchFilter = searchBuf;
        }
        ImGui::PopItemWidth();
        
        ImGui::Separator();
        
        // Weight range filter
        ImGui::Text("Weight Range:");
        ImGui::Checkbox("Enable Weight Filter", enableWeightFilter);
        
        if (*enableWeightFilter) {
            ImGui::DragFloat("Min##weightmin", weightMin, 0.01f, -1.0f, 1.0f);
            ImGui::DragFloat("Max##weightmax", weightMax, 0.01f, -1.0f, 1.0f);
            
            // Clamp values
            if (*weightMin < -1.0f) *weightMin = -1.0f;
            if (*weightMin > 1.0f) *weightMin = 1.0f;
            if (*weightMax < -1.0f) *weightMax = -1.0f;
            if (*weightMax > 1.0f) *weightMax = 1.0f;
            
            // Ensure min <= max
            if (*weightMin > *weightMax) {
                float temp = *weightMin;
                *weightMin = *weightMax;
                *weightMax = temp;
            }
        }
        
        ImGui::Separator();
        
        if (ImGui::Button("Clear Filters", ImVec2(-1, 0))) {
            searchFilter->clear();
            *enableWeightFilter = false;
            *weightMin = -1.0f;
            *weightMax = 1.0f;
        }
        
        if (ImGui::Button("Reset All Weights", ImVec2(-1, 0))) {
            for (float& w : mrphWeights) w = 0.0f;
        }
        
        ImGui::Separator();
        
        // Show stats
        int visibleCount = 0;
        for (size_t j = 0; j < morphTargetNames.size(); ++j) {
            if (j >= mrphWeights.size()) continue;
            
            float weight = mrphWeights[j];
            
            // Apply weight range filter
            if (*enableWeightFilter && (weight < *weightMin || weight > *weightMax)) {
                continue;
            }
            
            // Apply name filter
            if (!searchFilter->empty()) {
                const auto& targetInfo = morphTargetNames[j];
                if (targetInfo.name.find(*searchFilter) == std::string::npos) {
                    continue;
                }
            }
            
            visibleCount++;
        }
        
        ImGui::Text("Showing: %d / %zu", visibleCount, morphTargetNames.size());
        
        ImGui::EndChild();
        ImGui::PopStyleColor();
        
        ImGui::SameLine();
        split->render(0);
        ImGui::SameLine();
        
        // Right panel: Morph target sliders
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::BeginChild("MorphTargets", ImVec2(split->rSize(1), 0), true);
        
        ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.4f, 1.0f), "Morph Targets");
        ImGui::Separator();
        
        // Render filtered morph targets
        for (size_t j = 0; j < morphTargetNames.size(); ++j) {
            if (j >= mrphWeights.size()) continue;
            
            float& weight = mrphWeights[j];
            
            // Apply weight range filter
            if (*enableWeightFilter && (weight < *weightMin || weight > *weightMax)) {
                continue;
            }
            
            // Apply name filter
            const auto& targetInfo = morphTargetNames[j];
            if (!searchFilter->empty()) {
                if (targetInfo.name.find(*searchFilter) == std::string::npos) {
                    continue;
                }
            }
            
            std::string label = "[" + std::to_string(j) + "] " + targetInfo.name;
            std::string id = "##morph_" + std::to_string(nHandle.raw()) + "_" + std::to_string(j);
            
            ImGui::PushItemWidth(-80);
            if (ImGui::SliderFloat(id.c_str(), &weight, -1.0f, 1.0f, label.c_str())) {
                // Value changed
            }
            ImGui::PopItemWidth();
            
            ImGui::SameLine();
            
            // Reset button
            ImGui::PushID(static_cast<int>(j));
            if (ImGui::Button("Reset", ImVec2(70, 0))) {
                weight = 0.0f;
            }
            ImGui::PopID();
            
            // Show precise value on hover
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%.3f", weight);
                ImGui::EndTooltip();
            }
        }
        
        ImGui::EndChild();
        ImGui::PopStyleColor();
    };
    
    return tab;
}

static Editor::Tab CreateRtScriptEditorTab(const std::string& title, tinyHandle nHandle) {
    Editor::Tab tab;
    tab.title = title;
    tab.color = ImVec4(0.3f, 0.5f, 0.8f, 0.4f); // Blue color for script components
    
    // Initialize tab-specific state
    tab.setState<tinyHandle>("handle", nHandle);
    tab.setState<tinyType::ID>("type", tinyType::TypeID<rtSCRIPT>());
    tab.setState<Splitter>("splitter", Splitter());

    // Splitter start at 30 (variables) / 70 (runtime debug)
    Splitter* split = tab.getState<Splitter>("splitter");
    split->init(1);
    split->positions[0] = 0.3f;
    split->horizontal = false;

    tinyFS& fs = projRef->fs();

    // Define context menu for script component tab
    tab.contextMenuFunc = [](Editor::Tab& self) {
        tinyHandle* nHandlePtr = self.getState<tinyHandle>("handle");
        if (!nHandlePtr) return;
        tinyHandle nHandle = *nHandlePtr;

        rtScene* scene = sceneRef;
        rtSCRIPT* scriptComp = scene->nGetComp<rtSCRIPT>(nHandle);
        if (!scriptComp) return;
    };
    
    // Define hover tooltip for script component tab
    tab.hoverFunc = [](Editor::Tab& self) {
        tinyHandle* nHandlePtr = self.getState<tinyHandle>("handle");
        if (!nHandlePtr) return;
        tinyHandle nHandle = *nHandlePtr;

        rtScene* scene = sceneRef;
        rtSCRIPT* scriptComp = scene->nGetComp<rtSCRIPT>(nHandle);
        if (!scriptComp) return;
        
        ImGui::BeginTooltip();
        ImGui::Text("Script Component");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Node: [%u, %u]", nHandle.idx(), nHandle.ver());
        ImGui::EndTooltip();
    };
    
    // Define the custom render function for script component
    tab.renderFunc = [](Editor::Tab& self) {
        tinyHandle* nHandlePtr = self.getState<tinyHandle>("handle");
        if (!nHandlePtr) return;
        tinyHandle nHandle = *nHandlePtr;
        
        rtScene* scene = sceneRef;
        rtSCRIPT* scriptComp = scene->nGetComp<rtSCRIPT>(nHandle);
        if (!scriptComp) {
            ImGui::Text("Script component not found");
            return;
        }

        // Create verticle splitter for: Variables and Runtime Debug
        Splitter* split = self.getState<Splitter>("splitter");
        if (!split) return;

        split->init(1);
        split->horizontal = false;
        split->directionSize = ImGui::GetContentRegionAvail().x;
        split->calcRegionSizes();

        // Variables Editor

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImVec2 varEditorSize = ImVec2(split->rSize(0), 0); 
        ImGui::BeginChild("CodeEditor", varEditorSize, true);

        tinyVarsMap& vMap = scriptComp->vars;
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

                    std::string type = "Other";
                    if (val.is<rtNode>())  type = "Node"; else
                    if (val.is<rtScene>()) type = "Scene"; else
                    if (val.is<tinyScript>())  type = "Script";

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
                        [&]() {
                            if (ImGui::BeginDragDropTarget()) {
                                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PAYLOAD")) {
                                    Payload* data = (Payload*)payload->Data;
                                    val = data->handle;
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

        // Runtime Debug Panel
        ImGui::SameLine();
        split->render(0, "RtScriptEditorSplitter");
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImVec2 debugPanelSize = ImVec2(split->rSize(1), 0);
        ImGui::BeginChild("RuntimeDebug", debugPanelSize, true, ImGuiWindowFlags_HorizontalScrollbar);

        ImGui::TextColored(ImVec4(0.3f, 0.6f, 0.9f, 1.0f), "Runtime Debug");
        ImGui::SameLine();
        if (ImGui::Button("Clear")) scriptComp->debug.clear();

        ImGui::Separator();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, ImGui::GetStyle().ItemSpacing.y));
        for (const auto& line : scriptComp->debug.logs()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(line.color[0], line.color[1], line.color[2], 1.0f));
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    };

    return tab;
}

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
        scene->rootHandle(), 0,
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

            // Click on the icon to expand/collapse
            if (ImGui::IsItemClicked()) {
                if (hasChildren) State::setExpanded(h, !State::isExpanded(h));
            }

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
            if (!node) return {};

            std::vector<tinyHandle> children = node->children;
            std::sort(children.begin(), children.end(), [](tinyHandle a, tinyHandle b) {
                const rtNode* nodeA = sceneRef->node(a);
                const rtNode* nodeB = sceneRef->node(b);

                bool aHasChildren = nodeA && nodeA->childrenCount() > 0;
                bool bHasChildren = nodeB && nodeB->childrenCount() > 0;
                if (aHasChildren != bHasChildren) return aHasChildren > bHasChildren;
                return nodeA->name < nodeB->name;
            });
            return children;
        },
        // LClick
        [](tinyHandle h) {
            State::selected = h;
        },
        // RClick
        [](tinyHandle h) {
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

            bool canDelete = h != sceneRef->rootHandle();
            if (ImGui::MenuItem("Erase", nullptr, false, canDelete)) sceneRef->nErase(h);
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

                    // Helper function to display component info
                    bool hasComp = false;
                    auto displayCompInfo = [&](std::string name, float r, float g, float b) {
                        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "%s", name.c_str());
                        hasComp = true;
                    };

                    if (node->has<rtTRANFM3D>()) displayCompInfo("Transform 3D", 0.5f, 1.0f, 0.5f);
                    if (node->has<rtMESHRD3D>()) displayCompInfo("Mesh Renderer 3D", 0.5f, 0.5f, 1.0f);
                    if (node->has<rtSKELE3D>())  displayCompInfo("Skeleton 3D", 1.0f, 0.5f, 1.0f);
                    if (node->has<rtSCRIPT>())   displayCompInfo("Script", 1.0f, 1.0f, 0.5f);

                    if (!hasComp) { ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No Component"); }

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
                        scene->instantiate(dataHandle, h);
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
        fs.rootHandle(), 0,
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

            tinyHandle dHandle = fs.dataHandle(h);

            // Open a new script editor tab if not already open
            if (dHandle.is<tinyScript>()) {
                // bool alreadyOpen = false;
                int openTabIndex = -1;
                // for (auto& tab : Editor::tabs) {
                for (size_t i = 0; i < Editor::tabs.size(); ++i) {
                    Editor::Tab& tab = Editor::tabs[i];
                    tinyHandle* tabHandle = tab.getState<tinyHandle>("handle");
                    tinyType::ID* tabType = tab.getState<tinyType::ID>("type");
                    if (tabHandle && tabType && *tabHandle == h && *tabType == tinyType::TypeID<tinyScript>()) {
                        openTabIndex = static_cast<int>(i);
                        break;
                    }
                }
                if (openTabIndex == -1) {
                    Editor::addTab(CreateScriptEditorTab(node->name, h));
                } else {
                    Editor::selectTab(openTabIndex);
                }

                // There is not a lot of reason to select script file when the editor already opens it
            } else {
                State::selected = h;
            }
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
                rtScene* scene = fs.rGet<rtScene>(dHandle);
                if (RenderMenuItemToggle("Make Active", "Active", State::isActiveScene(dHandle))) {
                    State::sceneHandle = dHandle;
                }
                // if (RenderMenuItemToggle("Cleanse", "Cleansed", scene->isClean())) {
                //     scene->cleanse();
                // }
            }
            if (dHandle.is<tinyScript>()) {
                tinyScript* script = fs.rGet<tinyScript>(dHandle);
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

            bool canDelete = h != fs.rootHandle();
            if (ImGui::MenuItem("Delete", nullptr, nullptr, canDelete)) projRef->fRemove(h);
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
                Editor::addTab(CreateScriptEditorTab(node->name, h));
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
                    
                    tinyHandle dHandle = fs.dataHandle(h);

                    tinyFS::TypeInfo* typeInfo = fs.typeInfo(dHandle.tID());
                    ImGui::TextColored(IMVEC4_COLOR3(typeInfo->color, 1.0f), ".%s", typeInfo->c_str());

                    // If texture, show preview
                    if (dHandle.is<tinyTexture>()) {
                        const tinyTexture* tex = fs.rGet<tinyTexture>(dHandle);
                        if (tex) {
                            ImGui::Separator();
                            Texture::Render(tex, *imguiSampler, ImVec2(256, 256));
                        }
                    }
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

    // Decompose local transform
    glm::vec3 pos, scale, skew;
    glm::quat rot;
    glm::vec4 persp;
    glm::decompose(trfm3D->local, scale, rot, pos, skew, persp);

    static glm::quat initialRotation;
    static bool isDraggingRotation = false;
    static glm::vec3 displayEuler;

    bool changed = false;

    float vSpeed = ImGui::GetIO().KeyShift ? 1.0f : ImGui::GetIO().KeyCtrl ? 0.01f : 0.1f;

    if (ImGui::DragFloat3("Translation", &pos.x, vSpeed)) {
        changed = true;
    }

    if (ImGui::DragFloat3("Rotation", &displayEuler.x, vSpeed * 5.0f)) {
        // Euler angle is a b*tch
        if (!isDraggingRotation) {
            initialRotation = rot;
            isDraggingRotation = true;
        }
        glm::vec3 initialEuler = glm::eulerAngles(initialRotation) * (180.0f / 3.14159265f);
        glm::vec3 delta = displayEuler - initialEuler;
        rot = initialRotation * glm::quat(glm::radians(delta));
        changed = true;
    }

    if (!ImGui::IsItemActive()) {
        isDraggingRotation = false;
    }

    if (ImGui::DragFloat3("Scale", &scale.x, vSpeed)) {
        changed = true;
    }

    // Recompose transform if changed
    if (changed) {
        trfm3D->local = glm::translate(glm::mat4(1.0f), pos) * 
                        glm::mat4_cast(rot) * 
                        glm::scale(glm::mat4(1.0f), scale);
    }

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("Reset", ImVec2(-1, 0))) {
        trfm3D->local = glm::mat4(1.0f);
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

    tinyHandle meshHandle = meshRD->meshHandle();
    const tinyMesh* mesh = fs.rGet<tinyMesh>(meshHandle);

    tinyHandle meshFHandle = fs.rDataToFile(meshHandle);

    RenderDragField(
        [&fs, meshFHandle]() { return fs.nameCStr(meshFHandle); },
        "No Mesh Assigned",
        [&]() { 
            if (!mesh) return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
            return IMVEC4_COLOR3(fs.typeInfo<tinyMesh>()->color, 1.0);
        },
        ImVec4(0.2f, 0.2f, 0.2f, 1.0f),
        [mesh]() { return mesh != nullptr; },
        [&fs, meshRD]() {
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PAYLOAD")) {
                    Payload* data = (Payload*)payload->Data;
                    if (!data->is<tinyNodeFS>()) { ImGui::EndDragDropTarget(); return; }

                    tinyHandle fHandle = data->handle;
                    tinyHandle dHandle = fs.dataHandle(fHandle);

                    const tinyMesh* rMesh = fs.rGet<tinyMesh>(dHandle);
                    if (!rMesh) { ImGui::EndDragDropTarget(); return; }

                    meshRD->assignMesh(dHandle, rMesh);

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

    tinyHandle skeleNodeHandle = meshRD->skeleNodeHandle();

    // For skeleton node
    RenderDragField(
        [&]() { return "Skeleton Component"; },
        "No Skeleton Node Assigned",
        []() { return ImVec4(0.8f, 0.6f, 0.6f, 1.0f); },
        ImVec4(0.2f, 0.2f, 0.2f, 1.0f),
        [&]() { // Active condition
            rtSKELE3D* skel3D = scene->nGetComp<rtSKELE3D>(skeleNodeHandle);
            return skel3D && skel3D->rSkeleton() != nullptr;
        },
        [&]() { // Drag Drop handling
            if (!ImGui::BeginDragDropTarget()) return;

            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PAYLOAD")) {
                Payload* data = (Payload*)payload->Data;
                if (!data->is<rtNode>()) { ImGui::EndDragDropTarget(); return; }

                tinyHandle nHandle = data->handle;
                rtSKELE3D* skel3D = scene->nGetComp<rtSKELE3D>(nHandle);
                if (!skel3D || !skel3D->rSkeleton()) { ImGui::EndDragDropTarget(); return; }

                meshRD->assignSkeleNode(nHandle);

                ImGui::EndDragDropTarget();
            }
        },
        []() {  },
        [&]() {
            ImGui::BeginTooltip();

            rtSKELE3D* skel3D = scene->nGetComp<rtSKELE3D>(skeleNodeHandle);
            if (skel3D && skel3D->rSkeleton()) {
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "[Scene]");
                ImGui::SameLine();
                ImGui::Text("Skeleton Handle: [%u, %u]", skeleNodeHandle.idx(), skeleNodeHandle.ver());
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No Skeleton Assigned");
            }

            ImGui::EndTooltip();
        }
    );

    // Morph target editor button
    if (mesh && !mesh->mrphTargetNames().empty() && !meshRD->mrphWeights().empty()) {
        ImGui::Separator();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.5f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.6f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.4f, 0.2f, 1.0f));
        
        std::string buttonLabel = "Open Morph Target Editor (" + std::to_string(meshRD->mrphWeights().size()) + " targets)";
        if (ImGui::Button(buttonLabel.c_str(), ImVec2(-1, 0))) {
            // Create and open morph editor tab
            rtNode* node = scene->node(nHandle);
            std::string tabTitle = "Morph: " + (node ? node->name : "Unknown");
            Editor::Tab morphTab = CreateRtMorphTargetEditorTab(tabTitle, nHandle);
            Editor::addTab(morphTab);
        }
        
        ImGui::PopStyleColor(3);
    }
}

/*

static void RenderBONE3D(const tinyFS& fs, rtScene* scene, rtScene::NWrap& wrap) {
    tinyRT_BONE3D* bone3D = wrap.bone3D;
    if (!bone3D) return;

    tinyHandle skeleNodeHandle = bone3D->skeleNodeHandle;
}

*/


static void RenderSKEL3D(const tinyFS& fs, rtScene* scene, tinyHandle nHandle) {
    rtSKELE3D* skel3D = scene->nGetComp<rtSKELE3D>(nHandle);
    if (!skel3D) return;

    RenderDragField(
        [scene, nHandle]() { return scene->nName(nHandle).c_str(); },
        "Skeleton Component",
        []() { return ImVec4(0.8f, 0.6f, 0.6f, 1.0f); },
        ImVec4(0.2f, 0.2f, 0.2f, 1.0f),
        [&]() {
            return skel3D->rSkeleton() != nullptr;
        },
        [&]() {
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PAYLOAD")) {
                    Payload* data = (Payload*)payload->Data;
                    if (!data->is<tinyNodeFS>()) { ImGui::EndDragDropTarget(); return; }

                    tinyHandle fHandle = data->handle;
                    tinyHandle dHandle = fs.dataHandle(fHandle);
                    if (!dHandle.is<tinySkeleton>()) { ImGui::EndDragDropTarget(); return; }

                    skel3D->init(&fs.r().view<tinySkeleton>(), dHandle);

                    ImGui::EndDragDropTarget();
                }
            }
        },
        []() {  },
        [&]() {
            ImGui::BeginTooltip();

            if (skel3D->rSkeleton()) {
                tinyHandle skeleHandle = skel3D->skeleHandle();

                tinyHandle fHandle = fs.rDataToFile(skeleHandle);
                const char* fullPath = fs.path(fHandle);
                fullPath = fullPath ? fullPath : "<Invalid Skeleton>";

                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "[FS]");
                ImGui::SameLine(); ImGui::Text("%s", fullPath);
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No Skeleton Assigned");
            }

            ImGui::EndTooltip();
        }
    );

    // For the time being just open the editor
    if (ImGui::Button("Open Runtime Skeleton Editor", ImVec2(-1, 0))) {
        std::string handle = "[" + std::to_string(nHandle.idx()) + ", " + std::to_string(nHandle.ver()) + "]";
        std::string title = scene->nName(nHandle) + " " + handle + " - Skeleton";
        Editor::addTab(CreateRtSkeletonEditorTab(title, nHandle));
    }
}

static void RenderSCRIPT(const tinyFS& fs, rtScene* scene, tinyHandle nHandle) {
    rtSCRIPT* scriptComp = scene->nGetComp<rtSCRIPT>(nHandle);
    if (!scriptComp) return;

    rtNode* node = scene->node(nHandle);

    tinyHandle scriptHandle = scriptComp->scriptHandle;
    tinyHandle scriptFHandle = fs.rDataToFile(scriptHandle);

    const tinyScript* scriptPtr = fs.rGet<tinyScript>(scriptHandle);

    // Render the drag field for the script file
    RenderDragField(
        [&fs, scriptFHandle]() { return fs.name(scriptFHandle).c_str(); },
        "No Script Assigned",
        [&]() { return ImVec4(0.4f, 0.6f, 0.9f, 1.0f); },
        ImVec4(0.2f, 0.2f, 0.2f, 1.0f),
        [&]() { return scriptPtr != nullptr; },
        [&fs, scriptComp, &scriptPtr]() {
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PAYLOAD")) {
                    Payload* data = (Payload*)payload->Data;
                    if (!data->is<tinyNodeFS>()) { ImGui::EndDragDropTarget(); return; }
                    
                    tinyHandle fHandle = data->handle;
                    tinyHandle dHandle = fs.dataHandle(fHandle);
                    if (!dHandle.is<tinyScript>()) { ImGui::EndDragDropTarget(); return; }

                    scriptPtr = fs.rGet<tinyScript>(dHandle);
                    if (scriptPtr) {
                        scriptComp->scriptHandle = dHandle;

                        scriptPtr->initVars(scriptComp->vars);
                        scriptPtr->initLocals(scriptComp->locals);
                    }

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

    if (!scriptPtr) return; // No valid script assigned

    if (ImGui::Button("Open in Editor", ImVec2(-1, 0))) {
        std::string title = node->name;
        title += " [" + std::to_string(nHandle.idx()) + ", " + std::to_string(nHandle.ver()) + "] Script";

        Editor::addTab(CreateRtScriptEditorTab(title, nHandle));
    }
}

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
    if (!node) {
        ImGui::Text("Invalid node.");
        return;
    }

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
    components.push_back({
        "Skeleton 3D", node->has<rtSKELE3D>(),
        [&]() { RenderSKEL3D(fs, scene, handle); },
        [&]() { scene->nAddComp<rtSKELE3D>(handle); },
        [&]() { scene->nEraseComp<rtSKELE3D>(handle); }
    });
    components.push_back({
        "Runtime Script", node->has<rtSCRIPT>(),
        [&]() { RenderSCRIPT(fs, scene, handle); },
        [&]() { scene->nAddComp<rtSCRIPT>(handle); },
        [&]() { scene->nEraseComp<rtSCRIPT>(handle); }
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
        tinyScript* script = fs.rGet<tinyScript>(dHandle);

        // Log the version, and compilation status
        if (script->valid())
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Compiled [%u.0]", script->version());
        else if (script->debug().empty())
            ImGui::TextColored(ImVec4(0.9f, 0.5f, 0.2f, 1.0f), "Not Compiled ");
        else
            ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "Compilation Error ");

        // A button to open editor (overrides the editorSelection)

        if (ImGui::Button("Open in Editor", ImVec2(-1, 0))) {
            Editor::addTab(CreateScriptEditorTab(node->name, handle));
        }
    } else if (dHandle.is<tinyTexture>()) {
        tinyTexture* texture = fs.rGet<tinyTexture>(dHandle);

        ImVec2 availSize = ImGui::GetContentRegionAvail();
        ImVec2 imgSize = ImVec2(availSize.x, availSize.x / (float)texture->height() * (float)texture->width());

        Texture::Render(texture, *imguiSampler, imgSize);
    } else if (dHandle.is<tinyMaterial>()) {
        tinyMaterial* material = fs.rGet<tinyMaterial>(dHandle);

        glm::vec4& baseColor = material->baseColor;
        ImGui::ColorEdit4("Base Color", &baseColor.x);

        auto texDragField = [&](const char* label, tinyHandle& texHandle, ImVec4 activeColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f)) {
            const tinyTexture* tex = fs.rGet<tinyTexture>(texHandle);

            tinyHandle fHandle = fs.rDataToFile(texHandle);
            const char* fullPath = fs.path(fHandle);
            fullPath = fullPath ? fullPath : "<Invalid Path>";

            std::string invalidLabel = "No " + std::string(label) + " Assigned";

            RenderDragField(
                [&]() { return fullPath; },
                invalidLabel.c_str(),
                [&]() { 
                    if (!tex) return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                    return activeColor;
                },
                ImVec4(0.2f, 0.2f, 0.2f, 1.0),
                [&]() { return tex != nullptr; },
                [&]() { // Drag Drop handling
                    if (!ImGui::BeginDragDropTarget()) return;

                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PAYLOAD")) {
                        Payload* data = (Payload*)payload->Data;
                        if (!data->is<tinyNodeFS>()) { ImGui::EndDragDropTarget(); return; }

                        tinyHandle fHandle = data->handle;
                        tinyHandle dHandle = fs.dataHandle(fHandle);
                        const tinyTexture* tex = fs.rGet<tinyTexture>(dHandle);
                        if (!tex) { ImGui::EndDragDropTarget(); return; }

                        texHandle = dHandle;

                        ImGui::EndDragDropTarget();
                    }
                },
                []() {  },
                [&]() {
                    ImGui::BeginTooltip();

                    if (tex) {
                        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), label);
                        ImGui::Separator();

                        tinyHandle fHandle = fs.rDataToFile(texHandle);
                        const char* fullPath = fs.path(fHandle);
                        fullPath = fullPath ? fullPath : "<Invalid Path>";

                        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "[FS]");
                        ImGui::SameLine(); ImGui::Text("%s", fullPath);

                        float aspectRatio = static_cast<float>(tex->width()) / static_cast<float>(tex->height());
                        ImVec2 imgSize = ImVec2(256, 256 / aspectRatio);
                        Texture::Render(tex, *imguiSampler, imgSize);
                    } else {
                        std::string labelStr = "No " + std::string(label) + " Assigned";
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), labelStr.c_str());
                    }

                    ImGui::EndTooltip();
                }
            );

            // Right click for context menu
            if (ImGui::BeginPopupContextItem(label)) {
                if (ImGui::MenuItem("Remove")) {
                    texHandle = tinyHandle();
                }
                ImGui::EndPopup();
            }
        };

        texDragField("Albedo Texture", material->albTexture);
        texDragField("Normal Texture", material->nrmlTexture, ImVec4(0.5f, 0.5f, 1.0f, 1.0f));
        texDragField("Emission Texture", material->emissTexture, ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
    }
}

static void RenderInspector(tinyProject* project) {
    RenderSceneNodeInspector(project);
    RenderFileInspector(project);
}


// ============================================================================
// MAIN UI RENDERING FUNCTION
// ============================================================================

void tinyApp::renderUI() {
    auto& fpsRef = *fpsManager;
    auto& camRef = *project->camera();

    sceneRef = project->scene(State::sceneHandle);
    if (!sceneRef) State::sceneHandle = project->mainSceneHandle;
    sceneRef = project->scene(State::sceneHandle);

    static float frameTime = 1.0f;
    static const float printInterval = 1.0f; // Print fps every second
    static float currentFps = 0.0f;

    float deltaTime = fpsRef.deltaTime;

    tinyFS& fs = project->fs();


    // ===== THEME EDITOR WINDOW =====
    static bool showThemeEditor = false;
    if (showThemeEditor) {
        if (tinyUI::Begin("Theme Editor", &showThemeEditor)) {
            ImGuiStyle& style = tinyUI::Style();
            ImGuiIO& io = ImGui::GetIO();

            if (ImGui::CollapsingHeader("Colors")) {
                ImGui::ColorEdit4("Text", &style.Colors[ImGuiCol_Text].x);
                ImGui::ColorEdit4("Text Disabled", &style.Colors[ImGuiCol_TextDisabled].x);
                ImGui::ColorEdit4("Window Background", &style.Colors[ImGuiCol_WindowBg].x);
                ImGui::ColorEdit4("Child Background", &style.Colors[ImGuiCol_ChildBg].x);
                ImGui::ColorEdit4("Border", &style.Colors[ImGuiCol_Border].x);
                ImGui::ColorEdit4("Title Background", &style.Colors[ImGuiCol_TitleBg].x);
                ImGui::ColorEdit4("Title Background Active", &style.Colors[ImGuiCol_TitleBgActive].x);
                ImGui::ColorEdit4("Title Background Collapsed", &style.Colors[ImGuiCol_TitleBgCollapsed].x);
                ImGui::ColorEdit4("Button", &style.Colors[ImGuiCol_Button].x);
                ImGui::ColorEdit4("Button Hovered", &style.Colors[ImGuiCol_ButtonHovered].x);
                ImGui::ColorEdit4("Button Active", &style.Colors[ImGuiCol_ButtonActive].x);
                ImGui::ColorEdit4("Header", &style.Colors[ImGuiCol_Header].x);
                ImGui::ColorEdit4("Header Hovered", &style.Colors[ImGuiCol_HeaderHovered].x);
                ImGui::ColorEdit4("Header Active", &style.Colors[ImGuiCol_HeaderActive].x);
                ImGui::ColorEdit4("Frame Background", &style.Colors[ImGuiCol_FrameBg].x);
                ImGui::ColorEdit4("Frame Background Hovered", &style.Colors[ImGuiCol_FrameBgHovered].x);
                ImGui::ColorEdit4("Frame Background Active", &style.Colors[ImGuiCol_FrameBgActive].x);
                ImGui::ColorEdit4("Scrollbar Background", &style.Colors[ImGuiCol_ScrollbarBg].x);
                ImGui::ColorEdit4("Scrollbar Grab", &style.Colors[ImGuiCol_ScrollbarGrab].x);
                ImGui::ColorEdit4("Scrollbar Grab Hovered", &style.Colors[ImGuiCol_ScrollbarGrabHovered].x);
                ImGui::ColorEdit4("Scrollbar Grab Active", &style.Colors[ImGuiCol_ScrollbarGrabActive].x);
            }

            if (ImGui::CollapsingHeader("Sizes & Rounding")) {
                ImGui::DragFloat("Scrollbar Size", &style.ScrollbarSize, 0.01f, 0.0f, 20.0f);
                ImGui::DragFloat("Scrollbar Rounding", &style.ScrollbarRounding, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Frame Rounding", &style.FrameRounding, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Child Rounding", &style.ChildRounding, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Button Rounding", &style.GrabRounding, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Window Rounding", &style.WindowRounding, 0.01f, 0.0f, 10.0f);
                ImGui::DragFloat("Window Border Size", &style.WindowBorderSize, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("Font Scale", &io.FontGlobalScale, 0.01f, 0.5f, 2.0f);
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
            split.init(2);
            split.directionSize = ImGui::GetContentRegionAvail().y;
            split.calcRegionSizes();

            // ===== Row 1: DEBUG WINDOW =====

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0)); // Transparent background
            ImGui::BeginChild("DebugWindow", ImVec2(0, split.rSize(0)), true);

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

            ImGui::EndChild();
            ImGui::PopStyleColor();
            
            // ===== HORIZONTAL SPLITTER =====
            split.render(0);

            // ===== Row 2: SCENE HIERARCHY =====
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0)); // Transparent background
            ImGui::BeginChild("SceneHierarchy", ImVec2(0, split.rSize(1)), true);

            ImGui::Text("[HDL %u.%u]", sceneHandle.index, sceneHandle.version);
            ImGui::Separator();

            RenderSceneNodeHierarchy();

            ImGui::EndChild();
            ImGui::PopStyleColor();
            
            // ===== HORIZONTAL SPLITTER =====
            split.render(1);
            
            // ===== Row 3: FILE SYSTEM HIERARCHY =====
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0)); // Transparent background
            ImGui::BeginChild("FileHierarchy", ImVec2(0, split.rSize(2)), true);
            
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

    if (tinyUI::Begin("Editor", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
        size_t currentTab = Editor::selectedTab;

        float tabBarHeight = 32.0f;
        
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0)); // Transparent background
        ImGui::BeginChild("Tabs", ImVec2(0, tabBarHeight), false, ImGuiWindowFlags_HorizontalScrollbar);

        if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f) {
            float scrollAmount = -ImGui::GetIO().MouseWheel * 20.0f;
            ImGui::SetScrollX(ImGui::GetScrollX() + scrollAmount);
        }

        int tabToClose = -1;
        for (size_t i = 0; i < Editor::tabs.size(); ++i) {
            Editor::Tab& tab = Editor::tabs[i];
            ImGui::PushID(static_cast<int>(i));

            ImVec4 tabColor = (i == currentTab) ? tab.color : ImVec4(0.3f, 0.3f, 0.3f, 0.2f);
            ImGui::PushStyleColor(ImGuiCol_Button, tabColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(tabColor.x * 1.2f, tabColor.y * 1.2f, tabColor.z * 1.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(tabColor.x * 0.8f, tabColor.y * 0.8f, tabColor.z * 0.8f, 1.0f));

            std::string label = tab.title + " x";

            if (ImGui::Button(label.c_str())) Editor::selectTab(i);
            
            bool isHovered = ImGui::IsItemHovered();
            ImVec2 buttonMin = ImGui::GetItemRectMin();
            ImVec2 buttonMax = ImGui::GetItemRectMax();
            ImVec2 mousePos = ImGui::GetMousePos();

            if (isHovered) {
                float closeButtonWidth = ImGui::CalcTextSize(" x").x;
                if (mousePos.x > buttonMax.x - closeButtonWidth && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    tabToClose = static_cast<int>(i);
                }

                if (mousePos.x > buttonMax.x - closeButtonWidth) {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->AddRectFilled(
                        ImVec2(buttonMax.x - closeButtonWidth, buttonMin.y),
                        buttonMax,
                        IM_COL32(0, 0, 0, 60)
                    );
                }

                if (tab.hoverFunc) tab.hoverFunc(tab);
            }

            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                ImGui::OpenPopup(("TabContext##" + std::to_string(i)).c_str());
            }
            
            if (ImGui::BeginPopup(("TabContext##" + std::to_string(i)).c_str())) {
                if (tab.contextMenuFunc) {
                    tab.contextMenuFunc(tab);
                } else {
                    // Default context menu
                    if (ImGui::MenuItem("Close")) {
                        tabToClose = static_cast<int>(i);
                    }
                    if (ImGui::MenuItem("Close Others")) {
                        for (int j = static_cast<int>(Editor::tabs.size()) - 1; j >= 0; --j) {
                            if (static_cast<size_t>(j) != i) {
                                Editor::closeTab(static_cast<size_t>(j));
                                if (static_cast<size_t>(j) < i) {
                                    tabToClose = static_cast<int>(i) - 1;
                                }
                            }
                        }
                    }
                    if (ImGui::MenuItem("Close All")) {
                        for (int j = static_cast<int>(Editor::tabs.size()) - 1; j >= 0; --j) {
                            Editor::closeTab(static_cast<size_t>(j));
                        }
                    }
                }
                ImGui::EndPopup();
            }
            
            ImGui::PopStyleColor(3);
            ImGui::PopID();

            if (i < Editor::tabs.size() - 1) ImGui::SameLine();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        if (tabToClose >= 0) {
            Editor::closeTab(static_cast<size_t>(tabToClose));
        }

        Editor::Tab* tab = Editor::currentTab();
        if (tab && tab->renderFunc) tab->renderFunc(*tab);
        else if (tab) ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Empty Tab");

        tinyUI::End();
    }

    // Press Ctrl + Tab to cycle through tabs
    if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
        size_t tabCount = Editor::tabs.size();
        if (tabCount > 0) {
            Editor::selectedTab = (Editor::selectedTab + 1) % tabCount;
            Editor::Tab& tab = Editor::tabs[Editor::selectedTab];
            if (tab.selectFunc) tab.selectFunc(tab);
        }
    }


    // Fun debug: Hold space to instantiate the currently selected scene file to the root of the current scene
    if (ImGui::IsKeyDown(ImGuiKey_Space) && false) {
        tinyHandle fHandle = State::selected;
        if (fHandle.is<tinyNodeFS>()) {
            tinyFS& fs = project->fs();
            tinyHandle dHandle = fs.dataHandle(fHandle);
            if (dHandle.is<rtScene>()) {
                rtScene* srcScene = fs.rGet<rtScene>(dHandle);
                rtScene* curScene = project->scene(State::sceneHandle);
                if (srcScene && curScene) {
                    tinyHandle nodeHdl = curScene->instantiate(dHandle);

                    // Spawn them at completely random position
                    glm::vec3 randPos = glm::vec3(
                        (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 100.0f,
                        (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 100.0f,
                        (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 100.0f
                    );

                    rtTRANFM3D* transf = curScene->nGetComp<rtTRANFM3D>(nodeHdl);
                    if (transf) transf->local = glm::translate(glm::mat4(1.0f), randPos);
                }
            }
        }
    }
}
