// tinyApp_imgui.cpp - COMPLETELY REFACTORED with new component system
#include "tinyApp/tinyApp.hpp"
#include "tinyEngine/tinyLoader.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <imgui.h>
#include <algorithm>
#include <filesystem>

using namespace tinyVk;

// ===========================================================================================
// WINDOW SETUP
// ===========================================================================================

void tinyApp::setupImGuiWindows(const tinyChrono& fpsManager, const tinyCamera& camera, bool mouseFocus, float deltaTime) {
    imguiWrapper->clearWindows();

    // Hierarchy Editor Window
    imguiWrapper->addWindow("Hierarchy Editor", [this]() {
        renderHierarchyWindow();
    });
    
    // Debug Panel Window
    imguiWrapper->addWindow("Debug Panel", [this, &fpsManager, &camera, mouseFocus, deltaTime]() {
        renderDebugPanel(fpsManager, camera, mouseFocus, deltaTime);
    }, &showDebugWindow);
    
    // Inspector Window
    imguiWrapper->addWindow("Inspector", [this]() {
        renderInspectorWindow();
    });
    
    // Editor Settings Window
    imguiWrapper->addWindow("Editor Settings", [this]() {
        renderEditorSettingsWindow();
    }, &showEditorSettingsWindow);

    // Component Editor Window (Animation/Script)
    imguiWrapper->addWindow("Editor", [this]() {
        renderComponentEditorWindow();
    });

    // Script Editor Window
    imguiWrapper->addWindow("Script Editor", [this]() {
        renderScriptEditorWindow();
    });
    
    // Theme Editor Window
    imguiWrapper->addWindow("Theme Editor", [this]() {
        renderThemeEditorWindow();
    }, &showThemeEditor);
}

// ===========================================================================================
// HIERARCHY WINDOW (Scene + File Explorer)
// ===========================================================================================

void tinyApp::renderHierarchyWindow() {
    const tinyFS& fs = project->fs();
    
    static float splitterPos = 0.5f;
    float totalHeight = ImGui::GetContentRegionAvail().y;
    float hierarchyHeight = totalHeight * splitterPos;
    float explorerHeight = totalHeight * (1.0f - splitterPos);
    
    // ==================== SCENE HIERARCHY ====================
    tinySceneRT* activeScene = getActiveScene();
    if (activeScene) {
        ImGui::Text("%s", activeScene->name.c_str());
        imguiWrapper->tooltipOnHover(std::string("Scene: " + activeScene->name + "\nTotal Nodes: " + std::to_string(activeScene->nodeCount())).c_str());
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "No Active Scene");
    }
    
    imguiWrapper->separator();
    
    // Hierarchy tree
    imguiWrapper->beginScrollArea("HierarchyTree", ImVec2(0, hierarchyHeight - 50));
    if (activeScene && activeScene->nodeCount() > 0) {
        renderSceneHierarchy(activeScene->rootHandle());
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No nodes in scene");
    }
    imguiWrapper->endScrollArea();
    
    // Clear held handle when not dragging
    if (heldHandle.valid() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        clearHeld();
    }
    
    // ==================== SPLITTER ====================
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.4f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.7f, 0.7f, 0.8f));
    
    ImGui::Button("##Splitter", ImVec2(-1, 4));
    
    if (ImGui::IsItemActive()) {
        float delta = ImGui::GetIO().MouseDelta.y / totalHeight;
        splitterPos += delta;
        splitterPos = std::clamp(splitterPos, 0.2f, 0.8f);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    
    ImGui::PopStyleColor(3);
    
    // ==================== FILE EXPLORER ====================
    ImGui::Text("Project Files");
    imguiWrapper->separator();
    
    imguiWrapper->beginScrollArea("FileExplorer", ImVec2(0, explorerHeight - 50));
    renderFileExplorer(fs.rootHandle());
    imguiWrapper->endScrollArea();
    
    // Render dialogs
    renderFileDialog();
    renderLoadScriptDialog();
}

// ===========================================================================================
// SCENE HIERARCHY (Tree Rendering)
// ===========================================================================================

void tinyApp::renderSceneHierarchy(tinyHandle nodeHandle) {
    tinySceneRT* activeScene = getActiveScene();
    if (!activeScene || !nodeHandle.valid()) return;
    
    const tinyNodeRT* node = activeScene->node(nodeHandle);
    if (!node) return;
    
    ImGui::PushID(static_cast<int>(nodeHandle.index));
    
    bool hasChildren = !node->childrenHandles.empty();
    bool isSelected = (selectedHandle.isScene() && selectedHandle.handle == nodeHandle);
    bool isHighlighted = (selectedCompNode.valid() && selectedCompNode == nodeHandle);
    
    tinyImGui::TreeNodeConfig config;
    config.isLeaf = !hasChildren;
    config.isSelected = isSelected;
    config.forceOpen = isNodeExpanded(nodeHandle);
    
    if (isHighlighted) {
        config.customBgColor = ImVec4(0.4f, 0.5f, 0.7f, 0.5f);
    }
    
    // Selection
    config.onLeftClick = [this, nodeHandle]() {
        selectSceneNode(nodeHandle);
    };
    
    config.onRightClick = [this, nodeHandle]() {
        selectSceneNode(nodeHandle);
    };
    
    // Context menu
    config.contextMenu = [this, nodeHandle]() {
        tinySceneRT* scene = getActiveScene();
        if (!scene) return;
        
        const tinyNodeRT* node = scene->node(nodeHandle);
        bool isRoot = (nodeHandle == scene->rootHandle());
        
        if (ImGui::MenuItem("Create Child Node")) {
            tinyHandle childHandle = scene->addNode("NewNode", nodeHandle);
            expandNode(nodeHandle);
        }
        
        if (!isRoot) {
            if (ImGui::MenuItem("Delete Node")) {
                scene->removeNode(nodeHandle);
                clearSelection();
            }
            
            if (ImGui::MenuItem("Duplicate Node")) {
                // TODO: Implement duplication
            }
        }
        
        imguiWrapper->separator();
        
        if (ImGui::MenuItem("Expand All")) {
            // TODO: Recursively expand all children
        }
        
        if (ImGui::MenuItem("Collapse All")) {
            collapseNode(nodeHandle);
        }
    };
    
    // Drag source
    config.dragSource = [this, nodeHandle, node]() {
        ImGui::SetDragDropPayload("SCENE_NODE", &nodeHandle, sizeof(tinyHandle));
        ImGui::Text("Moving: %s", node->name.c_str());
        holdSceneNode(nodeHandle);
        return true;
    };
    
    // Drag target
    config.dragTarget = [this, nodeHandle]() {
        // Accept scene node reparenting
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_NODE")) {
            tinyHandle draggedHandle = *(tinyHandle*)payload->Data;
            tinySceneRT* scene = getActiveScene();
            if (scene) {
                scene->reparentNode(draggedHandle, nodeHandle);
                clearHeld();
            }
            return true;
        }
        
        // Accept file drops (for instantiating scenes)
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_NODE")) {
            tinyHandle fileHandle = *(tinyHandle*)payload->Data;
            const tinyFS::Node* fNode = project->fs().fNode(fileHandle);
            if (fNode && fNode->tHandle.isType<tinySceneRT>()) {
                tinyHandle sceneRegistryHandle = fNode->tHandle.handle;
                project->addSceneInstance(sceneRegistryHandle, getActiveSceneHandle(), nodeHandle);
                expandNode(nodeHandle); // Expand to show new child
                clearHeld();
                return true;
            }
        }
        
        return false;
    };
    
    // Track expansion state
    if (hasChildren) {
        bool wasExpanded = isNodeExpanded(nodeHandle);
        
        if (imguiWrapper->treeNode(node->name.c_str(), config)) {
            if (!wasExpanded) expandNode(nodeHandle);
            
            // Render children
            for (const tinyHandle& childHandle : node->childrenHandles) {
                renderSceneHierarchy(childHandle);
            }
            
            imguiWrapper->treeNodeEnd();
        } else {
            if (wasExpanded) collapseNode(nodeHandle);
        }
    } else {
        imguiWrapper->treeNode(node->name.c_str(), config);
    }
    
    ImGui::PopID();
}

// ===========================================================================================
// FILE EXPLORER (Tree Rendering)
// ===========================================================================================

void tinyApp::renderFileExplorer(tinyHandle nodeHandle) {
    const tinyFS& fs = project->fs();
    if (!nodeHandle.valid()) return;
    
    const tinyFS::Node* node = fs.fNode(nodeHandle);
    if (!node) return;
    
    ImGui::PushID(static_cast<int>(nodeHandle.index));
    
    bool isFile = node->isFile();
    bool hasChildren = !node->children.empty();
    bool isSelected = (selectedHandle.isFile() && selectedHandle.handle == nodeHandle);
    
    if (isFile) {
        // Render file
        tinyImGui::TreeNodeConfig config;
        config.isLeaf = true;
        config.isSelected = isSelected;
        
        config.onLeftClick = [this, nodeHandle]() {
            selectFileNode(nodeHandle);
        };
        
        // Drag source for files
        config.dragSource = [this, nodeHandle, node]() {
            ImGui::SetDragDropPayload("FILE_NODE", &nodeHandle, sizeof(tinyHandle));
            ImGui::Text("%s", node->name.c_str());
            holdFileNode(nodeHandle);
            return true;
        };
        
        imguiWrapper->treeNode(node->name.c_str(), config);
        
    } else {
        // Render folder
        tinyImGui::TreeNodeConfig config;
        config.isLeaf = !hasChildren;
        config.isSelected = isSelected;
        config.forceOpen = isFNodeExpanded(nodeHandle);
        
        config.onLeftClick = [this, nodeHandle]() {
            selectFileNode(nodeHandle);
        };
        
        // Context menu for folders
        config.contextMenu = [this, nodeHandle, node]() {
            if (ImGui::MenuItem("Load Model...")) {
                fileDialog.open(std::filesystem::path(".") / node->name, nodeHandle);
            }
            
            if (ImGui::MenuItem("Load Script...")) {
                loadScriptDialog.open(std::filesystem::path(".") / node->name, nodeHandle);
            }
            
            imguiWrapper->separator();
            
            if (ImGui::MenuItem("Refresh")) {
                // TODO: Refresh folder contents
            }
        };
        
        bool wasExpanded = isFNodeExpanded(nodeHandle);
        
        if (imguiWrapper->treeNode(node->name.c_str(), config)) {
            if (!wasExpanded) expandFNode(nodeHandle);
            
            // Render children
            for (const tinyHandle& childHandle : node->children) {
                renderFileExplorer(childHandle);
            }
            
            imguiWrapper->treeNodeEnd();
        } else {
            if (wasExpanded) collapseFNode(nodeHandle);
        }
    }
    
    ImGui::PopID();
}

// ===========================================================================================
// DEBUG PANEL
// ===========================================================================================

void tinyApp::renderDebugPanel(const tinyChrono& fpsManager, const tinyCamera& camera, bool mouseFocus, float deltaTime) {
    // FPS and Performance
    static auto lastFPSUpdate = std::chrono::steady_clock::now();
    static float displayFPS = 0.0f;
    static float displayFrameTime = 0.0f;
    static float displayAvgFPS = 0.0f;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFPSUpdate);
    
    if (elapsed.count() >= 1000) {
        displayFPS = fpsManager.currentFPS;
        displayFrameTime = fpsManager.frameTimeMs;
        displayAvgFPS = fpsManager.getAverageFPS();
        lastFPSUpdate = now;
    }
    
    ImGui::Text("Performance");
    imguiWrapper->separator();
    ImGui::Text("FPS: %.1f (%.2f ms)", displayFPS, displayFrameTime);
    ImGui::Text("Avg FPS: %.1f", displayAvgFPS);
    ImGui::Text("Delta Time: %.4f s", deltaTime);
    
    imguiWrapper->separator("Camera");
    ImGui::Text("Pos: %.2f, %.2f, %.2f", camera.pos.x, camera.pos.y, camera.pos.z);
    ImGui::Text("Fwd: %.2f, %.2f, %.2f", camera.forward.x, camera.forward.y, camera.forward.z);
    ImGui::Text("Mouse Focus: %s", mouseFocus ? "Yes" : "No");

    imguiWrapper->separator("Selection");
    if (selectedHandle.valid()) {
        if (selectedHandle.isScene()) {
            tinySceneRT* scene = getActiveScene();
            if (scene) {
                const tinyNodeRT* node = scene->node(selectedHandle.handle);
                if (node) {
                    ImGui::Text("Selected Node: %s", node->name.c_str());
                }
            }
        } else {
            const tinyFS::Node* fNode = project->fs().fNode(selectedHandle.handle);
            if (fNode) {
                ImGui::Text("Selected File: %s", fNode->name.c_str());
            }
        }
    } else {
        ImGui::TextDisabled("Nothing selected");
    }
    
    imguiWrapper->separator("Theme");
    if (imguiWrapper->button("Edit Theme", tinyImGui::ButtonStyle::Primary)) {
        showThemeEditor = !showThemeEditor;
    }
}

// ===========================================================================================
// EDITOR SETTINGS WINDOW  
// ===========================================================================================

