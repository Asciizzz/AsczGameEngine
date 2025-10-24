#include "TinyApp/TinyApp.hpp"

#include "TinyEngine/TinyLoader.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <string>

using namespace TinyVK;

void TinyApp::setupImGuiWindows(const TinyChrono& fpsManager, const TinyCamera& camera, bool mouseFocus, float deltaTime) {
    // Clear any existing windows first
    imguiWrapper->clearWindows();
    
    // Main Editor Window - full size, no scroll bars
    const TinyFS& fs = project->fs();

    imguiWrapper->addWindow("Editor", [this, &camera, &fs]() {
        // Static variable to store splitter position (persists between frames)
        static float splitterPos = 0.5f; // Start with 50% for hierarchy, 50% for file explorer
        
        // Get full available space
        float totalHeight = ImGui::GetContentRegionAvail().y;
        
        // Calculate section heights based on splitter position
        float hierarchyHeight = totalHeight * splitterPos;
        float explorerHeight = totalHeight * (1.0f - splitterPos);
        
        // =============================================================================
        // HIERARCHY SECTION (TOP)
        // =============================================================================
        
        // Active Scene Name Header
        TinyScene* activeScene = getActiveScene();
        if (activeScene) {
            ImGui::Text("%s", activeScene->name.c_str());
            
            // Show full scene info on hover
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Scene: %s", activeScene->name.c_str());
                ImGui::Text("Total Nodes: %u", activeScene->nodeCount());
                ImGui::EndTooltip();
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.7f, 1.0f), "No Active Scene");
        }
        
        ImGui::Separator();
        
        // Hierarchy Tree with background
        // Apply thin scrollbar styling to match component inspector
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f); // Thinner scrollbar
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 4.0f); // Rounded scrollbar
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.1f, 0.1f, 0.1f, 0.5f)); // Darker background
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.4f, 0.4f, 0.4f, 0.8f)); // Visible grab
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f)); // Hover state
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.6f, 0.6f, 0.6f, 1.0f)); // Active state
        
        ImGui::BeginChild("Hierarchy", ImVec2(0, hierarchyHeight - 30), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        // Clear held node when mouse is released and no dragging is happening
        if (heldHandle.valid() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            clearHeld();
        }
        
        if (activeScene && activeScene->nodeCount() > 0) {
            renderNodeTreeImGui();
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No active scene");
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Drag scenes here to create instances");
        }
        ImGui::EndChild();
        
        // Pop the scrollbar styling
        ImGui::PopStyleColor(4); // ScrollbarBg, ScrollbarGrab, ScrollbarGrabHovered, ScrollbarGrabActive
        ImGui::PopStyleVar(2); // ScrollbarSize, ScrollbarRounding
        
        // =============================================================================
        // HORIZONTAL SPLITTER
        // =============================================================================
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 0.4f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.7f, 0.7f, 0.8f));
        
        ImGui::Button("##HorizontalSplitter", ImVec2(-1, 4));
        
        if (ImGui::IsItemActive()) {
            float mouseDelta = ImGui::GetIO().MouseDelta.y;
            splitterPos += mouseDelta / totalHeight;
            splitterPos = std::max(0.2f, std::min(0.8f, splitterPos)); // Clamp between 20% and 80%
        }
        
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
        
        ImGui::PopStyleColor(3);
        
        // =============================================================================
        // FILE EXPLORER SECTION (BOTTOM)
        // =============================================================================
        
        ImGui::Text("File Explorer");
        ImGui::Separator();
        
        // File Explorer with background
        // Apply thin scrollbar styling to match component inspector
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 8.0f); // Thinner scrollbar
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 4.0f); // Rounded scrollbar
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.1f, 0.1f, 0.1f, 0.5f)); // Darker background
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.4f, 0.4f, 0.4f, 0.8f)); // Visible grab
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f)); // Hover state
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImVec4(0.6f, 0.6f, 0.6f, 1.0f)); // Active state
        
        ImGui::BeginChild("FileExplorer", ImVec2(0, explorerHeight - 30), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        
        renderFileExplorerImGui();
        
        // Render the file dialog once per frame, outside the file explorer tree
        renderFileDialog();

        ImGui::EndChild();
        
        // Pop the scrollbar styling
        ImGui::PopStyleColor(4); // ScrollbarBg, ScrollbarGrab, ScrollbarGrabHovered, ScrollbarGrabActive
        ImGui::PopStyleVar(2); // ScrollbarSize, ScrollbarRounding
        
    });  // End Editor window
    
    // Debug Panel Window (smaller, optional)
    imguiWrapper->addWindow("Debug Panel", [this, &fpsManager, &camera, mouseFocus, deltaTime]() {
        // FPS and Performance (update display only every second)
        static auto lastFPSUpdate = std::chrono::steady_clock::now();
        static float displayFPS = 0.0f;
        static float displayFrameTime = 0.0f;
        static float displayAvgFPS = 0.0f;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFPSUpdate);
        
        if (elapsed.count() >= 1000) { // Update every 1000ms (1 second)
            displayFPS = fpsManager.currentFPS;
            displayFrameTime = fpsManager.frameTimeMs;
            displayAvgFPS = fpsManager.getAverageFPS();
            lastFPSUpdate = now;
        }
        
        ImGui::Text("Performance");
        ImGui::Separator();
        ImGui::Text("FPS: %.1f (%.2f ms)", displayFPS, displayFrameTime);
        ImGui::Text("Avg FPS: %.1f", displayAvgFPS);
        ImGui::Text("Delta Time: %.4f s", deltaTime);
        
        ImGui::Spacing();
        
        // Camera Information
        ImGui::Text("Camera");
        ImGui::Separator();
        ImGui::Text("Position: (%.2f, %.2f, %.2f)", camera.pos.x, camera.pos.y, camera.pos.z);
        ImGui::Text("Forward: (%.2f, %.2f, %.2f)", camera.forward.x, camera.forward.y, camera.forward.z);
        ImGui::Text("Right: (%.2f, %.2f, %.2f)", camera.right.x, camera.right.y, camera.right.z);
        ImGui::Text("Up: (%.2f, %.2f, %.2f)", camera.up.x, camera.up.y, camera.up.z);
        ImGui::Text("Yaw: %.2f° | Pitch: %.2f° | Roll: %.2f°", 
                   camera.getYaw(true) * 57.2958f, // Convert radians to degrees
                   camera.getPitch(true) * 57.2958f,
                   camera.getRoll() * 57.2958f);

        ImGui::Spacing();
        
        // Window controls
        ImGui::Text("Windows");
        ImGui::Separator();
        ImGui::Checkbox("Show Inspector", &showInspectorWindow);
        ImGui::Checkbox("Show Editor Settings", &showEditorSettingsWindow);
    }, &showDebugWindow);
    
    // Inspector Window
    imguiWrapper->addWindow("Inspector", [this]() {
        renderInspectorWindow();
    }, &showInspectorWindow);
    
    // Editor Settings Window
    imguiWrapper->addWindow("Editor Settings", [this]() {
        ImGui::Text("UI & Display");
        ImGui::Separator();
        
        // Font scaling for better readability
        static float fontScale = 1.0f;
        ImGui::Text("Font Scale");
        if (ImGui::SliderFloat("##FontScale", &fontScale, 0.5f, 3.0f, "%.1fx")) {
            ImGui::GetIO().FontGlobalScale = fontScale;
        }

        ImGui::Spacing();
        
        // Quick preset buttons
        ImGui::Text("Presets:");
        if (ImGui::Button("Small##FontPreset")) {
            fontScale = 0.8f;
            ImGui::GetIO().FontGlobalScale = fontScale;
        }
        ImGui::SameLine();
        if (ImGui::Button("Normal##FontPreset")) {
            fontScale = 1.0f;
            ImGui::GetIO().FontGlobalScale = fontScale;
        }
        ImGui::SameLine();
        if (ImGui::Button("Large##FontPreset")) {
            fontScale = 1.5f;
            ImGui::GetIO().FontGlobalScale = fontScale;
        }
        ImGui::SameLine();
        if (ImGui::Button("XL##FontPreset")) {
            fontScale = 2.0f;
            ImGui::GetIO().FontGlobalScale = fontScale;
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Future settings placeholder
        ImGui::TextDisabled("More settings will be added here...");
        ImGui::TextDisabled("• Theme selection");
        ImGui::TextDisabled("• Window layout presets"); 
        ImGui::TextDisabled("• Performance options");
        ImGui::TextDisabled("• Keybind customization");
    }, &showEditorSettingsWindow);
}

