#pragma once

#include "tinyExt/tinyRegistry.hpp"

#include <string>
#include <sstream>
#include <unordered_map>

#include <functional>
#include <algorithm>

// tinyFS is like an extension of tinyRegistry
// that adds a virtual file system structure
// for easier organization of resources.

class tinyFS {
public:
    struct Node {
        std::string name;                     // segment name (relative)
        tinyHandle parent;                    // parent node handle
        std::vector<tinyHandle> children;     // child node handles (ordered)
        std::unordered_map<std::string, tinyHandle> childMap_; // O(1) name lookup
        typeHandle tHandle;                   // metadata / registry handle if file

        enum class Type { Folder, File, Other } type = Type::Folder;

        struct CFG {
            bool hidden = false;
            bool deletable = true;
        } cfg;

        [[nodiscard]] std::type_index typeIndex() const noexcept { return tHandle.typeIndex; }

        [[nodiscard]] bool hidden() const noexcept { return cfg.hidden; }
        [[nodiscard]] bool deletable() const noexcept { return cfg.deletable; }

        template<typename T>
        [[nodiscard]] bool isType() const noexcept { return tHandle.isType<T>(); }
        [[nodiscard]] bool isFile() const noexcept { return type == Type::File; }
        [[nodiscard]] bool isFolder() const noexcept { return type == Type::Folder; }

        [[nodiscard]] bool hasData() const noexcept { return tHandle.valid(); }

        [[nodiscard]] bool hasChild(tinyHandle childHandle) const noexcept {
            return std::find(children.begin(), children.end(), childHandle) != children.end();
        }
        
        // O(1) child lookup by name
        [[nodiscard]] tinyHandle findChild(const std::string& childName) const noexcept {
            auto it = childMap_.find(childName);
            return (it != childMap_.end()) ? it->second : tinyHandle();
        }

        [[nodiscard]] int childIndex(tinyHandle childHandle) const noexcept {
            auto it = std::find(children.begin(), children.end(), childHandle);
            return (it != children.end()) ? static_cast<int>(std::distance(children.begin(), it)) : -1;
        }

        int addChild(tinyHandle childHandle, const std::string& childName) { // Push back may throw
            if (hasChild(childHandle)) return -1;
            children.push_back(childHandle);
            childMap_[childName] = childHandle;
            return static_cast<int>(children.size()) - 1;
        }

        bool removeChild(tinyHandle childHandle, const std::string& childName) noexcept {
            int index = childIndex(childHandle);
            if (index != -1) {
                children.erase(children.begin() + index);
                childMap_.erase(childName);
                return true;
            }
            return false;
        }
    };

// -------------------- Public Interface --------------------

    tinyFS() noexcept {
        Node rootNode;
        rootNode.name = ".root";
        rootNode.parent = tinyHandle();
        rootNode.type = Node::Type::Folder;
        rootNode.cfg.deletable = false;
        rootHandle_ = fnodes_.add(std::move(rootNode));
    }

    ~tinyFS() noexcept {
        for (const auto& tIndex : typeOrder_) {
            registry_.clear(tIndex);
        }
    }

// ---------- Basic access ----------

    [[nodiscard]] tinyHandle rootHandle() const noexcept { return rootHandle_; }

    // Case sensitivity control
    [[nodiscard]] bool caseSensitive() const noexcept { return caseSensitive_; }
    void setCaseSensitive(bool caseSensitive) noexcept { caseSensitive_ = caseSensitive; }

    // Set root display name (full on-disk path etc.)
    void setRootPath(const std::string& rootPath) noexcept {
        Node* root = fnodes_.get(rootHandle_);
        if (root) root->name = rootPath;
    }

// ---------- Creation ----------

// Folder creation (non-template overload)
    [[nodiscard]] tinyHandle addFolder(tinyHandle parentHandle, const std::string& name, Node::CFG cfg = {}) {
        return addFNodeImpl(parentHandle, name, cfg);
    }
    [[nodiscard]] tinyHandle addFolder(const std::string& name, Node::CFG cfg = {}) {
        return addFolder(rootHandle_, name, cfg);
    }

    // File creation (templated, pass pointer to data)
    template<typename T>
    [[nodiscard]] tinyHandle addFile(tinyHandle parentHandle, const std::string& name, T&& data, Node::CFG cfg = {}) {
        return addFNodeImpl(parentHandle, name, std::forward<T>(data), cfg);
    }