void tinyApp::renderEditorSettingsWindow() {
    ImGui::Text("Editor Settings");
    imguiWrapper->separator();
    
    auto& theme = imguiWrapper->getTheme();
    
    if (ImGui::CollapsingHeader("Theme Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit4("Window Background", (float*)&theme.windowBg);
        ImGui::ColorEdit4("Child Background", (float*)&theme.childBg);
        ImGui::ColorEdit4("Border", (float*)&theme.border);
        
        imguiWrapper->separator();
        
        ImGui::ColorEdit4("Button Default", (float*)&theme.button);
        ImGui::ColorEdit4("Button Primary", (float*)&theme.buttonPrimary);
        ImGui::ColorEdit4("Button Success", (float*)&theme.buttonSuccess);
        ImGui::ColorEdit4("Button Danger", (float*)&theme.buttonDanger);
        ImGui::ColorEdit4("Button Warning", (float*)&theme.buttonWarning);
        
        if (imguiWrapper->button("Apply Theme", tinyImGui::ButtonStyle::Success)) {
            imguiWrapper->applyTheme();
        }
        
        ImGui::SameLine();
        
        if (imguiWrapper->button("Reset to Default", tinyImGui::ButtonStyle::Warning)) {
            theme = tinyImGui::Theme(); // Reset to defaults
            imguiWrapper->applyTheme();
        }
    }
    
    if (ImGui::CollapsingHeader("UI Layout")) {
        ImGui::DragFloat("Scrollbar Size", &theme.scrollbarSize, 0.1f, 4.0f, 20.0f);
        ImGui::DragFloat("Frame Rounding", &theme.frameRounding, 0.1f, 0.0f, 12.0f);
        ImGui::DragFloat("Window Rounding", &theme.windowRounding, 0.1f, 0.0f, 12.0f);
        
        if (imguiWrapper->button("Apply", tinyImGui::ButtonStyle::Primary)) {
            imguiWrapper->applyTheme();
        }
    }
    
    if (ImGui::CollapsingHeader("Font Settings")) {
        static float fontScale = 1.0f;
        ImGui::Text("Font Scale");
        if (ImGui::SliderFloat("##FontScale", &fontScale, 0.5f, 3.0f, "%.1fx")) {
            ImGui::GetIO().FontGlobalScale = fontScale;
        }
        
        ImGui::Text("Presets:");
        if (imguiWrapper->button("Small", tinyImGui::ButtonStyle::Default, ImVec2(60, 0))) {
            fontScale = 0.8f;
            ImGui::GetIO().FontGlobalScale = fontScale;
        }
        ImGui::SameLine();
        if (imguiWrapper->button("Normal", tinyImGui::ButtonStyle::Default, ImVec2(60, 0))) {
            fontScale = 1.0f;
            ImGui::GetIO().FontGlobalScale = fontScale;
        }
        ImGui::SameLine();
        if (imguiWrapper->button("Large", tinyImGui::ButtonStyle::Default, ImVec2(60, 0))) {
            fontScale = 1.5f;
            ImGui::GetIO().FontGlobalScale = fontScale;
        }
    }
}

// ===========================================================================================
// COMPONENT EDITOR WINDOW (Animation/Script)
// ===========================================================================================

void tinyApp::renderComponentEditorWindow() {
    // This window shows either animation timeline or script debug
    // For now, delegate to existing methods
    renderAnimationEditorWindow();
}

// ===========================================================================================
// THEME EDITOR WINDOW
// ===========================================================================================

void tinyApp::renderThemeEditorWindow() {
    ImGui::Text("Theme Editor");
    imguiWrapper->separator();
    
    auto& theme = imguiWrapper->getTheme();
    
    imguiWrapper->beginScrollArea("ThemeEditor");
    
    // Window Colors
    if (ImGui::CollapsingHeader("Window & Background", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit4("Window Background", (float*)&theme.windowBg);
        ImGui::ColorEdit4("Child Background", (float*)&theme.childBg);
        ImGui::ColorEdit4("Border", (float*)&theme.border);
        ImGui::ColorEdit4("Component Background", (float*)&theme.componentBg);
        ImGui::ColorEdit4("Component Border", (float*)&theme.componentBorder);
    }
    
    // Title Bar
    if (ImGui::CollapsingHeader("Title Bar")) {
        ImGui::ColorEdit4("Title Background", (float*)&theme.titleBg);
        ImGui::ColorEdit4("Title Active", (float*)&theme.titleBgActive);
        ImGui::ColorEdit4("Title Collapsed", (float*)&theme.titleBgCollapsed);
    }
    
    // Text Colors
    if (ImGui::CollapsingHeader("Text")) {
        ImGui::ColorEdit4("Text", (float*)&theme.text);
        ImGui::ColorEdit4("Text Disabled", (float*)&theme.textDisabled);
    }
    
    // Button Styles
    if (ImGui::CollapsingHeader("Buttons - Default")) {
        ImGui::ColorEdit4("Button", (float*)&theme.button);
        ImGui::ColorEdit4("Button Hovered", (float*)&theme.buttonHovered);
        ImGui::ColorEdit4("Button Active", (float*)&theme.buttonActive);
    }
    
    if (ImGui::CollapsingHeader("Buttons - Primary")) {
        ImGui::ColorEdit4("Primary", (float*)&theme.buttonPrimary);
        ImGui::ColorEdit4("Primary Hovered", (float*)&theme.buttonPrimaryHovered);
        ImGui::ColorEdit4("Primary Active", (float*)&theme.buttonPrimaryActive);
    }
    
    if (ImGui::CollapsingHeader("Buttons - Success")) {
        ImGui::ColorEdit4("Success", (float*)&theme.buttonSuccess);
        ImGui::ColorEdit4("Success Hovered", (float*)&theme.buttonSuccessHovered);
        ImGui::ColorEdit4("Success Active", (float*)&theme.buttonSuccessActive);
    }
    
    if (ImGui::CollapsingHeader("Buttons - Danger")) {
        ImGui::ColorEdit4("Danger", (float*)&theme.buttonDanger);
        ImGui::ColorEdit4("Danger Hovered", (float*)&theme.buttonDangerHovered);
        ImGui::ColorEdit4("Danger Active", (float*)&theme.buttonDangerActive);
    }
    
    if (ImGui::CollapsingHeader("Buttons - Warning")) {
        ImGui::ColorEdit4("Warning", (float*)&theme.buttonWarning);
        ImGui::ColorEdit4("Warning Hovered", (float*)&theme.buttonWarningHovered);
        ImGui::ColorEdit4("Warning Active", (float*)&theme.buttonWarningActive);
    }
    
    // Headers & Highlights
    if (ImGui::CollapsingHeader("Headers & Selection")) {
        ImGui::ColorEdit4("Header", (float*)&theme.header);
        ImGui::ColorEdit4("Header Hovered", (float*)&theme.headerHovered);
        ImGui::ColorEdit4("Header Active", (float*)&theme.headerActive);
        ImGui::ColorEdit4("Selected", (float*)&theme.selected);
        ImGui::ColorEdit4("Drag Drop Highlight", (float*)&theme.dragDrop);
    }
    
    // Scrollbar
    if (ImGui::CollapsingHeader("Scrollbar")) {
        ImGui::ColorEdit4("Scrollbar Background", (float*)&theme.scrollbarBg);
        ImGui::ColorEdit4("Scrollbar Grab", (float*)&theme.scrollbarGrab);
        ImGui::ColorEdit4("Scrollbar Grab Hovered", (float*)&theme.scrollbarGrabHovered);
        ImGui::ColorEdit4("Scrollbar Grab Active", (float*)&theme.scrollbarGrabActive);
        ImGui::DragFloat("Scrollbar Size", &theme.scrollbarSize, 0.1f, 4.0f, 20.0f);
        ImGui::DragFloat("Scrollbar Rounding", &theme.scrollbarRounding, 0.1f, 0.0f, 12.0f);
    }
    
    // Frame/Input
    if (ImGui::CollapsingHeader("Input Fields")) {
        ImGui::ColorEdit4("Frame Background", (float*)&theme.frameBg);
        ImGui::ColorEdit4("Frame Hovered", (float*)&theme.frameBgHovered);
        ImGui::ColorEdit4("Frame Active", (float*)&theme.frameBgActive);
        ImGui::DragFloat("Frame Rounding", &theme.frameRounding, 0.1f, 0.0f, 12.0f);
    }
    
    // Rounding
    if (ImGui::CollapsingHeader("Rounding & Shape")) {
        ImGui::DragFloat("Window Rounding", &theme.windowRounding, 0.1f, 0.0f, 12.0f);
        ImGui::DragFloat("Child Rounding", &theme.childRounding, 0.1f, 0.0f, 12.0f);
        ImGui::DragFloat("Button Rounding", &theme.buttonRounding, 0.1f, 0.0f, 12.0f);
    }
    
    imguiWrapper->endScrollArea();
    
    imguiWrapper->separator();
    
    // Action buttons
    if (imguiWrapper->button("Apply Theme", tinyImGui::ButtonStyle::Success)) {
        imguiWrapper->applyTheme();
    }
    
    ImGui::SameLine();
    
    if (imguiWrapper->button("Reset to Default", tinyImGui::ButtonStyle::Warning)) {
        theme = tinyImGui::Theme();
        imguiWrapper->applyTheme();
    }
    
    ImGui::SameLine();
    
    if (imguiWrapper->button("Close", tinyImGui::ButtonStyle::Default)) {
        showThemeEditor = false;
    }
}

// ===========================================================================================
// LEGACY METHODS - TO BE REFACTORED
// ===========================================================================================

// ===========================================================================================
// INSPECTOR WINDOW (Component Editor)
// ===========================================================================================

void tinyApp::renderInspectorWindow() {
    // Unified Inspector Window: Show details for whatever is currently selected
    ImGui::Text("Inspector");
    ImGui::Separator();
    
    // Check what type of selection we have
    if (!selectedHandle.valid()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No selection");
        return;
    }
    
    // Apply thin scrollbar styling for the main content area
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.1f, 0.1f, 0.1f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.4f, 0.4f, 0.4f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    
    ImGui::BeginChild("UnifiedInspectorContent", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    
    if (selectedHandle.isScene()) {
        // Render scene node inspector
        renderSceneNodeInspector();
    } else if (selectedHandle.isFile()) {
        // Render file system inspector
        renderFileSystemInspector();
    }
    
    ImGui::EndChild();
    
    // Pop the scrollbar styling
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

void tinyApp::renderSceneNodeInspector() {
    const tinyFS& fs = project->fs();

    tinySceneRT* activeScene = getActiveScene();
    if (!activeScene) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No active scene");
        return;
    }

    // Get the selected scene node handle from unified selection
    tinyHandle selectedSceneNodeHandle = getSelectedSceneNode();
    if (!selectedSceneNodeHandle.valid()) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "No scene node selected");
        ImGui::Text("This should not happen in unified selection.");
        return;
    }

    const tinyNodeRT* selectedNode = activeScene->node(selectedSceneNodeHandle);
    if (!selectedNode) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid node selection");
        selectSceneNode(activeSceneRootHandle());
        return;
    }

    bool isRootNode = (selectedSceneNodeHandle == activeSceneRootHandle());

    // GENERAL INFO SECTION (non-scrollable)
    // Editable node name field
    ImGui::Text("Name:");
    ImGui::SameLine();
    
    // Create a unique ID for the input field
    std::string inputId = "##NodeName_" + std::to_string(selectedSceneNodeHandle.index);
    
    // Static buffer to hold the name during editing
    static char nameBuffer[256];
    static tinyHandle lastSelectedNode;
    
    // Initialize buffer if we switched to a different node
    if (lastSelectedNode != selectedSceneNodeHandle) {
        strncpy_s(nameBuffer, selectedNode->name.c_str(), sizeof(nameBuffer) - 1);
        nameBuffer[sizeof(nameBuffer) - 1] = '\0';
        lastSelectedNode = selectedSceneNodeHandle;
    }
    
    // Editable text input
    ImGui::SetNextItemWidth(-1); // Use full available width
    bool enterPressed = ImGui::InputText(inputId.c_str(), nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
    
    // Apply rename on Enter or when focus is lost
    if (enterPressed || (ImGui::IsItemDeactivatedAfterEdit())) {
        activeScene->renameNode(selectedSceneNodeHandle, std::string(nameBuffer));
    }
    
    // Show parent and children count
    if (selectedNode->parentHandle.valid()) {
        const tinyNodeRT* parentNode = activeScene->node(selectedNode->parentHandle);
        if (parentNode) {
            ImGui::Text("Parent: %s", parentNode->name.c_str());
        }
    } else {
        ImGui::Text("Parent: None (Root)");
    }
    ImGui::Text("Children: %zu", selectedNode->childrenHandles.size());
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // SCROLLABLE COMPONENTS SECTION 
    float remainingHeight = ImGui::GetContentRegionAvail().y - 35; // Reserve space for add component dropdown
    
    // Customize scrollbar appearance for this specific window
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f); // Thinner scrollbar (default is usually 14-16)
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 4.0f); // Rounded scrollbar
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.1f, 0.1f, 0.1f, 0.5f)); // Darker background
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.4f, 0.4f, 0.4f, 0.8f)); // Visible grab
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f)); // Hover state
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.6f, 0.6f, 0.6f, 1.0f)); // Active state
    
    ImGui::BeginChild("ComponentsScrollable", ImVec2(0, remainingHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    
    // Helper function to render a component with consistent styling
    auto renderComponent = [&](const char* componentName, ImVec4 backgroundColor, ImVec4 borderColor, bool showRemoveButton, std::function<void()> renderContent, std::function<void()> onAction = nullptr) {
        // Component container with styled background
        ImGui::PushStyleColor(ImGuiCol_ChildBg, backgroundColor);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
        
        // Create unique ID for this component
        std::string childId = std::string(componentName) + "Component";
        
        // CRITICAL: Always call EndChild() regardless of BeginChild() return value!
        bool childVisible = ImGui::BeginChild(childId.c_str(), ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
        
        bool actionTriggered = false;
        if (childVisible) {
            // Header with optional action button (Add or Remove)
            // Gray out component name if it's inactive (showRemoveButton = false means it's a placeholder)
            if (!showRemoveButton) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.8f)); // Grayed out text
            }
            ImGui::Text("%s", componentName);
            if (!showRemoveButton) {
                ImGui::PopStyleColor(); // Pop the grayed text color
            }
            
            if (onAction) {
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70);
                
                if (showRemoveButton) {
                    // Remove button (red)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                    
                    std::string buttonId = "Remove##" + std::string(componentName);
                    if (ImGui::Button(buttonId.c_str(), ImVec2(65, 0))) {
                        actionTriggered = true;
                    }
                } else {
                    // Add button (green)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
                    
                    std::string buttonId = "Add##" + std::string(componentName);
                    if (ImGui::Button(buttonId.c_str(), ImVec2(65, 0))) {
                        actionTriggered = true;
                    }
                }
                ImGui::PopStyleColor(3);
            }
            
            // Only show separator and content for active components
            if (!actionTriggered && showRemoveButton) {
                ImGui::Separator();
                
                // Render the component-specific content
                if (renderContent) {
                    renderContent();
                }
            }
        }
        
        // CRITICAL: Always call EndChild()
        ImGui::EndChild();
        
        // Call onAction after EndChild to avoid ImGui state corruption
        if (actionTriggered && onAction) {
            onAction();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
            return; // Exit early since action was triggered
        }
        
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        
        // Add spacing between components
        ImGui::Spacing();
        ImGui::Spacing();
    };
    
    // ========== TRANSFORM COMPONENT ==========
    if (selectedNode->has<tinyNodeRT::TRFM3D>()) {
        if (imguiWrapper->componentHeader("Transform")) {
            imguiWrapper->componentBody([&]() {
                tinyNodeRT::TRFM3D* compPtr = activeScene->rtComp<tinyNodeRT::TRFM3D>(selectedSceneNodeHandle);
                glm::mat4 local = compPtr->local;

                // Extract transform data
                glm::vec3 translation, rotation, scale;
                glm::quat rotationQuat;
                glm::vec3 skew;
                glm::vec4 perspective;
                
                // Reset button
                if (imguiWrapper->button("Reset Transform", tinyImGui::ButtonStyle::Warning)) {
                    compPtr->reset();
                }

                // Validate decomposition
                bool validDecomposition = glm::decompose(local, scale, rotationQuat, translation, skew, perspective);
                if (!validDecomposition) {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid transform matrix!");
                    return;
                }
                
                // Validate values
                auto validVec3 = [](const glm::vec3& v) {
                    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
                };
                
                if (!validVec3(translation) || !validVec3(scale)) {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "NaN/Infinite values detected!");
                    if (imguiWrapper->button("Reset", tinyImGui::ButtonStyle::Danger)) {
                        compPtr->reset();
                    }
                    return;
                }
                
                // Clamp scale
                const float MIN_SCALE = 0.001f;
                scale.x = std::max(MIN_SCALE, std::abs(scale.x));
                scale.y = std::max(MIN_SCALE, std::abs(scale.y));
                scale.z = std::max(MIN_SCALE, std::abs(scale.z));
                
                // Convert to Euler angles
                rotation = glm::degrees(glm::eulerAngles(rotationQuat));
                auto normalizeAngle = [](float angle) {
                    while (angle > 180.0f) angle -= 360.0f;
                    while (angle < -180.0f) angle += 360.0f;
                    return angle;
                };
                rotation.x = normalizeAngle(rotation.x);
                rotation.y = normalizeAngle(rotation.y);
                rotation.z = normalizeAngle(rotation.z);
                
                // Store original values
                glm::vec3 originalTranslation = translation;
                glm::vec3 originalRotation = rotation;
                glm::vec3 originalScale = scale;
                
                imguiWrapper->separator();
                
                // Position
                ImGui::Text("Position");
                ImGui::DragFloat3("##Position", &translation.x, 0.01f, -1000.0f, 1000.0f, "%.3f");
                ImGui::SameLine();
                if (imguiWrapper->button("To Cam", tinyImGui::ButtonStyle::Primary)) {
                    translation = project->camera()->pos;
                }
                
                // Rotation
                ImGui::Text("Rotation (degrees)");
                ImGui::DragFloat3("##Rotation", &rotation.x, 0.5f, -180.0f, 180.0f, "%.1f°");
                
                // Scale
                ImGui::Text("Scale");
                ImGui::DragFloat3("##Scale", &scale.x, 0.01f, MIN_SCALE, 10.0f, "%.3f");
                ImGui::SameLine();
                if (imguiWrapper->button("Uniform")) {
                    float avgScale = (scale.x + scale.y + scale.z) / 3.0f;
                    scale = glm::vec3(avgScale);
                }
                
                // Apply changes
                if (translation != originalTranslation || rotation != originalRotation || scale != originalScale) {
                    if (validVec3(translation) && validVec3(scale)) {
                        glm::quat newRotQuat = glm::quat(glm::radians(rotation));
                        glm::mat4 translateMat = glm::translate(glm::mat4(1.0f), translation);
                        glm::mat4 rotateMat = glm::mat4_cast(newRotQuat);
                        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
                        compPtr->set(translateMat * rotateMat * scaleMat);
                    }
                }
            });
        }
    }
    
    // Mesh Renderer Component - Always show
    if (selectedNode->has<tinyNodeRT::MESHRD>()) {
        renderComponent("Mesh Renderer", ImVec4(0.15f, 0.15f, 0.2f, 0.8f), ImVec4(0.3f, 0.3f, 0.4f, 0.6f), true, [&]() {
            // Get runtime component using tinySceneRT auto type resolver
            tinyRT_MESHRD* rtMeshRender = activeScene->rtComp<tinyNodeRT::MESHRD>(selectedSceneNodeHandle);
            if (!rtMeshRender) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Runtime component not initialized");
                return;
            }
            
            ImGui::Spacing();
            
            // ===== MESH RESOURCE FIELD =====
            ImGui::Text("Mesh Resource:");
            
            tinyHandle meshHandle = rtMeshRender->meshHandle();
            const tinyMeshVk* meshVk = rtMeshRender->rMesh();
            
            // Create enhanced drag-drop target area for mesh
            std::string meshDisplayText;
            ImVec4 meshButtonColor, meshHoveredColor, meshActiveColor;
            
            if (meshHandle.valid() && meshVk) {
                const tinyMesh& cpuMesh = meshVk->cpu();
                meshButtonColor = ImVec4(0.2f, 0.4f, 0.2f, 1.0f);   // Green for valid
                meshHoveredColor = ImVec4(0.3f, 0.5f, 0.3f, 1.0f);
                meshActiveColor = ImVec4(0.1f, 0.3f, 0.1f, 1.0f);
            } else {
                meshDisplayText = "None";
                meshButtonColor = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);   // Gray for empty
                meshHoveredColor = ImVec4(0.4f, 0.4f, 0.6f, 1.0f);
                meshActiveColor = ImVec4(0.2f, 0.2f, 0.4f, 1.0f);
            }
            
            ImGui::PushStyleColor(ImGuiCol_Button, meshButtonColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, meshHoveredColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, meshActiveColor);
            
            if (ImGui::Button((meshDisplayText + "##MeshHandle").c_str(), ImVec2(-1, 30))) {
                // Clear mesh on click
                if (meshHandle.valid()) {
                    rtMeshRender->setMesh(tinyHandle());
                }
            }
            
            ImGui::PopStyleColor(3);
            
            // Tooltip
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                if (meshHandle.valid()) {
                    ImGui::Text("Click to clear mesh");
                    ImGui::Text("Current: %s", meshDisplayText.c_str());
                    if (meshVk) {
                        const tinyMesh& cpuMesh = meshVk->cpu();
                        ImGui::Text("Vertices: %zu", cpuMesh.vrtxCount());
                        ImGui::Text("Morph Targets: %u", meshVk->mrphCount());
                    }
                } else {
                    ImGui::Text("Drag a mesh file from the File Explorer");
                }
                ImGui::EndTooltip();
            }
            
            // Drag-drop handling for mesh files
            if (ImGui::BeginDragDropTarget()) {
                ImGui::PushStyleColor(ImGuiCol_DragDropTarget, ImVec4(0.3f, 0.6f, 1.0f, 0.7f));
                
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_FILESYSTEM_MESH")) {
                    tinyHandle droppedHandle = *(tinyHandle*)payload->Data;
                    // CRITICAL: Call setMesh function for proper initialization
                    rtMeshRender->setMesh(droppedHandle);
                }
                
                ImGui::PopStyleColor();
                ImGui::EndDragDropTarget();
            }
            
            ImGui::Spacing();
            
            // ===== SKELETON NODE HANDLE FIELD =====
            ImGui::Text("Skeleton Node:");

            tinyHandle prevSkeleHandle = rtMeshRender->skeleNodeHandle();
            bool skeleModified = renderHandleField("##MeshRenderer_SkeletonNodeHandle", 
                prevSkeleHandle, "SkeletonNode",
                "Drag a skeleton node from the Hierarchy",
                "Select skeleton node for bone animation");
            
            if (skeleModified) {
                // Update skeleton node in runtime component
                rtMeshRender->setSkeleNode(prevSkeleHandle);
            }

            // Show skeleton node information if valid
            if (rtMeshRender->skeleNodeHandle().valid()) {
                const tinyNodeRT* skeleNode = activeScene->node(rtMeshRender->skeleNodeHandle());
                if (skeleNode && skeleNode->has<tinyNodeRT::SKEL3D>()) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", skeleNode->name.c_str());
                } else {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Invalid/No Skeleton");
                }
            }
            
            ImGui::Spacing();
            
            // ===== MORPH TARGETS SECTION =====
            if (rtMeshRender->hasMrph()) {
                ImGui::Separator();
                ImGui::Text("Morph Targets (%u)", rtMeshRender->mrphCount());
                
                ImGui::Spacing();
                
                // Scrollable area for morph targets
                ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.0f);
                ImGui::BeginChild("MorphTargets", ImVec2(0, 150), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
                
                for (uint32_t i = 0; i < rtMeshRender->mrphCount(); ++i) {
                    ImGui::PushID(i);
                    
                    // Get current weight
                    float weight = rtMeshRender->mrphWeight(i);
                    
                    // Get morph target name
                    const std::string& mrphName = rtMeshRender->mrphName(i);
                    std::string label = mrphName.empty() ? ("Target " + std::to_string(i)) : mrphName;
                    
                    ImGui::Text("%s", label.c_str());
                    ImGui::SameLine(150); // Align sliders
                    
                    ImGui::SetNextItemWidth(-1);
                    // SLOW drag speed for precise morph target control (0.005 instead of default 0.01)
                    if (ImGui::DragFloat(("##MorphWeight" + std::to_string(i)).c_str(), 
                                        &weight, 0.005f, 0.0f, 1.0f, "%.3f")) {
                        rtMeshRender->setMrphWeight(i, weight);
                    }
                    
                    ImGui::SameLine();
                    if (ImGui::Button(("Reset##" + std::to_string(i)).c_str())) {
                        rtMeshRender->setMrphWeight(i, 0.0f);
                    }
                    
                    ImGui::PopID();
                    ImGui::Spacing();
                }
                
                ImGui::EndChild();
                ImGui::PopStyleVar();
                
                ImGui::Spacing();
                
                // Global controls
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.5f, 0.1f, 1.0f));
                
                if (ImGui::Button("Reset All Morph Targets", ImVec2(-1, 0))) {
                    for (uint32_t i = 0; i < rtMeshRender->mrphCount(); ++i) {
                        rtMeshRender->setMrphWeight(i, 0.0f);
                    }
                }
                
                ImGui::PopStyleColor(3);
            } else if (meshHandle.valid()) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "This mesh has no morph targets");
            }
            
            ImGui::Spacing();
        },
        [&]() { activeScene->removeComp<tinyNodeRT::MESHRD>(selectedSceneNodeHandle); });
    } else {
        renderComponent("Mesh Renderer", ImVec4(0.05f, 0.05f, 0.05f, 0.3f), ImVec4(0.15f, 0.15f, 0.15f, 0.3f), false, [&]() {},
        [&]() { activeScene->writeComp<tinyNodeRT::MESHRD>(selectedSceneNodeHandle); });
    }
    
    // Bone Attachment Component - Always show
    if (selectedNode->has<tinyNodeRT::BONE3D>()) {
        renderComponent("Bone Attachment", ImVec4(0.15f, 0.2f, 0.15f, 0.8f), ImVec4(0.3f, 0.4f, 0.3f, 0.6f), true, [&]() {
            // Get component copy using tinySceneRT method
            tinyNodeRT::BONE3D* compPtr = activeScene->rtComp<tinyNodeRT::BONE3D>(selectedSceneNodeHandle);
            bool componentModified = false;
            
            ImGui::Spacing();
            
            // Skeleton Node Handle field with enhanced drag-drop support
            ImGui::Text("Skeleton Node:");
            bool skeleModified = renderHandleField("##BoneAttach_SkeletonNodeHandle", compPtr->skeleNodeHandle, "SkeletonNode",
                "Drag a skeleton node from the Hierarchy",
                "Select skeleton node to attach to");
            if (skeleModified) componentModified = true;
            
            // Show skeleton node information if valid
            if (compPtr->skeleNodeHandle.valid()) {
                const tinyNodeRT* skeleNode = activeScene->node(compPtr->skeleNodeHandle);
                if (skeleNode && skeleNode->has<tinyNodeRT::SKEL3D>()) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", skeleNode->name.c_str());
                } else {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Invalid/No Skeleton");
                }
            }
            
            ImGui::Spacing();
            
            // Bone Index field with validation
            ImGui::Text("Bone Index:");
            int boneIndex = static_cast<int>(compPtr->boneIndex);
            
            // Determine max bone count for validation
            int maxBoneIndex = 255; // Default max
            if (compPtr->skeleNodeHandle.valid()) {
                const tinyNodeRT* skeleNode = activeScene->node(compPtr->skeleNodeHandle);
                if (skeleNode && skeleNode->has<tinyNodeRT::SKEL3D>()) {
                    const tinyNodeRT::SKEL3D* skeleComp = skeleNode->get<tinyNodeRT::SKEL3D>();
                    if (skeleComp) {
                        // Retrieve runtime skeleton data
                        const tinyRT_SKEL3D* rtSkeleton = activeScene->rtGet<tinyRT_SKEL3D>(skeleComp->pHandle);
                        if (rtSkeleton) {
                            maxBoneIndex = static_cast<int>(rtSkeleton->boneCount() - 1); // Zero-based index
                        }
                    }
                }
            }
            
            if (ImGui::DragInt("##BoneIndex", &boneIndex, 1.0f, 0, maxBoneIndex)) {
                compPtr->boneIndex = static_cast<size_t>(std::max(0, boneIndex));
                componentModified = true;
            }
            
            ImGui::SameLine();
            if (boneIndex <= maxBoneIndex && compPtr->skeleNodeHandle.valid()) {
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "✓");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "✗");
            }
            
            ImGui::Spacing();
            
            // Component status
            ImGui::Separator();
            ImGui::Text("Status:");
            ImGui::SameLine();

            bool hasValidSkeleton = compPtr->skeleNodeHandle.valid();
            bool hasValidBoneIndex = boneIndex <= maxBoneIndex;

            if (hasValidSkeleton && hasValidBoneIndex) {
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Ready for bone attachment");
            } else if (hasValidSkeleton) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "Invalid bone index");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Missing skeleton node");
            }

            ImGui::Spacing();
        },
        [&]() { activeScene->removeComp<tinyNodeRT::BONE3D>(selectedSceneNodeHandle); });
    } else {
        renderComponent("Bone Attachment", ImVec4(0.05f, 0.05f, 0.05f, 0.3f), ImVec4(0.15f, 0.15f, 0.15f, 0.3f), false, [&]() {},
        [&]() { activeScene->writeComp<tinyNodeRT::BONE3D>(selectedSceneNodeHandle); });
    }
    
    // Skeleton Component - Always show
    if (selectedNode->has<tinyNodeRT::SKEL3D>()) {
        renderComponent("Skeleton", ImVec4(0.2f, 0.15f, 0.15f, 0.8f), ImVec4(0.4f, 0.3f, 0.3f, 0.6f), true, [&]() {
            // Retrieve the component (using tinySceneRT special method which return appropriate runtime/static data)
            tinyRT_SKEL3D* rtSkeleComp = activeScene->rtComp<tinyNodeRT::SKEL3D>(selectedSceneNodeHandle);
            bool hasSkeleton = rtSkeleComp->hasSkeleton();

            ImGui::Spacing();
            // Skeleton Handle field - always edit the static skeleton handle
            ImGui::Text("Skeleton Resource:");
            bool skeleModified = renderHandleField("##SkeletonHandle", rtSkeleComp->skeleHandle(), "Skeleton",
                "Drag a skeleton file from the File Explorer",
                "Select skeleton resource for bone data");

            // Show skeleton information if valid
            const tinySkeleton* skeleton = rtSkeleComp->rSkeleton();
            if (skeleton) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "(%zu bones)", skeleton->bones.size());
            } else {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Invalid skeleton resource");
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Status:");
            ImGui::SameLine();
            if (hasSkeleton) {
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Skeleton loaded and ready for animation");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No skeleton resource assigned");
            }

            // ===== BONE HIERARCHY EDITOR =====
            if (hasSkeleton) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("Bone Animation Editor");
                
                // Global controls
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.5f, 0.1f, 1.0f));
                
                if (ImGui::Button("Refresh All to Bind Pose", ImVec2(-1, 0))) {
                    rtSkeleComp->refreshAll();
                }
                ImGui::PopStyleColor(3);
                
                ImGui::Spacing();
                
                // Static variables for bone selection
                static int selectedBoneIndex = -1;
                static tinyHandle lastSkeletonHandle;
                
                // Reset selection if skeleton changed (track by static skeleton handle)
                if (lastSkeletonHandle != rtSkeleComp->skeleHandle()) {
                    selectedBoneIndex = -1;
                    lastSkeletonHandle = rtSkeleComp->skeleHandle();
                }
                
                // Bone hierarchy tree (similar to scene explorer)
                ImGui::Text("Bone Hierarchy:");
                ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.0f);
                ImGui::BeginChild("BoneHierarchy", ImVec2(0, 150), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
                
                // Helper function to render bone tree recursively
                std::function<void(int, int)> renderBoneTree = [&](int boneIndex, int depth) {
                    if (boneIndex < 0 || boneIndex >= static_cast<int>(skeleton->bones.size())) return;
                    
                    const tinyBone& bone = skeleton->bones[boneIndex];
                    
                    ImGui::PushID(boneIndex);
                    
                    // Check if this bone has children
                    bool hasChildren = !bone.children.empty();
                    bool isSelected = (selectedBoneIndex == boneIndex);
                    
                    // Create tree node flags
                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
                    if (!hasChildren) {
                        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    }
                    if (isSelected) {
                        flags |= ImGuiTreeNodeFlags_Selected;
                    }
                    
                    // Styling for bone items
                    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.4f, 0.3f, 0.3f, 0.6f));
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.5f, 0.3f, 0.3f, 0.8f));
                    
                    // Create label with bone index
                    std::string boneLabel = std::to_string(boneIndex) + ": " + bone.name;
                    bool nodeOpen = ImGui::TreeNodeEx(boneLabel.c_str(), flags);
                    
                    // Handle selection
                    if (ImGui::IsItemClicked()) {
                        selectedBoneIndex = boneIndex;
                    }
                    
                    if (nodeOpen && hasChildren) {
                        for (int childIndex : bone.children) {
                            renderBoneTree(childIndex, depth + 1);
                        }
                        ImGui::TreePop();
                    }
                    
                    ImGui::PopStyleColor(2);
                    ImGui::PopID();
                };
                
                // Start with root bones (bones with parent = -1)
                for (int i = 0; i < static_cast<int>(skeleton->bones.size()); ++i) {
                    if (skeleton->bones[i].parent == -1) {
                        renderBoneTree(i, 0);
                    }
                }
                
                ImGui::EndChild();
                ImGui::PopStyleVar();
                
                // ===== BONE TRANSFORM EDITOR =====
                if (selectedBoneIndex >= 0 && selectedBoneIndex < static_cast<int>(skeleton->bones.size())) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    
                    const tinyBone& selectedBone = skeleton->bones[selectedBoneIndex];
                    ImGui::Text("Transform Editor - Bone %d: %s", selectedBoneIndex, selectedBone.name.c_str());
                    
                    // Refresh button for selected bone
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                    
                    if (ImGui::Button(("Refresh Bone " + std::to_string(selectedBoneIndex) + " to Bind Pose").c_str(), ImVec2(-1, 0))) {
                        rtSkeleComp->refresh(selectedBoneIndex, true);
                    }
                    ImGui::PopStyleColor(3);
                    
                    ImGui::Spacing();
                    
                    // Get current local pose matrix
                    glm::mat4 localPose = rtSkeleComp->localPose(selectedBoneIndex);

                    // Decompose matrix into translation, rotation, scale
                    glm::vec3 translation, rotation, scale;
                    glm::quat rotationQuat;
                    glm::vec3 skew;
                    glm::vec4 perspective;
                    
                    bool validDecomposition = glm::decompose(localPose, scale, rotationQuat, translation, skew, perspective);
                    
                    if (validDecomposition) {
                        // Convert quaternion to Euler angles (in degrees)
                        rotation = glm::degrees(glm::eulerAngles(rotationQuat));
                        
                        // Normalize angles to [-180, 180] range
                        auto normalizeAngle = [](float angle) {
                            while (angle > 180.0f) angle -= 360.0f;
                            while (angle < -180.0f) angle += 360.0f;
                            return angle;
                        };
                        
                        rotation.x = normalizeAngle(rotation.x);
                        rotation.y = normalizeAngle(rotation.y);
                        rotation.z = normalizeAngle(rotation.z);
                        
                        // Store original values to detect changes
                        glm::vec3 originalTranslation = translation;
                        glm::vec3 originalRotation = rotation;
                        glm::vec3 originalScale = scale;
                        
                        // Transform controls
                        ImGui::Text("Position");
                        ImGui::DragFloat3("##BonePosition", &translation.x, 0.01f, -100.0f, 100.0f, "%.3f");
                        
                        ImGui::Text("Rotation (degrees)");
                        ImGui::DragFloat3("##BoneRotation", &rotation.x, 0.5f, -180.0f, 180.0f, "%.1f°");
                        
                        ImGui::Text("Scale");
                        ImGui::DragFloat3("##BoneScale", &scale.x, 0.01f, 0.001f, 10.0f, "%.3f");
                        
                        // Apply changes if any values changed
                        if (translation != originalTranslation || rotation != originalRotation || scale != originalScale) {
                            // Rebuild transformation matrix
                            glm::quat newRotQuat = glm::quat(glm::radians(rotation));
                            
                            glm::mat4 translateMat = glm::translate(glm::mat4(1.0f), translation);
                            glm::mat4 rotateMat = glm::mat4_cast(newRotQuat);
                            glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);

                            // Update the local pose and recalculate
                            rtSkeleComp->setLocalPose(selectedBoneIndex, translateMat * rotateMat * scaleMat);
                        }
                        
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid transformation matrix!");
                        
                        if (ImGui::Button("Reset to Identity")) {
                            rtSkeleComp->setLocalPose(selectedBoneIndex);
                        }
                    }
                    
                } else {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Select a bone to edit its transformation");
                }
            }
            
            ImGui::Spacing();
        }, 
        [&]() { activeScene->removeComp<tinyNodeRT::SKEL3D>(selectedSceneNodeHandle); });
    } else {
        renderComponent("Skeleton", ImVec4(0.05f, 0.05f, 0.05f, 0.3f), ImVec4(0.15f, 0.15f, 0.15f, 0.3f), false, [&]() {},
        [&]() { activeScene->writeComp<tinyNodeRT::SKEL3D>(selectedSceneNodeHandle); });
    }

    if (selectedNode->has<tinyNodeRT::ANIM3D>()) {
        renderComponent("Animation", ImVec4(0.15f, 0.15f, 0.2f, 0.8f), ImVec4(0.3f, 0.3f, 0.4f, 0.6f), true, [&]() {
            // Get component copy using tinySceneRT method
            tinyRT_ANIM3D* compPtr = activeScene->rtComp<tinyNodeRT::ANIM3D>(selectedSceneNodeHandle);

            // Create a search field
            static char searchBuffer[128] = "";
            ImGui::Text("Search:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1); // Full width
            ImGui::InputText("##AnimationSearch", searchBuffer, sizeof(searchBuffer));

            ImGui::Spacing();
            ImGui::Text("Animations:");
            ImGui::Separator();

            // Scrollable animation list
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.0f);
            ImGui::BeginChild("AnimationList", ImVec2(0, 200), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

            for (const auto& [name, handle] : compPtr->MAL()) {
                // Convert search buffer to lowercase for case-insensitive search
                std::string searchStr = std::string(searchBuffer);
                std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);
                std::string nameLower = name;
                std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
                if (!searchStr.empty() && nameLower.find(searchStr) == std::string::npos) {
                    continue; // Skip non-matching animation names
                }

                // Check if this animation is selected
                bool isSelected = (selectedAnimationHandle == handle);
                
                // Highlight selected animation
                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.6f, 0.8f, 0.8f));
                }

                // Selectable animation
                if (ImGui::Selectable(name.c_str(), isSelected)) {
                    selectedAnimationHandle = handle;
                    selectedCompNode = selectedSceneNodeHandle;
                    compMode = CompMode::Animation;
                    showAnimationEditor = true;
                    
                    // Set as current animation without auto-playing (user can press Play button)
                    compPtr->setCurrent(handle, false);
                }

                if (isSelected) {
                    ImGui::PopStyleColor();
                }
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();

            ImGui::Spacing();
        },
        [&]() { activeScene->removeComp<tinyNodeRT::ANIM3D>(selectedSceneNodeHandle); });
    } else {
        renderComponent("Animation", ImVec4(0.05f, 0.05f, 0.05f, 0.3f), ImVec4(0.15f, 0.15f, 0.15f, 0.3f), false, [&]() {},
        [&]() { activeScene->writeComp<tinyNodeRT::ANIM3D>(selectedSceneNodeHandle); });
    }

    if (selectedNode->has<tinyNodeRT::SCRIPT>()) {
        renderComponent("Script", ImVec4(0.0f, 0.0f, 0.0f, 0.8f), ImVec4(1.0f, 1.0f, 1.0f, 0.6f), true, [&]() {
            tinyRT::Script* rtScript = activeScene->rtComp<tinyNodeRT::SCRIPT>(selectedSceneNodeHandle);
            if (rtScript) {
                // Display script file handle
                ImGui::Text("Script File:");
                ImGui::SameLine();
                
                // Custom drag-drop target for script assignment
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.2f, 1.0f));
                
                std::string scriptName = "None";
                tinyHandle scriptFileHandle = rtScript->scriptHandle();
                if (scriptFileHandle.valid()) {
                    auto* fileNode = fs.fNode(scriptFileHandle);
                    if (fileNode) {
                        scriptName = fileNode->name;
                    }
                }
                
                ImGui::Button(scriptName.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0));
                
                // Accept drag-drop of script files
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
                        tinyHandle droppedHandle = *(tinyHandle*)payload->Data;

                        // Retrieve the actual data handle (not the file node handle)
                        typeHandle scripthandle = fs.fTypeHandle(droppedHandle);
                        rtScript->assign(scripthandle.handle);
                    }
                    ImGui::EndDragDropTarget();
                }
                
                ImGui::PopStyleColor(3);
                
                // Button to open debug window for this script
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.8f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.9f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.7f, 1.0f));
                
                if (ImGui::Button("Open Debug Window", ImVec2(-1, 0))) {
                    selectedCompNode = selectedSceneNodeHandle;
                    compMode = CompMode::Script;
                    // Clear animation handles when switching to script mode
                    selectedAnimationHandle = tinyHandle();
                    selectedChannelIndex = -1;
                    showAnimationEditor = true;  // Reusing animation window for debug
                }
                
                ImGui::PopStyleColor(3);
                
                // Display script variables dynamically from the var map
                if (rtScript->valid()) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    
                    auto& varMap = rtScript->vMap();
                    const auto& varOrder = rtScript->vOrder();
                    
                    if (varMap.empty()) {
                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No variables defined");
                    } else {
                        ImGui::Text("Variables (%zu):", varMap.size());
                        ImGui::Spacing();
                        
                        // Calculate the width for labels (left column) - find the longest key name
                        float maxLabelWidth = 0.0f;
                        for (const auto& key : varOrder) {
                            float labelWidth = ImGui::CalcTextSize(key.c_str()).x;
                            if (labelWidth > maxLabelWidth) maxLabelWidth = labelWidth;
                        }
                        maxLabelWidth += 10.0f; // Add some padding
                        
                        // Iterate through variables in sorted order (by type, then alphabetically)
                        for (const auto& key : varOrder) {
                            auto& value = varMap.at(key);
                            ImGui::PushID(key.c_str());
                            
                            // Display key (label) on the left
                            ImGui::Text("%s:", key.c_str());
                            ImGui::SameLine(maxLabelWidth);
                            
                            // Set the width for the input widget to fill remaining space
                            float availableWidth = ImGui::GetContentRegionAvail().x;
                            ImGui::SetNextItemWidth(availableWidth);
                            
                            std::visit([&](auto&& val) {
                                using T = std::decay_t<decltype(val)>;
                                
                                if constexpr (std::is_same_v<T, float>) {
                                    ImGui::DragFloat("##value", &val, 0.01f);
                                }
                                else if constexpr (std::is_same_v<T, int>) {
                                    ImGui::DragInt("##value", &val);
                                }
                                else if constexpr (std::is_same_v<T, bool>) {
                                    ImGui::Checkbox("##value", &val);
                                }
                                else if constexpr (std::is_same_v<T, glm::vec2>) {
                                    ImGui::DragFloat2("##value", &val.x, 0.01f);
                                }
                                else if constexpr (std::is_same_v<T, glm::vec3>) {
                                    ImGui::DragFloat3("##value", &val.x, 0.01f);
                                }
                                else if constexpr (std::is_same_v<T, glm::vec4>) {
                                    ImGui::DragFloat4("##value", &val.x, 0.01f);
                                }
                                else if constexpr (std::is_same_v<T, std::string>) {
                                    // For strings, use a text input
                                    char buffer[256];
                                    strncpy(buffer, val.c_str(), sizeof(buffer) - 1);
                                    buffer[sizeof(buffer) - 1] = '\0';
                                    if (ImGui::InputText("##value", buffer, sizeof(buffer))) {
                                        val = std::string(buffer);
                                    }
                                }
                                else if constexpr (std::is_same_v<T, typeHandle>) {
                                    // Handle as a draggable node reference
                                    // Display current handle or node name with type prefix
                                    std::string displayText;
                                    bool isNodeHandle = val.isType<tinyNodeRT>();
                                    std::string prefix = isNodeHandle ? "[Node] " : "[File] ";
                                    
                                    if (val.valid()) {
                                        if (isNodeHandle) {
                                            // Try to get node name from scene
                                            auto node = activeScene->node(val.handle);
                                            if (node) {
                                                displayText = prefix + node->name + " (" + std::to_string(val.handle.index) + ", " + std::to_string(val.handle.version) + ")";
                                            } else {
                                                displayText = prefix + "Invalid Handle";
                                            }
                                        } else {
                                            // File handle (type void)
                                            displayText = prefix + "RHandle(" + std::to_string(val.handle.index) + ", " + std::to_string(val.handle.version) + ")";
                                        }
                                    } else {
                                        displayText = "(Drop Node Here)";
                                    }
                                    
                                    // Make it look like a drop target
                                    ImGui::PushStyleColor(ImGuiCol_Button, isNodeHandle ? ImVec4(0.2f, 0.3f, 0.2f, 1.0f) : ImVec4(0.3f, 0.2f, 0.2f, 1.0f));
                                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, isNodeHandle ? ImVec4(0.3f, 0.4f, 0.3f, 1.0f) : ImVec4(0.4f, 0.3f, 0.3f, 1.0f));
                                    ImGui::Button(displayText.c_str(), ImVec2(availableWidth, 0));
                                    ImGui::PopStyleColor(2);
                                    
                                    // Accept drag-drop from scene hierarchy or file browser
                                    if (ImGui::BeginDragDropTarget()) {
                                        // Accept node handles (from scene hierarchy)
                                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NODE_HANDLE")) {
                                            tinyHandle draggedNode = *(const tinyHandle*)payload->Data;
                                            val = typeHandle::make<tinyNodeRT>(draggedNode);  // Node handle (type tinyNodeRT)
                                        }
                                        // Accept file handles (from file browser)
                                        else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
                                            tinyHandle draggedFileNode = *(const tinyHandle*)payload->Data;
                                            typeHandle resourceHandle = fs.fTypeHandle(draggedFileNode);

                                            if (val.sameType(resourceHandle)) val = resourceHandle;
                                        }
                                        ImGui::EndDragDropTarget();
                                    }
                                    
                                    // Right-click context menu
                                    if (ImGui::BeginPopupContextItem()) {
                                        if (ImGui::MenuItem("Clear")) {
                                            val.handle = tinyHandle();  // Invalid handle
                                        }
                                        if (val.valid() && isNodeHandle) {
                                            if (ImGui::MenuItem("Select in Scene")) {
                                                selectedSceneNodeHandle = val.handle;
                                            }
                                        }
                                        ImGui::EndPopup();
                                    }
                                }
                            }, value);
                            
                            ImGui::PopID();
                        }
                    }
                } else {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "Invalid Script");
                }
            }
        },
        [&]() { activeScene->removeComp<tinyNodeRT::SCRIPT>(selectedSceneNodeHandle); });
    } else {
        renderComponent("Script", ImVec4(0.05f, 0.05f, 0.05f, 0.3f), ImVec4(0.15f, 0.15f, 0.15f, 0.3f), false, [&]() {},
        [&]() { activeScene->writeComp<tinyNodeRT::SCRIPT>(selectedSceneNodeHandle); });
    }
    
    ImGui::EndChild();
    
    // Pop the scrollbar style customizations
    ImGui::PopStyleColor(4); // Pop ScrollbarBg, ScrollbarGrab, ScrollbarGrabHovered, ScrollbarGrabActive
    ImGui::PopStyleVar(2); // Pop ScrollbarSize, ScrollbarRounding
}




