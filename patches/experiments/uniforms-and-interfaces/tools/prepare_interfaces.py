"""Prepare a separate, exact-input specialized-varying experiment."""
from pathlib import Path
import argparse, datetime, difflib, hashlib, json, shutil

def sha(data): return hashlib.sha256(data).hexdigest()
def replace(text, old, new, count=1):
    if text.count(old) != count:
        raise RuntimeError('Unexpected source shape: '+repr(old[:100])+' count '+str(text.count(old)))
    return text.replace(old, new)

HELPERS = r'''
// Fragment consumers only. Vertex calculations and their dependency order stay
// complete; this first pass prunes the VS/FS interface, not GX computation.
struct FragmentInputs {
  bool color0 = true;
  bool color1 = true;
  std::uint32_t texcoords = 0;
};

FragmentInputs fragment_inputs(const ShaderKey& key, bool prune) {
  FragmentInputs use{true, true, (1u << key.num_tex_gens) - 1u};
  if (!prune) return use;
  use = {false, false, 0u};
  if (key.tev_valid == 0) {
    // Preserve the passthrough's color seed even when texturing overwrites it.
    use.color0 = true;
    if (key.textured != 0) use.texcoords = 1u;
    return use;
  }
  for (std::uint32_t n = 0; n < key.num_tev_stages; ++n) {
    const TevStageKey& s = key.tev_stages[n];
    const bool ras = cc_uses(s, 10) || cc_uses(s, 11) || ac_uses(s, 5);
    if (ras && s.tevorders_colorchan == 0u) use.color0 = true;
    if (ras && s.tevorders_colorchan == 1u) use.color1 = true;
    if (key.num_tex_gens == 0u) continue;
    if (s.tevorders_enable != 0 && key.textured != 0) {
      const auto coord = s.tevorders_texcoord < key.num_tex_gens ? s.tevorders_texcoord : 0u;
      use.texcoords |= 1u << coord;
    }
    // Indirect sampling can consume a coordinate without a direct sample.
    if (s.ind_stage < key.num_ind_stages && (s.ind_matrix_index != 0u || s.ind_bump_alpha != 0u)) {
      const auto coord = key.ind_stages[s.ind_stage].texcoord;
      use.texcoords |= 1u << (coord < key.num_tex_gens ? coord : 0u);
    }
  }
  return use;
}

// Keep intermediate colors and every texgen as local zero-initialized values,
// including a forward emboss reference's existing zero value. Export only the
// fragment consumers. Locations stay exactly where this key put them before.
void localize_vertex_outputs(std::string& out, const ShaderKey& key, bool emit_color1,
                             const FragmentInputs& use) {
  const auto start = out.find("@vertex\nfn vs_main");
  const auto end = out.find("    return o;\n}", start);
  if (start == std::string::npos || end == std::string::npos) std::abort();
  std::string body = out.substr(start, end - start);
  const auto substitute = [&](const std::string& field, const std::string& local) {
    std::size_t pos = 0;
    while ((pos = body.find(field, pos)) != std::string::npos) {
      // The admitted texgen count is five; still reject longer field tokens.
      const auto next = pos + field.size();
      if (next < body.size() && ((body[next] >= '0' && body[next] <= '9') ||
          (body[next] >= 'a' && body[next] <= 'z') || body[next] == '_')) {
        pos = next; continue;
      }
      body.replace(pos, field.size(), local); pos += local.size();
    }
  };
  substitute("o.color0", "gx_color0");
  if (emit_color1) substitute("o.color1", "gx_color1");
  for (std::uint32_t i = 0; i < key.num_tex_gens; ++i)
    substitute("o.uv" + std::to_string(i), "gx_uv" + std::to_string(i));
  std::string locals = "    var gx_color0 = vec4f(0.0);\n";
  if (emit_color1) locals += "    var gx_color1 = vec4f(0.0);\n";
  for (std::uint32_t i = 0; i < key.num_tex_gens; ++i)
    locals += "    var gx_uv" + std::to_string(i) + " = vec3f(0.0);\n";
  const std::string anchor = "    var o: VertexOut;\n";
  const auto insertion = body.find(anchor);
  if (insertion == std::string::npos) std::abort();
  body.insert(insertion + anchor.size(), locals);
  if (use.color0) body += "    o.color0 = gx_color0;\n";
  if (emit_color1 && use.color1) body += "    o.color1 = gx_color1;\n";
  for (std::uint32_t i = 0; i < key.num_tex_gens; ++i)
    if ((use.texcoords & (1u << i)) != 0u)
      body += "    o.uv" + std::to_string(i) + " = gx_uv" + std::to_string(i) + ";\n";
  out.replace(start, end - start, body);
}

'''