    template<typename T>
    [[nodiscard]] tinyHandle addFile(const std::string& name, T&& data, Node::CFG cfg = {}) {
        return addFile(rootHandle_, name, std::forward<T>(data), cfg);
    }

// -------------------- Special Type Handlers --------------------

    struct TypeExt {
        // Identification
        std::string ext;
        float color[3];

        // Assume folder, max priority
        TypeExt(const std::string& ext = "", float r = 1.0f, float g = 1.0f, float b = 1.0f)
        : ext(ext) { color[0] = r; color[1] = g; color[2] = b; }

        bool operator<(const TypeExt& other) const noexcept { return ext < other.ext; }
        bool operator>(const TypeExt& other) const noexcept { return other < *this; }

        // Compare for equality
        bool operator==(const TypeExt& other) const noexcept {
            return 
                ext == other.ext &&
                color[0] == other.color[0] &&
                color[1] == other.color[1] &&
                color[2] == other.color[2];
        }

        bool empty() const noexcept { return ext.empty(); }
        const char* c_str() const noexcept { return ext.c_str(); }
    };

    using RmRuleFn = std::function<bool(const void*)>;

    struct IRmRule {
        virtual ~IRmRule() = default;
        virtual bool check(const void* dataPtr) const noexcept = 0;
    };

    template<typename T>
    struct RmRule : IRmRule {
        std::function<bool(const T&)> rule;

        RmRule() = default;

        explicit RmRule(std::function<bool(const T&)> r) : rule(std::move(r)) {}

        // Default behavior: allow removal if no rule set
        bool check(const void* dataPtr) const noexcept override {
            try {
                if (!rule) return true;
                return rule(*static_cast<const T*>(dataPtr));
            } catch (...) {
                return true;
            }
        }
    };

    struct TypeInfo {
        TypeExt typeExt;

        uint8_t priority = 0; // Higher priority = delete last
        bool safeDelete = false;
        std::unique_ptr<IRmRule> rmRule;

        const char* c_str() const noexcept { return typeExt.c_str(); }

        template<typename T>
        void setRmRule(std::function<bool(const T&)> ruleFn) {
            rmRule = std::make_unique<RmRule<T>>(std::move(ruleFn));
        }

        void clearRmRule() noexcept { rmRule.reset(); }

        bool checkRmRule(const void* dataPtr) const noexcept {
            return rmRule ? rmRule->check(dataPtr) : true;
        }
    };

    [[nodiscard]] TypeInfo* typeInfo(std::type_index typeIndx) noexcept {
        return ensureTypeInfo(typeIndx);
    }

    template<typename T>
    [[nodiscard]] TypeInfo* typeInfo() noexcept {
        return ensureTypeInfo(std::type_index(typeid(T)));
    }

    [[nodiscard]] TypeExt typeExt(std::type_index typeIndx) const noexcept {
        auto it = typeInfos_.find(typeIndx);
        return (it != typeInfos_.end()) ? it->second.typeExt : TypeExt();
    }

    template<typename T>
    [[nodiscard]] TypeExt typeExt() const noexcept {
        return typeExt(std::type_index(typeid(T)));
    }

    [[nodiscard]] bool safeDelete(std::type_index typeIndex) const noexcept {
        auto it = typeInfos_.find(typeIndex);
        return (it != typeInfos_.end()) ? it->second.safeDelete : false;
    }

    template<typename T>
    [[nodiscard]] bool safeDelete() const noexcept {
        return safeDelete(std::type_index(typeid(T)));
    }

// -------------------- Move with cycle prevention --------------------

    bool fMove(tinyHandle nodeHandle, tinyHandle parentHandle) {
        parentHandle = parentHandle.valid() ? parentHandle : rootHandle_;
        if (nodeHandle == parentHandle) return false; // Move to self

        Node* node = fnodes_.get(nodeHandle);
        if (!node) return false;

        if (node->parent == parentHandle) return true; // already there

        // Avoid parent being a descendant of node
        if (isDescendant(nodeHandle, parentHandle)) return false;

        Node* newParent = fnodes_.get(parentHandle);
        if (!newParent) return false;
        
        // Check for name conflicts in the new parent (excluding this node)
        // We pass nodeHandle to exclude it from conflict checking
        std::string resolvedName = resolveRepeatName(parentHandle, node->name, nodeHandle);
        
        // Remove from old parent children vector
        Node* oldParent = fnodes_.get(node->parent);
        if (oldParent) oldParent->removeChild(nodeHandle, node->name);

        // Update node's name and parent
        node->name = resolvedName;
        node->parent = parentHandle;
        
        // Add to new parent
        newParent->addChild(nodeHandle, resolvedName);

        return true;
    }

// -------------------- "Safe" recursive remove --------------------