void tinyApp::renderFileSystemInspector() {
    const tinyFS& fs = project->fs();
    
    // Get the selected file node handle from unified selection
    tinyHandle selectedFNodeHandle = getSelectedFileNode();
    if (!selectedFNodeHandle.valid()) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "No file selected");
        ImGui::Text("This should not happen in unified selection.");
        return;
    }
    
    const tinyFS::Node* selectedFNode = fs.fNode(selectedFNodeHandle);
    if (!selectedFNode) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid filesystem node selection");
        clearSelection(); // Clear invalid selection
        return;
    }
    
    bool isFile = selectedFNode->isFile();
    bool isFolder = !isFile;
    
    // === SHARED HEADER ===
    ImGui::Text("%s Inspector", isFile ? "File" : "Folder");
    ImGui::Separator();
    
    // === EDITABLE NAME FIELD - Only for root node ===
    bool isRootNode = (selectedFNodeHandle == fs.rootHandle());
    
    ImGui::Text("FName:");
    ImGui::SameLine();

    if (isRootNode) {
        // Only display name
        ImGui::Text("%s", selectedFNode->name.c_str());
    } else {
        // Create a unique ID for the input field
        std::string inputId = "##FNodeName_" + std::to_string(selectedFNodeHandle.index);

        // Static buffer to hold the name during editing
        static char nameBuffer[256];
        static tinyHandle lastSelectedFNode;
        
        // Initialize buffer if we switched to a different file/folder
        if (lastSelectedFNode != selectedFNodeHandle) {
            strncpy_s(nameBuffer, selectedFNode->name.c_str(), sizeof(nameBuffer) - 1);
            nameBuffer[sizeof(nameBuffer) - 1] = '\0';
            lastSelectedFNode = selectedFNodeHandle;
        }
        
        // Editable text input
        ImGui::SetNextItemWidth(-1); // Use full available width
        bool enterPressed = ImGui::InputText(inputId.c_str(), nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
        
        // Apply rename on Enter or when focus is lost
        if (enterPressed || (ImGui::IsItemDeactivatedAfterEdit())) {
            tinyFS::Node* mutableNode = const_cast<tinyFS::Node*>(fs.fNode(selectedFNodeHandle));
            if (mutableNode) {
                mutableNode->name = std::string(nameBuffer);
            }
        }
        
        ImGui::Spacing();
    }

    ImGui::Separator();

    // === FOLDER-SPECIFIC: FOLDER INFO ===
    if (isFolder) {
        // Folder info
        ImGui::Text("Children: %zu", selectedFNode->children.size());
    }
    
    // === FILE-SPECIFIC: TYPE INFORMATION ===
    if (isFile) {
        // Determine file type and show specific information
        std::string fileType = "Unknown";
        typeHandle tHandle = selectedFNode->tHandle;

        if (tHandle.isType<tinyScript>()) {
            fileType = "Script";
            ImGui::Text("Type: %s", fileType.c_str());

            // Extended data
            const tinyScript* script = fs.registry().get<tinyScript>(tHandle);
            if (script) {
                ImGui::Text("Valid: %s", script->valid() ? "Yes" : "No");
                ImGui::Text("Version: %u", script->version());
                
                // Automatically select this script for the script editor (persistent selection)
                selectedScriptHandle = tHandle.handle;
            }
        } else if (tHandle.isType<tinySceneRT>()) {
            fileType = "Scene";
            ImGui::Text("Type: %s", fileType.c_str());

            // Extended data
            const tinySceneRT* scene = fs.rGet<tinySceneRT>(tHandle.handle);
            if (scene) {
                ImGui::Text("Scene Nodes: %u", scene->nodeCount());
                
                // Make Active button
                ImGui::Spacing();
                tinyHandle sceneRegistryHandle = tHandle.handle;
                bool isActiveScene = (getActiveSceneHandle() == sceneRegistryHandle);
                
                if (isActiveScene) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                    ImGui::Button("Active Scene", ImVec2(-1, 30));
                    ImGui::PopStyleColor(4);
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                    
                    if (ImGui::Button("Make Active", ImVec2(-1, 30))) {
                        if (setActiveScene(sceneRegistryHandle)) {
                            selectSceneNode(activeSceneRootHandle()); // Reset node selection
                        }
                    }
                    ImGui::PopStyleColor(3);
                }
            }
        } else if (tHandle.isType<tinyTextureVk>()) {
            fileType = "Texture";
            ImGui::Text("Type: %s", fileType.c_str());

            // Extended data
            const tinyTextureVk* textureVk = fs.registry().get<tinyTextureVk>(tHandle);
            if (textureVk) {
                const tinyTexture& textureCPU = textureVk->cpu();

                ImGui::Text("Usage Count: %d", textureVk->useCount());

                ImGui::Separator();

                ImGui::Text("Dimensions: %dx%d", textureCPU.width(), textureCPU.height());
                ImGui::Text("Channels: %d", textureCPU.channels());
                ImGui::Text("Hash: %u", textureCPU.hash());
            }
        } else if (tHandle.isType<tinyMaterialVk>()) {
            fileType = "Material";
            ImGui::Text("Type: %s", fileType.c_str());

            // Extended data
            if (const tinyMaterialVk* materialVk = fs.registry().get<tinyMaterialVk>(tHandle)) {
                ImGui::Separator();

                if (const tinyTextureVk* albedoTex = materialVk->texture(MTexSlot::Albedo)) {
                    const tinyTexture& textureCPU = albedoTex->cpu();

                    ImGui::Text("Albedo Texture [%dx%d, %d channels]", textureCPU.width(), textureCPU.height(), textureCPU.channels());
                }

                if (const tinyTextureVk* normalTex = materialVk->texture(MTexSlot::Normal)) {
                    const tinyTexture& textureCPU = normalTex->cpu();

                    ImGui::Text("Normal Texture [%dx%d, %d channels]", textureCPU.width(), textureCPU.height(), textureCPU.channels());
                }
            }

        } else if (tHandle.isType<tinyMeshVk>()) {
            fileType = "Mesh";
            ImGui::Text("Type: %s", fileType.c_str());
        } else if (tHandle.isType<tinySkeleton>()) {
            fileType = "Skeleton";
            ImGui::Text("Type: %s", fileType.c_str());
        } else {
            ImGui::Text("Type: %s", fileType.c_str());
        }
    }
}

