#pragma once

#include "TinyExt/TinyRegistry.hpp"

#include <string>
#include <sstream>

class TinyFS {
public:
    struct Node {
        std::string name;                     // segment name (relative)
        TinyHandle parent;                    // parent node handle
        std::vector<TinyHandle> children;     // child node handles
        TypeHandle tHandle;                   // metadata / registry handle if file

        enum class Type { Folder, File, Other } type = Type::Folder;

        struct CFG {
            bool hidden = false;
            bool deletable = true;
        } cfg;

        bool hidden() const { return cfg.hidden; }
        bool deletable() const { return cfg.deletable; }

        template<typename T>
        bool isType() const { return tHandle.isType<T>(); }
        bool isFile() const { return type == Type::File; }
        bool isFolder() const { return type == Type::Folder; }

        bool hasData() const { return tHandle.valid(); }

        bool hasChild(TinyHandle childHandle) const {
            // Check if childHandle exists in children vector
            return std::find(children.begin(), children.end(), childHandle) != children.end();
        }

        int childIndex(TinyHandle childHandle) const {
            // Return the index of the child if found, -1 otherwise
            auto it = std::find(children.begin(), children.end(), childHandle);
            return (it != children.end()) ? static_cast<int>(std::distance(children.begin(), it)) : -1;
        }

        int addChild(TinyHandle childHandle) {
            if (hasChild(childHandle)) return -1;
            children.push_back(childHandle);
            return static_cast<int>(children.size()) - 1;
        }

        bool removeChild(TinyHandle childHandle) {
            int index = childIndex(childHandle);
            if (index != -1) {
                children.erase(children.begin() + index);
                return true;
            }
            return false;
        }
    };

    struct TypeExt {
        // Identification
        std::string ext;
        bool safeDelete; // CRITICAL
        uint8_t priority;

        // Safe Delete explanation:
        // =
        // Some files' data contains Vulkan resources which
        // could potentially be in use (command buffers etc.)
        // thus cannot be safely deleted at arbitrary times.
        // -
        // This flag indicates whether the file can be instantly
        // delete or add to a pending deletion queue for further
        // deletion processing.
        // -
        // For safety, all types default safeDelete = false;

        // Style
        float color[3];

        // Assume folder, max priority
        TypeExt(const std::string& ext = "", bool safeDelete = false, uint8_t priority = UINT8_MAX, float r = 1.0f, float g = 1.0f, float b = 1.0f)
        : ext(ext), priority(priority), safeDelete(safeDelete) {
            color[0] = r;
            color[1] = g;
            color[2] = b;
        }

        // Helpful comparison operator for sorting
        // Order: priority desc, then ext asc
        bool operator<(const TypeExt& other) const {
            if (priority != other.priority) {
                return priority > other.priority; // Higher priority first
            }
            return ext < other.ext; // Lexicographical order
        }

        bool operator>(const TypeExt& other) const {
            return other < *this;
        }

        // Compare for equality
        bool operator==(const TypeExt& other) const {
            return 
                ext == other.ext &&
                priority == other.priority &&
                color[0] == other.color[0] &&
                color[1] == other.color[1] &&
                color[2] == other.color[2];
        }

        // Implicit conversion to string for easy access
        operator std::string() const { return ext; }

        bool empty() const { return ext.empty(); }
    };


    TinyFS() {
        Node rootNode;
        rootNode.name = ".root";
        rootNode.parent = TinyHandle();
        rootNode.type = Node::Type::Folder;
        rootNode.cfg.deletable = false; // root is not deletable

        // Insert root and store explicitly the returned handle
        rootHandle_ = fnodes_.add(std::move(rootNode));
    }

    // ---------- Basic access ----------

    TinyHandle rootHandle() const { return rootHandle_; }

    // Case sensitivity control
    bool caseSensitive() const { return caseSensitive_; }
    void setCaseSensitive(bool caseSensitive) { caseSensitive_ = caseSensitive; }

    // Set root display name (full on-disk path etc.)
    void setRootPath(const std::string& rootPath) {
        Node* root = fnodes_.get(rootHandle_);
        if (root) root->name = rootPath;
    }

    // ---------- Creation ----------

    // Folder creation (non-template overload)
    TinyHandle addFolder(TinyHandle parentHandle, const std::string& name, Node::CFG cfg = {}) {
        return addFNodeImpl<void>(parentHandle, name, nullptr, cfg);
    }
    TinyHandle addFolder(const std::string& name, Node::CFG cfg = {}) {
        return addFolder(rootHandle_, name, cfg);
    }

    // File creation (templated, pass pointer to data)
    template<typename T>
    TinyHandle addFile(TinyHandle parentHandle, const std::string& name, T* data, Node::CFG cfg = {}) {
        return addFNodeImpl<T>(parentHandle, name, data, cfg);
    }
    template<typename T>
    TinyHandle addFile(const std::string& name, T* data, Node::CFG cfg = {}) {
        return addFile(rootHandle_, name, data, cfg);
    }