    bool fRemove(tinyHandle handle, bool recursive = true) noexcept {
        Node* node = fnodes_.get(handle);
        if (!node || !node->deletable()) return false;

        // Public interface - use the node's parent as the rescue parent
        tinyHandle rescueParent = node->parent;
        if (!fnodes_.valid(rescueParent)) {
            rescueParent = rootHandle_;
        }

        return fRemoveRecursive(handle, rescueParent, recursive);
    }

    bool fFlatten(tinyHandle handle) noexcept {
        return fRemove(handle, false);
    }

// ------------------- Node Inspection -------------------

    template<typename T>
    [[nodiscard]] T* fData(tinyHandle fileHandle) noexcept {
        Node* node = fnodes_.get(fileHandle);
        if (!node || !node->hasData()) return nullptr;

        return registry_.get<T>(node->tHandle);
    }

    template<typename T>
    [[nodiscard]] const T* fData(tinyHandle fileHandle) const noexcept {
        return const_cast<tinyFS*>(this)->fData<T>(fileHandle);
    }

    [[nodiscard]] typeHandle fTypeHandle(tinyHandle fileHandle) const noexcept {
        const Node* node = fnodes_.get(fileHandle);
        return node ? node->tHandle : typeHandle();
    }

    template<typename T>
    [[nodiscard]] bool fIsType(tinyHandle fileHandle) const {
        const Node* node = fnodes_.get(fileHandle);
        return node ? node->isType<T>() : false;
    }

// -------------------- Node access (highly limited) --------------------

    [[nodiscard]] const Node* fNode(tinyHandle fileHandle) const noexcept {
        return fnodes_.get(fileHandle);
    }

    [[nodiscard]] const tinyPool<Node>& fNodes() const noexcept { return fnodes_; }

// -------------------- Registry data management -------------------
    
    [[nodiscard]] tinyRegistry& registry() noexcept { return registry_; }
    [[nodiscard]] const tinyRegistry& registry() const noexcept { return registry_; }

    [[nodiscard]] void* rGet(typeHandle th) noexcept { return registry_.get(th); }
    [[nodiscard]] const void* rGet(typeHandle th) const noexcept { return registry_.get(th); }

    template<typename T>
    T* rGet(typeHandle th) noexcept { return registry_.get<T>(th); }
    template<typename T>
    const T* rGet(typeHandle th) const noexcept { return registry_.get<T>(th); }

    template<typename T>
    T* rGet(tinyHandle h) noexcept { return registry_.get<T>(h); }
    template<typename T>
    const T* rGet(tinyHandle h) const noexcept { return registry_.get<T>(h); }

    template<typename T>
    bool rHas(const tinyHandle& handle) const noexcept { return registry_.has<T>(handle); }
    bool rHas(const typeHandle& th) const noexcept { return registry_.has(th); }

    template<typename T>
    typeHandle rAdd(T&& val) {
        return registry_.add<T>(std::forward<T>(val));
    }

    template<typename T>
    void rRemove(const tinyHandle& handle) noexcept {
        rRemove(typeHandle<T>(handle));
    }

    void rRemove(const typeHandle& th) noexcept {
        if (registry_.has(th)) registry_.tRemove(th);
    }

// -------------------- FS-level Removal Queue (for deletion order control) --------------------

    struct RmQueueEntry {
        tinyHandle fnodeHandle;     // File node to remove
        tinyHandle rescueParent;    // Where to rescue if removal fails (RESCUE parent)
        typeHandle dataHandle;      // Registry data handle
    };

    // Execute removal for a specific type - processes all queued removals for that type
    template<typename T>
    void execRemove() noexcept {
        execRemove(std::type_index(typeid(T)));
    }

    void execRemove(std::type_index typeIndx) noexcept {
        auto it = rmQueues_.find(typeIndx);
        if (it == rmQueues_.end()) return;

        std::vector<RmQueueEntry>& queue = it->second;
        TypeInfo* tInfo = typeInfo(typeIndx);

        for (const auto& entry : queue) {
            void* dataPtr = registry_.get(entry.dataHandle);
            bool canRemove = dataPtr && tInfo && tInfo->checkRmRule(dataPtr);

            if (!canRemove) fMove(entry.fnodeHandle, entry.rescueParent);
            else        fRemoveTrue(entry.fnodeHandle, entry.dataHandle);
        }

        queue.clear();
    }