void tinyApp::renderComponent(const char* componentName, ImVec4 backgroundColor, ImVec4 borderColor, bool showRemoveButton, std::function<void()> renderContent, std::function<void()> onRemove) {
    // Component container with styled background
    ImGui::PushStyleColor(ImGuiCol_ChildBg, backgroundColor);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, borderColor);
    
    // Create unique ID for this component
    std::string childId = std::string(componentName) + "Component";
    
    if (ImGui::BeginChild(childId.c_str(), ImVec2(0, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders)) {
        // Header with optional remove button
        ImGui::Text("%s", componentName);
        
        if (showRemoveButton && onRemove) {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            
            std::string buttonId = "Remove##" + std::string(componentName);
            if (ImGui::Button(buttonId.c_str(), ImVec2(65, 0))) {
                onRemove(); // Call the removal function
                ImGui::PopStyleColor(3);
                ImGui::EndChild();
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar(2);
                return; // Exit early since component was removed
            }
            ImGui::PopStyleColor(3);
        }
        
        ImGui::Separator();
        
        // Render the component-specific content
        if (renderContent) {
            renderContent();
        }
        
        ImGui::EndChild();
    }
    
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    
    // Add spacing between components
    ImGui::Spacing();
    ImGui::Spacing();
}

bool tinyApp::renderHandleField(const char* fieldId, tinyHandle& handle, const char* targetType, const char* dragTooltip, const char* description) {
    bool modified = false;
    
    // Create enhanced drag-drop target area with better styling
    std::string displayText;
    ImVec4 buttonColor, hoveredColor, activeColor;

    const tinyFS& fs = project->fs();
    
    if (handle.valid()) {
        displayText = "RHandle(" + std::to_string(handle.index) + ", " + std::to_string(handle.version) + ")";
        buttonColor = ImVec4(0.2f, 0.4f, 0.2f, 1.0f);   // Green tint for valid handle
        hoveredColor = ImVec4(0.3f, 0.5f, 0.3f, 1.0f);
        activeColor = ImVec4(0.1f, 0.3f, 0.1f, 1.0f);
    } else {
        displayText = "None";
        buttonColor = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);   // Gray for empty
        hoveredColor = ImVec4(0.4f, 0.4f, 0.6f, 1.0f); // Blue tint on hover
        activeColor = ImVec4(0.2f, 0.2f, 0.4f, 1.0f);
    }
    
    ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
    
    // Create the drag-drop button with clear styling
    if (ImGui::Button((displayText + fieldId).c_str(), ImVec2(-1, 30))) {
        // Clear handle on click
        if (handle.valid()) {
            handle = tinyHandle();
            modified = true;
        }
    }
    
    ImGui::PopStyleColor(3);
    
    if (ImGui::BeginDragDropTarget()) {
        ImGui::PushStyleColor(ImGuiCol_DragDropTarget, ImVec4(0.3f, 0.6f, 1.0f, 0.7f));
        
        // Accept different payload types based on target type
        if (strcmp(targetType, "Mesh") == 0) {
            // Accept mesh files from file explorer
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
                tinyHandle fileNodeHandle = *(const tinyHandle*)payload->Data;
                const tinyFS::Node* fileNode = project->fs().fNode(fileNodeHandle);
                if (fileNode && fileNode->isFile() && fileNode->tHandle.isType<tinyMesh>()) {
                    handle = fileNode->tHandle.handle; // Use registry handle
                    modified = true;
                }
            }
        } else if (strcmp(targetType, "Skeleton") == 0) {
            // Accept skeleton files from file explorer
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
                tinyHandle fileNodeHandle = *(const tinyHandle*)payload->Data;
                const tinyFS::Node* fileNode = project->fs().fNode(fileNodeHandle);
                if (fileNode && fileNode->isFile() && fileNode->tHandle.isType<tinySkeleton>()) {
                    handle = fileNode->tHandle.handle; // Use registry handle
                    modified = true;
                }
            }
        } else if (strcmp(targetType, "SkeletonNode") == 0) {
            // Accept skeleton nodes from hierarchy
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NODE_HANDLE")) {
                tinyHandle nodeHandle = *(const tinyHandle*)payload->Data;
                tinySceneRT* activeScene = getActiveScene();
                if (activeScene) {
                    const tinyNodeRT* node = activeScene->node(nodeHandle);
                    if (node && node->has<tinyNodeRT::SKEL3D>()) {
                        handle = nodeHandle; // Use node handle directly
                        modified = true;
                    }
                }
            }
        }
        
        ImGui::PopStyleColor();
        ImGui::EndDragDropTarget();
    }
    
    return modified;
}

