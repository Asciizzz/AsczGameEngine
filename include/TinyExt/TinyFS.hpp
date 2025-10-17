#pragma once

#include "TinyExt/TinyRegistry.hpp"

#include <algorithm>
#include <string>
#include <vector>
#include <iostream>
#include <type_traits>
#include <sstream>

struct TinyFNode {
    std::string name;                     // segment name (relative)
    TinyHandle parent;                    // parent node handle
    std::vector<TinyHandle> children;     // child node handles
    TypeHandle tHandle;                   // metadata / registry handle if file
    enum class Type { Folder, File, Other } type = Type::Folder;
    
    struct CFG {
        bool hidden = false;
        bool deletable = true;
    } cfg;

    TinyFNode& setHidden(bool h) { cfg.hidden = h; return *this; }
    bool hidden() const { return cfg.hidden; }

    TinyFNode& setDeletable(bool d) { cfg.deletable = d; return *this; }
    bool deletable() const { return cfg.deletable; }

    bool isFile() const { return type == Type::File; }
    bool hasData() const { return tHandle.valid(); }
};

class TinyFS {
public:
    TinyFS() {
        TinyFNode rootNode;
        rootNode.name = ".root";
        rootNode.parent = TinyHandle();
        rootNode.type = TinyFNode::Type::Folder;
        rootNode.cfg.deletable = false; // root is not deletable

        // Insert root and store explicitly the returned handle
        rootHandle_ = fnodes.insert(std::move(rootNode));
    }

    // ---------- Basic access ----------
    TinyHandle rootHandle() const { return rootHandle_; }
    TinyHandle regHandle() const { return regHandle_; }

    // Optional: expose registry for read-only access
    const TinyRegistry& registryRef() const { return registry; }
    TinyRegistry& registryRef() { return registry; }

    // Set root display name (full on-disk path etc.)
    void setRootPath(const std::string& rootPath) {
        TinyFNode* root = fnodes.get(rootHandle_);
        if (root) root->name = rootPath;
    }

    // Explicitly set registry folder handle (if you create it manually)
    void setRegistryHandle(TinyHandle h) {
        if (!fnodes.isValid(h)) return;

        TinyFNode* node = fnodes.get(h);
        node->setHidden(true).setDeletable(false);

        regHandle_ = h;
    }

    // ---------- Creation ----------

    // Folder creation (non-template overload)
    TinyHandle addFolder(TinyHandle parentHandle, const std::string& name, TinyFNode::CFG cfg = {}) {
        return addFNodeImpl<void>(parentHandle, name, nullptr, cfg);
    }
    TinyHandle addFolder(const std::string& name, TinyFNode::CFG cfg = {}) {
        return addFolder(rootHandle_, name, cfg);
    }

    // File creation (templated, pass pointer to data)
    template<typename T>
    TinyHandle addFile(TinyHandle parentHandle, const std::string& name, T* data, TinyFNode::CFG cfg = {}) {
        return addFNodeImpl<T>(parentHandle, name, data, cfg);
    }
    template<typename T>
    TinyHandle addFile(const std::string& name, T* data, TinyFNode::CFG cfg = {}) {
        return addFile(rootHandle_, name, data, cfg);
    }

    template<typename T>
    TypeHandle addToRegistry(T&& val) {
        // Remove const and reference qualifiers (f you too)
        using CleanT = std::remove_cv_t<std::remove_reference_t<T>>;
        return registry.add<CleanT>(std::forward<T>(val));
    }

    // ---------- Move with cycle prevention ----------
    void moveFNode(TinyHandle nodeHandle, TinyHandle newParent) {
        if (!fnodes.isValid(nodeHandle) || !fnodes.isValid(newParent)) return;
        if (nodeHandle == newParent) return;

        // prevent moving under descendant (no cycles)
        if (isAncestor(nodeHandle, newParent)) return;

        TinyFNode* node = fnodes.get(nodeHandle);
        if (!node) return;

        // remove from old parent children vector
        if (fnodes.isValid(node->parent)) {
            TinyFNode* oldParent = fnodes.get(node->parent);
            if (oldParent) {
                auto& s = oldParent->children;
                s.erase(std::remove(s.begin(), s.end(), nodeHandle), s.end());
            }
        }

        // attach to new parent
        node->parent = newParent;
        TinyFNode* newP = fnodes.get(newParent);
        if (newP) newP->children.push_back(nodeHandle);
    }