void TinyApp::renderInspectorWindow() {
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

void TinyApp::renderSceneNodeInspector() {
    const TinyFS& fs = project->fs();

    TinyScene* activeScene = getActiveScene();
    if (!activeScene) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No active scene");
        return;
    }

    // Get the selected scene node handle from unified selection
    TinyHandle selectedSceneNodeHandle = getSelectedSceneNode();
    if (!selectedSceneNodeHandle.valid()) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "No scene node selected");
        ImGui::Text("This should not happen in unified selection.");
        return;
    }

    const TinyNode* selectedNode = activeScene->node(selectedSceneNodeHandle);
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
    static TinyHandle lastSelectedNode;
    
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
        const TinyNode* parentNode = activeScene->node(selectedNode->parentHandle);
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
    
    // Render Transform component (always present, no delete button)
    if (selectedNode->has<TinyNode::Transform>()) {
        renderComponent("Transform", ImVec4(0.2f, 0.2f, 0.15f, 0.8f), ImVec4(0.4f, 0.4f, 0.3f, 0.6f), true, [&]() {
            {
                TinyNode::Transform* compPtr = activeScene->nodeComp<TinyNode::Transform>(selectedSceneNodeHandle);

                glm::mat4 local = compPtr->local;

                // Extract translation, rotation, and scale from the matrix
                glm::vec3 translation, rotation, scale;
                glm::quat rotationQuat;
                glm::vec3 skew;
                glm::vec4 perspective;
                
                // Check if decomposition is valid
                bool validDecomposition = glm::decompose(local, scale, rotationQuat, translation, skew, perspective);
                
                if (!validDecomposition) {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Warning: Invalid transform matrix detected!");
                    if (ImGui::Button("Reset Transform")) {
                        compPtr->local = glm::mat4(1.0f);
                        activeScene->update(selectedSceneNodeHandle);
                    }
                    return;
                }
                
                // Validate extracted values for NaN/infinity
                auto validVec3 = [](const glm::vec3& v) {
                    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
                };
                
                if (!validVec3(translation) || !validVec3(scale)) {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Warning: NaN/Infinite values detected!");
                    if (ImGui::Button("Reset Transform")) {
                        compPtr->local = glm::mat4(1.0f);
                        activeScene->update(selectedSceneNodeHandle);
                    }
                    return;
                }
                
                // Clamp scale to prevent zero/negative values
                const float MIN_SCALE = 0.001f;
                scale.x = std::max(MIN_SCALE, std::abs(scale.x));
                scale.y = std::max(MIN_SCALE, std::abs(scale.y));
                scale.z = std::max(MIN_SCALE, std::abs(scale.z));
                
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
                
                ImGui::Spacing();
                
                // Translation controls
                ImGui::Text("Position");
                ImGui::DragFloat3("##Position", &translation.x, 0.01f, -1000.0f, 1000.0f, "%.3f");
                ImGui::SameLine();
                if (ImGui::Button("To Cam")) {
                    translation = project->getCamera()->pos;
                }
                
                // Rotation controls
                ImGui::Text("Rotation (degrees)");
                ImGui::DragFloat3("##Rotation", &rotation.x, 0.5f, -180.0f, 180.0f, "%.1f°");
                
                // Scale controls
                ImGui::Text("Scale");
                ImGui::DragFloat3("##Scale", &scale.x, 0.01f, MIN_SCALE, 10.0f, "%.3f");
                ImGui::SameLine();
                if (ImGui::Button("Uniform")) {
                    float avgScale = (scale.x + scale.y + scale.z) / 3.0f;
                    scale = glm::vec3(avgScale);
                }
                
                // Apply changes if any values changed (copy->modify->reapply pattern)
                if (translation != originalTranslation || rotation != originalRotation || scale != originalScale) {
                    if (validVec3(translation) && validVec3(scale)) {
                        // Convert back to quaternion and reconstruct matrix
                        glm::quat newRotQuat = glm::quat(glm::radians(rotation));
                        
                        // Create transform matrix: T * R * S
                        glm::mat4 translateMat = glm::translate(glm::mat4(1.0f), translation);
                        glm::mat4 rotateMat = glm::mat4_cast(newRotQuat);
                        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);

                        compPtr->local = translateMat * rotateMat * scaleMat;
                        activeScene->update(selectedSceneNodeHandle); // Only update this node
                    }
                }
                
                ImGui::Spacing();
            }
        }, [&]() {
            // Remove component using TinyScene method
            activeScene->removeComp<TinyNode::Transform>(selectedSceneNodeHandle);
        });
    } else {
        // Show grayed-out placeholder for missing Transform component
        renderComponent("Transform", ImVec4(0.05f, 0.05f, 0.05f, 0.3f), ImVec4(0.15f, 0.15f, 0.15f, 0.3f), false, [&]() {
            // Minimal placeholder - no content, just the header with add button
        }, [&]() {
            // Add component using TinyScene method
            activeScene->writeComp<TinyNode::Transform>(selectedSceneNodeHandle);
        });
    }
    
    // Mesh Renderer Component - Always show
    if (selectedNode->has<TinyNode::MeshRender>()) {
        renderComponent("Mesh Renderer", ImVec4(0.15f, 0.15f, 0.2f, 0.8f), ImVec4(0.3f, 0.3f, 0.4f, 0.6f), true, [&]() {
            // Get component copy using TinyScene method
            TinyNode::MeshRender* compPtr = activeScene->nodeComp<TinyNode::MeshRender>(selectedSceneNodeHandle);
            bool componentModified = false;
            
            ImGui::Spacing();
            
            // Mesh Handle field with enhanced drag-drop support
            ImGui::Text("Mesh Resource:");

            // Check if mesh resource is valid
            bool meshModified = renderHandleField("##MeshHandle", compPtr->pMeshHandle, "Mesh",
                "Drag a mesh file from the File Explorer", 
                "Select mesh resource for rendering");
            if (meshModified) componentModified = true;

            // Show mesh information if valid
            if (compPtr->pMeshHandle.valid()) {
                const TinyMesh* mesh = fs.rGet<TinyMesh>(compPtr->pMeshHandle);
                if (mesh) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", mesh->name.c_str());
                } else {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Invalid");
                }
            }
            
            ImGui::Spacing();
            
            // Skeleton Node Handle field with enhanced drag-drop support  
            ImGui::Text("Skeleton Node:");
            bool skeleModified = renderHandleField("##MeshRenderer_SkeletonNodeHandle", compPtr->skeleNodeHandle, "SkeletonNode",
                "Drag a skeleton node from the Hierarchy",
                "Select skeleton node for bone animation");
            if (skeleModified) componentModified = true;

            // Show skeleton node information if valid
            if (compPtr->skeleNodeHandle.valid()) {
                const TinyNode* skeleNode = activeScene->node(compPtr->skeleNodeHandle);
                if (skeleNode && skeleNode->has<TinyNode::Skeleton>()) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", skeleNode->name.c_str());
                } else {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Invalid/No Skeleton");
                }
            }
            
            ImGui::Spacing();
        }, [&]() {
            // Remove component using TinyScene method
            activeScene->removeComp<TinyNode::MeshRender>(selectedSceneNodeHandle);
        });
    } else {
        // Show grayed-out placeholder for missing Mesh Renderer component
        renderComponent("Mesh Renderer", ImVec4(0.05f, 0.05f, 0.05f, 0.3f), ImVec4(0.15f, 0.15f, 0.15f, 0.3f), false, [&]() {
            // Minimal placeholder - no content, just the header with add button
        }, [&]() {
            // Add component using TinyScene method
            activeScene->writeComp<TinyNode::MeshRender>(selectedSceneNodeHandle);
        });
    }
    
    // Bone Attachment Component - Always show
    if (selectedNode->has<TinyNode::BoneAttach>()) {
        renderComponent("Bone Attachment", ImVec4(0.15f, 0.2f, 0.15f, 0.8f), ImVec4(0.3f, 0.4f, 0.3f, 0.6f), true, [&]() {
            // Get component copy using TinyScene method
            TinyNode::BoneAttach* compPtr = activeScene->nodeComp<TinyNode::BoneAttach>(selectedSceneNodeHandle);
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
                const TinyNode* skeleNode = activeScene->node(compPtr->skeleNodeHandle);
                if (skeleNode && skeleNode->has<TinyNode::Skeleton>()) {
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
                const TinyNode* skeleNode = activeScene->node(compPtr->skeleNodeHandle);
                if (skeleNode && skeleNode->has<TinyNode::Skeleton>()) {
                    const TinyNode::Skeleton* skeleComp = skeleNode->get<TinyNode::Skeleton>();
                    if (skeleComp) {
                        // Retrieve runtime skeleton data
                        const TinySkeletonRT* rtSkeleton = activeScene->rtGet<TinySkeletonRT>(skeleComp->pSkeleHandle);
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
            
            // Show validation status
            ImGui::SameLine();
            if (boneIndex <= maxBoneIndex && compPtr->skeleNodeHandle.valid()) {
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "✓");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Valid bone index (%d/%d)", boneIndex, maxBoneIndex);
                }
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "✗");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Invalid bone index (max: %d)", maxBoneIndex);
                }
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
        }, [&]() {
            // Remove component using TinyScene method
            activeScene->removeComp<TinyNode::BoneAttach>(selectedSceneNodeHandle);
        });
    } else {
        // Show grayed-out placeholder for missing Bone Attachment component
        renderComponent("Bone Attachment", ImVec4(0.05f, 0.05f, 0.05f, 0.3f), ImVec4(0.15f, 0.15f, 0.15f, 0.3f), false, [&]() {
            // Minimal placeholder - no content, just the header with add button
        }, [&]() {
            // Add component using TinyScene method
            activeScene->writeComp<TinyNode::BoneAttach>(selectedSceneNodeHandle);
        });
    }
    
    // Skeleton Component - Always show
    if (selectedNode->has<TinyNode::Skeleton>()) {
        renderComponent("Skeleton", ImVec4(0.2f, 0.15f, 0.15f, 0.8f), ImVec4(0.4f, 0.3f, 0.3f, 0.6f), true, [&]() {
            // Retrieve the component (using TinyScene special method which return appropriate runtime/static data)
            TinySkeletonRT* rtSkeleComp = activeScene->nodeComp<TinyNode::Skeleton>(selectedSceneNodeHandle);
            bool hasSkeleton = rtSkeleComp->hasSkeleton();

            ImGui::Spacing();
            // Skeleton Handle field - always edit the static skeleton handle
            ImGui::Text("Skeleton Resource:");
            bool skeleModified = renderHandleField("##SkeletonHandle", rtSkeleComp->skeleHandle(), "Skeleton",
                "Drag a skeleton file from the File Explorer",
                "Select skeleton resource for bone data");

            // Show skeleton information if valid
            const TinySkeleton* skeleton = rtSkeleComp->skeleton();
            if (skeleton) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s (%zu bones)", skeleton->name.c_str(), skeleton->bones.size());
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
                static TinyHandle lastSkeletonHandle;
                
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
                    
                    const TinyBone& bone = skeleton->bones[boneIndex];
                    
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
                    
                    // Show bone tooltip with details
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Bone Index: %d", boneIndex);
                        ImGui::Text("Name: %s", bone.name.c_str());
                        ImGui::Text("Parent: %d", bone.parent);
                        ImGui::Text("Children: %zu", bone.children.size());
                        ImGui::EndTooltip();
                    }
                    
                    // If node is open and has children, render children
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
                    
                    const TinyBone& selectedBone = skeleton->bones[selectedBoneIndex];
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
                            rtSkeleComp->update();
                        }
                    }
                    
                } else {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Select a bone to edit its transformation");
                }
            }
            
            ImGui::Spacing();
        }, [&]() {
            // Remove component using TinyScene specialized method for skeleton
            activeScene->removeComp<TinyNode::Skeleton>(selectedSceneNodeHandle);
        });
    } else {
        // Show grayed-out placeholder for missing Skeleton component
        renderComponent("Skeleton", ImVec4(0.05f, 0.05f, 0.05f, 0.3f), ImVec4(0.15f, 0.15f, 0.15f, 0.3f), false, [&]() {
            // Minimal placeholder - no content, just the header with add button
        }, [&]() {
            // Add empty skeleton component using TinyScene method
            activeScene->writeComp<TinyNode::Skeleton>(selectedSceneNodeHandle);
        });
    }
    
    ImGui::EndChild();
    
    // Pop the scrollbar style customizations
    ImGui::PopStyleColor(4); // Pop ScrollbarBg, ScrollbarGrab, ScrollbarGrabHovered, ScrollbarGrabActive
    ImGui::PopStyleVar(2); // Pop ScrollbarSize, ScrollbarRounding
}