// FileDialog method implementations
void FileDialog::open(const std::filesystem::path& startPath, tinyHandle folder) {
    // Don't open if we're in the process of closing
    if (shouldClose) return;
    
    isOpen = true;
    justOpened = true;  // Mark that we need to open the popup
    currentPath = startPath;
    targetFolder = folder;
    selectedFile.clear();
    refreshFileList();
}

void FileDialog::close() {
    shouldClose = true;  // Mark for closing instead of immediate close
    selectedFile.clear();
    targetFolder = tinyHandle();
}

void FileDialog::update() {
    // Handle delayed closing after ImGui has processed the popup
    if (shouldClose && !ImGui::IsPopupOpen("Load Model File")) {
        isOpen = false;
        justOpened = false;
        shouldClose = false;
    }
}

void FileDialog::refreshFileList() {
    currentFiles.clear();
    try {
        if (std::filesystem::exists(currentPath) && std::filesystem::is_directory(currentPath)) {
            for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
                currentFiles.push_back(entry);
            }
            
            // Sort: directories first, then files, both alphabetically
            std::sort(currentFiles.begin(), currentFiles.end(), [](const auto& a, const auto& b) {
                if (a.is_directory() != b.is_directory()) {
                    return a.is_directory(); // Directories first
                }
                return a.path().filename() < b.path().filename();
            });
        }
    } catch (const std::exception&) {
        // Handle permission errors or other filesystem issues
    }
}

bool FileDialog::isModelFile(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".glb" || ext == ".gltf" || ext == ".obj";
}

// LoadScriptDialog method implementations
void LoadScriptDialog::open(const std::filesystem::path& startPath, tinyHandle folder) {
    // Don't open if we're in the process of closing
    if (shouldClose) return;
    
    isOpen = true;
    justOpened = true;
    currentPath = startPath;
    targetFolder = folder;
    selectedFile.clear();
    refreshFileList();
}

void LoadScriptDialog::close() {
    shouldClose = true;
    selectedFile.clear();
    targetFolder = tinyHandle();
}

void LoadScriptDialog::update() {
    // Handle delayed closing after ImGui has processed the popup
    if (shouldClose && !ImGui::IsPopupOpen("Load Lua Script")) {
        isOpen = false;
        justOpened = false;
        shouldClose = false;
    }
}

void LoadScriptDialog::refreshFileList() {
    currentFiles.clear();
    try {
        if (std::filesystem::exists(currentPath) && std::filesystem::is_directory(currentPath)) {
            for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
                currentFiles.push_back(entry);
            }
            
            // Sort: directories first, then files, both alphabetically
            std::sort(currentFiles.begin(), currentFiles.end(), [](const auto& a, const auto& b) {
                if (a.is_directory() != b.is_directory()) {
                    return a.is_directory(); // Directories first
                }
                return a.path().filename() < b.path().filename();
            });
        }
    } catch (const std::exception&) {
        // Handle permission errors or other filesystem issues
    }
}

bool LoadScriptDialog::isLuaFile(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".lua";
}



