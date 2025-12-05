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

def copy_inner_texture_to_material(mat, group_node, inner_tex_node):
    """Copy an inner texture node to the material tree."""
    m_nodes = mat.node_tree.nodes
    m_links = mat.node_tree.links

    new_tex = m_nodes.new('ShaderNodeTexImage')
    new_tex.image = inner_tex_node.image
    new_tex.label = inner_tex_node.label or "CopiedTex"
    new_tex.location = (-300, 0)

    # attempt to wire external UV source if it exists
    try:
        g_nodes = group_node.node_tree.nodes
        g_links = group_node.node_tree.links
        group_input_node = next((n for n in g_nodes if n.type == 'GROUP_INPUT'), None)
        if group_input_node:
            for gl in g_links:
                if gl.to_node == inner_tex_node and gl.from_node == group_input_node:
                    group_input_name = gl.from_socket.name
                    for outer_link in mat.node_tree.links:
                        if outer_link.to_node == group_node and outer_link.to_socket.name == group_input_name:
                            ext_from = outer_link.from_node
                            try:
                                m_links.new(ext_from.outputs[0], new_tex.inputs['Vector'])
                            except Exception:
                                pass
                            break
                    break
    except Exception:
        pass

    return new_tex

def find_mmd_textures(mat):
    """Return a dictionary of relevant texture nodes: base_color, metallic, roughness, normal, alpha"""
    textures = {
        "base_color": None,
        "metallic": None,
        "roughness": None,
        "normal": None,
        "alpha": None
    }

    nodes = mat.node_tree.nodes
    group_nodes = find_mmd_group_nodes(nodes)

    # first, check for textures inside MMD groups
    for g in group_nodes:
        try:
            for n in g.node_tree.nodes:
                if n.type != 'TEX_IMAGE' or not getattr(n, "image", None):
                    continue
                name = ((n.label or "") + " " + (n.name or "") + " " + (n.image.name or "")).lower()
                if any(k in name for k in ('base', 'diff', 'albedo', 'main')):
                    textures['base_color'] = (n, g)
                elif any(k in name for k in ('metal', 'spec')):
                    textures['metallic'] = (n, g)
                elif any(k in name for k in ('rough', 'gloss')):
                    textures['roughness'] = (n, g)
                elif 'normal' in name or 'bump' in name:
                    textures['normal'] = (n, g)
                elif 'alpha' in name or 'transp' in name:
                    textures['alpha'] = (n, g)
        except Exception:
            continue

    # fallback: search material nodes outside groups
    for n in nodes:
        if n.type != 'TEX_IMAGE' or not getattr(n, "image", None):
            continue
        name = ((n.label or "") + " " + (n.name or "") + " " + (n.image.name or "")).lower()
        if not textures['base_color'] and any(k in name for k in ('base', 'diff', 'albedo', 'main')):
            textures['base_color'] = (n, None)
        elif not textures['metallic'] and any(k in name for k in ('metal', 'spec')):
            textures['metallic'] = (n, None)
        elif not textures['roughness'] and any(k in name for k in ('rough', 'gloss')):
            textures['roughness'] = (n, None)
        elif not textures['normal'] and ('normal' in name or 'bump' in name):
            textures['normal'] = (n, None)
        elif not textures['alpha'] and ('alpha' in name or 'transp' in name):
            textures['alpha'] = (n, None)

    # ensure every entry is a tuple to prevent unpack errors
    for key in textures:
        if textures[key] is None:
            textures[key] = (None, None)

    return textures

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

    tex_dict = find_mmd_textures(mat)

    # copy inner textures if inside group nodes
    for key, val in tex_dict.items():
        node, group = val
        if group and node and node.id_data != mat.node_tree:
            tex_dict[key] = (copy_inner_texture_to_material(mat, group, node), None)

    # ensure output exists
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    output = next((n for n in nodes if n.type == 'OUTPUT_MATERIAL'), None)
    if not output:
        output = nodes.new('ShaderNodeOutputMaterial')
        output.location = (400, 0)

    # keep all upstream nodes of textures + output
    keep = set([output])
    for node, _ in tex_dict.values():
        if node:
            keep.add(node)
            upstream = get_upstream_nodes(node, links)
            keep.update(upstream)

    clear_except_keep(mat, keep)

    # create Principled BSDF
    bsdf = nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.location = (150, 0)
    bsdf.inputs["Metallic"].default_value = 0.0
    bsdf.inputs["Roughness"].default_value = 0.6

    # connect textures
    def connect_tex(input_name, tex_node, invert=False, normal_map=False):
        if not tex_node:
            return
        if normal_map:
            nmap = nodes.new('ShaderNodeNormalMap')
            nmap.location = (0, -300)
            links.new(tex_node.outputs['Color'], nmap.inputs['Color'])
            links.new(nmap.outputs['Normal'], bsdf.inputs[input_name])
        else:
            if invert:
                invert_node = nodes.new('ShaderNodeInvert')
                invert_node.location = (0, -200)
                links.new(tex_node.outputs['Color'], invert_node.inputs['Color'])
                links.new(invert_node.outputs['Color'], bsdf.inputs[input_name])
            else:
                links.new(tex_node.outputs['Color'], bsdf.inputs[input_name])

    connect_tex('Base Color', tex_dict['base_color'][0])
    connect_tex('Metallic', tex_dict['metallic'][0])
    connect_tex('Roughness', tex_dict['roughness'][0], invert=True)  # gloss->rough
    connect_tex('Normal', tex_dict['normal'][0], normal_map=True)
    if tex_dict['alpha'][0]:
        mat.blend_method = 'BLEND'
        links.new(tex_dict['alpha'][0].outputs['Color'], bsdf.inputs['Alpha'])

    # connect BSDF to output
    try:
        links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])
    except Exception as e:
        log("failed to connect bsdf->output for", mat.name, e)

    processed += 1
    log("Converted material:", mat.name)

log("Done. Materials processed:", processed)