    void execRemoveAll() noexcept {
        for (const auto& typeIndx : typeOrder_) execRemove(typeIndx);
    }

    template<typename T>
    bool hasRmQueue() const noexcept {
        return hasRmQueue(std::type_index(typeid(T)));
    }

    bool hasRmQueue(std::type_index typeIndx) const noexcept {
        auto it = rmQueues_.find(typeIndx);
        return it != rmQueues_.end() && !it->second.empty();
    }

    // Check if there are any pending removals at all
    bool hasAnyRmQueue() const noexcept {
        for (const auto& [typeIndx, queue] : rmQueues_) {
            if (!queue.empty()) return true;
        }
        return false;
    }

private:
    tinyPool<Node> fnodes_;
    tinyRegistry registry_;
    tinyHandle rootHandle_;
    bool caseSensitive_{false};

    // Removal queue: maps type_index -> vector of RmQueueEntry
    std::unordered_map<std::type_index, std::vector<RmQueueEntry>> rmQueues_;

// -------------------- TypeInfo management --------------------

    TypeInfo* ensureTypeInfo(std::type_index typeIndx) {
        auto it = typeInfos_.find(typeIndx);
        if (it != typeInfos_.end()) return &it->second;

        TypeInfo typeInfo;
        typeInfos_[typeIndx] = std::move(typeInfo);

        typeOrder_.push_back(typeIndx);

        // Sort typeOrder_ by priority (higher priority last)
        std::sort(typeOrder_.begin(), typeOrder_.end(),
            [this](std::type_index a, std::type_index b) {
                const TypeInfo& infoA = typeInfos_.at(a);
                const TypeInfo& infoB = typeInfos_.at(b);
                return infoA.priority < infoB.priority;
            }
        );

        return &typeInfos_[typeIndx];
    }

    std::unordered_map<std::type_index, TypeInfo> typeInfos_;
    std::vector<std::type_index> typeOrder_; // For priority-based operations

// -------------------- Internal helpers --------------------

    bool namesEqual(const std::string& a, const std::string& b) const noexcept {
        if (caseSensitive_) {
            return a == b;
        } else {
            // Case-insensitive comparison
            if (a.size() != b.size()) return false;
            return std::equal(a.begin(), a.end(), b.begin(), [](char c1, char c2) {
                return std::tolower(c1) == std::tolower(c2);
            });
        }
    }

    bool hasRepeatName(tinyHandle parentHandle, const std::string& name) const noexcept {
        if (!fnodes_.valid(parentHandle)) return false;

        const Node* parent = fnodes_.get(parentHandle);
        if (!parent) return false;

        for (const tinyHandle& childHandle : parent->children) {
            const Node* child = fnodes_.get(childHandle);
            if (!child) continue;

            if (namesEqual(child->name, name)) return true;
        }

        return false;
    }
    
    // Check for name conflict, excluding a specific handle (useful for moves)
    bool hasRepeatNameExcept(tinyHandle parentHandle, const std::string& name, tinyHandle excludeHandle) const noexcept {
        if (!fnodes_.valid(parentHandle)) return false;

        const Node* parent = fnodes_.get(parentHandle);
        if (!parent) return false;

        for (const tinyHandle& childHandle : parent->children) {
            if (childHandle == excludeHandle) continue; // Skip the node being moved
            
            const Node* child = fnodes_.get(childHandle);
            if (!child) continue;

            if (namesEqual(child->name, name)) return true;
        }

        return false;
    }