def shader(text):
    text = replace(text, '#include <cstdarg>', '#include <cstdarg>\n#include <cstdlib>')
    text = replace(text, 'void emit_tev_fragment(std::string& out, const ShaderKey& key) {',
        HELPERS+'void emit_tev_fragment(std::string& out, const ShaderKey& key, const FragmentInputs& use) {')
    text = replace(text, '''  emit(out,
            "    let col0i = vec4i(round(in.color0 * 255.0));\\n"
            "    let col1i = vec4i(round(in.color1 * 255.0));\\n");''',
        '''  if (use.color0) emit(out, "    let col0i = vec4i(round(in.color0 * 255.0));\\n");
  if (use.color1) emit(out, "    let col1i = vec4i(round(in.color1 * 255.0));\\n");''')
    # Only the first texgen loop belongs to fragment fixed-point conversion.
    anchor = '  for (std::uint32_t i = 0; i < key.num_tex_gens; ++i) {\n    if (key.tex_gens[i].projection != 0u)'
    text = replace(text, anchor, '  for (std::uint32_t i = 0; i < key.num_tex_gens; ++i) {\n    if ((use.texcoords & (1u << i)) == 0u) continue;\n    if (key.tex_gens[i].projection != 0u)')
    text = replace(text, 'std::string generate_wgsl(const ShaderKey& key, bool sparse_uniforms) {',
        'std::string generate_wgsl(const ShaderKey& key, bool sparse_uniforms, bool prune_interfaces) {')
    text = replace(text, '  const bool tev = key.tev_valid != 0;',
        '  const FragmentInputs fragment_use = fragment_inputs(key, prune_interfaces);\n  const bool tev = key.tev_valid != 0;')
    old = '''  emit(out, "struct VertexOut {\\n"
            "    @builtin(position) pos: vec4f,\\n"
            "    @location(0) color0: vec4f,\\n");
  if (emit_color1)
    emit(out, "    @location(1) color1: vec4f,\\n");
  for (std::uint32_t i = 0; i < key.num_tex_gens; ++i)
    emitf(out, "    @location(%u) uv%u: vec3f,\\n", uv_loc_base + i, i);'''
    new = '''  emit(out, "struct VertexOut {\\n"
            "    @builtin(position) pos: vec4f,\\n");
  if (fragment_use.color0) emit(out, "    @location(0) color0: vec4f,\\n");
  if (emit_color1 && fragment_use.color1)
    emit(out, "    @location(1) color1: vec4f,\\n");
  for (std::uint32_t i = 0; i < key.num_tex_gens; ++i)
    if ((fragment_use.texcoords & (1u << i)) != 0u)
      emitf(out, "    @location(%u) uv%u: vec3f,\\n", uv_loc_base + i, i);'''
    text = replace(text, old, new)
    text = replace(text, '    emit_tev_fragment(out, key);', '    emit_tev_fragment(out, key, fragment_use);')
    # The sparse prototype has one shared finalizer for TEV and passthrough.
    anchor = '  // Finalize both TEV and passthrough programs.'
    text = replace(text, anchor, '  if (prune_interfaces) localize_vertex_outputs(out, key, emit_color1, fragment_use);\n'+anchor)
    return text

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--source', required=True, type=Path); ap.add_argument('--out', required=True, type=Path)
    args = ap.parse_args(); source = args.source.resolve(); out = args.out.resolve()
    if out.exists(): raise RuntimeError('Fresh experiment directory required')
    out.mkdir(parents=True); tree = out/'tree'; shutil.copytree(source, tree)
    originals = {p.relative_to(source).as_posix(): p.read_bytes() for p in source.rglob('*') if p.is_file()}
    changes = {}
    paths = {
        'shader': 'GXRuntime/graphics/gxcore/src/gxcore_shader.cpp',
        'header': 'GXRuntime/graphics/gxcore/include/gxruntime/gxcore/shader.hpp',
        'draw': 'GXRuntime/graphics/aurora/lib/gfx/gxcore_draw.cpp',
        'config': 'GXRuntime/graphics/aurora/lib/gfx/gxcore_draw.hpp',
    }
    for role, name in paths.items():
        before = originals[name]; text = before.decode('utf-8').replace('\r\n', '\n')
        if role == 'shader': text = shader(text)
        elif role == 'header':
            old = 'std::string generate_wgsl(const ShaderKey& key, bool sparse_uniforms = true);'
            if old not in text: old = old.replace('= true', '= false')
            text = replace(text, old, old.replace(');', ', bool prune_interfaces = false);'))
        elif role == 'config':
            text = replace(text, 'GXCorePipelineConfigVersion = 16;', 'GXCorePipelineConfigVersion = 17;')
            text = replace(text, '  uint32_t sparseUniforms = 0;', '  uint32_t prunedInterfaces = 0; // process-fixed, included in persisted identity\n  uint32_t sparseUniforms = 0;')
        elif role == 'draw':
            text = replace(text, '  CHECK(config.sparseUniforms <= 1u, "Invalid GX vertex-uniform layout");',
                '  CHECK(config.prunedInterfaces <= 1u, "Invalid GX shader interface");\n  CHECK(config.sparseUniforms <= 1u, "Invalid GX vertex-uniform layout");')
            text = replace(text, 'gxc::generate_wgsl(key.shader, sparse)', 'gxc::generate_wgsl(key.shader, sparse, config.prunedInterfaces != 0u)')
            anchor = 'namespace aurora::gfx::gxcore {'
            helper = '''\n// Exact opt-in; a separate pipeline identity preserves both interfaces.\nstatic bool pruned_interfaces_enabled() {\n  static const bool enabled = [] {\n    const char* value = std::getenv("DOL_GX_SHADER_INTERFACES");\n    return value != nullptr && value[0] == '1' && value[1] == '\\0';\n  }();\n  return enabled;\n}\n'''
            text = replace(text, anchor, anchor+helper)
            # Both the early selective request and final canonical request.
            text = replace(text, '.sparseUniforms = sparse_vertex_uniforms_enabled() ? 1u : 0u,',
                '.prunedInterfaces = pruned_interfaces_enabled() ? 1u : 0u,\n      .sparseUniforms = sparse_vertex_uniforms_enabled() ? 1u : 0u,', count=2)
        after = text.replace('\n', '\r\n').encode('utf-8') if b'\r\n' in before else text.encode('utf-8')
        (tree/name).write_bytes(after)
        changes[name] = {'before': sha(before), 'after': sha(after), 'bytes': len(after)}
    patch = []
    for name in sorted(changes):
        patch.append('diff --git a/'+name+' b/'+name+'\n')
        patch.extend(difflib.unified_diff(originals[name].decode().replace('\r\n','\n').splitlines(True),
            (tree/name).read_text(encoding='utf-8').splitlines(True), fromfile='a/'+name, tofile='b/'+name))
    (out/'interface.patch').write_text(''.join(patch), encoding='utf-8', newline='\n')
    receipt = {'status':'SOURCE_ONLY_UNCOMPILED', 'created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'source':str(source), 'changes':changes, 'preserved':{n:sha(b) for n,b in originals.items() if n not in changes},
        'patch_sha256':sha((out/'interface.patch').read_bytes())}
    (out/'source-receipt.json').write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8', newline='\n')
    print(json.dumps({'status':receipt['status'], 'changes':len(changes), 'patch_sha256':receipt['patch_sha256']}))

if __name__ == '__main__': main()
