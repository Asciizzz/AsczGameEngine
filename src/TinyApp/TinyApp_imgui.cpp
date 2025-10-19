#include "TinyApp/TinyApp.hpp"

#include "TinyEngine/TinyLoader.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

using namespace TinyVK;

void TinyApp::setupImGuiWindows(const TinyChrono& fpsManager, const TinyCamera& camera, bool mouseFocus, float deltaTime) {
    // Clear any existing windows first
    imguiWrapper->clearWindows();
    
    // Main Editor Window - full size, no scroll bars
    const TinyRegistry& registry = project->registryRef();
    TinyFS& fs = project->filesystem();

    imguiWrapper->addWindow("Editor", [this, &camera, &registry, &fs]() {
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
        TinyRScene* activeScene = project->getActiveScene();
        if (activeScene) {
            ImGui::Text("Active Scene: %s", activeScene->name.c_str());
            
            // Show full scene info on hover
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Scene: %s", activeScene->name.c_str());
                ImGui::Text("Total Nodes: %u", activeScene->nodes.count());
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
        
        // Clear filesystem selection when clicking in node tree area
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()) {
            project->clearSelection();
        }
        
        // Clear held node when mouse is released and no dragging is happening
        if (project->heldHandle.valid() && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            project->clearHeld();
        }
        
        if (activeScene && activeScene->nodes.count() > 0) {
            project->renderNodeTreeImGui();
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
        
        // Clear scene node selection when clicking in file tree area (but not on items)
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()) {
            project->clearSelection();
        }
        
        project->renderFileExplorerImGui();
        
        // Render the file dialog once per frame, outside the file explorer tree
        project->renderFileDialog();

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
    if (!project->selectedHandle.valid()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No selection");
        ImGui::Text("Select a scene node or file to inspect its properties.");
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
    
    if (project->selectedHandle.isScene()) {
        // Render scene node inspector
        renderSceneNodeInspector();
    } else if (project->selectedHandle.isFile()) {
        // Render file system inspector
        renderFileSystemInspector();
    }
    
    ImGui::EndChild();
    
    // Pop the scrollbar styling
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

void TinyApp::renderSceneNodeInspector() {
    TinyRScene* activeScene = project->getActiveScene();
    if (!activeScene) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "No active scene");
        return;
    }

    // Get the selected scene node handle from unified selection
    TinyHandle selectedSceneNodeHandle = project->getSelectedSceneNode();
    if (!selectedSceneNodeHandle.valid()) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "No scene node selected");
        ImGui::Text("This should not happen in unified selection.");
        return;
    }

    const TinyNode* selectedNode = activeScene->nodes.get(selectedSceneNodeHandle);
    if (!selectedNode) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid node selection");
        project->selectSceneNode(project->getRootNodeHandle());
        return;
    }

    bool isRootNode = (selectedSceneNodeHandle == project->getRootNodeHandle());

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
        const TinyNode* parentNode = activeScene->nodes.get(selectedNode->parentHandle);
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
    
    TinyNode* mutableNode = const_cast<TinyNode*>(selectedNode);
    
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
        
        // CRITICAL: Always call EndChild() - this was the bug!
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
    renderComponent("Transform", ImVec4(0.2f, 0.2f, 0.15f, 0.8f), ImVec4(0.4f, 0.4f, 0.3f, 0.6f), true, [&]() {
        // Transform component content - now available for all nodes including root
        {
            // Get the current transform
            glm::mat4 localTransform = selectedNode->localTransform;
            
            // Extract translation, rotation, and scale from the matrix
            glm::vec3 translation, rotation, scale;
            glm::quat rotationQuat;
            glm::vec3 skew;
            glm::vec4 perspective;
            
            // Check if decomposition is valid
            bool validDecomposition = glm::decompose(localTransform, scale, rotationQuat, translation, skew, perspective);
            
            if (!validDecomposition) {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Warning: Invalid transform matrix detected!");
                if (ImGui::Button("Reset Transform")) {
                    TinyNode* mutableSelectedNode = const_cast<TinyNode*>(selectedNode);
                    mutableSelectedNode->localTransform = glm::mat4(1.0f);
                    if (TinyRScene* scene = project->getActiveScene()) scene->updateGlbTransform();
                }
                return;
            }
            
            // Validate extracted values for NaN/infinity
            auto isValidVec3 = [](const glm::vec3& v) {
                return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
            };
            
            if (!isValidVec3(translation) || !isValidVec3(scale)) {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Warning: NaN/Infinite values detected!");
                if (ImGui::Button("Reset Transform")) {
                    TinyNode* mutableSelectedNode = const_cast<TinyNode*>(selectedNode);
                    mutableSelectedNode->localTransform = glm::mat4(1.0f);
                    if (TinyRScene* scene = project->getActiveScene()) scene->updateGlbTransform();
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
            
            // Apply changes if any values changed
            if (translation != originalTranslation || rotation != originalRotation || scale != originalScale) {
                if (isValidVec3(translation) && isValidVec3(scale)) {
                    // Convert back to quaternion and reconstruct matrix
                    glm::quat newRotQuat = glm::quat(glm::radians(rotation));
                    
                    // Create transform matrix: T * R * S
                    glm::mat4 translateMat = glm::translate(glm::mat4(1.0f), translation);
                    glm::mat4 rotateMat = glm::mat4_cast(newRotQuat);
                    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
                    
                    TinyNode* mutableSelectedNode = const_cast<TinyNode*>(selectedNode);
                    mutableSelectedNode->localTransform = translateMat * rotateMat * scaleMat;
                    
                    if (TinyRScene* scene = project->getActiveScene()) scene->updateGlbTransform();
                }
            }
            
            ImGui::Spacing();
        }
    });
    
    // Mesh Renderer Component - Always show
    if (selectedNode->hasComponent<TinyNode::MeshRender>()) {
        renderComponent("Mesh Renderer", ImVec4(0.15f, 0.15f, 0.2f, 0.8f), ImVec4(0.3f, 0.3f, 0.4f, 0.6f), true, [&]() {
            // Mesh Renderer content
            TinyNode::MeshRender* meshComp = mutableNode->get<TinyNode::MeshRender>();
            if (!meshComp) return;
            
            ImGui::Spacing();
            
            // Mesh Handle field with enhanced drag-drop support
            ImGui::Text("Mesh Resource:");

            // Check if mesh resource is valid
            renderHandleField("##MeshHandle", meshComp->meshHandle, "Mesh",
                "Drag a mesh file from the File Explorer", 
                "Select mesh resource for rendering");

            // Show mesh information if valid
            if (meshComp->meshHandle.valid()) {
                const TinyMesh* mesh = project->registryRef().get<TinyMesh>(meshComp->meshHandle);
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
            bool skeleModified = renderHandleField("##MeshRenderer_SkeletonNodeHandle", meshComp->skeleNodeHandle, "SkeletonNode",
                "Drag a skeleton node from the Hierarchy",
                "Select skeleton node for bone animation");

            // Show skeleton node information if valid
            if (meshComp->skeleNodeHandle.valid()) {
                TinyRScene* activeScene = project->getActiveScene();
                if (activeScene) {
                    const TinyNode* skeleNode = activeScene->nodes.get(meshComp->skeleNodeHandle);
                    if (skeleNode && skeleNode->hasComponent<TinyNode::Skeleton>()) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", skeleNode->name.c_str());
                    } else {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Invalid/No Skeleton");
                    }
                }
            }
            
            ImGui::Spacing();
        }, [&]() {
            mutableNode->remove<TinyNode::MeshRender>();
        });
    } else {
        // Show grayed-out placeholder for missing Mesh Renderer component
        renderComponent("Mesh Renderer", ImVec4(0.05f, 0.05f, 0.05f, 0.3f), ImVec4(0.15f, 0.15f, 0.15f, 0.3f), false, [&]() {
            // Minimal placeholder - no content, just the header with add button
        }, [&]() {
            mutableNode->add(TinyNode::MeshRender{});
        });
    }
    
    // Bone Attachment Component - Always show
    if (selectedNode->hasComponent<TinyNode::BoneAttach>()) {
        renderComponent("Bone Attachment", ImVec4(0.15f, 0.2f, 0.15f, 0.8f), ImVec4(0.3f, 0.4f, 0.3f, 0.6f), true, [&]() {
            // Bone Attachment content
            TinyNode::BoneAttach* boneComp = mutableNode->get<TinyNode::BoneAttach>();
            if (!boneComp) return;
            
            ImGui::Spacing();
            
            // Skeleton Node Handle field with enhanced drag-drop support
            ImGui::Text("Skeleton Node:");
            bool skeleModified = renderHandleField("##BoneAttach_SkeletonNodeHandle", boneComp->skeleNodeHandle, "SkeletonNode",
                "Drag a skeleton node from the Hierarchy",
                "Select skeleton node to attach to");
            
            // Show skeleton node information if valid
            if (boneComp->skeleNodeHandle.valid()) {
                TinyRScene* activeScene = project->getActiveScene();
                if (activeScene) {
                    const TinyNode* skeleNode = activeScene->nodes.get(boneComp->skeleNodeHandle);
                    if (skeleNode && skeleNode->hasComponent<TinyNode::Skeleton>()) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "✓ %s", skeleNode->name.c_str());
                    } else {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "✗ Invalid/No Skeleton");
                    }
                }
            }
            
            ImGui::Spacing();
            
            // Bone Index field with validation
            ImGui::Text("Bone Index:");
            int boneIndex = static_cast<int>(boneComp->boneIndex);
            
            // Determine max bone count for validation
            int maxBoneIndex = 255; // Default max
            if (boneComp->skeleNodeHandle.valid()) {
                TinyRScene* activeScene = project->getActiveScene();
                if (activeScene) {
                    const TinyNode* skeleNode = activeScene->nodes.get(boneComp->skeleNodeHandle);
                    if (skeleNode && skeleNode->hasComponent<TinyNode::Skeleton>()) {
                        const TinyNode::Skeleton* skeleComp = skeleNode->get<TinyNode::Skeleton>();
                        if (skeleComp && skeleComp->skeleHandle.valid()) {
                            const TinyRegistry& registry = project->registryRef();
                            const TinySkeleton* skeleton = registry.get<TinySkeleton>(skeleComp->skeleHandle);
                            if (skeleton) {
                                maxBoneIndex = static_cast<int>(skeleton->bones.size()) - 1;
                            }
                        }
                    }
                }
            }
            
            if (ImGui::DragInt("##BoneIndex", &boneIndex, 1.0f, 0, maxBoneIndex)) {
                boneComp->boneIndex = static_cast<size_t>(std::max(0, boneIndex));
            }
            
            // Show validation status
            ImGui::SameLine();
            if (boneIndex <= maxBoneIndex && boneComp->skeleNodeHandle.valid()) {
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
            
            bool hasValidSkeleton = boneComp->skeleNodeHandle.valid();
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
            mutableNode->remove<TinyNode::BoneAttach>();
        });
    } else {
        // Show grayed-out placeholder for missing Bone Attachment component
        renderComponent("Bone Attachment", ImVec4(0.05f, 0.05f, 0.05f, 0.3f), ImVec4(0.15f, 0.15f, 0.15f, 0.3f), false, [&]() {
            // Minimal placeholder - no content, just the header with add button
        }, [&]() {
            mutableNode->add(TinyNode::BoneAttach{});
        });
    }
    
    // Skeleton Component - Always show
    if (selectedNode->hasComponent<TinyNode::Skeleton>()) {
        renderComponent("Skeleton", ImVec4(0.2f, 0.15f, 0.15f, 0.8f), ImVec4(0.4f, 0.3f, 0.3f, 0.6f), true, [&]() {
            // Skeleton content
            TinyNode::Skeleton* skeleComp = mutableNode->get<TinyNode::Skeleton>();
            if (!skeleComp) return;
            
            ImGui::Spacing();
            
            // Skeleton Handle field
            ImGui::Text("Skeleton Resource:");
            bool skeleModified = renderHandleField("##SkeletonHandle", skeleComp->skeleHandle, "Skeleton",
                "Drag a skeleton file from the File Explorer",
                "Select skeleton resource for bone data");
            
            // Show skeleton information if valid
            if (skeleComp->skeleHandle.valid()) {
                const TinyRegistry& registry = project->registryRef();
                const TinySkeleton* skeleton = registry.get<TinySkeleton>(skeleComp->skeleHandle);
                if (skeleton) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "✓ %s (%zu bones)", skeleton->name.c_str(), skeleton->bones.size());
                } else {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "✗ Invalid");
                }
            }
            
            ImGui::Spacing();
            
            // Show bone transform count (read-only)
            ImGui::Text("Active Bone Transforms: %zu", skeleComp->boneTransformsFinal.size());
            
            // Component status
            ImGui::Separator();
            ImGui::Text("Status:");
            ImGui::SameLine();
            
            if (skeleComp->skeleHandle.valid()) {
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Skeleton loaded and ready");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "Missing skeleton resource");
            }
            
            ImGui::Spacing();
        }, [&]() {
            mutableNode->remove<TinyNode::Skeleton>();
        });
    } else {
        // Show grayed-out placeholder for missing Skeleton component
        renderComponent("Skeleton", ImVec4(0.05f, 0.05f, 0.05f, 0.3f), ImVec4(0.15f, 0.15f, 0.15f, 0.3f), false, [&]() {
            // Minimal placeholder - no content, just the header with add button
        }, [&]() {
            mutableNode->add(TinyNode::Skeleton{});
        });
    }
    
    ImGui::EndChild();
    
    // Pop the scrollbar style customizations
    ImGui::PopStyleColor(4); // Pop ScrollbarBg, ScrollbarGrab, ScrollbarGrabHovered, ScrollbarGrabActive
    ImGui::PopStyleVar(2); // Pop ScrollbarSize, ScrollbarRounding
}




