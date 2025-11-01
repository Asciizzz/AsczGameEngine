#pragma once

#include "tinyExt/tinyRegistry.hpp"

#include <string>
#include <sstream>

#include <functional>
#include <algorithm>

class tinyFS {
public:
    struct Node {
        std::string name;                     // segment name (relative)
        tinyHandle parent;                    // parent node handle
        std::vector<tinyHandle> children;     // child node handles
        typeHandle tHandle;                   // metadata / registry handle if file

        enum class Type { Folder, File, Other } type = Type::Folder;

        struct CFG {
            bool hidden = false;
            bool deletable = true;
        } cfg;

        std::type_index typeIndex() const noexcept { return tHandle.typeIndex; }

        bool hidden() const noexcept { return cfg.hidden; }
        bool deletable() const noexcept { return cfg.deletable; }

        template<typename T>
        bool isType() const noexcept { return tHandle.isType<T>(); }
        bool isFile() const noexcept { return type == Type::File; }
        bool isFolder() const noexcept { return type == Type::Folder; }

        bool hasData() const noexcept { return tHandle.valid(); }

        bool hasChild(tinyHandle childHandle) const noexcept {
            return std::find(children.begin(), children.end(), childHandle) != children.end();
        }

        int childIndex(tinyHandle childHandle) const noexcept {
            auto it = std::find(children.begin(), children.end(), childHandle);
            return (it != children.end()) ? static_cast<int>(std::distance(children.begin(), it)) : -1;
        }

        int addChild(tinyHandle childHandle) { // Push back may throw
            if (hasChild(childHandle)) return -1;
            children.push_back(childHandle);
            return static_cast<int>(children.size()) - 1;
        }

        bool removeChild(tinyHandle childHandle) noexcept {
            int index = childIndex(childHandle);
            if (index != -1) {
                children.erase(children.begin() + index);
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
        rootNode.cfg.deletable = false; // root is not deletable

        // Insert root and store explicitly the returned handle
        rootHandle_ = fnodes_.add(std::move(rootNode));
    }

    // ---------- Basic access ----------

    tinyHandle rootHandle() const noexcept { return rootHandle_; }

    // Case sensitivity control
    bool caseSensitive() const noexcept { return caseSensitive_; }
    void setCaseSensitive(bool caseSensitive) noexcept { caseSensitive_ = caseSensitive; }

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
        return addFNodeImpl<T>(parentHandle, name, std::forward<T>(data), cfg);
    }

    template<typename T>
    tinyHandle addFile(const std::string& name, T&& data, Node::CFG cfg = {}) {
        return addFile<T>(rootHandle_, name, std::forward<T>(data), cfg);
    }

    // ---------- Move with cycle prevention ----------
    
    bool fMove(tinyHandle nodeHandle, tinyHandle parentHandle) {
        parentHandle = parentHandle.valid() ? parentHandle : rootHandle_;
        if (nodeHandle == parentHandle) return false; // Move to self

        Node* node = fnodes_.get(nodeHandle);
        if (!node) return false;

        if (node->parent == parentHandle) return true; // already there

        // Avoid parent being a descendant of node
        if (isDescendant(nodeHandle, parentHandle)) return false;

        Node* newParent = fnodes_.get(parentHandle);
        if (newParent) newParent->addChild(nodeHandle);
        else return false;

        // remove from old parent children vector
        Node* oldParent = fnodes_.get(node->parent);
        if (oldParent) oldParent->removeChild(nodeHandle);

        node->name = resolveRepeatName(parentHandle, node->name);
        node->parent = parentHandle;

        return true;
    }

    // ---------- "Safe" recursive remove ----------

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

    // ---------- Data Inspection ----------

    template<typename T>
    T* fData(tinyHandle fileHandle) noexcept {
        Node* node = fnodes_.get(fileHandle);
        if (!node || !node->hasData()) return nullptr;

        return registry_.get<T>(node->tHandle);
    }

    template<typename T>
    const T* fData(tinyHandle fileHandle) const noexcept {
        return const_cast<tinyFS*>(this)->fData<T>(fileHandle);
    }

    typeHandle fTypeHandle(tinyHandle fileHandle) const noexcept {
        const Node* node = fnodes_.get(fileHandle);
        return node ? node->tHandle : typeHandle();
    }

    template<typename T>
    bool fIsType(tinyHandle fileHandle) const {
        const Node* node = fnodes_.get(fileHandle);
        return node ? node->isType<T>() : false;
    }

// -------------------- Node access (highly limited) --------------------

    const Node* fNode(tinyHandle fileHandle) const noexcept {
        return fnodes_.get(fileHandle);
    }

    const tinyPool<Node>& fNodes() const noexcept { return fnodes_; }

// -------------------- Registry data management -------------------
    
    // No non-const access for safety
    const tinyRegistry& registry() const noexcept { return registry_; }

    void* rGet(typeHandle th) noexcept { return registry_.get(th); }
    const void* rGet(typeHandle th) const noexcept { return registry_.get(th); }

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
        if (!registry_.has(th)) return; // nothing to remove :/

        if (safeDelete(th.typeIndex)) registry_.tInstaRm(th); // Safe to remove instantly
        else                          registry_.tQueueRm(th); // Queue for pending removal
    }

    template<typename T>
    bool rTHasPendingRms() const noexcept {
        return registry_.tHasPendingRms<T>();
    }
    bool rTHasPendingRms(std::type_index typeIndx) const noexcept {
        return registry_.tHasPendingRms(typeIndx);
    }

    template<typename T>
    void rTFlushRm(uint32_t index) noexcept {
        registry_.tFlushRm<T>(index);
    }
    void rTFlushRm(std::type_index typeIndx, uint32_t index) noexcept {
        registry_.tFlushRm(typeIndx, index);
    }

