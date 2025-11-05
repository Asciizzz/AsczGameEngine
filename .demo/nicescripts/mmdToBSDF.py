# A script to convert MMD-style Blender materials to simple Principled BSDF PBR materials.
# You can only use this script in Blender (obviously).

import bpy

# ----------------- CONFIG -----------------
DUPLICATE_MATERIALS = True   # if True, create a new "MaterialName_PBR" and modify that (safe)
VERBOSE = True
# ------------------------------------------

def log(*args):
    if VERBOSE:
        print(*args)

def is_mmd_group(node):
    try:
        nn = (node.node_tree.name if node.node_tree else "") or ""
    except Exception:
        nn = ""
    return node.type == 'GROUP' and ('MMD' in nn.upper() or 'MMD' in (node.name or '').upper() or 'MMD' in (node.label or '').upper())

def find_mmd_group_nodes(nodes):
    return [n for n in nodes if is_mmd_group(n)]

def find_base_tex_node(mat):
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    # 1) If there's an MMD shader group, try to find TEX_IMAGE nodes that feed it
    group_nodes = find_mmd_group_nodes(nodes)
    if group_nodes:
        candidates = []
        for link in links:
            if link.to_node in group_nodes and link.from_node.type == 'TEX_IMAGE' and getattr(link.from_node, "image", None):
                candidates.append((link.from_node, link.to_socket.name))
        # prefer ones where the group socket name indicates base
        for node, sock in candidates:
            if 'base' in (sock or '').lower() or 'diff' in (sock or '').lower():
                return node, None  # external node found; no group context needed
        if candidates:
            return candidates[0][0], None

        # 1b) fallback: maybe the image node is *inside* the group. search inside the group node-trees
        for g in group_nodes:
            try:
                for inner in g.node_tree.nodes:
                    if inner.type == 'TEX_IMAGE' and getattr(inner, "image", None):
                        # return inner node + the group node reference so we can copy it outside
                        return inner, g
            except Exception:
                continue

    # 2) No MMD group or nothing found: search for a texture node that looks like base/diff/albedo
    for n in nodes:
        if n.type == 'TEX_IMAGE' and getattr(n, "image", None):
            name = ((n.label or "") + " " + (n.name or "") + " " + (n.image.name or "")).lower()
            if any(k in name for k in ('base', 'diff', 'albedo', 'main', 'diffuse')):
                return n, None

    # 3) last resort: return the first image texture node
    for n in nodes:
        if n.type == 'TEX_IMAGE' and getattr(n, "image", None):
            return n, None

    return None, None

def get_upstream_nodes(node, links):
    visited = set()
    stack = [node]
    while stack:
        n = stack.pop()
        for l in links:
            if l.to_node == n and l.from_node not in visited:
                visited.add(l.from_node)
                stack.append(l.from_node)
    return visited

def copy_inner_texture_to_material(mat, group_node, inner_tex_node):
    """If the found texture lives inside a node group, create an equivalent Image Texture
       node in the *material's* node tree and try to wire any external UV source if possible."""
    m_nodes = mat.node_tree.nodes
    m_links = mat.node_tree.links

    new_tex = m_nodes.new('ShaderNodeTexImage')
    new_tex.image = inner_tex_node.image
    new_tex.label = inner_tex_node.label or "BaseTex_from_group"
    new_tex.location = (-300, 0)

    # try to find which Group Input output fed the inner tex's Vector input,
    # so we can rewire the corresponding external node to the new tex node Vector.
    try:
        g_nodes = group_node.node_tree.nodes
        g_links = group_node.node_tree.links
        group_input_node = next((n for n in g_nodes if n.type == 'GROUP_INPUT'), None)
        if group_input_node:
            # find link inside the group that goes into the inner tex (usually the Vector input)
            for gl in g_links:
                if gl.to_node == inner_tex_node:
                    # if the from_node is the GROUP_INPUT, then gl.from_socket.name is the input name
                    if gl.from_node == group_input_node:
                        group_input_name = gl.from_socket.name
                        # now find an outer link feeding that group input (i.e. an external node linked to group_node)
                        for outer_link in mat.node_tree.links:
                            if outer_link.to_node == group_node and outer_link.to_socket.name == group_input_name:
                                ext_from = outer_link.from_node
                                # attempt to connect ext_from output to new_tex.Vector
                                try:
                                    m_links.new(ext_from.outputs[0], new_tex.inputs['Vector'])
                                except Exception:
                                    pass
                                break
                    # only need the first relevant one
                    break
    except Exception:
        pass

    return new_tex

def clear_except_keep(mat, keep_nodes):
    nodes = mat.node_tree.nodes
    to_remove = [n for n in list(nodes) if n not in keep_nodes]
    for n in to_remove:
        try:
            nodes.remove(n)
        except Exception as e:
            log("couldn't remove node", n, e)

# --------- main loop ----------
processed = 0
for mat in list(bpy.data.materials):
    if not mat.use_nodes:
        continue

    original_mat = mat
    if DUPLICATE_MATERIALS:
        try:
            mat = original_mat.copy()
            mat.name = original_mat.name + "_PBR"
            # reassign material slots that used the original to the new copy (so you can preview)
            for ob in bpy.data.objects:
                if not hasattr(ob, "material_slots"):
                    continue
                for i, slot in enumerate(ob.material_slots):
                    if slot.material == original_mat:
                        ob.material_slots[i].material = mat
            log("Created PBR duplicate:", mat.name)
        except Exception as e:
            log("Failed to duplicate material", original_mat.name, e)
            mat = original_mat

    base_node, group_node = find_base_tex_node(mat)
    if base_node is None:
        log("⚠ No image texture found for material:", mat.name)
        continue

    # if the base_node was inside a group_node, copy it out to the material tree
    if group_node and base_node and base_node.id_data != mat.node_tree:  # inner node
        log("Found inner texture in group. Copying out...")
        base_node = copy_inner_texture_to_material(mat, group_node, base_node)

    # ensure there's an output node
    output = next((n for n in mat.node_tree.nodes if n.type == 'OUTPUT_MATERIAL'), None)
    if not output:
        output = mat.node_tree.nodes.new('ShaderNodeOutputMaterial')
        output.location = (400, 0)

    # Keep the base image node and any upstream nodes (UV mapping nodes etc.) + output
    links = mat.node_tree.links
    keep = set([base_node, output])
    upstream = get_upstream_nodes(base_node, links)
    keep.update(upstream)

    # also keep nodes that are already connected to output (not required but safe)
    for l in links:
        if l.to_node == output:
            keep.add(l.from_node)

    # clear everything else
    clear_except_keep(mat, keep)

    # create Principled BSDF and connect
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    bsdf = nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.location = (150, 0)
    bsdf.inputs["Metallic"].default_value = 0.0
    bsdf.inputs["Roughness"].default_value = 0.6

    # remove existing surface links to output
    old = [l for l in links if l.to_node == output and l.to_socket.name.lower() == "surface"]
    for l in old:
        try:
            links.remove(l)
        except Exception:
            pass

    # connect base texture color to Base Color
    try:
        links.new(base_node.outputs['Color'], bsdf.inputs['Base Color'])
    except Exception as e:
        log("failed to connect base color for", mat.name, e)

    # connect BSDF to output
    try:
        links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])
    except Exception as e:
        log("failed to connect bsdf->output for", mat.name, e)

    processed += 1
    log("Converted material:", mat.name)

log("Done. Materials processed:", processed)