    std::string resolveRepeatName(tinyHandle parentHandle, const std::string& baseName, 
                                   tinyHandle excludeHandle = tinyHandle()) const noexcept {
        if (!fnodes_.valid(parentHandle)) return baseName;

        // Check if parent has children with the same name
        const Node* parent = fnodes_.get(parentHandle);
        if (!parent) return baseName;

        // Fast path: if no conflict (excluding the specified handle), return immediately
        if (excludeHandle.valid()) {
            if (!hasRepeatNameExcept(parentHandle, baseName, excludeHandle)) {
                return baseName;
            }
        } else {
            if (!hasRepeatName(parentHandle, baseName)) {
                return baseName;
            }
        }

        // Find the highest numbered suffix
        size_t maxNumberedIndex = 0;
        
        for (const tinyHandle& childHandle : parent->children) {
            if (childHandle == excludeHandle) continue; // Skip excluded handle
            
            const Node* child = fnodes_.get(childHandle);
            if (!child) continue;

            // Check if name matches baseName exactly
            if (namesEqual(child->name, baseName)) {
                maxNumberedIndex = std::max(maxNumberedIndex, size_t(1));
            }
            // Check for numbered variants: "baseName (n)"
            else if (child->name.size() > baseName.size() + 3) { // " (n)" is at least 4 chars
                // Check if it starts with baseName
                if (child->name.compare(0, baseName.size(), baseName) == 0) {
                    // Check if followed by " (number)"
                    size_t pos = baseName.size();
                    if (child->name[pos] == ' ' && child->name[pos + 1] == '(') {
                        size_t endPos = child->name.find(')', pos + 2);
                        if (endPos != std::string::npos && endPos == child->name.size() - 1) {
                            // Extract number
                            std::string numStr = child->name.substr(pos + 2, endPos - pos - 2);
                            try {
                                size_t num = std::stoull(numStr);
                                maxNumberedIndex = std::max(maxNumberedIndex, num);
                            } catch (...) {
                                // Invalid number, ignore
                            }
                        }
                    }
                }
            }
        }

        // Return the next available name
        return baseName + " (" + std::to_string(maxNumberedIndex + 1) + ")";
    }

    bool isDescendant(tinyHandle possibleAncestor, tinyHandle possibleDescendant) const noexcept {
        if (!fnodes_.valid(possibleAncestor) || !fnodes_.valid(possibleDescendant))
            return true; // For safety, treat invalid handles as descendants

        const Node* current = fnodes_.get(possibleDescendant);
        while (current && fnodes_.valid(current->parent)) {
            if (current->parent == possibleAncestor) return true; // Found ancestor
            current = fnodes_.get(current->parent);
        }
        return false;
    }

// -------------------- Internal implementations --------------------

    template<typename T>
    tinyHandle addFNodeImpl(tinyHandle parentHandle, const std::string& name, T&& data, Node::CFG cfg) {
        Node* parent = fnodes_.get(parentHandle);
        if (!parent || parent->isFile()) return tinyHandle(); // Invalid or parent is a file

        std::string resolvedName = resolveRepeatName(parentHandle, name);
        
        Node child;
        child.name = resolvedName;
        child.parent = parentHandle;
        child.cfg = cfg;
        child.type = Node::Type::File;
        child.tHandle = registry_.add(std::forward<T>(data));

        tinyHandle h = fnodes_.add(std::move(child));
        parent->addChild(h, resolvedName);

        return h;
    }

    // Overload for folder creation
    tinyHandle addFNodeImpl(tinyHandle parentHandle, const std::string& name, Node::CFG cfg) {
        Node* parent = fnodes_.get(parentHandle);
        if (!parent || parent->isFile()) return tinyHandle(); // Invalid or parent is a file

        std::string resolvedName = resolveRepeatName(parentHandle, name);
        
        Node folder;
        folder.name = resolvedName;
        folder.parent = parentHandle;
        folder.cfg = cfg;

        tinyHandle h = fnodes_.add(std::move(folder));
        parent->addChild(h, resolvedName);

        return h;
    }


    void fRemoveTrue(tinyHandle nodeHandle, typeHandle dataHandle) noexcept {
        Node* node = fnodes_.get(nodeHandle);
        if (!node) return;

        registry_.tRemove(dataHandle);

        Node* parent = fnodes_.get(node->parent);
        if (parent) parent->removeChild(nodeHandle, node->name);

        fnodes_.remove(nodeHandle);
    }

    // Internal recursive function implementing the new removal logic
    bool fRemoveRecursive(tinyHandle handle, tinyHandle rescueParent, bool recursive) {
        Node* node = fnodes_.get(handle);
        if (!node) return false;

        std::vector<tinyHandle> childCopy = node->children;
        for (tinyHandle ch : childCopy) {
            Node* child = fnodes_.get(ch);
            bool canRemove = child && child->deletable() && recursive;

            if (!canRemove) fMove(ch, rescueParent);
            else fRemoveRecursive(ch, rescueParent, recursive);
        }

        // Folder or safe type or instant delete
        if (node->isFolder() || safeDelete(node->typeIndex())) {
            fRemoveTrue(handle, node->tHandle);
            return true;
        // Unsafe type queue for removal
        } else {
            std::type_index typeIndx = node->typeIndex();

            RmQueueEntry entry;
            entry.fnodeHandle = handle;
            entry.rescueParent = rescueParent;
            entry.dataHandle = node->tHandle;

            rmQueues_[typeIndx].push_back(entry);
            return false;
        }
    }

};