void tinyApp::renderNodeTreeImGui(tinyHandle nodeHandle, int depth) {
    const tinyFS& fs = project->fs();

    tinySceneRT* activeScene = getActiveScene();
    if (!activeScene) return;

    // Use root node if no valid handle provided
    if (!nodeHandle.valid()) nodeHandle = activeScene->rootHandle();

    const tinyNodeRT* node = activeScene->node(nodeHandle);
    if (!node) return;
    
    // Create a unique ID for this node
    ImGui::PushID(static_cast<int>(nodeHandle.index));

    // Check if this node has children
    bool hasChildren = !node->childrenHandles.empty();
    bool isSelected = (selectedHandle.isScene() && selectedHandle.handle.index == nodeHandle.index && selectedHandle.handle.version == nodeHandle.version);
    bool isHeld = (heldHandle.isScene() && heldHandle.handle.index == nodeHandle.index && heldHandle.handle.version == nodeHandle.version);
    bool isCompNode = (selectedCompNode.valid() && selectedCompNode.index == nodeHandle.index && selectedCompNode.version == nodeHandle.version);
    
    // Create tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // Add consistent styling to match File explorer theme, with special highlight for component node
    if (isCompNode) {
        // Highlight component node (animation/script) in cyan/blue
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.2f, 0.4f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.5f, 0.8f, 0.8f));
    } else {
        // Normal styling
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f));
    }

    // Force open if this node is in the expanded set
    bool forceOpen = isNodeExpanded(nodeHandle);

    // Set the default open state (this will be overridden by user interaction)
    if (forceOpen) ImGui::SetNextItemOpen(true, ImGuiCond_Always);

    bool nodeOpen = ImGui::TreeNodeEx(node->name.c_str(), flags);

    // Track expansion state changes (only for nodes with children)
    if (hasChildren) {
        // Manual expansion/collapse tracking
        if (nodeOpen && !forceOpen) expandedNodes.insert(nodeHandle);
        else if (!nodeOpen && isNodeExpanded(nodeHandle)) expandedNodes.erase(nodeHandle);
    }
    
    // Drag and drop source (including root node - tinyScene has safeguards)
    bool isDragging = false;
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        isDragging = true;
        holdSceneNode(nodeHandle);  // Set the held node during drag

        // Set payload to carry the node handle
        ImGui::SetDragDropPayload("NODE_HANDLE", &nodeHandle, sizeof(tinyHandle));
        
        // Display preview
        ImGui::Text("Moving: %s", node->name.c_str());
        ImGui::EndDragDropSource();
    }

    // Clear held node when not actively dragging from this item
    // Note: held node will be cleared in the drag drop target accept logic
    
    // Handle selection - only select on mouse release if we didn't drag
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        // Check if this was a click (no significant drag distance)
        ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
        float dragDistance = sqrtf(dragDelta.x * dragDelta.x + dragDelta.y * dragDelta.y);
        
        // Only select if drag distance was minimal (treat as click)
        if (dragDistance < 5.0f) { // 5 pixel tolerance
            selectSceneNode(nodeHandle);
        }
        
        // Reset drag delta for next interaction
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
    }
    
    
    // Select node on right-click (immediate selection before context menu)
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        selectSceneNode(nodeHandle);
    }
    
    
    // Drag and drop target
    if (ImGui::BeginDragDropTarget()) {
        // Accept node reparenting
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NODE_HANDLE")) {
            tinyHandle draggedNode = *(const tinyHandle*)payload->Data;
            
            // Attempt to reparent the dragged node to this node
            if (activeScene->reparentNode(draggedNode, nodeHandle)) {
                // Auto-expand the parent chain to show the newly dropped node
                expandParentChain(nodeHandle);
                
                // Keep the dragged node selected (maintain selection across drag operations)
                selectSceneNode(draggedNode);
                
                // Clear held state after successful drop
                clearHeld();
            }
        }
        
        // Accept scene drops (from filesystem nodes)
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_FNODE")) {
            tinyHandle sceneFNodeHandle = *(const tinyHandle*)payload->Data;
            
            // Get the filesystem node to access its typeHandle
            const tinyFS::Node* sceneFile = project->fs().fNode(sceneFNodeHandle);
            if (sceneFile && sceneFile->isFile() && sceneFile->tHandle.isType<tinySceneRT>()) {
                // Extract the registry handle from the typeHandle
                tinyHandle sceneRegistryHandle = sceneFile->tHandle.handle;
                
                // Verify the scene exists and instantiate it at this node
                const tinySceneRT* scene = fs.rGet<tinySceneRT>(sceneRegistryHandle);
                if (scene) {
                    // Place the scene at this node
                    project->addSceneInstance(sceneRegistryHandle, getActiveSceneHandle(), nodeHandle);
                    // CRITICAL: Set the nodeptr again to avoid dangling held state
                    node = activeScene->node(nodeHandle);
                    // Auto-expand the parent chain to show the newly instantiated scene
                    expandParentChain(nodeHandle);
                }
            }
        }
        
        // Also accept FILE_HANDLE payloads and check if they're scene files
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
            tinyHandle fileNodeHandle = *(const tinyHandle*)payload->Data;
            
            // Get the filesystem node to check if it's a scene file
            const tinyFS::Node* fileNode = project->fs().fNode(fileNodeHandle);
            if (fileNode && fileNode->isFile() && fileNode->tHandle.isType<tinySceneRT>()) {
                // This is a scene file - instantiate it at this node
                tinyHandle sceneRegistryHandle = fileNode->tHandle.handle;
                
                // Safety check: prevent dropping a scene into itself
                if (sceneRegistryHandle == getActiveSceneHandle()) {
                    // Cannot drop active scene into itself - ignore the operation
                    ImGui::SetTooltip("Cannot drop a scene into itself!");
                } else {
                    // Verify the scene exists and instantiate it at this node
                    const tinySceneRT* scene = fs.rGet<tinySceneRT>(sceneRegistryHandle);
                    if (scene) {
                        // Place the scene at this node
                        project->addSceneInstance(sceneRegistryHandle, getActiveSceneHandle(), nodeHandle);
                        // CRITICAL: Set the nodeptr again to avoid dangling held state
                        node = activeScene->node(nodeHandle);
                        // Auto-expand the parent chain to show the newly instantiated scene
                        expandParentChain(nodeHandle);
                    }
                }
            }
        }

        ImGui::EndDragDropTarget();
    }
    
    
    // Context menu for nodes - direct manipulation
    if (ImGui::BeginPopupContextItem()) {
        ImGui::Text("%s", node->name.c_str());
        ImGui::Separator();
        
        if (ImGui::MenuItem("Add Child")) {
            tinySceneRT* scene = getActiveScene();
            if (scene) {
                tinyHandle newNodeHandle = scene->addNode("New Node", nodeHandle);
                selectSceneNode(newNodeHandle);
                expandNode(nodeHandle); // Expand parent to show new child
            }
        }
        
        ImGui::Separator();

        bool isRootNode = (nodeHandle == activeScene->rootHandle());
        if (ImGui::MenuItem("Delete", nullptr, false, !isRootNode)) {
            tinySceneRT* scene = getActiveScene();
            tinyHandle parentHandle = node->parentHandle;
            if (scene && scene->removeNode(nodeHandle)) {
                const tinyNodeRT* parentNode = scene->node(parentHandle);
                selectSceneNode(parentNode ? parentHandle : scene->rootHandle()); // Select parent or root if no parent
            }
        }

        if (ImGui::MenuItem("Flatten", nullptr, false, !isRootNode && hasChildren)) {
            tinySceneRT* scene = getActiveScene();
            tinyHandle parentHandle = node->parentHandle;
            if (scene && scene->flattenNode(nodeHandle)) {
                const tinyNodeRT* parentNode = scene->node(parentHandle);
                selectSceneNode(parentNode ? parentHandle : scene->rootHandle()); // Select parent or root if no parent
            }
        }
        
        ImGui::EndPopup();

        // CRITICAL: RESET Node handle AND node ptr
        nodeHandle = selectedHandle.isScene() ? selectedHandle.handle : tinyHandle();
        node = activeScene->node(nodeHandle);
    }

    if (nodeOpen && hasChildren && node != nullptr) {
        // Sort children by have-children? -> name
        std::vector<tinyHandle> sortedChildren = node->childrenHandles;
        std::sort(sortedChildren.begin(), sortedChildren.end(), [&](const tinyHandle& a, const tinyHandle& b) {
            const tinyNodeRT* nodeA = activeScene->node(a);
            const tinyNodeRT* nodeB = activeScene->node(b);
            // First sort by whether they have children
            bool aHasChildren = nodeA && !nodeA->childrenHandles.empty();
            bool bHasChildren = nodeB && !nodeB->childrenHandles.empty();
            if (aHasChildren != bHasChildren) return aHasChildren;
            // Otherwise sort by name
            std::string nameA = nodeA ? nodeA->name : "";
            std::string nameB = nodeB ? nodeB->name : "";
            return nameA < nameB;
        });

        for (const tinyHandle& childHandle : sortedChildren) {
            renderNodeTreeImGui(childHandle, depth + 1);
        }
        ImGui::TreePop();
    }

    // Pop the style colors we pushed earlier
    ImGui::PopStyleColor(2); // Pop HeaderHovered and Header
    
    ImGui::PopID();
}

void tinyApp::expandParentChain(tinyHandle nodeHandle) {
    tinySceneRT* activeScene = getActiveScene();
    if (!activeScene) return;
    
    // Get the target node
    const tinyNodeRT* targetNode = activeScene->node(nodeHandle);
    if (!targetNode) return;
    
    // Walk up the parent chain and expand all parents
    tinyHandle currentHandle = targetNode->parentHandle;
    while (currentHandle.valid()) {
        expandedNodes.insert(currentHandle);
        
        const tinyNodeRT* currentNode = activeScene->node(currentHandle);
        if (!currentNode) break;
        
        currentHandle = currentNode->parentHandle;
    }
    
    // Also expand the target node itself if it has children
    if (!targetNode->childrenHandles.empty()) {
        expandedNodes.insert(nodeHandle);
    }
}

void tinyApp::expandFNodeParentChain(tinyHandle fNodeHandle) {
    // Get the target file node
    const tinyFS::Node* targetFNode = project->fs().fNode(fNodeHandle);
    if (!targetFNode) return;
    
    // Walk up the parent chain and expand all parents
    tinyHandle currentHandle = targetFNode->parent;
    while (currentHandle.valid()) {
        expandedFNodes.insert(currentHandle);
        
        const tinyFS::Node* currentFNode = project->fs().fNode(currentHandle);
        if (!currentFNode) break;
        
        currentHandle = currentFNode->parent;
    }
    
    // Also expand the target node itself if it's a folder with children
    if (!targetFNode->isFile() && !targetFNode->children.empty()) {
        expandedFNodes.insert(fNodeHandle);
    }
}

void tinyApp::selectSceneNode(tinyHandle nodeHandle) {
    // Store previous scene node before changing selection
    if (selectedHandle.isScene() && selectedHandle.handle.valid()) {
        prevSceneNode = selectedHandle.handle;
    }
    
    // Update selection to new scene node
    selectedHandle = SelectHandle(nodeHandle, SelectHandle::Type::Scene);
    
    // Update selectedCompNode if the new node has animation or script component
    if (nodeHandle.valid()) {
        tinySceneRT* activeScene = getActiveScene();
        if (activeScene) {
            const tinyNodeRT* node = activeScene->node(nodeHandle);
            if (node && (node->has<tinyNodeRT::ANIM3D>() || node->has<tinyNodeRT::SCRIPT>())) {
                // Only update if it's a different node (not the same node being re-selected)
                if (!selectedCompNode.valid() || 
                    selectedCompNode.index != nodeHandle.index || 
                    selectedCompNode.version != nodeHandle.version) {
                    selectedCompNode = nodeHandle;
                }
            }
        }
    }
}

void tinyApp::selectFileNode(tinyHandle fileHandle) {
    // Check if the file handle is valid
    if (!fileHandle.valid()) {
        clearSelection();
        return;
    }
    
    // Get the filesystem node
    const tinyFS& fs = project->fs();
    const tinyFS::Node* node = fs.fNode(fileHandle);
    if (!node) {
        // Invalid node, clear selection
        clearSelection();
        return;
    }
    
    // Only select if it's an actual file, not a folder
    if (node->isFile()) {
        // Store previous file node before changing selection
        if (selectedHandle.isFile() && selectedHandle.handle.valid()) {
            prevFileNode = selectedHandle.handle;
        }
        
        selectedHandle = SelectHandle(fileHandle, SelectHandle::Type::File);
    }
    // If it's a folder, ignore the selection (don't set selectedHandle)
}

void tinyApp::renderFileExplorerImGui(tinyHandle nodeHandle, int depth) {
    tinyFS& fs = project->fs();
    
    // Use root handle if no valid handle provided
    if (!nodeHandle.valid()) {
        nodeHandle = fs.rootHandle();
    }
    
    const tinyFS::Node* node = fs.fNode(nodeHandle);
    if (!node) return;
    
    // Create a unique ID for this node
    ImGui::PushID(static_cast<int>(nodeHandle.index));
    
    // Check if this node has children and is selected
    bool hasChildren = !node->children.empty();
    bool isSelected = (selectedHandle.isFile() && selectedHandle.handle.index == nodeHandle.index && selectedHandle.handle.version == nodeHandle.version);
    
    if (node->isFolder()) {
        // Display root folder as ".root" instead of full path
        std::string displayName = (nodeHandle == fs.rootHandle()) ? ".root" : node->name;
        
        // Create tree node flags
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        // Always show arrow for folders (even empty ones), but prevent tree push if no children
        if (!hasChildren) {
            flags |= ImGuiTreeNodeFlags_NoTreePushOnOpen; // Don't push tree state, but keep the arrow
        }
        if (isSelected) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        
        // Add consistent styling to match node hierarchy theme
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.4f)); // Gray hover background (same as nodes)
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f)); // Gray selection background (same as nodes)
        
        // Force open if this node is in the expanded set
        bool forceOpen = isFNodeExpanded(nodeHandle);
        
        // Set the default open state (this will be overridden by user interaction)
        if (forceOpen) {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }
        
        bool nodeOpen = ImGui::TreeNodeEx(displayName.c_str(), flags);
        
        // Track expansion state changes (only for folders with children)
        if (hasChildren) {
            if (nodeOpen && !forceOpen) {
                // User expanded this folder manually
                expandedFNodes.insert(nodeHandle);
            } else if (!nodeOpen && isFNodeExpanded(nodeHandle)) {
                // User collapsed this folder manually
                expandedFNodes.erase(nodeHandle);
            }
        }
        
        // Pop the style colors for folders right after TreeNodeEx
        ImGui::PopStyleColor(2);
        
        // Handle selection - only select on mouse release if we didn't drag
        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            // Check if this was a click (no significant drag distance)
            ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
            float dragDistance = sqrtf(dragDelta.x * dragDelta.x + dragDelta.y * dragDelta.y);
            
            // Only select if drag distance was minimal (treat as click)
            if (dragDistance < 5.0f) { // 5 pixel tolerance
                selectFileNode(nodeHandle);
            }
            
            // Reset drag delta for next interaction
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }
        
        // Select folder on right-click (immediate selection before context menu)
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            selectFileNode(nodeHandle);
        }
        
        // Drag source for folders
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            // Set this folder as held when dragging begins
            holdFileNode(nodeHandle);
            ImGui::SetDragDropPayload("FOLDER_HANDLE", &nodeHandle, sizeof(tinyHandle));
            ImGui::Text("Moving: %s", displayName.c_str());
            ImGui::EndDragDropSource();
        }
        
        // Drag drop target for folders and files
        if (ImGui::BeginDragDropTarget()) {
            // Accept folders being dropped
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FOLDER_HANDLE")) {
                tinyHandle draggedFolder = *(const tinyHandle*)payload->Data;
                if (draggedFolder != nodeHandle) { // Can't drop folder on itself
                    if (fs.fMove(draggedFolder, nodeHandle)) {
                        // Auto-expand the target folder and select the moved folder
                        expandedFNodes.insert(nodeHandle);
                        selectFileNode(draggedFolder);
                        expandFNodeParentChain(draggedFolder); // Ensure parent chain is expanded
                        clearHeld(); // Clear held state after successful move
                    }
                }
            }
            
            // Accept files being dropped  
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
                tinyHandle draggedFile = *(const tinyHandle*)payload->Data;
                if (fs.fMove(draggedFile, nodeHandle)) {
                    // Auto-expand the target folder and select the moved file
                    expandedFNodes.insert(nodeHandle);
                    selectFileNode(draggedFile);
                    expandFNodeParentChain(draggedFile); // Ensure parent chain is expanded
                    clearHeld(); // Clear held state after successful move
                }
            }
            
            ImGui::EndDragDropTarget();
        }
        
        // Context menu for folders - direct manipulation
        if (ImGui::BeginPopupContextItem()) {
            ImGui::Text("%s", displayName.c_str());
            ImGui::Separator();
            
            if (ImGui::MenuItem("Add Folder")) {
                tinyHandle newFolderHandle = fs.addFolder(nodeHandle, "New Folder");
                selectFileNode(newFolderHandle);
                // Expand parent chain to show the new folder
                expandFNodeParentChain(newFolderHandle);
            }

            if (ImGui::MenuItem("Add Scene")) {
                tinySceneRT newScene("New Scene");
                newScene.addRoot("Root");
                newScene.setSharedRes(project->sharedRes());

                tinyHandle fileHandle = fs.addFile(nodeHandle, "New Scene", std::move(newScene));
                selectFileNode(fileHandle);
                expandFNodeParentChain(fileHandle);
            }

            if (ImGui::MenuItem("Add Script")) {
                tinyScript newScript;

                tinyHandle fileHandle = fs.addFile(nodeHandle, "New Script", std::move(newScript));
                selectFileNode(fileHandle);
                expandFNodeParentChain(fileHandle);
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Delete", nullptr, false, node->deletable())) {
                tinyHandle parentHandle = node->parent;
                fs.fRemove(nodeHandle);
                if (selectedHandle.handle == nodeHandle) {
                    selectFileNode(parentHandle);
                }
            }

            // Folder operations don't affect file, no need for pending deletion
            if (ImGui::MenuItem("Flatten", nullptr, false, node->deletable())) {
                if (node->deletable()) fs.fFlatten(nodeHandle);
            }

            ImGui::Separator();
            
            if (ImGui::MenuItem("Load Model...")) {
                // Start from current working directory for full file system access
                std::filesystem::path startPath = std::filesystem::current_path();
                fileDialog.open(startPath, nodeHandle);
            }
            
            if (ImGui::MenuItem("Load Lua Script...")) {
                // Start from current working directory for full file system access
                std::filesystem::path startPath = std::filesystem::current_path();
                loadScriptDialog.open(startPath, nodeHandle);
            }

            ImGui::EndPopup();
        }
        
        // If folder is open and has children, recurse for children
        if (nodeOpen && hasChildren) {
            // Create a sorted copy of children handles - folders first, then files, both sorted by name
            std::vector<tinyHandle> sortedChildren;
            for (const tinyHandle& childHandle : node->children) {
                const tinyFS::Node* child = fs.fNode(childHandle);
                if (!child || child->hidden()) continue; // Skip invalid or hidden nodes
                sortedChildren.push_back(childHandle);
            }
            
            std::sort(sortedChildren.begin(), sortedChildren.end(), [&fs](const tinyHandle& a, const tinyHandle& b) {
                const tinyFS::Node* nodeA = fs.fNode(a);
                const tinyFS::Node* nodeB = fs.fNode(b);
                if (!nodeA || !nodeB) return false;

                // Helpful function to retrieve type extensions
                auto getTypeExt = [&fs](const tinyFS::Node* node) -> tinyFS::TypeExt {
                    return fs.typeInfo(node->tHandle.typeIndex)->typeExt.ext;
                };
                
                tinyFS::TypeExt extA = fs.typeInfo(nodeA->tHandle.typeIndex)->typeExt.ext;
                tinyFS::TypeExt extB = fs.typeInfo(nodeB->tHandle.typeIndex)->typeExt.ext;

                // No need for folder check, automatically set max priority

                if (extA < extB) return true;
                else if (extA > extB) return false;
                return nodeA->name < nodeB->name;
            });
            
            for (const tinyHandle& childHandle : sortedChildren) {
                renderFileExplorerImGui(childHandle, depth + 1);
            }

            ImGui::TreePop();
        }
        
    } else if (node->isFile()) {
        // General file handling - completely generic
        std::string fileName = node->name;
        tinyFS::TypeExt fileExt = fs.typeExt(node->tHandle.typeIndex);

        // Set consistent colors for all files
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.4f)); // Hover background
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f)); // Selection background
        
        // Create a selectable item for the full filename + extension
        std::string fullDisplayName = fileExt.empty() ? fileName : (fileName + "." + fileExt.ext);
        bool wasClicked = ImGui::Selectable(("##file_" + std::to_string(nodeHandle.index)).c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick);

        // Capture interaction state immediately after the Selectable
        bool itemHovered = ImGui::IsItemHovered();
        bool leftClicked = itemHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        bool rightClicked = itemHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        bool dragStarted = itemHovered && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None);
        
        // Render the filename and extension with different colors on top of the selectable
        ImGui::SameLine(0, 0); // No spacing, start from beginning of selectable
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemInnerSpacing.x); // Add some padding
        
        // Render filename in white
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f)); // White text
        ImGui::Text("%s", fileName.c_str());
        ImGui::PopStyleColor();
        
        // Render extension in gray if it exists
        if (!fileExt.empty()) {
            ImGui::SameLine(0, 0); // No spacing between filename and extension
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(fileExt.color[0], fileExt.color[1], fileExt.color[2], 1.0f)); // Even grayer text
            ImGui::Text(".%s", fileExt.ext.c_str());
            ImGui::PopStyleColor();
        }
        
        // Generic selection handling using captured state
        if (leftClicked) {
            ImVec2 dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
            float dragDistance = sqrtf(dragDelta.x * dragDelta.x + dragDelta.y * dragDelta.y);
            
            if (dragDistance < 5.0f) { // 5 pixel tolerance
                selectFileNode(nodeHandle);
            }
            
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }
        
        // Select on right-click using captured state
        if (rightClicked) {
            selectFileNode(nodeHandle);
            ImGui::OpenPopup(("FileContext_" + std::to_string(nodeHandle.index)).c_str());
        }
        
        // Generic drag source using captured state
        if (dragStarted) {
            holdFileNode(nodeHandle);
            ImGui::SetDragDropPayload("FILE_HANDLE", &nodeHandle, sizeof(tinyHandle));
            ImGui::Text("%s", fileName.c_str());
            ImGui::EndDragDropSource();
        }
        
        // Spacebar keybind - instantiate selected scene into active hierarchy node
        if (isSelected && ImGui::IsKeyPressed(ImGuiKey_Space)) {
            // Check if this is a scene file
            if (node->tHandle.isType<tinySceneRT>()) {
                tinyHandle sceneRegistryHandle = node->tHandle.handle;
                
                // Determine target node: use previous scene node if we're on a file, otherwise current selection
                tinyHandle targetNodeHandle;
                if (selectedHandle.isFile()) {
                    // We're selecting a file, use the previous scene node from hierarchy
                    targetNodeHandle = prevSceneNode;
                    
                    // Fallback: if no previous scene node, use the active scene's root
                    if (!targetNodeHandle.valid()) {
                        tinySceneRT* activeScene = getActiveScene();
                        if (activeScene) {
                            targetNodeHandle = activeScene->rootHandle();
                        }
                    }
                } else if (selectedHandle.isScene()) {
                    // We're selecting a scene node directly
                    targetNodeHandle = selectedHandle.handle;
                }
                
                // Only instantiate if we have a valid target node
                if (targetNodeHandle.valid()) {
                    // Safety check: prevent instantiating a scene into itself
                    if (sceneRegistryHandle != getActiveSceneHandle()) {
                        // Verify the scene exists and instantiate it
                        const tinySceneRT* scene = fs.rGet<tinySceneRT>(sceneRegistryHandle);
                        if (scene) {
                            project->addSceneInstance(sceneRegistryHandle, getActiveSceneHandle(), targetNodeHandle);
                            // Auto-expand the parent chain to show the newly instantiated scene
                            expandParentChain(targetNodeHandle);
                        }
                    }
                }
            }
        }
        
        // Generic context menu with type-specific options
        if (ImGui::BeginPopup(("FileContext_" + std::to_string(nodeHandle.index)).c_str())) {
            ImGui::Text("%s", fileName.c_str());
            ImGui::Separator();
            
            // Type-specific context menu options
            if (node->tHandle.isType<tinySceneRT>()) {
                // Scene file options
                tinyHandle sceneRegistryHandle = node->tHandle.handle;
                bool isCurrentlyActive = (getActiveSceneHandle() == sceneRegistryHandle);
                
                if (isCurrentlyActive) {
                    ImGui::TextColored(ImVec4(0.7f, 1.0f, 0.7f, 1.0f), "Active Scene");
                } else {
                    if (ImGui::MenuItem("Make Active")) {
                        if (setActiveScene(sceneRegistryHandle)) {
                            selectSceneNode(activeSceneRootHandle());
                        }
                    }
                }
            } else if (node->tHandle.isType<tinyMeshVk>()) {
                // Mesh file options
                if (ImGui::MenuItem("Preview Mesh")) {
                    // TODO: Add mesh preview functionality
                }
            } else if (node->tHandle.isType<tinyTextureVk>()) {
                // Texture file options
                if (ImGui::MenuItem("Preview Texture")) {
                    // TODO: Add texture preview functionality
                }
            } else if (node->tHandle.isType<tinyMaterialVk>()) {
                // Material file options
                if (ImGui::MenuItem("Edit Material")) {
                    // TODO: Add material editor functionality
                }
            }
            // Add more file types as needed...
            
            ImGui::Separator();

            // Common delete option for all files
            if (ImGui::MenuItem("Delete", nullptr, false, node->deletable())) {
                tinyHandle parentHandle = node->parent;
                fs.fRemove(nodeHandle);
                if (selectedHandle.handle == nodeHandle) {
                    selectFileNode(parentHandle);
                }
            }
            
            ImGui::EndPopup();
        }
        
        ImGui::PopStyleColor(2);
    }
    
    // Clear held handle if no drag operation is active
    if (heldHandle.valid() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        clearHeld();
    }
    
    ImGui::PopID();
}