void TinyApp::renderFileSystemInspector() {
    TinyFS& fs = project->filesystem();
    
    // Get the selected file node handle from unified selection
    TinyHandle selectedFNodeHandle = project->getSelectedFileNode();
    if (!selectedFNodeHandle.valid()) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "No file selected");
        ImGui::Text("This should not happen in unified selection.");
        return;
    }
    
    const TinyFS::Node* selectedFNode = fs.getFNodes().get(selectedFNodeHandle);
    if (!selectedFNode) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Invalid filesystem node selection");
        project->clearSelection(); // Clear invalid selection
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
            TinyFS::Node* mutableNode = const_cast<TinyFS::Node*>(fs.getFNodes().get(selectedFNodeHandle));
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
        if (selectedFNode->tHandle.isType<TinyRScene>()) {
            fileType = "Scene";
            TinyRScene* scene = static_cast<TinyRScene*>(fs.registryRef().get(selectedFNode->tHandle));
            if (scene) {
                ImGui::Text("Type: %s", fileType.c_str());
                ImGui::Text("Scene Nodes: %u", scene->nodes.count());
                
                // Make Active Scene button
                ImGui::Spacing();
                TinyHandle sceneRegistryHandle = selectedFNode->tHandle.handle;
                bool isActiveScene = (project->getActiveSceneHandle() == sceneRegistryHandle);
                
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
                        if (project->setActiveScene(sceneRegistryHandle)) {
                            project->selectSceneNode(project->getRootNodeHandle()); // Reset node selection
                        }
                    }
                    ImGui::PopStyleColor(3);
                }
            }
        } else if (selectedFNode->tHandle.isType<TinyTexture>()) {
            fileType = "Texture";
            ImGui::Text("Type: %s", fileType.c_str());
        } else if (selectedFNode->tHandle.isType<TinyRMaterial>()) {
            fileType = "Material";
            ImGui::Text("Type: %s", fileType.c_str());
        } else if (selectedFNode->tHandle.isType<TinyMesh>()) {
            fileType = "Mesh";
            ImGui::Text("Type: %s", fileType.c_str());
        } else if (selectedFNode->tHandle.isType<TinySkeleton>()) {
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
    
    if (handle.valid()) {
        // Get the actual name based on target type
        if (strcmp(targetType, "Mesh") == 0) {
            const TinyRegistry& registry = project->registryRef();
            const TinyMesh* mesh = registry.get<TinyMesh>(handle);
            displayText = mesh ? mesh->name : "Unknown Mesh";
        } else if (strcmp(targetType, "Skeleton") == 0) {
            const TinyRegistry& registry = project->registryRef();
            const TinySkeleton* skeleton = registry.get<TinySkeleton>(handle);
            displayText = skeleton ? skeleton->name : "Unknown Skeleton";
        } else if (strcmp(targetType, "SkeletonNode") == 0) {
            TinyRScene* activeScene = project->getActiveScene();
            if (activeScene) {
                const TinyNode* node = activeScene->nodes.get(handle);
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
                const TinyFS::Node* fileNode = project->filesystem().getFNodes().get(fileNodeHandle);
                if (fileNode && fileNode->isFile() && fileNode->tHandle.isType<TinyMesh>()) {
                    handle = fileNode->tHandle.handle; // Use registry handle
                    modified = true;
                }
            }
        } else if (strcmp(targetType, "Skeleton") == 0) {
            // Accept skeleton files from file explorer
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_HANDLE")) {
                TinyHandle fileNodeHandle = *(const TinyHandle*)payload->Data;
                const TinyFS::Node* fileNode = project->filesystem().getFNodes().get(fileNodeHandle);
                if (fileNode && fileNode->isFile() && fileNode->tHandle.isType<TinySkeleton>()) {
                    handle = fileNode->tHandle.handle; // Use registry handle
                    modified = true;
                }
            }
        } else if (strcmp(targetType, "SkeletonNode") == 0) {
            // Accept skeleton nodes from hierarchy
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("NODE_HANDLE")) {
                TinyHandle nodeHandle = *(const TinyHandle*)payload->Data;
                TinyRScene* activeScene = project->getActiveScene();
                if (activeScene) {
                    const TinyNode* node = activeScene->nodes.get(nodeHandle);
                    if (node && node->hasComponent<TinyNode::Skeleton>()) {
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