    // ---------- Move with cycle prevention ----------
    
    bool fMove(TinyHandle nodeHandle, TinyHandle newParent) {
        if (nodeHandle == newParent) return false; // Move to self

        // prevent moving under descendant (no cycles)
        Node* parent = fnodes_.get(newParent);
        if (parent && parent->hasChild(nodeHandle)) return false;

        Node* node = fnodes_.get(nodeHandle);
        if (!node) return false;

        // resolve name conflicts
        node->name = resolveRepeatName(newParent, node->name);

        // remove from old parent children vector
        if (fnodes_.isValid(node->parent)) {
            Node* oldParent = fnodes_.get(node->parent);
            if (oldParent) oldParent->removeChild(nodeHandle);
        }

        Node* newP = fnodes_.get(newParent);
        if (newP) newP->addChild(nodeHandle);

        // attach to new parent
        node->parent = newParent;

        return true;
    }

    // ---------- "Safe" recursive remove ----------

    bool fRemove(TinyHandle handle, bool recursive = true) {
        Node* node = fnodes_.get(handle);
        if (!node || !node->deletable()) return false;

        // Public interface - use the node's parent as the rescue parent
        TinyHandle rescueParent = node->parent;
        if (!fnodes_.isValid(rescueParent)) {
            rescueParent = rootHandle_;
        }

        return fRemoveRecursive(handle, rescueParent, recursive);
    }

    bool fFlatten(TinyHandle handle) {
        return fRemove(handle, false);
    }

    // ---------- Data Inspection ----------

    template<typename T>
    T* fData(TinyHandle fileHandle) {
        Node* node = fnodes_.get(fileHandle);
        if (!node || !node->hasData()) return nullptr;

        return registry_.get<T>(node->tHandle);
    }

    TypeHandle fTypeHandle(TinyHandle fileHandle) const {
        const Node* node = fnodes_.get(fileHandle);
        return node ? node->tHandle : TypeHandle();
    }

    template<typename T>
    bool fIs(TinyHandle fileHandle) const {
        const Node* node = fnodes_.get(fileHandle);
        return node ? node->isType<T>() : false;
    }

    // ---------- Node access ----------

    const Node* fNode(TinyHandle fileHandle) const {
        return fnodes_.get(fileHandle);
    }

    const TinyPool<Node>& fNodes() const { return fnodes_; }

    // ---------- Type extension management ----------

    template<typename T>
    void setTypeExt(const std::string& ext, bool safeDelete = true, uint8_t priority = 0, float r = 1.0f, float g = 1.0f, float b = 1.0f) {
        TypeExt typeExt;
        typeExt.ext = ext;
        typeExt.safeDelete = safeDelete;
        typeExt.priority = priority;
        typeExt.color[0] = r;
        typeExt.color[1] = g;
        typeExt.color[2] = b;
        typeHashToExt[typeid(T).hash_code()] = typeExt;
    }

    // Get the full TypeExt info for a type
    TypeExt typeExt(size_t typeHash) const {
        auto it = typeHashToExt.find(typeHash);
        return (it != typeHashToExt.end()) ? it->second : TypeExt();
    }

    template<typename T>
    TypeExt typeExt() const {
        return typeExt(typeid(T).hash_code());
    }

    // Get the full TypeExt info for a file
    TypeExt fTypeExt(TinyHandle fileHandle) const {
        const Node* node = fnodes_.get(fileHandle);

        // Invalid, minimum priority
        if (!node) return TypeExt();

        // Folder type, max priority
        if (node->isFolder()) return TypeExt("", true, UINT8_MAX);

        return typeExt(node->tHandle.typeHash);
    }

    bool fSafeDelete(TinyHandle fileHandle) const {
        return fTypeExt(fileHandle).safeDelete;
    }

    template<typename T>
    bool safeDelete() const {
        return typeExt<T>().safeDelete;
    }

    bool safeDelete(size_t typeHash) const {
        return typeExt(typeHash).safeDelete;
    }

    // ---------- Registry data management ----------

    void* rGet(TypeHandle th) { return registry_.get(th); }

    const void* rGet(TypeHandle th) const { return registry_.get(th); }

    template<typename T>
    T* rGet(TypeHandle th) { return registry_.get<T>(th); }

    template<typename T>
    const T* rGet(TypeHandle th) const { return registry_.get<T>(th);}

    template<typename T>
    T* rGet(TinyHandle h) { return registry_.get<T>(h); }

    template<typename T>
    const T* rGet(TinyHandle h) const { return registry_.get<T>(h); }

    template<typename T>
    bool rHas(const TinyHandle& handle) const { return registry_.has<T>(handle); }

    bool rHas(const TypeHandle& th) const { return registry_.has(th); }

    template<typename T>
    TypeHandle rAdd(T&& val) {
        using CleanT = std::remove_cv_t<std::remove_reference_t<T>>;
        return registry_.add<CleanT>(std::forward<T>(val));
    }