void tinyApp::renderFileDialog() {
    // Update the dialog state first
    fileDialog.update();
    
    // Only open the popup once when first requested, right before trying to begin it
    if (fileDialog.justOpened && !ImGui::IsPopupOpen("Load Model File")) {
        ImGui::OpenPopup("Load Model File");
        fileDialog.justOpened = false;
    }
    
    // Use a local variable for the open state to avoid ImGui modifying our state directly
    bool modalOpen = fileDialog.isOpen && !fileDialog.shouldClose;
    if (ImGui::BeginPopupModal("Load Model File", &modalOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Current path display
        ImGui::Text("Path: %s", fileDialog.currentPath.string().c_str());
        
        ImGui::Separator();
        
        // File list
        ImGui::BeginChild("FileList", ImVec2(600, 400), true);
        
        // Parent directory button
        if (fileDialog.currentPath.has_parent_path()) {
            if (ImGui::Selectable(".. (Parent Directory)", false)) {
                fileDialog.currentPath = fileDialog.currentPath.parent_path();
                fileDialog.refreshFileList();
                fileDialog.selectedFile.clear();
            }
        }
        
        // File and directory listing
        for (const auto& entry : fileDialog.currentFiles) {
            std::string fileName = entry.path().filename().string();
            bool isDirectory = entry.is_directory();
            bool isModelFile = !isDirectory && fileDialog.isModelFile(entry.path());
            
            // Color coding: directories in blue, model files in green, others in gray
            if (isDirectory) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f)); // Light blue
                fileName = "[DIR] " + fileName;
            } else if (isModelFile) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.5f, 1.0f)); // Light green
                fileName = "[MDL] " + fileName;
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f)); // Gray
                fileName = "[FILE] " + fileName;
            }
            
            bool isSelected = (fileDialog.selectedFile == entry.path().string());
            
            if (ImGui::Selectable(fileName.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (isDirectory) {
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        // Navigate into directory
                        fileDialog.currentPath = entry.path();
                        fileDialog.refreshFileList();
                        fileDialog.selectedFile.clear();
                    }
                } else if (isModelFile) {
                    fileDialog.selectedFile = entry.path().string();
                    
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        // Double-click to load model
                        loadModelFromPath(entry.path().string(), fileDialog.targetFolder);
                        fileDialog.close();
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            
            ImGui::PopStyleColor();
        }
        
        ImGui::EndChild();
        
        ImGui::Separator();
        
        // Selected file display
        if (!fileDialog.selectedFile.empty()) {
            std::filesystem::path selectedPath(fileDialog.selectedFile);
            ImGui::Text("Selected: %s", selectedPath.filename().string().c_str());
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No file selected");
        }
        
        ImGui::Separator();
        
        // Action buttons
        bool canLoad = !fileDialog.selectedFile.empty() && 
                        fileDialog.isModelFile(std::filesystem::path(fileDialog.selectedFile));
        
        if (ImGui::Button("Load", ImVec2(120, 0))) {
            if (canLoad) {
                loadModelFromPath(fileDialog.selectedFile, fileDialog.targetFolder);
                fileDialog.close();
                ImGui::CloseCurrentPopup();
            }
        }
        
        if (!canLoad) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "Please select a .glb or .gltf file");
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            fileDialog.close();
            ImGui::CloseCurrentPopup();
        }
        
        // Handle close button (X) or ESC key
        if (!modalOpen) {
            fileDialog.close();
        }

        ImGui::EndPopup();
    }
}

void tinyApp::renderLoadScriptDialog() {
    loadScriptDialog.update();
    
    // Only open the popup once when first requested
    if (loadScriptDialog.justOpened && !ImGui::IsPopupOpen("Load Lua Script")) {
        ImGui::OpenPopup("Load Lua Script");
        loadScriptDialog.justOpened = false;
    }
    
    bool modalOpen = loadScriptDialog.isOpen && !loadScriptDialog.shouldClose;
    if (ImGui::BeginPopupModal("Load Lua Script", &modalOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Path: %s", loadScriptDialog.currentPath.string().c_str());
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // File list
        ImGui::BeginChild("ScriptFileList", ImVec2(600, 400), true);
        
        // Parent directory navigation
        if (loadScriptDialog.currentPath.has_parent_path()) {
            if (ImGui::Selectable("[..]", false)) {
                loadScriptDialog.currentPath = loadScriptDialog.currentPath.parent_path();
                loadScriptDialog.refreshFileList();
                loadScriptDialog.selectedFile.clear();
            }
        }
        
        for (const auto& entry : loadScriptDialog.currentFiles) {
            bool isDirectory = entry.is_directory();
            bool isLuaFile = !isDirectory && loadScriptDialog.isLuaFile(entry.path());
            
            std::string fileName = entry.path().filename().string();
            
            if (isDirectory) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.5f, 1.0f)); // Yellow
                fileName = "[DIR] " + fileName;
            } else if (isLuaFile) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f)); // Light blue
                fileName = "[LUA] " + fileName;
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f)); // Gray
                fileName = "[FILE] " + fileName;
            }
            
            bool isSelected = (loadScriptDialog.selectedFile == entry.path().string());
            
            if (ImGui::Selectable(fileName.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (isDirectory) {
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        // Navigate into directory
                        loadScriptDialog.currentPath = entry.path();
                        loadScriptDialog.refreshFileList();
                        loadScriptDialog.selectedFile.clear();
                    }
                } else if (isLuaFile) {
                    loadScriptDialog.selectedFile = entry.path().string();
                    
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        // Double-click to load script
                        loadScriptFromPath(entry.path().string(), loadScriptDialog.targetFolder);
                        loadScriptDialog.close();
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            
            ImGui::PopStyleColor();
        }
        
        ImGui::EndChild();
        
        ImGui::Separator();
        
        // Selected file display
        if (!loadScriptDialog.selectedFile.empty()) {
            std::filesystem::path selectedPath(loadScriptDialog.selectedFile);
            ImGui::Text("Selected: %s", selectedPath.filename().string().c_str());
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No file selected");
        }
        
        ImGui::Separator();
        
        // Action buttons
        bool canLoad = !loadScriptDialog.selectedFile.empty() && 
                        loadScriptDialog.isLuaFile(std::filesystem::path(loadScriptDialog.selectedFile));
        
        if (ImGui::Button("Load", ImVec2(120, 0))) {
            if (canLoad) {
                loadScriptFromPath(loadScriptDialog.selectedFile, loadScriptDialog.targetFolder);
                loadScriptDialog.close();
                ImGui::CloseCurrentPopup();
            }
        }
        
        if (!canLoad) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "Please select a .lua file");
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            loadScriptDialog.close();
            ImGui::CloseCurrentPopup();
        }
        
        // Handle close button (X) or ESC key
        if (!modalOpen) {
            loadScriptDialog.close();
        }

        ImGui::EndPopup();
    }
}

void tinyApp::loadScriptFromPath(const std::string& filePath, tinyHandle targetFolder) {
    try {
        // Read the Lua script file content
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open file");
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string scriptCode = buffer.str();
        file.close();
        
        // Extract script name from file path (without .lua extension)
        std::filesystem::path path(filePath);
        std::string scriptName = path.stem().string();
        
        // Create tinyScript with the loaded code
        tinyScript script;
        script.name = scriptName;
        script.code = scriptCode;
        
        // Add to file system - addFile(parentFolder, name, data)
        tinyHandle scriptHandle = project->fs().addFile(targetFolder, scriptName, std::move(script));
        
        if (scriptHandle.valid()) {
            // Select the newly loaded script
            selectFileNode(scriptHandle);
            
            // Expand the target folder to show the new script
            expandFNode(targetFolder);
            expandFNodeParentChain(targetFolder);
        }
        
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to load script from path '") + filePath + "': " + e.what());
    }
}

void tinyApp::loadModelFromPath(const std::string& filePath, tinyHandle targetFolder) {
    try {
        // Load the model using tinyLoader
        tinyModel model = tinyLoader::loadModel(filePath);
        
        // Add the model to the project in the specified folder (returns model folder handle)
        tinyHandle modelFolderHandle = project->addModel(model, targetFolder);
        
        if (modelFolderHandle.valid()) {
            // Success! The model has been loaded and added to the project
            
            // Select the newly created model folder
            selectFileNode(modelFolderHandle);
            
            // Expand the target folder to show the newly imported model folder
            expandedFNodes.insert(targetFolder);
            
            // Also expand the parent chain of the target folder to ensure it's visible
            expandFNodeParentChain(targetFolder);
        }
        
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to load model from path '") + filePath + "': " + e.what());
    }
}

bool tinyApp::setActiveScene(tinyHandle sceneHandle) {
    // Verify the handle points to a valid tinySceneRT in the registry
    const tinySceneRT* scene = project->fs().rGet<tinySceneRT>(sceneHandle);
    if (!scene) return false; // Invalid handle or not a scene

    // Switch the active scene
    activeSceneHandle = sceneHandle;

    return true;
}

void tinyApp::renderAnimationEditorWindow() {
    tinySceneRT* activeScene = getActiveScene();
    if (!activeScene) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No active scene");
        return;
    }

    // Check if we have a valid component node selected
    if (!selectedCompNode.valid()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No component node selected");
        return;
    }

    const tinyNodeRT* compNode = activeScene->node(selectedCompNode);
    if (!compNode) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Invalid component node");
        selectedCompNode = tinyHandle();
        return;
    }

    bool hasAnimation = compNode->has<tinyNodeRT::ANIM3D>();
    bool hasScript = compNode->has<tinyNodeRT::SCRIPT>();

    if (!hasAnimation && !hasScript) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Node has no Animation or Script component");
        selectedCompNode = tinyHandle();
        return;
    }

    // Use the mode flag to determine which component to show
    if (compMode == CompMode::Animation && hasAnimation) {
        // Render animation editor
        renderAnimationEditor(activeScene, compNode);
    } else if (compMode == CompMode::Script && hasScript) {
        // Render script debug window
        renderScriptDebug(activeScene, compNode);
    } else {
        // Fallback: show animation if available, otherwise script
        if (hasAnimation) {
            renderAnimationEditor(activeScene, compNode);
        } else {
            renderScriptDebug(activeScene, compNode);
        }
    }
}

