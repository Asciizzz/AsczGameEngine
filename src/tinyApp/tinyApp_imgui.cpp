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
// TEST DATA STRUCTURES - For experimenting with UI functionality
// ============================================================================

namespace TestData {
    // Simple tree node structure for testing hierarchies
    struct TreeNode {
        std::string name;
        bool isExpanded = true;
        std::vector<TreeNode*> children;
        
        TreeNode(const std::string& n) : name(n) {}
        ~TreeNode() {
            for (auto* child : children) delete child;
        }
        
        void addChild(TreeNode* child) {
            children.push_back(child);
        }
    };
    
    // Drag-drop payload types
    struct DragDropItem {
        std::string type;  // "NODE", "FILE", "MATERIAL", etc.
        TreeNode* nodePtr; // Direct pointer for easy access
        std::string name;
    };
    
    // Helper function to remove a child from a parent
    static bool removeChildFromParent(TreeNode* root, TreeNode* childToRemove, TreeNode** outParent = nullptr) {
        if (!root || !childToRemove) return false;
        
        // Check if childToRemove is a direct child of root
        auto it = std::find(root->children.begin(), root->children.end(), childToRemove);
        if (it != root->children.end()) {
            root->children.erase(it);
            if (outParent) *outParent = root;
            return true;
        }
        
        // Recursively search in children
        for (auto* child : root->children) {
            if (removeChildFromParent(child, childToRemove, outParent)) {
                return true;
            }
        }
        
        return false;
    }
    
    // Test scene hierarchy - static initialization
    static TreeNode* createTestTree() {
        static TreeNode* root = nullptr;
        if (!root) {
            root = new TreeNode("Root");
            
            auto* player = new TreeNode("Player");
            player->addChild(new TreeNode("Head"));
            player->addChild(new TreeNode("Body"));
            
            auto* leftArm = new TreeNode("LeftArm");
            leftArm->addChild(new TreeNode("LeftHand"));
            player->addChild(leftArm);
            
            auto* rightArm = new TreeNode("RightArm");
            rightArm->addChild(new TreeNode("RightHand"));
            player->addChild(rightArm);
            
            root->addChild(player);
            
            auto* environment = new TreeNode("Environment");
            environment->addChild(new TreeNode("Ground"));
            environment->addChild(new TreeNode("Sky"));
            environment->addChild(new TreeNode("Trees"));
            root->addChild(environment);
            
            auto* lighting = new TreeNode("Lighting");
            lighting->addChild(new TreeNode("DirectionalLight"));
            lighting->addChild(new TreeNode("PointLight1"));
            lighting->addChild(new TreeNode("PointLight2"));
            root->addChild(lighting);
        }
        return root;
    }
    
    // Selected node for testing
    static TreeNode* selectedNode = nullptr;
    static TreeNode* draggedNode = nullptr;
}

// ============================================================================
// HELPER FUNCTIONS - Test UI Rendering
// ============================================================================

