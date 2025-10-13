// js/ui.js
import { DemoVFS } from './vfs.js';
import { DemoAssetManager } from './assetManager.js';

// Basic DOM helpers
const $ = s => document.querySelector(s);
const fileTree = $('#fileTree');
const selectedInfo = $('#selectedInfo');
const assetInfo = $('#assetInfo');

let currentRoot = 'res://';
let selectedVirtual = null;

function renderTree(){
    fileTree.innerHTML = '';
    const tree = DemoVFS.scanTree(currentRoot);
    if (!tree) { 
        fileTree.innerText = '(empty)'; 
        return; 
    }
    const ul = document.createElement('div');
    buildNodeUI(tree, ul);
    fileTree.appendChild(ul);
}

function buildNodeUI(node, parentEl){
    const el = document.createElement('div');
    el.className = 'node ' + (node.isDirectory? 'directory':'file');
    el.textContent = node.name === '/' ? currentRoot : node.name;
    el.dataset.vpath = node.virtualPath;
    el.addEventListener('click', (e)=>{
        e.stopPropagation();
        onSelect(node.virtualPath, node);
    });
    if (selectedVirtual === node.virtualPath) el.classList.add('selected');
    parentEl.appendChild(el);

    if (node.isDirectory && node.children && node.children.length) {
        const childWrap = document.createElement('div');
        childWrap.style.paddingLeft = '12px';
        for (const c of node.children) {
            buildNodeUI(c, childWrap);
        }
        parentEl.appendChild(childWrap);
    }
}

function onSelect(vpath, node){
    selectedVirtual = vpath;
    selectedInfo.textContent = JSON.stringify({ 
        vpath, 
        isDirectory: node.isDirectory 
    }, null, 4);
    renderTree();
}

// Buttons
$('#btnMount').addEventListener('click', ()=>{
    const mountPoint = $('#mountPoint').value.trim();
    const physicalName = $('#physicalPath').value.trim();
    if (!mountPoint || !physicalName) return alert('Both fields required');
    try{
        // ensure store exists
        if (!DemoVFS.physicalStores.has(physicalName)) {
            DemoVFS.createPhysicalStore(physicalName, { name:'/', children:[] });
        }
        DemoVFS.mount(mountPoint, physicalName);
        renderTree();
    } catch(e){ 
        alert(e.message); 
    }
});

$('#btnCreateFile').addEventListener('click', ()=>{
    const name = $('#newName').value.trim();
    if (!name) return;
    try{ 
        DemoVFS.createFile(selectedVirtual||currentRoot, name); 
        renderTree(); 
    }catch(e){ 
        alert(e.message); 
    }
});

$('#btnCreateFolder').addEventListener('click', ()=>{
    const name = $('#newName').value.trim();
    if (!name) return;
    try{ 
        DemoVFS.createFolder(selectedVirtual||currentRoot, name); 
        renderTree(); 
    }catch(e){ 
        alert(e.message); 
    }
});

$('#btnDelete').addEventListener('click', ()=>{
    if (!selectedVirtual) return alert('Select something first');
    try{ 
        DemoVFS.delete(selectedVirtual); 
        selectedVirtual = null; 
        selectedInfo.textContent='(none)'; 
        renderTree(); 
    }catch(e){ 
        alert(e.message); 
    }
});

// double click to load asset
fileTree.addEventListener('dblclick', ()=>{
    if (!selectedVirtual) return;
    try{
        const h = DemoAssetManager.load(selectedVirtual);
        assetInfo.textContent = JSON.stringify(h, null, 4);
    } catch(e){ 
        alert(e.message); 
    }
});

// initial render
renderTree();