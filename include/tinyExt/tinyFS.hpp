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
        dataToFile_.clear();
    }

// ---------- Basic access ----------

    [[nodiscard]] tinyHandle rootHandle() const noexcept { return rootHandle_; }

    // Case sensitivity control
    [[nodiscard]] bool caseSensitive() const noexcept { return caseSensitive_; }
    void setCaseSensitive(bool caseSensitive) noexcept { caseSensitive_ = caseSensitive; }

// ---------- Bidirectional mapping: Data ↔ File ----------

    // Get the file handle that contains specific registry data
    [[nodiscard]] tinyHandle dataToFileHandle(const typeHandle& dataHandle) const noexcept {
        auto it = dataToFile_.find(dataHandle);
        return (it != dataToFile_.end()) ? it->second : tinyHandle();
    }

    // Template overload for convenience
    template<typename T>
    [[nodiscard]] tinyHandle dataToFileHandle(tinyHandle dataHandle) const noexcept {
        return dataToFileHandle(typeHandle::make<T>(dataHandle));
    }

    // Set root display name (full on-disk path etc.)
    void setRootPath(const std::string& rootPath) noexcept {
        Node* root = fnodes_.get(rootHandle_);
        if (root) root->name = rootPath;
    }

// ---------- Creation ----------

// Folder creation (non-template overload)
    tinyHandle addFolder(tinyHandle parentHandle, const std::string& name, Node::CFG cfg = {}) {
        return addFNodeImpl(parentHandle, name, cfg);
    }
    tinyHandle addFolder(const std::string& name, Node::CFG cfg = {}) {
        return addFolder(rootHandle_, name, cfg);
    }

    // File creation (templated, pass pointer to data)
    template<typename T>
    tinyHandle addFile(tinyHandle parentHandle, const std::string& name, T&& data, Node::CFG cfg = {}) {
        return addFNodeImpl(parentHandle, name, std::forward<T>(data), cfg);
    }

    template<typename T>
    tinyHandle addFile(const std::string& name, T&& data, Node::CFG cfg = {}) {
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

// -------------------- Name Resolution --------------------

    void fRename(tinyHandle fileHandle, const std::string& newName) {
        Node* node = fnodes_.get(fileHandle);
        if (!node) return;

        tinyHandle parentHandle = node->parent;
        Node* parentNode = fnodes_.get(parentHandle);
        if (!parentNode) return;

        // Resolve name conflicts
        std::string resolvedName = resolveRepeatName(parentHandle, newName, fileHandle);

        // Update parent's child map
        parentNode->childMap_.erase(node->name);
        parentNode->childMap_[resolvedName] = fileHandle;

        // Update node's name
        node->name = resolvedName;
    }

    const char* fName(tinyHandle fileHandle, bool fullPath = false, const char* rootAlias = nullptr) const noexcept {
        const Node* node = fnodes_.get(fileHandle);
        if (!node) return nullptr;

        if (!fullPath) return node->name.c_str();

        if (filePathCache_.find(fileHandle) == filePathCache_.end()) return nullptr;

        std::vector<std::string> path = filePathCache_.at(fileHandle);
        if (rootAlias && !path.empty()) path[0] = rootAlias;

        static thread_local std::string fullPathStr;
        std::ostringstream oss;
        for (size_t i = 0; i < path.size(); ++i) {
            oss << path[i];
            if (i + 1 < path.size()) oss << "/";
        }
        fullPathStr = oss.str();
        return fullPathStr.c_str();
    }

// -------------------- Move with cycle prevention --------------------

    bool fMove(tinyHandle nodeHandle, tinyHandle parentHandle) {
        parentHandle = parentHandle.valid() ? parentHandle : rootHandle_;
        if (nodeHandle == parentHandle) return false; // Move to self

        Node* node = fnodes_.get(nodeHandle);
        if (!node) return false;

        if (node->parent == parentHandle) return true; // already there

        Node* newParent = fnodes_.get(parentHandle);
        if (!newParent || newParent->isFile()) return false;

        // Avoid parent being a descendant of node
        if (isDescendant(nodeHandle, parentHandle)) return false;

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

    size_t fRemove(tinyHandle handle, bool recursive = true) noexcept {
        Node* node = fnodes_.get(handle);
        if (!node || !node->deletable()) return 0;

        tinyHandle rescueParent = node->parent;

        size_t rmCount = 0;
        fRemoveRecursive(handle, rescueParent, rmCount, recursive);
        return rmCount;
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

    [[nodiscard]] const TypeInfo* fTypeInfo(tinyHandle fileHandle) const noexcept {
        const Node* node = fnodes_.get(fileHandle);
        if (!node) return nullptr;

        auto it = typeInfos_.find(node->tHandle.typeIndex);
        return (it != typeInfos_.end()) ? &it->second : nullptr;
    }

    [[nodiscard]] TypeExt fTypeExt(tinyHandle fileHandle) const noexcept {
        const Node* node = fnodes_.get(fileHandle);
        if (!node) return TypeExt();

        return typeExt(node->tHandle.typeIndex);
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

    template<typename T> T* rGet(typeHandle th) noexcept { return registry_.get<T>(th); }
    template<typename T> const T* rGet(typeHandle th) const noexcept { return registry_.get<T>(th); }

    template<typename T> T* rGet(tinyHandle h) noexcept { return registry_.get<T>(h); }
    template<typename T> const T* rGet(tinyHandle h) const noexcept { return registry_.get<T>(h); }

    template<typename T>
    bool rHas(const tinyHandle& handle) const noexcept { return registry_.has<T>(handle); }
    bool rHas(const typeHandle& th) const noexcept { return registry_.has(th); }

    template<typename T> typeHandle rAdd(T&& val) { return registry_.add<T>(std::forward<T>(val)); }

    template<typename T>
    void rRemove(const tinyHandle& handle) noexcept { rRemove(typeHandle<T>(handle)); }
    void rRemove(const typeHandle& th) noexcept { if (registry_.has(th)) registry_.tRemove(th); }

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

// ---- Some helpful utilities and function that make this class act like an actual filesystem manager ----

    static std::string name(const std::string& filepath) noexcept {
        size_t pos = filepath.find_last_of("/\\");
        if (pos == std::string::npos) return filepath;
        return filepath.substr(pos + 1);
    }

    static std::string ext(const std::string& filename) noexcept {
        size_t pos = filename.find_last_of('.');

        if (pos == std::string::npos || pos == filename.size() - 1) return "";
        return filename.substr(pos + 1);
    }

    static std::string nameDotExt(const std::string& filename) noexcept {
        size_t pos = filename.find_last_of('.');

        if (pos == std::string::npos) return filename;
        return filename.substr(0, pos);
    }

private:
    tinyPool<Node> fnodes_;
    tinyRegistry registry_;
    tinyHandle rootHandle_;
    bool caseSensitive_{false};

    // Bidirectional mapping: typeHandle -> file node handle
    std::unordered_map<typeHandle, tinyHandle> dataToFile_;

    // Cache file's full path
    std::unordered_map<tinyHandle, std::vector<std::string>> filePathCache_;

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

    bool hasRepeatName(tinyHandle parentHandle, const std::string& name, tinyHandle excludeHandle = tinyHandle()) const noexcept {
        if (!fnodes_.valid(parentHandle)) return false;

        const Node* parent = fnodes_.get(parentHandle);
        if (!parent) return false;

        for (const tinyHandle& childHandle : parent->children) {
            if (childHandle == excludeHandle) continue; // Skip excluded handle

            const Node* child = fnodes_.get(childHandle);
            if (!child) continue;

            if (namesEqual(child->name, name)) return true;
        }

        return false;
    }

    std::string resolveRepeatName(tinyHandle parentHandle, const std::string& baseName, 
                                   tinyHandle excludeHandle = tinyHandle()) const noexcept {
        if (!fnodes_.valid(parentHandle)) return baseName;

        const Node* parent = fnodes_.get(parentHandle);
        if (!parent) return baseName;

        if (!hasRepeatName(parentHandle, baseName, excludeHandle)) return baseName;

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

    void cacheFullPath(tinyHandle fileHandle) noexcept {
        const Node* node = fnodes_.get(fileHandle);
        if (!node) return;

        std::vector<std::string> pathComponents;

        tinyHandle currentHandle = fileHandle;
        while (currentHandle.valid()) {
            const Node* currentNode = fnodes_.get(currentHandle);
            if (!currentNode) break;

            pathComponents.push_back(currentNode->name);
            currentHandle = currentNode->parent;
        }

        std::reverse(pathComponents.begin(), pathComponents.end());

        filePathCache_[fileHandle] = std::move(pathComponents);
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
        if (!parent || parent->isFile()) return tinyHandle();

        std::string resolvedName = resolveRepeatName(parentHandle, name);
        
        Node child;
        child.name = resolvedName;
        child.parent = parentHandle;
        child.cfg = cfg;
        child.type = Node::Type::File;
        child.tHandle = registry_.add(std::forward<T>(data));

        tinyHandle h = fnodes_.add(std::move(child));
        parent->addChild(h, resolvedName);

        dataToFile_[child.tHandle] = h;
        cacheFullPath(h);

        ensureTypeInfo(child.tHandle.typeIndex);

        return h;
    }

    // Overload for folder creation
    tinyHandle addFNodeImpl(tinyHandle parentHandle, const std::string& name, Node::CFG cfg) {
        Node* parent = fnodes_.get(parentHandle);
        if (!parent || parent->isFile()) return tinyHandle();

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

        if (dataToFile_.find(dataHandle) != dataToFile_.end()) dataToFile_.erase(dataHandle);
        if (filePathCache_.find(nodeHandle) != filePathCache_.end()) filePathCache_.erase(nodeHandle);

        registry_.tRemove(dataHandle);

        Node* parent = fnodes_.get(node->parent);
        if (parent) parent->removeChild(nodeHandle, node->name);

        fnodes_.remove(nodeHandle);
    }

    // Internal recursive function implementing the new removal logic
    void fRemoveRecursive(tinyHandle handle, tinyHandle rescueParent, size_t& rmCount, bool recursive) {
        Node* node = fnodes_.get(handle);
        if (!node) return;

        std::vector<tinyHandle> childCopy = node->children;
        for (tinyHandle ch : childCopy) {
            Node* child = fnodes_.get(ch);
            bool canRemove = child && child->deletable() && recursive;

            if (!canRemove) fMove(ch, rescueParent);
            else fRemoveRecursive(ch, rescueParent, rmCount, recursive);
        }

        // Folder or safe type or instant delete
        if (node->isFolder() || safeDelete(node->typeIndex())) {
            fRemoveTrue(handle, node->tHandle);
        // Unsafe type queue for removal
        } else {
            std::type_index typeIndx = node->typeIndex();

            RmQueueEntry entry;
            entry.fnodeHandle = handle;
            entry.rescueParent = rescueParent;
            entry.dataHandle = node->tHandle;

            rmQueues_[typeIndx].push_back(entry);
        }

        // Add to removal count
        rmCount++;
    }
};

using tinyNodeFS = tinyFS::Node;