static void RenderTestTreeNode(TestData::TreeNode* node, int depth = 0) {
    if (!node) return;
    
    ImGui::PushID(node);
    
    bool hasChildren = !node->children.empty();
    bool isSelected = (TestData::selectedNode == node);
    bool isDragged = (TestData::draggedNode == node);
    
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
    
    bool nodeOpen = ImGui::TreeNodeEx(node->name.c_str(), flags);
    
    ImGui::PopStyleColor(2);
    
    // Selection on click
    if (ImGui::IsItemClicked()) {
        TestData::selectedNode = node;
    }
    
    // Drag source
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        TestData::draggedNode = node;
        TestData::DragDropItem payload{"NODE", node, node->name};
        ImGui::SetDragDropPayload("TEST_NODE", &payload, sizeof(payload));
        ImGui::Text("Moving: %s", node->name.c_str());
        ImGui::EndDragDropSource();
    }
    
    // Drop target - only allow drops on different nodes (can't drop on self)
    if (node != TestData::draggedNode && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEST_NODE")) {
            TestData::DragDropItem* item = (TestData::DragDropItem*)payload->Data;
            TestData::TreeNode* draggedNode = item->nodePtr;
            TestData::TreeNode* targetNode = node;
            
            // Verify we're not trying to drop a parent into its own child (would create cycle)
            bool wouldCreateCycle = false;
            TestData::TreeNode* check = targetNode;
            while (check != nullptr) {
                if (check == draggedNode) {
                    wouldCreateCycle = true;
                    break;
                }
                // We'd need parent tracking for this, so for now just do basic check
                break;
            }
            
            if (!wouldCreateCycle && draggedNode != targetNode) {
                // Remove from old parent
                TestData::TreeNode* root = TestData::createTestTree();
                if (TestData::removeChildFromParent(root, draggedNode)) {
                    // Add to new parent
                    targetNode->addChild(draggedNode);
                    targetNode->isExpanded = true; // Auto-expand to show the moved node
                }
            }
            
            TestData::draggedNode = nullptr; // Clear drag state
        }
        ImGui::EndDragDropTarget();
    }
    
    // Clear drag state when mouse is released
    if (TestData::draggedNode && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        TestData::draggedNode = nullptr;
    }
    
    // Context menu
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Add Child")) {
            node->addChild(new TestData::TreeNode("NewNode"));
        }
        if (ImGui::MenuItem("Delete") && node != TestData::createTestTree()) {
            // In real implementation, would delete from parent
            tinyUI::Exec::Text("Delete not implemented in test");
        }
        ImGui::EndPopup();
    }
    
    // Render children
    if (nodeOpen && hasChildren) {
        for (auto* child : node->children) {
            RenderTestTreeNode(child, depth + 1);
        }
        ImGui::TreePop();
    }
    
    ImGui::PopID();
}

// ============================================================================
// MAIN UI RENDERING FUNCTION
// ============================================================================