void TinyApp::renderFileSystemInspector() {
    const TinyFS& fs = project->fs();
    
    // Get the selected file node handle from unified selection
    TinyHandle selectedFNodeHandle = getSelectedFileNode();
    if (!selectedFNodeHandle.valid()) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "No file selected");
        ImGui::Text("This should not happen in unified selection.");
        return;
    }
    
    const TinyFS::Node* selectedFNode = fs.fNode(selectedFNodeHandle);
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
        static TinyHandle lastSelectedFNode;
        
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
            TinyFS::Node* mutableNode = const_cast<TinyFS::Node*>(fs.fNode(selectedFNodeHandle));
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
        TypeHandle tHandle = selectedFNode->tHandle;

        if (tHandle.isType<TinyScene>()) {
            fileType = "Scene";
            ImGui::Text("Type: %s", fileType.c_str());

            // Extended data
            const TinyScene* scene = fs.rGet<TinyScene>(tHandle.handle);
            if (scene) {
                ImGui::Text("Scene Nodes: %u", scene->nodeCount());
                
                // Make Active button
                ImGui::Spacing();
                TinyHandle sceneRegistryHandle = tHandle.handle;
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
        } else if (tHandle.isType<TinyTexture>()) {
            fileType = "Texture";
            ImGui::Text("Type: %s", fileType.c_str());

            // Extended data
            const TinyTexture* texture = fs.registry().get<TinyTexture>(tHandle);
            if (texture) {
                ImGui::Text("Dimensions: %dx%d", texture->width, texture->height);
                ImGui::Text("Channels: %d", texture->channels);
                ImGui::Text("Hash: %u", texture->hash);
            }
        } else if (tHandle.isType<TinyRMaterial>()) {
            fileType = "Material";
            ImGui::Text("Type: %s", fileType.c_str());
        } else if (tHandle.isType<TinyMesh>()) {
            fileType = "Mesh";
            ImGui::Text("Type: %s", fileType.c_str());
        } else if (tHandle.isType<TinySkeleton>()) {
            fileType = "Skeleton";
            ImGui::Text("Type: %s", fileType.c_str());
        } else {
            ImGui::Text("Type: %s", fileType.c_str());
        }
    }
}