    template<typename T>
    void rRemove(const TinyHandle& handle) {
        rRemove(TypeHandle::make<T>(handle));
    }

    void rRemove(const TypeHandle& th) {
        if (!registry_.has(th)) return; // nothing to remove :/

        if (safeDelete(th.typeHash)) {
            execDelete(th); // Safe to delete immediately
        } else {
            rQueueDelete(th); // Queue for pending deletion
        }
    }

    // Pending deletion system for registry data
    template<typename T>
    void rQueueDelete(const TinyHandle& handle) {
        rQueueDelete(TypeHandle::make<T>(handle));
    }

    void rQueueDelete(const TypeHandle& th) {
        rPendingDeletions.push_back(th);
    }

    // Execute all pending deletions
    void rExecPendingDeletions() {
        for (const auto& th : rPendingDeletions) {
            registry_.remove(th);
        }
        rPendingDeletions.clear();
    }

    // Get pending deletions (read-only access for external systems like renderer)
    const std::vector<TypeHandle>& rPendingDel() const {
        return rPendingDeletions;
    }

    const TinyRegistry& registry() const { return registry_; }

private:
    TinyPool<Node> fnodes_;
    TinyRegistry registry_;
    TinyHandle rootHandle_;
    bool caseSensitive_{false}; // Global case sensitivity setting

    // Type to extension info map (using new TypeExt structure)
    UnorderedMap<size_t, TypeExt> typeHashToExt;

    // Pending deletion system for registry data
    std::vector<TypeHandle> rPendingDeletions;

    template<typename T> // True delete
    void execDelete(const TinyHandle& handle) {
        registry_.remove<T>(handle); 
    }

    void execDelete(const TypeHandle& th) {
        registry_.remove(th);
    }

    bool namesEqual(const std::string& a, const std::string& b) const {
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

    bool hasRepeatName(TinyHandle parentHandle, const std::string& name) const {
        if (!fnodes_.isValid(parentHandle)) return false;

        const Node* parent = fnodes_.get(parentHandle);
        if (!parent) return false;

        for (const TinyHandle& childHandle : parent->children) {
            const Node* child = fnodes_.get(childHandle);
            if (!child) continue;

            if (namesEqual(child->name, name)) return true;
        }

        return false;
    }

    std::string resolveRepeatName(TinyHandle parentHandle, const std::string& baseName) const {
        if (!fnodes_.isValid(parentHandle)) return baseName;

        // Check if parent has children with the same name
        const Node* parent = fnodes_.get(parentHandle);
        if (!parent) return baseName;

        std::string resolvedName = baseName;
        size_t maxNumberedIndex = 0;

        for (const TinyHandle& childHandle : parent->children) {
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

    // Implementation function: templated but uses if constexpr to allow T=void
    template<typename T>
    TinyHandle addFNodeImpl(TinyHandle parentHandle, const std::string& name, T* data, Node::CFG cfg) {
        if (!fnodes_.isValid(parentHandle)) return TinyHandle(); // invalid parent

        Node child;
        child.name = resolveRepeatName(parentHandle, name);
        child.parent = parentHandle;
        child.cfg = cfg;

        if constexpr (!std::is_same_v<T, void>) {
            if (data) {
                child.tHandle = registry_.add(*data);
                child.type = Node::Type::File;
            }
        }

        TinyHandle h = fnodes_.add(std::move(child));
        // parent might have been invalidated in a multithreaded scenario; guard
        if (fnodes_.isValid(parentHandle)) {
            Node* parent = fnodes_.get(parentHandle);
            if (parent) parent->addChild(h);
        }
        return h;
    }

    // Internal recursive function that tracks the original parent for non-deletable rescues
    bool fRemoveRecursive(TinyHandle handle, TinyHandle rescueParent, bool recursive) {
        Node* node = fnodes_.get(handle);
        if (!node) return false;

        // Note: We can assume node is deletable since non-deletable nodes are moved, not recursed

        // copy children to avoid mutation during recursion
        std::vector<TinyHandle> childCopy = node->children;
        for (TinyHandle ch : childCopy) {
            Node* child = fnodes_.get(ch);
            if (!child) continue;
            
            if (child->deletable() && recursive) {
                // Child is deletable and we're in normal delete mode - remove it recursively
                fRemoveRecursive(ch, rescueParent, recursive);
            } else {
                // Child is non-deletable OR we're in flatten mode - move it to the rescue parent
                fMove(ch, rescueParent);
            }
        }

        if (node->hasData()) {
            // Special remove (instant delete or queue)
            rRemove(node->tHandle);
        }

        // remove from parent children list
        if (fnodes_.isValid(node->parent)) {
            Node* parent = fnodes_.get(node->parent);
            parent->removeChild(handle);
        }

        // finally remove node from pool
        fnodes_.remove(handle);
        return true;
    }

};
