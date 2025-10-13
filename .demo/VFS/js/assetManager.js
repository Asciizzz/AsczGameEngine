// js/assetManager.js
// Very small simulated AssetManager that maps UIDs -> handles and keeps a "loaded" cache.

import { DemoVFS } from './vfs.js';

export class AssetManager {
    constructor(vfs) {
        this.vfs = vfs;
        this.uidToHandle = new Map();
        this.nextHandle = 1;
        this.handles = new Map();
    }

    // load a virtual file (reads UID from node meta)
    load(virtualPath) {
        try {
            const { physicalName, relative } = this.vfs.resolve(virtualPath);
            const node = this.vfs._walk(physicalName, relative).node;
            if (!node || node.isDirectory) throw new Error('Not a file');
            const uid = node.meta && node.meta.uid ? node.meta.uid : null;
            const handle = { 
                handle: this.nextHandle++, 
                uid, 
                path: virtualPath, 
                loadedAt: Date.now() 
            };
            this.uidToHandle.set(uid, handle);
            this.handles.set(handle.handle, handle);
            return handle;
        } catch (e) {
            console.error('Asset load error', e);
            return null;
        }
    }

    getHandleByUID(uid){ 
        return this.uidToHandle.get(uid); 
    }
    
    getHandleById(id){ 
        return this.handles.get(id); 
    }
}

export const DemoAssetManager = new AssetManager(DemoVFS);