void TinyApp::renderComponent(const char* componentName, ImVec4 backgroundColor, ImVec4 borderColor, bool showRemoveButton, std::function<void()> renderContent, std::function<void()> onRemove) {
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

bool TinyApp::renderHandleField(const char* fieldId, TinyHandle& handle, const char* targetType, const char* dragTooltip, const char* description) {
    bool modified = false;
    
    // Create enhanced drag-drop target area with better styling
    std::string displayText;
    ImVec4 buttonColor, hoveredColor, activeColor;

    const TinyFS& fs = project->fs();
    
    if (handle.valid()) {
        // Get the actual name based on target type
        if (strcmp(targetType, "Mesh") == 0) {
            const TinyMesh* mesh = fs.rGet<TinyMesh>(handle);
            displayText = mesh ? mesh->name : "Unknown Mesh";
        } else if (strcmp(targetType, "Skeleton") == 0) {
            const TinySkeleton* skeleton = fs.rGet<TinySkeleton>(handle);
            displayText = skeleton ? skeleton->name : "Unknown Skeleton";
        } else if (strcmp(targetType, "SkeletonNode") == 0) {
            TinyScene* activeScene = getActiveScene();
            if (activeScene) {
                const TinyNode* node = activeScene->node(handle);
                displayText = node ? node->name : "Unknown Node";
            } else {
                displayText = "No Node";
            }
        } else {
            displayText = "Unknown Resource";
        }
        
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
            handle = TinyHandle();
            modified = true;
        }
    }
    
    ImGui::PopStyleColor(3);
    
    // Show appropriate tooltip based on state
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        if (handle.valid()) {
            ImGui::Text("Click to clear");
            if (description) ImGui::Text("%s", description);
        } else {
            if (dragTooltip) ImGui::Text("%s", dragTooltip);
            if (description) ImGui::Text("%s", description);
        }
        ImGui::EndTooltip();
    }
    
    // Enhanced drag-drop handling with visual feedback
    if (ImGui::BeginDragDropTarget()) {
        ImGui::PushStyleColor(ImGuiCol_DragDropTarget, ImVec4(0.3f, 0.6f, 1.0f, 0.7f));
        
        // Accept different payload types based on target type
        if (strcmp(targetType, "Mesh") == 0) {
            // Accept mesh files from file explorer
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
                TinyHandle fileNodeHandle = *(const TinyHandle*)payload->Data;
                const TinyFS::Node* fileNode = project->fs().fNode(fileNodeHandle);
                if (fileNode && fileNode->isFile() && fileNode->tHandle.isType<TinyMesh>()) {
                    handle = fileNode->tHandle.handle; // Use registry handle
                    modified = true;
                }
            }
        } else if (strcmp(targetType, "Skeleton") == 0) {
            // Accept skeleton files from file explorer
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
                TinyHandle fileNodeHandle = *(const TinyHandle*)payload->Data;
                const TinyFS::Node* fileNode = project->fs().fNode(fileNodeHandle);
                if (fileNode && fileNode->isFile() && fileNode->tHandle.isType<TinySkeleton>()) {
                    handle = fileNode->tHandle.handle; // Use registry handle
                    modified = true;
                }
            }
        } else if (strcmp(targetType, "SkeletonNode") == 0) {
            // Accept skeleton nodes from hierarchy
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NODE_HANDLE")) {
                TinyHandle nodeHandle = *(const TinyHandle*)payload->Data;
                TinyScene* activeScene = getActiveScene();
                if (activeScene) {
                    const TinyNode* node = activeScene->node(nodeHandle);
                    if (node && node->has<TinyNode::Skeleton>()) {
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
void FileDialog::open(const std::filesystem::path& startPath, TinyHandle folder) {
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
    targetFolder = TinyHandle();
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

void TinyApp::renderNodeTreeImGui(TinyHandle nodeHandle, int depth) {
    const TinyFS& fs = project->fs();

    TinyScene* activeScene = getActiveScene();
    if (!activeScene) return;

    // Use root node if no valid handle provided
    if (!nodeHandle.valid()) nodeHandle = activeScene->rootHandle();

    const TinyNode* node = activeScene->node(nodeHandle);
    if (!node) return;
    
    // Create a unique ID for this node
    ImGui::PushID(static_cast<int>(nodeHandle.index));

    // Check if this node has children
    bool hasChildren = !node->childrenHandles.empty();
    bool isSelected = (selectedHandle.isScene() && selectedHandle.handle.index == nodeHandle.index && selectedHandle.handle.version == nodeHandle.version);
    bool isHeld = (heldHandle.isScene() && heldHandle.handle.index == nodeHandle.index && heldHandle.handle.version == nodeHandle.version);
    
    // Create tree node flags
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // Add consistent styling to match File explorer theme
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.4f)); // Gray hover background (same as File explorer)
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f)); // Gray selection background (same as File explorer)

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
    
    // Drag and drop source (only if not root node)
    bool isDragging = false;
    if (nodeHandle != activeScene->rootHandle() && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        isDragging = true;
        holdSceneNode(nodeHandle);  // Set the held node during drag

        // Set payload to carry the node handle
        ImGui::SetDragDropPayload("NODE_HANDLE", &nodeHandle, sizeof(TinyHandle));
        
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
            TinyHandle draggedNode = *(const TinyHandle*)payload->Data;
            
            // Attempt to reparent the dragged node to this node
            if (activeScene->reparentNode(draggedNode, nodeHandle)) {
                // Update the newly reparented node
                activeScene->update(nodeHandle);

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
            TinyHandle sceneFNodeHandle = *(const TinyHandle*)payload->Data;
            
            // Get the filesystem node to access its TypeHandle
            const TinyFS::Node* sceneFile = project->fs().fNode(sceneFNodeHandle);
            if (sceneFile && sceneFile->isFile() && sceneFile->tHandle.isType<TinyScene>()) {
                // Extract the registry handle from the TypeHandle
                TinyHandle sceneRegistryHandle = sceneFile->tHandle.handle;
                
                // Verify the scene exists and instantiate it at this node
                const TinyScene* scene = fs.rGet<TinyScene>(sceneRegistryHandle);
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
            TinyHandle fileNodeHandle = *(const TinyHandle*)payload->Data;
            
            // Get the filesystem node to check if it's a scene file
            const TinyFS::Node* fileNode = project->fs().fNode(fileNodeHandle);
            if (fileNode && fileNode->isFile() && fileNode->tHandle.isType<TinyScene>()) {
                // This is a scene file - instantiate it at this node
                TinyHandle sceneRegistryHandle = fileNode->tHandle.handle;
                
                // Safety check: prevent dropping a scene into itself
                if (sceneRegistryHandle == getActiveSceneHandle()) {
                    // Cannot drop active scene into itself - ignore the operation
                    ImGui::SetTooltip("Cannot drop a scene into itself!");
                } else {
                    // Verify the scene exists and instantiate it at this node
                    const TinyScene* scene = fs.rGet<TinyScene>(sceneRegistryHandle);
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
            TinyScene* scene = getActiveScene();
            if (scene) {
                TinyHandle newNodeHandle = scene->addNode("New Node", nodeHandle);
                selectSceneNode(newNodeHandle);
                expandNode(nodeHandle); // Expand parent to show new child
            }
        }
        
        ImGui::Separator();

        bool isRootNode = (nodeHandle == activeScene->rootHandle());
        if (ImGui::MenuItem("Delete", nullptr, false, !isRootNode)) {
            TinyScene* scene = getActiveScene();
            if (scene) {
                const TinyNode* parentNode = scene->node(node->parentHandle);
                if (parentNode) selectSceneNode(node->parentHandle);

                scene->removeNode(nodeHandle);
            }
        }

        if (ImGui::MenuItem("Flatten", nullptr, false, !isRootNode && hasChildren)) {
            TinyScene* scene = getActiveScene();
            TinyHandle parentHandle = node->parentHandle;
            if (scene && scene->flattenNode(nodeHandle)) {
                selectSceneNode(parentHandle); // Select the parent node after flattening
            }
        }
        
        ImGui::EndPopup();

        // CRITICAL: Re-fetch node pointer after potential modifications
        node = activeScene->node(nodeHandle);
    }
    

    // Show node details in tooltip (slicker version like the old function)
    if (ImGui::IsItemHovered() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImGui::BeginTooltip();

        // Create the node label with useful information
        std::string typeLabel = "";
        if (node->has<TinyNode::Transform>()) {
            typeLabel += "[Transform] ";
        }
        if (node->has<TinyNode::MeshRender>()) {
            typeLabel += "[MeshRender] ";
        }
        if (node->has<TinyNode::BoneAttach>()) {
            typeLabel += "[BoneAttach] ";
        }
        if (node->has<TinyNode::Skeleton>()) {
            typeLabel += "[Skeleton] ";
        }

        typeLabel = typeLabel.empty() ? "[None]" : typeLabel;
        
        ImGui::Text("%s", node->name.c_str());
        ImGui::Text("Types: %s", typeLabel.c_str());

        if (!node->childrenHandles.empty()) {
            ImGui::Text("Children: %zu", node->childrenHandles.size());
        }
        
        ImGui::EndTooltip();
    }
    
    
    // If node is open and has children, recurse for children
    if (nodeOpen && hasChildren) {
        // Sort children by have-children? -> name
        std::vector<TinyHandle> sortedChildren = node->childrenHandles;
        std::sort(sortedChildren.begin(), sortedChildren.end(), [&](const TinyHandle& a, const TinyHandle& b) {
            const TinyNode* nodeA = activeScene->node(a);
            const TinyNode* nodeB = activeScene->node(b);
            // First sort by whether they have children
            bool aHasChildren = nodeA && !nodeA->childrenHandles.empty();
            bool bHasChildren = nodeB && !nodeB->childrenHandles.empty();
            if (aHasChildren != bHasChildren) return aHasChildren;
            // Otherwise sort by name
            std::string nameA = nodeA ? nodeA->name : "";
            std::string nameB = nodeB ? nodeB->name : "";
            return nameA < nameB;
        });

        for (const TinyHandle& childHandle : sortedChildren) {
            renderNodeTreeImGui(childHandle, depth + 1);
        }
        ImGui::TreePop();
    }
    
    
    // Pop the style colors we pushed earlier
    ImGui::PopStyleColor(2); // Pop HeaderHovered and Header
    
    ImGui::PopID();
}

void TinyApp::expandParentChain(TinyHandle nodeHandle) {
    TinyScene* activeScene = getActiveScene();
    if (!activeScene) return;
    
    // Get the target node
    const TinyNode* targetNode = activeScene->node(nodeHandle);
    if (!targetNode) return;
    
    // Walk up the parent chain and expand all parents
    TinyHandle currentHandle = targetNode->parentHandle;
    while (currentHandle.valid()) {
        expandedNodes.insert(currentHandle);
        
        const TinyNode* currentNode = activeScene->node(currentHandle);
        if (!currentNode) break;
        
        currentHandle = currentNode->parentHandle;
    }
    
    // Also expand the target node itself if it has children
    if (!targetNode->childrenHandles.empty()) {
        expandedNodes.insert(nodeHandle);
    }
}

void TinyApp::expandFNodeParentChain(TinyHandle fNodeHandle) {
    // Get the target file node
    const TinyFS::Node* targetFNode = project->fs().fNode(fNodeHandle);
    if (!targetFNode) return;
    
    // Walk up the parent chain and expand all parents
    TinyHandle currentHandle = targetFNode->parent;
    while (currentHandle.valid()) {
        expandedFNodes.insert(currentHandle);
        
        const TinyFS::Node* currentFNode = project->fs().fNode(currentHandle);
        if (!currentFNode) break;
        
        currentHandle = currentFNode->parent;
    }
    
    // Also expand the target node itself if it's a folder with children
    if (!targetFNode->isFile() && !targetFNode->children.empty()) {
        expandedFNodes.insert(fNodeHandle);
    }
}

void TinyApp::selectFileNode(TinyHandle fileHandle) {
    // Check if the file handle is valid
    if (!fileHandle.valid()) {
        clearSelection();
        return;
    }
    
    // Get the filesystem node
    const TinyFS& fs = project->fs();
    const TinyFS::Node* node = fs.fNode(fileHandle);
    if (!node) {
        // Invalid node, clear selection
        clearSelection();
        return;
    }
    
    // Only select if it's an actual file, not a folder
    if (node->isFile()) {
        selectedHandle = SelectHandle(fileHandle, SelectHandle::Type::File);
    }
    // If it's a folder, ignore the selection (don't set selectedHandle)
}

void TinyApp::renderFileExplorerImGui(TinyHandle nodeHandle, int depth) {
    TinyFS& fs = project->fs();
    
    // Use root handle if no valid handle provided
    if (!nodeHandle.valid()) {
        nodeHandle = fs.rootHandle();
    }
    
    const TinyFS::Node* node = fs.fNode(nodeHandle);
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
            ImGui::SetDragDropPayload("FOLDER_HANDLE", &nodeHandle, sizeof(TinyHandle));
            ImGui::Text("Moving: %s", displayName.c_str());
            ImGui::EndDragDropSource();
        }
        
        // Drag drop target for folders and files
        if (ImGui::BeginDragDropTarget()) {
            // Accept folders being dropped
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FOLDER_HANDLE")) {
                TinyHandle draggedFolder = *(const TinyHandle*)payload->Data;
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
                TinyHandle draggedFile = *(const TinyHandle*)payload->Data;
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
                TinyHandle newFolderHandle = fs.addFolder(nodeHandle, "New Folder");
                selectFileNode(newFolderHandle);
                // Expand parent chain to show the new folder
                expandFNodeParentChain(newFolderHandle);
            }

            if (ImGui::MenuItem("Add Scene")) {
                TinyScene newScene("New Scene");
                newScene.addRoot("Root");
                newScene.setSceneReq(project->sceneReq());

                TinyHandle fileHandle = fs.addFile(nodeHandle, "New Scene", std::move(&newScene));
                selectFileNode(fileHandle);
                // Expand parent chain to show the new scene
                expandFNodeParentChain(fileHandle);
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Delete", nullptr, false, node->deletable())) {
                TinyHandle parentHandle = node->parent;
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

            ImGui::EndPopup();
        }
        
        // If folder is open and has children, recurse for children
        if (nodeOpen && hasChildren) {
            // Create a sorted copy of children handles - folders first, then files, both sorted by name
            std::vector<TinyHandle> sortedChildren;
            for (const TinyHandle& childHandle : node->children) {
                const TinyFS::Node* child = fs.fNode(childHandle);
                if (!child || child->hidden()) continue; // Skip invalid or hidden nodes
                sortedChildren.push_back(childHandle);
            }
            
            std::sort(sortedChildren.begin(), sortedChildren.end(), [&fs](const TinyHandle& a, const TinyHandle& b) {
                const TinyFS::Node* nodeA = fs.fNode(a);
                const TinyFS::Node* nodeB = fs.fNode(b);
                if (!nodeA || !nodeB) return false;
                
                TinyFS::TypeExt extA = fs.fTypeExt(a);
                TinyFS::TypeExt extB = fs.fTypeExt(b);

                // No need for folder check, automatically set max priority

                if (extA < extB) return true;
                else if (extA > extB) return false;
                return nodeA->name < nodeB->name;
            });
            
            for (const TinyHandle& childHandle : sortedChildren) {
                renderFileExplorerImGui(childHandle, depth + 1);
            }

            ImGui::TreePop();
        }
        
    } else if (node->isFile()) {
        // General file handling - completely generic
        std::string fileName = node->name;
        TinyFS::TypeExt fileExt = fs.fTypeExt(nodeHandle);

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
            ImGui::SetDragDropPayload("FILE_HANDLE", &nodeHandle, sizeof(TinyHandle));
            ImGui::Text("%s", fileName.c_str());
            ImGui::EndDragDropSource();
        }
        
        // Generic context menu with type-specific options
        if (ImGui::BeginPopup(("FileContext_" + std::to_string(nodeHandle.index)).c_str())) {
            ImGui::Text("%s", fileName.c_str());
            ImGui::Separator();
            
            // Type-specific context menu options
            if (node->tHandle.isType<TinyScene>()) {
                // Scene file options
                TinyHandle sceneRegistryHandle = node->tHandle.handle;
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
            } else if (node->tHandle.isType<TinyMesh>()) {
                // Mesh file options
                if (ImGui::MenuItem("Preview Mesh")) {
                    // TODO: Add mesh preview functionality
                }
            } else if (node->tHandle.isType<TinyTexture>()) {
                // Texture file options
                if (ImGui::MenuItem("Preview Texture")) {
                    // TODO: Add texture preview functionality
                }
            } else if (node->tHandle.isType<TinyRMaterial>()) {
                // Material file options
                if (ImGui::MenuItem("Edit Material")) {
                    // TODO: Add material editor functionality
                }
            }
            // Add more file types as needed...
            
            // Common separator before delete (only if there were type-specific options)
            if (node->tHandle.isType<TinyScene>() || node->tHandle.isType<TinyMesh>() || 
                node->tHandle.isType<TinyTexture>() || node->tHandle.isType<TinyRMaterial>()) {
                ImGui::Separator();
            }
            
            // Common delete option for all files
            if (ImGui::MenuItem("Delete", nullptr, false, node->deletable())) {
                TinyHandle parentHandle = node->parent;
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

void TinyApp::renderFileDialog() {
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

void TinyApp::loadModelFromPath(const std::string& filePath, TinyHandle targetFolder) {
    try {
        // Load the model using TinyLoader
        TinyModel model = TinyLoader::loadModel(filePath);
        
        // Add the model to the project in the specified folder (returns model folder handle)
        TinyHandle modelFolderHandle = project->addModel(model, targetFolder);
        
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

bool TinyApp::setActiveScene(TinyHandle sceneHandle) {
    // Verify the handle points to a valid TinyScene in the registry
    const TinyScene* scene = project->fs().rGet<TinyScene>(sceneHandle);
    if (!scene) {
        return false; // Invalid handle or not a scene
    }
    
    // Switch the active scene
    activeSceneHandle = sceneHandle;

    // Update transforms for the new active scene
    TinyScene* activeScene = getActiveScene();
    if (activeScene) activeScene->update();
    
    return true;
}