void tinyApp::renderAnimationEditor(tinySceneRT* activeScene, const tinyNodeRT* animNode) {
    tinyRT_ANIM3D* animComp = activeScene->rtComp<tinyNodeRT::ANIM3D>(selectedCompNode);
    if (!animComp) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Failed to get animation component");
        return;
    }

    // Get the selected animation
    if (!selectedAnimationHandle.valid()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No animation selected");
        ImGui::Text("Select an animation from the Animation component");
        return;
    }

    const tinyRT_ANIM3D::Clip* currentAnime = animComp->get(selectedAnimationHandle);
    if (!currentAnime) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Invalid animation selection");
        selectedAnimationHandle = tinyHandle();
        return;
    }

    // ===== HEADER =====
    ImGui::Text("Editing: %s", animNode->name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "> %s", currentAnime->name.c_str());
    
    ImGui::Separator();
    ImGui::Spacing();

    // ===== SINGLE LINE: PLAY/PAUSE, LOOP, SPEED, TIME =====
    // Get component state
    bool isPlaying = animComp->isPlaying();
    bool isLooping = animComp->getLoop();
    float currentTime = animComp->getTime();
    float currentSpeed = animComp->getSpeed();
    
    // Play/Pause button with fixed width to prevent size jumping
    if (isPlaying) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.5f, 0.1f, 1.0f));
        
        if (ImGui::Button("Pause", ImVec2(55, 0))) {
            animComp->pause();
        }
        
        ImGui::PopStyleColor(3);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.9f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
        
        if (ImGui::Button("Play ", ImVec2(55, 0))) {
            animComp->resume();
        }
        
        ImGui::PopStyleColor(3);
    }

    // Loop toggle button
    ImGui::SameLine();
    if (isLooping) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.9f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.4f, 0.7f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.2f, 0.7f));
    }
    
    if (ImGui::Button("Loop", ImVec2(50, 0))) {
        animComp->setLoop(!isLooping);
    }
    
    ImGui::PopStyleColor(3);

    // Speed control
    ImGui::SameLine();
    ImGui::Text("Speed:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##AnimSpeed", &currentSpeed, 0.01f, 0.0f, 0.0f, "%.2fx")) {
        animComp->setSpeed(currentSpeed);
    }

    // Time display and control
    ImGui::SameLine();
    ImGui::Text("Time:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    if (ImGui::DragFloat("##AnimTime", &currentTime, 0.01f, 0.0f, currentAnime->duration, "%.2f")) {
        currentTime = std::max(0.0f, std::min(currentTime, currentAnime->duration));
        animComp->setTime(currentTime);
    }
    
    ImGui::SameLine();
    ImGui::Text("/ %.2f s", currentAnime->duration);

    ImGui::Spacing();

    // ===== FULL WIDTH TIMELINE (DEDICATED LINE) =====
    float progress = currentAnime->duration > 0.0f ? (currentTime / currentAnime->duration) : 0.0f;
    
    ImVec2 progressBarPos = ImGui::GetCursorScreenPos();
    float timelineHeight = 12.0f; // Thinner timeline
    ImVec2 progressBarSize = ImVec2(ImGui::GetContentRegionAvail().x, timelineHeight);
    
    // Style the progress bar with blue colors to match the UI theme
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.5f, 0.8f, 1.0f)); // Blue progress
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 0.8f)); // Dark background
    
    ImGui::ProgressBar(progress, progressBarSize, "");
    
    ImGui::PopStyleColor(2);
    
    // Track drag state independently so dragging continues even when mouse leaves the bar
    static bool isDraggingTimeline = false;
    
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        isDraggingTimeline = true;
    }
    
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        isDraggingTimeline = false;
    }
    
    if (isDraggingTimeline) {
        ImVec2 mousePos = ImGui::GetMousePos();
        float relativeX = mousePos.x - progressBarPos.x;
        float normalizedPos = std::max(0.0f, std::min(1.0f, relativeX / progressBarSize.x));
        animComp->setTime(normalizedPos * currentAnime->duration);
    }
    
    ImGui::Spacing();
    ImGui::Separator();

    // ===== VERTICAL SPLITTER =====
    static float splitterPos = 0.6f; // 60% for channels, 40% for sampler details
    
    float totalWidth = ImGui::GetContentRegionAvail().x;
    float channelsWidth = totalWidth * splitterPos;
    float samplerWidth = totalWidth * (1.0f - splitterPos);
    
    // ===== LEFT SIDE: CHANNELS & TIMELINE =====
    ImGui::BeginChild("ChannelsTimeline", ImVec2(channelsWidth, 0), true);
    
    ImGui::Text("Channels (%zu)", currentAnime->channels.size());
    ImGui::Separator();
    
    // Scrollable channel list
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f);
    ImGui::BeginChild("ChannelList", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    
    for (size_t i = 0; i < currentAnime->channels.size(); ++i) {
        const auto& channel = currentAnime->channels[i];
        
        ImGui::PushID(static_cast<int>(i));
        
        bool isSelected = (selectedChannelIndex == static_cast<int>(i));
        
        // Channel header with selection highlight
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.6f, 0.8f, 0.8f));
        }
        
        // Determine channel name
        std::string channelName;
        std::string targetName = "Unknown";
        
        if (channel.node.valid()) {
            const tinyNodeRT* targetNode = activeScene->node(channel.node);
            if (targetNode) targetName = targetNode->name;
        }
        
        // Path type
        const char* pathStr = "?";
        switch (channel.path) {
            case tinyRT_ANIM3D::Channel::Path::T: pathStr = "Translation"; break;
            case tinyRT_ANIM3D::Channel::Path::R: pathStr = "Rotation"; break;
            case tinyRT_ANIM3D::Channel::Path::S: pathStr = "Scale"; break;
            case tinyRT_ANIM3D::Channel::Path::W: pathStr = "Weight"; break;
        }
        
        // Target type
        const char* targetStr = "";
        switch (channel.target) {
            case tinyRT_ANIM3D::Channel::Target::Node: targetStr = "Node"; break;
            case tinyRT_ANIM3D::Channel::Target::Bone: targetStr = "Bone"; break;
            case tinyRT_ANIM3D::Channel::Target::Morph: targetStr = "Morph"; break;
        }
        
        channelName = std::string(targetStr) + " [" + targetName + "] - " + pathStr;
        if (channel.target == tinyRT_ANIM3D::Channel::Target::Morph || 
            channel.target == tinyRT_ANIM3D::Channel::Target::Bone) {
            channelName += " #" + std::to_string(channel.index);
        }
        
        // Selectable channel
        if (ImGui::Selectable(channelName.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
            selectedChannelIndex = static_cast<int>(i);
        }
        
        if (isSelected) {
            ImGui::PopStyleColor();
        }
        
        ImGui::PopID();
    }
    
    ImGui::EndChild();
    ImGui::PopStyleVar();
    
    ImGui::EndChild();
    
    // ===== VERTICAL SPLITTER =====
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.4f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.7f, 0.7f, 0.8f));
    
    ImGui::Button("##VerticalSplitter", ImVec2(4, -1));
    
    if (ImGui::IsItemActive()) {
        float mouseDelta = ImGui::GetIO().MouseDelta.x;
        splitterPos += mouseDelta / totalWidth;
        splitterPos = std::max(0.3f, std::min(0.8f, splitterPos));
    }
    
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine();
    
    // ===== RIGHT SIDE: SAMPLER DETAILS =====
    ImGui::BeginChild("SamplerDetails", ImVec2(samplerWidth, 0), true);
    
    if (selectedChannelIndex >= 0 && selectedChannelIndex < static_cast<int>(currentAnime->channels.size())) {
        const auto& channel = currentAnime->channels[selectedChannelIndex];
        
        ImGui::Text("Sampler Details");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Get the sampler
        if (channel.sampler < currentAnime->samplers.size()) {
            auto& sampler = const_cast<tinyRT_ANIM3D::Sampler&>(currentAnime->samplers[channel.sampler]);
            
            // Sampler info
            ImGui::Text("Sampler Index: %u", channel.sampler);
            
            // Interpolation mode
            const char* interpStr = "Linear";
            switch (sampler.interp) {
                case tinyRT_ANIM3D::Sampler::Interp::Linear: interpStr = "Linear"; break;
                case tinyRT_ANIM3D::Sampler::Interp::Step: interpStr = "Step"; break;
                case tinyRT_ANIM3D::Sampler::Interp::CubicSpline: interpStr = "Cubic Spline"; break;
            }
            ImGui::Text("Interpolation: %s", interpStr);
            
            ImGui::Text("Keyframes: %zu", sampler.times.size());
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Keyframe Editor");
            ImGui::Separator();
            
            // Scrollable keyframe list
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.0f);
            ImGui::BeginChild("KeyframeList", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            
            for (size_t k = 0; k < sampler.times.size(); ++k) {
                ImGui::PushID(static_cast<int>(k));
                
                ImGui::Text("Keyframe %zu", k);
                
                // Time
                ImGui::Text("Time:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat("##KeyTime", &sampler.times[k], 0.01f, 0.0f, currentAnime->duration, "%.3f s");
                
                // Value (vec4)
                ImGui::Text("Value:");
                ImGui::SetNextItemWidth(-1);
                ImGui::DragFloat4("##KeyValue", &sampler.values[k].x, 0.01f, -100.0f, 100.0f, "%.3f");
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                ImGui::PopID();
            }
            
            ImGui::EndChild();
            ImGui::PopStyleVar();
            
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Invalid sampler index");
        }
        
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No channel selected");
        ImGui::Text("Select a channel from the list to edit its sampler");
    }
    
    ImGui::EndChild();
}

void tinyApp::renderScriptDebug(tinySceneRT* activeScene, const tinyNodeRT* scriptNode) {
    tinyRT::Script* rtScript = activeScene->rtComp<tinyNodeRT::SCRIPT>(selectedCompNode);
    if (!rtScript) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Failed to get script component");
        return;
    }

    // ===== HEADER =====
    ImGui::Text("Script Debug: %s", scriptNode->name.c_str());
    
    const tinyScript* script = rtScript->rScript();
    if (script) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "> %s", script->name.c_str());
    }
    
    ImGui::Separator();
    ImGui::Spacing();

    // ===== DEBUG OUTPUT =====
    ImGui::Text("Runtime Debug Logs:");
    ImGui::SameLine();
    
    // Clear button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
    
    if (ImGui::Button("Clear##RuntimeDebug")) {
        rtScript->debug().clear();
    }
    
    ImGui::PopStyleColor(3);
    ImGui::Separator();
    
    ImGui::BeginChild("ScriptDebugOutput", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    
    const auto& logs = rtScript->debug().logs();
    if (logs.empty()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No debug logs");
    } else {
        for (const auto& log : logs) {
            ImGui::TextColored(ImVec4(log.color[0], log.color[1], log.color[2], 1.0f), "%s", log.str.c_str());
        }
        
        // Auto-scroll to bottom
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }
    
    ImGui::EndChild();
}

void tinyApp::renderScriptEditorWindow() {
    tinyFS& fs = project->fs();
    
    // Check if we have a valid script selected
    if (!selectedScriptHandle.valid()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "No script selected");
        return;
    }
    
    // Get the script from registry
    tinyScript* script = fs.rGet<tinyScript>(selectedScriptHandle);
    if (!script) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid script handle");
        ImGui::Text("The selected script no longer exists.");
        selectedScriptHandle = tinyHandle();
        return;
    }
    
    // ===== HEADER =====
    ImGui::Text("Editing Script: %s [", script->name.c_str());
    ImGui::SameLine();
    if (script->valid()) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Compiled");
        ImGui::SameLine();
        ImGui::Text("(v%u)", script->version());
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Not Compiled");
    }
    ImGui::SameLine();
    ImGui::Text("]");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Static buffer to hold the code during editing
    // Use a map to store separate char buffers for each script (16KB per script)
    static std::unordered_map<uint64_t, std::array<char, 16384>> scriptCodeBuffers;
    
    // Get unique key for this script handle
    uint64_t scriptKey = (static_cast<uint64_t>(selectedScriptHandle.index) << 32) | selectedScriptHandle.version;
    
    // Initialize buffer if we don't have one for this script yet
    if (scriptCodeBuffers.find(scriptKey) == scriptCodeBuffers.end()) {
        std::array<char, 16384> newBuffer = {};
        strncpy_s(newBuffer.data(), newBuffer.size(), script->code.c_str(), newBuffer.size() - 1);
        newBuffer[newBuffer.size() - 1] = '\0';
        scriptCodeBuffers[scriptKey] = newBuffer;
    }
    
    // Get reference to this script's buffer
    auto& codeBuffer = scriptCodeBuffers[scriptKey];
    
    // Static variable to store splitter position (persists between frames)
    static float splitterPos = 0.7f; // Start with 70% for code, 30% for debug
    
    // Get full available space
    float totalHeight = ImGui::GetContentRegionAvail().y;
    
    // Reserve space for UI elements
    float buttonHeight = ImGui::GetFrameHeight(); // Height of the compile button
    float textHeight = ImGui::GetTextLineHeightWithSpacing(); // Height of "Debug Output:" text
    float splitterHeight = 4.0f; // Height of the splitter
    float spacing = ImGui::GetStyle().ItemSpacing.y * 4; // Account for spacing between elements
    float reservedSpace = buttonHeight + splitterHeight + textHeight + spacing;
    
    // Calculate section heights based on splitter position
    float availableHeight = totalHeight - reservedSpace;
    float codeHeight = availableHeight * splitterPos;
    float debugHeight = availableHeight * (1.0f - splitterPos);
    
    // ===== LUA CODE EDITOR =====
    
    // Compile button on the same line
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.7f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.5f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.2f, 1.0f));

    if (ImGui::Button("Compile", ImVec2(80, 0))) {
        // Update script code from buffer before compiling
        script->code = std::string(codeBuffer.data());
        
        // Compile the script - errors will be displayed in the debug panel
        script->compile();
    }
    ImGui::PopStyleColor(3);
    ImGui::Separator();

    // Multi-line text editor with scrolling - transparent background
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Transparent background
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.1f, 0.1f, 0.1f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.4f, 0.4f, 0.4f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    
    // Multi-line text editor - now properly editable!
    ImGui::InputTextMultiline("##ScriptCode", codeBuffer.data(), codeBuffer.size(), 
        ImVec2(-1, codeHeight),
        ImGuiInputTextFlags_AllowTabInput);
    
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);
    
    // ===== HORIZONTAL SPLITTER =====
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.4f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.7f, 0.7f, 0.8f));
    
    ImGui::Button("##HorizontalSplitter", ImVec2(-1, 4));
    
    if (ImGui::IsItemActive()) {
        float delta = ImGui::GetIO().MouseDelta.y / availableHeight;
        splitterPos += delta;
        splitterPos = std::clamp(splitterPos, 0.2f, 0.9f); // Limit between 20% and 90%
    }
    
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    
    ImGui::PopStyleColor(3);
    
    // ===== DEBUG PANEL =====
    ImGui::Text("Debug Output:");
    ImGui::SameLine();
    
    // Clear button for compilation logs
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
    
    if (ImGui::Button("Clear##CompilationDebug")) {
        script->debug().clear();
    }
    
    ImGui::PopStyleColor(3);
    
    // Debug panel with scrolling - transparent background
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Transparent background
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.1f, 0.1f, 0.1f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.4f, 0.4f, 0.4f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    
    ImGui::BeginChild("DebugOutput", ImVec2(-1, debugHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    
    // Display compilation debug logs
    const auto& logs = script->debug().logs();
    if (logs.empty()) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No compilation logs");
    } else {
        for (const auto& log : logs) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(log.color[0], log.color[1], log.color[2], 1.0f));
            ImGui::TextWrapped("%s", log.str.c_str());
            ImGui::PopStyleColor();
        }
    }

    ImGui::EndChild();
    
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);
}