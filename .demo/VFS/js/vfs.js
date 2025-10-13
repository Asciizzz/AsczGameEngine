// js/vfs.js
// A tiny virtual filesystem for the demo. Browser-only: simulated physical "paths" stored in memory.

export class VirtualFileSystem {
    constructor() {
        // mount table: { mountPoint -> physicalName }
        this.mounts = new Map();
        // physicalStores: { physicalName -> tree }
        this.physicalStores = new Map();
    }

    // Create a new simulated physical store with an initial tree
    createPhysicalStore(name, tree) {
        this.physicalStores.set(name, tree || { name: '/', children: [] });
    }

    mount(mountPoint, physicalName) {
        if (!mountPoint.endsWith('/')) mountPoint += '/';
        if (!this.physicalStores.has(physicalName)) throw new Error('Unknown physical store: ' + physicalName);
        this.mounts.set(mountPoint, physicalName);
    }

    unmount(mountPoint) {
        if (!mountPoint.endsWith('/')) mountPoint += '/';
        this.mounts.delete(mountPoint);
    }

    resolve(virtualPath) {
        // returns { physicalName, relative }
        for (let [mountPoint, physicalName] of this.mounts.entries()) {
            if (virtualPath === mountPoint.slice(0, -1) || virtualPath.startsWith(mountPoint)) {
                let rel = virtualPath.slice(mountPoint.length);
                if (!rel) rel = '/';
                return { physicalName, relative: rel };
            }
        }
        throw new Error('No mount matches: ' + virtualPath);
    }

    // Utility: walk a physical store by a path like 'assets/models/tree.fbx' or '/'
    _walk(physicalName, relative, createIfMissing=false) {
        const root = this.physicalStores.get(physicalName);
        if (!root) throw new Error('No physical store: ' + physicalName);
        if (!relative || relative === '/' ) return { parent: null, node: root };
        const parts = relative.split('/').filter(p=>p.length);
        let node = root;
        let parent = null;
        for (let i=0;i<parts.length;i++){
            parent = node;
            const part = parts[i];
            let child = node.children.find(c=>c.name === part);
            if (!child) {
                if (createIfMissing) {
                    child = { name: part, children: [], isDirectory: i < parts.length-1 ? true : false };
                    node.children.push(child);
                } else {
                    return { parent: node, node: null };
                }
            }
            node = child;
        }
        return { parent, node };
    }

    exists(virtualPath) {
        try { 
            const { physicalName, relative } = this.resolve(virtualPath); 
            return !!this._walk(physicalName, relative).node; 
        }
        catch(e){
            return false;
        }
    }

    list(virtualPath) {
        const { physicalName, relative } = this.resolve(virtualPath);
        const r = this._walk(physicalName, relative);
        if (!r.node) return [];
        return (r.node.children||[]).map(c=>({ 
            name:c.name, 
            isDirectory: !!c.children, 
            path: this._joinVirtual(physicalName, relative, c.name) 
        }));
    }

    _joinVirtual(physicalName, relative, name){
        // return virtual path string for the child
        for (let [mountPoint, pName] of this.mounts.entries()){
            if (pName === physicalName) {
                const base = mountPoint + (relative==='/'? '': relative + (relative.endsWith('/')? '': '/'));
                return base + name;
            }
        }
        return name;
    }

    scanTree(virtualPath) {
        // returns a nested object for UI
        const { physicalName, relative } = this.resolve(virtualPath);
        const r = this._walk(physicalName, relative);
        if (!r.node) return null;
        
        function cloneNode(n, prefix, fs){
            return {
                name: n.name,
                isDirectory: !!n.children,
                children: (n.children||[]).map(c=>cloneNode(c, prefix + '/' + c.name, fs)),
                virtualPath: prefix === '/' ? mountRootForPhysical(fs, physicalName) + n.name : prefix
            };
        }
        
        // find mount root
        function mountRootForPhysical(vfs, p){
            for (let [mp, pn] of vfs.mounts.entries()) {
                if (pn===p) return mp;
            }
            return '/';
        }
        
        // build virtual prefix for this node
        let prefix = null;
        for (let [mp,pn] of this.mounts.entries()){
            if (pn===physicalName) {
                prefix = mp + (relative==='/'? '': relative);
                if (!prefix.endsWith('/')) prefix += '/';
                break;
            }
        }
        
        function build(n, curPath){
            return {
                name: n.name,
                isDirectory: !!n.children,
                virtualPath: curPath,
                children: (n.children||[]).map(c=> build(c, curPath + (curPath.endsWith('/')? '':'/') + c.name))
            }
        }
        
        const curPath = prefix + (relative==='/'? '' : relative);
        return build(r.node, prefix + (relative==='/'? '' : relative));
    }

    createFile(virtualParent, name){
        const { physicalName, relative } = this.resolve(virtualParent);
        const p = this._walk(physicalName, relative, true);
        if (!p.node) throw new Error('Parent folder not found: ' + virtualParent);
        // add file node
        p.node.children.push({ 
            name, 
            isDirectory:false, 
            children: null, 
            meta: { uid: this._makeUID() } 
        });
    }

    createFolder(virtualParent, name){
        const { physicalName, relative } = this.resolve(virtualParent);
        const p = this._walk(physicalName, relative, true);
        if (!p.node) throw new Error('Parent folder not found: ' + virtualParent);
        p.node.children.push({ 
            name, 
            isDirectory:true, 
            children: [], 
            meta: null 
        });
    }

    delete(virtualPath){
        const { physicalName, relative } = this.resolve(virtualPath);
        const parts = relative.split('/').filter(Boolean);
        if (parts.length===0) throw new Error('Cannot delete root');
        const name = parts.pop();
        const parentRel = parts.join('/');
        const parent = this._walk(physicalName, parentRel);
        if (!parent.node) throw new Error('Parent not found');
        parent.node.children = parent.node.children.filter(c=>c.name!==name);
    }

    _makeUID(){
        return Math.random().toString(36).slice(2,10);
    }
}

// expose a tiny convenience factory for the demo
export const DemoVFS = new VirtualFileSystem();

// create demo physical stores
DemoVFS.createPhysicalStore('proj1', {
    name:'/', 
    children: [
        { 
            name:'assets', 
            isDirectory:true, 
            children:[
                { 
                    name:'models', 
                    isDirectory:true, 
                    children:[ 
                        { 
                            name:'tree.fbx', 
                            isDirectory:false, 
                            children:null, 
                            meta:{ uid:'mesh1' } 
                        } 
                    ] 
                },
                { 
                    name:'textures', 
                    isDirectory:true, 
                    children:[ 
                        { 
                            name:'wood.png', 
                            isDirectory:false, 
                            children:null, 
                            meta:{ uid:'tex1' } 
                        } 
                    ] 
                }
            ]
        },
        { 
            name:'scenes', 
            isDirectory:true, 
            children:[ 
                { 
                    name:'level1.scene', 
                    isDirectory:false, 
                    children:null, 
                    meta:{ uid:'scene1' } 
                } 
            ] 
        }
    ]
});

// mount default
DemoVFS.mount('res://', 'proj1');