    // ---------- Safe recursive remove ----------
    void removeFNode(TinyHandle handle) {
        TinyFNode* node = fnodes.get(handle);
        if (!node || !node->deletable()) return;

        // If node is non-deletable, move it to root instead
        if (!node->deletable()) {
            moveFNode(handle, rootHandle_);
            return;
        }

        // copy children to avoid mutation during recursion
        std::vector<TinyHandle> childCopy = node->children;
        for (TinyHandle ch : childCopy) {
            removeFNode(ch);
        }

        // if file / has data, remove registry entry
        if (node->hasData()) {
            registry.remove(node->tHandle);
            node->tHandle = TypeHandle(); // invalidate
        }

        // remove from parent children list
        if (fnodes.isValid(node->parent)) {
            TinyFNode* parent = fnodes.get(node->parent);
            if (parent) {
                auto& s = parent->children;
                s.erase(std::remove(s.begin(), s.end(), handle), s.end());
            }
        }

        // finally remove node from pool
        fnodes.remove(handle);
    }

    // ---------- Path resolution ----------
    std::string getFullPath(TinyHandle handle) const {
        if (!fnodes.isValid(handle)) return std::string();

        std::vector<std::string> parts;
        TinyHandle cur = handle;
        while (fnodes.isValid(cur)) {
            const TinyFNode* n = fnodes.get(cur);
            if (!n) break;
            parts.push_back(n->name);
            if (cur == rootHandle_) break;
            cur = n->parent;
        }
        std::reverse(parts.begin(), parts.end());
        std::ostringstream oss;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i) oss << '/';
            oss << parts[i];
        }
        return oss.str();
    }

    // ---------- Data retrieval ----------
    template<typename T>
    T* getFileData(TinyHandle fileHandle) {
        TinyFNode* node = fnodes.get(fileHandle);
        if (!node || !node->hasMeta()) return nullptr;

        return registry.get<T>(node->tHandle);
    }

    TypeHandle getTHandle(TinyHandle handle) const {
        const TinyFNode* node = fnodes.get(handle);
        return node ? node->tHandle : TypeHandle();
    }

    // ---------- Debug print (ANSI colors + tree lines) ----------
    // Files are printed in red. Works on terminals that support ANSI.
    void debugPrint() const {
        debugPrintRecursive(rootHandle_, /*indentPrefix=*/"", /*isLast=*/true);
    }

    // Access to file system nodes (needed for UI)
    const TinyPool<TinyFNode>& getFNodes() const { return fnodes; }
    // TinyPool<TinyFNode>& getFNodes() { return fnodes; } // No non-const access to prevent external mutations

private:
    TinyPool<TinyFNode> fnodes;
    TinyRegistry registry;
    TinyHandle rootHandle_{};
    TinyHandle regHandle_{};

    // Implementation function: templated but uses if constexpr to allow T=void
    template<typename T>
    TinyHandle addFNodeImpl(TinyHandle parentHandle, const std::string& name, T* data, TinyFNode::CFG cfg) {
        if (!fnodes.isValid(parentHandle)) return TinyHandle(); // invalid parent

        TinyFNode child;
        child.name = name;
        child.parent = parentHandle;
        child.cfg = cfg;

        if constexpr (!std::is_same_v<T, void>) {
            if (data) {
                child.tHandle = registry.add(*data);
                child.type = TinyFNode::Type::File;
            }
        }

        TinyHandle h = fnodes.insert(std::move(child));
        // parent might have been invalidated in a multithreaded scenario; guard
        if (fnodes.isValid(parentHandle)) {
            TinyFNode* parent = fnodes.get(parentHandle);
            if (parent) parent->children.push_back(h);
        }
        return h;
    }

    // helper: check if maybeAncestor is ancestor of maybeDescendant
    bool isAncestor(TinyHandle maybeAncestor, TinyHandle maybeDescendant) const {
        if (!fnodes.isValid(maybeAncestor) || !fnodes.isValid(maybeDescendant)) return false;
        TinyHandle cur = maybeDescendant;
        while (fnodes.isValid(cur)) {
            if (cur == maybeAncestor) return true;
            if (cur == rootHandle_) break;
            const TinyFNode* n = fnodes.get(cur);
            if (!n) break;
            cur = n->parent;
        }
        return false;
    }

    // Pretty recursive print using branch characters
    void debugPrintRecursive(TinyHandle handle, std::string indentPrefix, bool isLast) const {
        if (!fnodes.isValid(handle)) return;
        const TinyFNode* node = fnodes.get(handle);
        if (!node) return;

        // connector
        std::string connector = indentPrefix.empty() ? "" : (isLast ? "└─ " : "├─ ");
        // print line
        if (node->isFile()) {
            // red for files
            std::cout << indentPrefix << connector << "\033[31m" << node->name << "\033[0m\n";
        } else {
            std::cout << indentPrefix << connector << node->name << "\n";
        }

        // prepare next prefix
        std::string childPrefix = indentPrefix;
        if (!indentPrefix.empty()) childPrefix += (isLast ? "   " : "│  ");

        // iterate children
        const auto& children = node->children;
        for (size_t i = 0; i < children.size(); ++i) {
            bool childIsLast = (i + 1 == children.size());
            debugPrintRecursive(children[i], childPrefix, childIsLast);
        }
    }

};