    template<typename T>
    void rTFlushAllRms() noexcept {
        registry_.tFlushAllRms<T>();
    }
    void rTFlushAllRms(std::type_index typeIndx) noexcept {
        registry_.tFlushAllRms(typeIndx);
    }


    bool rHasPendingRms() const noexcept {
        return registry_.hasPendingRms();
    }

    void rFlushAllRms() noexcept {
        // Perform specific type flush in order of priority
        // using registry_.tFlushAllRms(std::type_index)

        for (const auto& typeIndx : typeOrder_) {
            const TypeInfo& info = typeInfos_.at(typeIndx);

            registry_.tFlushAllRms(typeIndx);
        }

        // Flush everything at the end to be sure
        registry_.flushAllRms();
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

        // Set function for T
        void set(std::function<bool(const T&)> r) noexcept {
            rule = std::move(r);
        }

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

    TypeInfo* typeInfo(std::type_index typeIndx) noexcept {
        return ensureTypeInfo(typeIndx);
    }

    template<typename T>
    TypeInfo* typeInfo() noexcept {
        return ensureTypeInfo(std::type_index(typeid(T)));
    }

    TypeExt typeExt(std::type_index typeIndx) const noexcept {
        auto it = typeInfos_.find(typeIndx);
        return (it != typeInfos_.end()) ? it->second.typeExt : TypeExt();
    }

    template<typename T>
    TypeExt typeExt() const noexcept {
        return typeExt(std::type_index(typeid(T)));
    }


    template<typename T>
    bool safeDelete() const noexcept {
        return safeDelete(std::type_index(typeid(T)));
    }

    bool safeDelete(std::type_index typeIndex) const noexcept {
        auto it = typeInfos_.find(typeIndex);
        return (it != typeInfos_.end()) ? it->second.safeDelete : false;
    }

private:
    tinyPool<Node> fnodes_;
    tinyRegistry registry_;
    tinyHandle rootHandle_;
    bool caseSensitive_{false};

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

    std::string resolveRepeatName(tinyHandle parentHandle, const std::string& baseName) const noexcept {
        if (!fnodes_.valid(parentHandle)) return baseName;

        // Check if parent has children with the same name
        const Node* parent = fnodes_.get(parentHandle);
        if (!parent) return baseName;

        std::string resolvedName = baseName;
        size_t maxNumberedIndex = 0;

        for (const tinyHandle& childHandle : parent->children) {
            const Node* child = fnodes_.get(childHandle);
            if (!child) continue;

            // Check if name matches baseName exactly
            if (namesEqual(child->name, resolvedName)) {
                maxNumberedIndex += 1;
                resolvedName = baseName + " (" + std::to_string(maxNumberedIndex) + ")";
            }
        }

        return resolvedName;
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

        Node child;
        child.name = resolveRepeatName(parentHandle, name);
        child.parent = parentHandle;
        child.cfg = cfg;
        child.type = Node::Type::File;
        child.tHandle = registry_.add(std::forward<T>(data));

        tinyHandle h = fnodes_.add(std::move(child));
        parent->addChild(h);

        return h;
    }

    // Overload for folder creation
    tinyHandle addFNodeImpl(tinyHandle parentHandle, const std::string& name, Node::CFG cfg) {
        Node* parent = fnodes_.get(parentHandle);
        if (!parent || parent->isFile()) return tinyHandle(); // Invalid or parent is a file

        Node folder;
        folder.name = resolveRepeatName(parentHandle, name);
        folder.parent = parentHandle;
        folder.cfg = cfg;

        tinyHandle h = fnodes_.add(std::move(folder));
        parent->addChild(h);

        return h;
    }

    // Internal recursive function that tracks the original parent for non-deletable rescues
    bool fRemoveRecursive(tinyHandle handle, tinyHandle rescueParent, bool recursive) {
        Node* node = fnodes_.get(handle);
        if (!node) return false;

        // Remove with regards to registry data
        if (void* dataPtr = registry_.get(node->tHandle)) {
            TypeInfo* tInfo = typeInfo(node->tHandle.typeIndex);

            if (tInfo && !tInfo->checkRmRule(dataPtr)) {
                fMove(handle, rescueParent);
                return false;
            }

            rRemove(node->tHandle); // remove data from registry
        }

        // remove from parent children list
        if (fnodes_.valid(node->parent)) {
            Node* parent = fnodes_.get(node->parent);
            parent->removeChild(handle);
        }

        // copy children to avoid mutation during recursion
        std::vector<tinyHandle> childCopy = node->children;
        // Sort children by priority (low to high - low delete first)
        std::sort(childCopy.begin(), childCopy.end(), [this](tinyHandle a, tinyHandle b) {
            Node* nodeA = fnodes_.get(a);
            Node* nodeB = fnodes_.get(b);
            if (!nodeA || !nodeB) return false;

            // Guaranteed TypeInfo presence
            uint8_t prioA = typeInfo(nodeA->tHandle.typeIndex)->priority;
            uint8_t prioB = typeInfo(nodeB->tHandle.typeIndex)->priority;

            return prioA < prioB; // lower priority first
        });

        for (tinyHandle ch : childCopy) {
            Node* child = fnodes_.get(ch);
            if (!child) continue;

            bool canRemove = child->deletable() && recursive;
            
            TypeInfo* tInfo = typeInfo(child->tHandle.typeIndex);

            // Can remove if satisfies type info rule
            canRemove = canRemove && (!tInfo || tInfo->checkRmRule(registry_.get(child->tHandle)));

            if (canRemove) fRemoveRecursive(ch, rescueParent, recursive);
            else           fMove(ch, rescueParent);
        }

        // finally remove node from pool
        fnodes_.instaRm(handle);

        return true;
    }

};