void tinyApp::renderUI() {
    auto& fpsRef = *fpsManager;
    auto& camRef = *project->camera();
    
    // ===== DEBUG PANEL WINDOW =====
    if (tinyUI::Exec::Begin("Debug Panel", nullptr, 0)) {
        tinyUI::Exec::Text("FPS: %.1f", fpsRef.currentFPS);
        tinyUI::Exec::Text("Frame Time: %.3f ms", fpsRef.deltaTime * 1000.0f);
        tinyUI::Exec::Separator();
        
        // Camera info - Display only (not editable)
        if (tinyUI::Exec::CollapsingHeader("Camera")) {
            tinyUI::Exec::Text("Position: (%.2f, %.2f, %.2f)", camRef.pos.x, camRef.pos.y, camRef.pos.z);
            tinyUI::Exec::Text("Forward: (%.2f, %.2f, %.2f)", camRef.forward.x, camRef.forward.y, camRef.forward.z);
            tinyUI::Exec::Text("Right: (%.2f, %.2f, %.2f)", camRef.right.x, camRef.right.y, camRef.right.z);
            tinyUI::Exec::Text("Up: (%.2f, %.2f, %.2f)", camRef.up.x, camRef.up.y, camRef.up.z);
        }
        
        tinyUI::Exec::End();
    }
    
    // ===== TEST: TREE HIERARCHY WINDOW =====
    static bool showTestTree = true;
    if (showTestTree && tinyUI::Exec::Begin("Test: Scene Hierarchy", &showTestTree)) {
        tinyUI::Exec::Text("Experiment with tree rendering & drag-drop");
        tinyUI::Exec::Separator();
        
        if (tinyUI::Exec::Button("Reset Selection")) {
            TestData::selectedNode = nullptr;
        }
        tinyUI::Exec::SameLine();
        if (tinyUI::Exec::Button("Expand All")) {
            // Would traverse and expand all in real implementation
        }
        
        tinyUI::Exec::Separator();
        
        // Render the test tree
        ImGui::BeginChild("TreeView", ImVec2(0, 0), true);
        RenderTestTreeNode(TestData::createTestTree());
        ImGui::EndChild();
        
        tinyUI::Exec::End();
    }
    
    // ===== TEST: INSPECTOR WINDOW =====
    static bool showTestInspector = true;
    if (showTestInspector && tinyUI::Exec::Begin("Test: Inspector", &showTestInspector)) {
        if (TestData::selectedNode) {
            tinyUI::Exec::Text("Selected: %s", TestData::selectedNode->name.c_str());
            tinyUI::Exec::Separator();
            
            // Editable name field
            static char nameBuffer[256] = "";
            static TestData::TreeNode* lastSelected = nullptr;
            if (lastSelected != TestData::selectedNode) {
                strncpy(nameBuffer, TestData::selectedNode->name.c_str(), 255);
                nameBuffer[255] = '\0';
                lastSelected = TestData::selectedNode;
            }
            
            tinyUI::Exec::Text("Name:");
            tinyUI::Exec::SameLine();
            if (ImGui::InputText("##NodeName", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                TestData::selectedNode->name = nameBuffer;
            }
            
            tinyUI::Exec::Separator();
            
            // Properties section
            if (tinyUI::Exec::CollapsingHeader("Transform")) {
                static float pos[3] = {0.0f, 0.0f, 0.0f};
                static float rot[3] = {0.0f, 0.0f, 0.0f};
                static float scale[3] = {1.0f, 1.0f, 1.0f};
                
                tinyUI::Exec::DragFloat3("Position", pos, 0.1f);
                tinyUI::Exec::DragFloat3("Rotation", rot, 0.1f);
                tinyUI::Exec::DragFloat3("Scale", scale, 0.01f);
            }
            
            if (tinyUI::Exec::CollapsingHeader("Mesh Renderer")) {
                tinyUI::Exec::Text("Mesh: None");
                if (tinyUI::Exec::Button("Select Mesh...", ImVec2(-1, 30))) {
                    tinyUI::Exec::Text("Would open mesh selector");
                }
                
                tinyUI::Exec::Spacing();
                tinyUI::Exec::Text("Material: None");
                if (tinyUI::Exec::Button("Select Material...", ImVec2(-1, 30))) {
                    tinyUI::Exec::Text("Would open material selector");
                }
            }
            
        } else {
            tinyUI::Exec::Text("No selection");
            tinyUI::Exec::Text("Select a node from the hierarchy");
        }
        
        tinyUI::Exec::End();
    }
    
    // ===== TEST: DRAG & DROP DEMO WINDOW =====
    static bool showDragDropTest = true;
    if (showDragDropTest && tinyUI::Exec::Begin("Test: Drag & Drop", &showDragDropTest)) {
        tinyUI::Exec::Text("Test drag-drop between different areas");
        tinyUI::Exec::Separator();
        
        // Source items
        tinyUI::Exec::Text("Drag from here:");
        ImGui::Indent();
        
        const char* items[] = {"Item A", "Item B", "Item C", "Item D", "Item E"};
        for (int i = 0; i < 5; i++) {
            ImGui::PushID(i);
            if (tinyUI::Exec::Button(items[i], ImVec2(150, 30))) {}
            
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                ImGui::SetDragDropPayload("TEST_ITEM", &i, sizeof(int));
                tinyUI::Exec::Text("Dragging: %s", items[i]);
                ImGui::EndDragDropSource();
            }
            ImGui::PopID();
        }
        ImGui::Unindent();
        
        tinyUI::Exec::Separator();
        
        // Drop targets
        tinyUI::Exec::Text("Drop into slots:");
        static int slots[3] = {-1, -1, -1};
        
        for (int i = 0; i < 3; i++) {
            ImGui::PushID(100 + i);
            ImGui::PushStyleColor(ImGuiCol_Button, slots[i] >= 0 ? ImVec4(0.3f, 0.7f, 0.4f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            
            std::string label = slots[i] >= 0 ? items[slots[i]] : "Empty Slot";
            tinyUI::Exec::Button(label.c_str(), ImVec2(-1, 40));
            
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEST_ITEM")) {
                    int item_id = *(const int*)payload->Data;
                    slots[i] = item_id;
                }
                ImGui::EndDragDropTarget();
            }
            
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
        
        if (tinyUI::Exec::Button("Clear All Slots")) {
            slots[0] = slots[1] = slots[2] = -1;
        }
        
        tinyUI::Exec::End();
    }
    
    // ===== TEST: STYLED BUTTONS DEMO =====
    static bool showButtonTest = true;
    if (showButtonTest && tinyUI::Exec::Begin("Test: Button Styles", &showButtonTest)) {
        tinyUI::Exec::Text("Testing button style variants");
        tinyUI::Exec::Separator();
        
        if (tinyUI::Exec::StyledButton("Default Button", tinyUI::ButtonStyle::Default, ImVec2(200, 30))) {
            tinyUI::Exec::Text("Default clicked!");
        }
        
        if (tinyUI::Exec::StyledButton("Primary Action", tinyUI::ButtonStyle::Primary, ImVec2(200, 30))) {
            tinyUI::Exec::Text("Primary clicked!");
        }
        
        if (tinyUI::Exec::StyledButton("Success Action", tinyUI::ButtonStyle::Success, ImVec2(200, 30))) {
            tinyUI::Exec::Text("Success clicked!");
        }
        
        if (tinyUI::Exec::StyledButton("Danger Action", tinyUI::ButtonStyle::Danger, ImVec2(200, 30))) {
            tinyUI::Exec::Text("Danger clicked!");
        }
        
        if (tinyUI::Exec::StyledButton("Warning Action", tinyUI::ButtonStyle::Warning, ImVec2(200, 30))) {
            tinyUI::Exec::Text("Warning clicked!");
        }
        
        tinyUI::Exec::Separator();
        tinyUI::Exec::Text("Mix with regular buttons:");
        
        if (tinyUI::Exec::Button("Regular Button")) {
            tinyUI::Exec::Text("Regular clicked!");
        }
        
        tinyUI::Exec::End();
    }
    
    // ===== TEST: INPUT WIDGETS DEMO =====
    static bool showInputTest = true;
    if (showInputTest && tinyUI::Exec::Begin("Test: Input Widgets", &showInputTest)) {
        tinyUI::Exec::Text("Testing various input types");
        tinyUI::Exec::Separator();
        
        static int intValue = 42;
        static float floatValue = 3.14f;
        static double doubleValue = 2.718;
        static bool boolValue = true;
        static std::string stringValue = "Hello World";
        static glm::vec3 vec3Value(1.0f, 2.0f, 3.0f);
        static glm::vec4 colorValue(1.0f, 0.5f, 0.2f, 1.0f);
        
        tinyUI::Exec::EditProperty("Integer", intValue);
        tinyUI::Exec::EditProperty("Float", floatValue);
        tinyUI::Exec::EditProperty("Double", doubleValue);
        tinyUI::Exec::EditProperty("Boolean", boolValue);
        tinyUI::Exec::EditProperty("String", stringValue);
        tinyUI::Exec::EditProperty("Vector3", vec3Value);
        tinyUI::Exec::EditProperty("Color", colorValue);
        
        tinyUI::Exec::Separator();
        
        // Color picker
        tinyUI::ColorPicker3("RGB Color", vec3Value);
        tinyUI::ColorPicker4("RGBA Color", colorValue);
        
        tinyUI::Exec::End();
    }
    
    // ===== TEST WINDOW TOGGLES =====
    static bool showWindowToggles = true;
    if (showWindowToggles && tinyUI::Exec::Begin("Test Window Controls", &showWindowToggles)) {
        tinyUI::Exec::Text("Show/Hide Test Windows");
        tinyUI::Exec::Separator();
        
        tinyUI::Exec::Checkbox("Scene Hierarchy", &showTestTree);
        tinyUI::Exec::Checkbox("Inspector", &showTestInspector);
        tinyUI::Exec::Checkbox("Drag & Drop Demo", &showDragDropTest);
        tinyUI::Exec::Checkbox("Button Styles", &showButtonTest);
        tinyUI::Exec::Checkbox("Input Widgets", &showInputTest);
        
        tinyUI::Exec::Separator();
        
        if (tinyUI::Exec::Button("Show All")) {
            showTestTree = showTestInspector = showDragDropTest = showButtonTest = showInputTest = true;
        }
        tinyUI::Exec::SameLine();
        if (tinyUI::Exec::Button("Hide All Tests")) {
            showTestTree = showTestInspector = showDragDropTest = showButtonTest = showInputTest = false;
        }
        
        tinyUI::Exec::Separator();
        
        static bool showImGuiDemo = false;
        tinyUI::Exec::Checkbox("ImGui Demo Window", &showImGuiDemo);
        if (showImGuiDemo) {
            tinyUI::Exec::ShowDemoWindow(&showImGuiDemo);
        }
        
        tinyUI::Exec::End();
    }
}
