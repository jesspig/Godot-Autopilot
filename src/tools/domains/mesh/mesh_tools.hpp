#ifndef GODOT_AUTOPILOT_MESH_TOOLS_HPP
#define GODOT_AUTOPILOT_MESH_TOOLS_HPP

#include "util/json_value.hpp"
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/mesh/mesh_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace mesh_tools {

namespace {

const std::vector<ParamSpec> kMeshSurfaceParams = {
    {"session", "object", "SurfaceTool session locator for every action except surface_begin: the session name returned by surface_begin, memory://name, a decimal object_id string, or {name|object_id|object_id_str}", false},
    {"primitive", "string", "Only with action='surface_begin': points, lines, line_strip, triangles (default) or triangle_strip", false},
    {"vertex", "object", "Only with action='add_vertex': one vertex object {x, y, z}", false},
    {"vertices", "array", "With action='add_vertex': an array of {x, y, z} objects appended in order; with action='add_triangle_fan': the ordered fan/strip vertices, at least 3", false},
    {"uvs", "array", "Only with action='add_triangle_fan': UV array parallel to vertices, each {x, y}", false},
    {"colors", "array", "Only with action='add_triangle_fan': vertex color array parallel to vertices, each {r, g, b, a} with 0-1 components", false},
    {"uv2s", "array", "Only with action='add_triangle_fan': second UV array parallel to vertices, each {x, y}", false},
    {"normals", "array", "Only with action='add_triangle_fan': normal array parallel to vertices, each {x, y, z}", false},
    {"uv", "object", "Only with action='set_uv': UV {x, y} applied to vertices added afterwards", false},
    {"normal", "object", "Only with action='set_normal': normal {x, y, z} applied to vertices added afterwards", false},
    {"color", "object", "Only with action='set_color': vertex color {r, g, b, a} with 0-1 components applied to vertices added afterwards", false},
    {"material", "object", "Only with action='set_material': Material locator as memory://name, bare registration name, decimal object_id string, res:// material path, or {name|path|object_id}", false},
    {"name", "string", "With action='surface_begin': optional session name (default surface_N); with action='commit': registration name for the committed ArrayMesh so later calls can use memory://name", false},
    {"mesh", "object", "Only with action='commit': existing ArrayMesh locator to append one surface to; omit to create a new ArrayMesh", false},
    {"surface_name", "string", "Only with action='commit': optional name for the appended surface", false},
    {"generate_normals", "boolean", "Only with action='commit': generate vertex normals before committing (default: false)", false},
    {"flip_normals", "boolean", "Only with action='commit' and generate_normals=true: flip the generated normals (default: false)", false},
    {"generate_tangents", "boolean", "Only with action='commit': generate tangents before committing; requires normals and UVs (default: false)", false},
    {"index", "boolean", "Only with action='commit': run index() to share duplicate vertices (default: false)", false},
    {"deindex", "boolean", "Only with action='commit': run deindex() to expand shared vertices (default: false); mutually exclusive with index", false},
};

const std::vector<ParamSpec> kMeshInspectParams = {
    {"mesh", "object", "Mesh locator for actions inspect/surface_arrays/faces/create_trimesh_shape/create_convex_shape: memory://name string, bare registration name, decimal object_id string, res:// mesh path, or {name|path|object_id|object_id_str} object", false},
    {"surface_index", "integer", "Only with action='surface_arrays': zero-based surface index (default: 0)", false},
    {"session", "object", "Only with action='free': SurfaceTool session locator returned by mesh_surface action='surface_begin'", false},
    {"node_path", "string", "Only with action='csg_bake': scene-relative path of a root CSGShape3D node in the edited scene, e.g. Level/CSGRoot; provide exactly one of node_path and node", false},
    {"node", "object", "Only with action='csg_bake': decimal object_id or {object_id|object_id_str} of a root CSGShape3D instance; provide exactly one of node_path and node", false},
    {"name", "string", "With action='create_trimesh_shape' or 'create_convex_shape': registration name for the derived shape; with action='csg_bake': registration name for the baked ArrayMesh (default: <node>_baked); addressable later as memory://name", false},
    {"shape_name", "string", "Only with action='csg_bake' and shape='collision' or 'both': registration name for the baked ConcavePolygonShape3D (default: <name>_shape)", false},
    {"shape", "string", "Only with action='csg_bake': mesh, collision or both (default: both)", false},
    {"clean", "boolean", "Only with action='create_convex_shape': clean the generated convex hull (default: true)", false},
    {"simplify", "boolean", "Only with action='create_convex_shape': simplify the generated convex hull (default: false)", false},
};

const std::vector<ParamSpec> kMultiMeshParams = {
    {"multimesh", "object", "MultiMesh locator for actions set_transforms/set_colors/set_buffer/info: memory://name string, bare registration name, decimal object_id string, res:// resource path, or {name|path|object_id|object_id_str} object", false},
    {"mesh", "object", "Only with action='create': optional Mesh locator assigned as the rendered mesh, in the same formats as multimesh", false},
    {"name", "string", "Only with action='create': registration name for the new MultiMesh so later calls can use memory://name", false},
    {"instance_count", "integer", "Only with action='create': instance capacity, 0-100000 (default: 0); set after transform_format/use_colors/use_custom_data because changing those resets the buffer", false},
    {"transform_format", "string", "Only with action='create': 3d (default, Transform3D instances) or 2d (Transform2D instances)", false},
    {"use_colors", "boolean", "Only with action='create': enable per-instance colors (default: false); required before set_colors", false},
    {"use_custom_data", "boolean", "Only with action='create': enable per-instance custom data (default: false)", false},
    {"transforms", "array", "Only with action='set_transforms': array of transforms, at most instance_count entries; each is {origin:{x,y,z}, rotation:{x,y,z} degrees, scale:{x,y,z}, basis:{x|y|z:{x,y,z}} or [[3 floats] x3]} for 3d, or {origin:{x,y}, rotation degrees, scale:{x,y}} for 2d", false},
    {"colors", "array", "Only with action='set_colors': per-instance colors, each {r, g, b, a} with 0-1 components, at most instance_count entries", false},
    {"buffer", "array", "Only with action='set_buffer': flat numbers in engine buffer order; length must equal instance_count x stride where stride is 12 (3d) or 8 (2d) plus 4 for colors and 4 for custom data", false},
    {"include_buffer", "boolean", "Only with action='info': include the raw buffer array, cut at 10000 values and flagged by buffer_truncated (default: false)", false},
};

const std::vector<ParamSpec> kMultiMeshInstanceParams = {
    {"node_path", "string", "Scene-relative or /root/... path of the MultiMeshInstance3D to receive the MultiMesh, e.g. Level/Scatter", true},
    {"multimesh", "object", "MultiMesh locator: memory://name string, bare registration name, decimal object_id string, res:// resource path, or {name|path|object_id|object_id_str} object", true},
};

const std::vector<std::string> kMeshSurfaceActions = {
    "surface_begin", "add_vertex", "add_triangle_fan", "set_uv", "set_normal",
    "set_color", "set_material", "commit"};

const std::vector<std::string> kMeshInspectActions = {
    "inspect", "surface_arrays", "faces", "create_trimesh_shape",
    "create_convex_shape", "csg_bake", "free"};

const std::vector<std::string> kMultiMeshActions = {
    "create", "set_transforms", "set_colors", "set_buffer", "info"};

const std::vector<std::string> kMultiMeshInstanceActions = {"assign_to_instance"};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(4);
  v.push_back(make_spec_tool(ToolSpec{
      "mesh_surface",
      "Build an ArrayMesh from a SurfaceTool session (None side effect; the committed mesh stays in memory and is addressable as memory://name). action selects the operation: 'surface_begin' starts a session (primitive points/lines/line_strip/triangles/triangle_strip, default triangles; optional name, default surface_N); 'add_vertex' appends one vertex {x, y, z} or an array via vertices; 'add_triangle_fan' appends an ordered fan/strip of vertices with optional parallel uvs/colors/uv2s/normals arrays; 'set_uv', 'set_normal' and 'set_color' set the attribute applied to vertices added afterwards; 'set_material' assigns a Material resolved from memory://name, a bare name, object_id or a res:// path; 'commit' commits the session to an ArrayMesh (optional name registers it, optional mesh appends a surface to an existing ArrayMesh, optional generate_normals/flip_normals/generate_tangents/index/deindex/surface_name) and releases the session. Sessions are capped at 32 active entries and 65536 vertices per session. Locate a session by its name, memory://name or decimal object_id. Example: {\"action\": \"surface_begin\", \"name\": \"tri\", \"primitive\": \"triangles\"} then {\"action\": \"add_triangle_fan\", \"session\": \"tri\", \"vertices\": [{\"x\": 0, \"y\": 0, \"z\": 0}, {\"x\": 1, \"y\": 0, \"z\": 0}, {\"x\": 0, \"y\": 1, \"z\": 0}]} then {\"action\": \"commit\", \"session\": \"tri\", \"name\": \"tri_mesh\", \"generate_normals\": true}.",
      "Resources", {"mesh", "surface", "surfacetool", "vertices", "triangle", "uv", "normal", "vertex_color", "material", "commit", "array_mesh"}, SideEffect::None, tool_flags::kNone,
      kMeshSurfaceParams, mesh_ops::handle_surface_action,
      schema::build_action_schema(
          kMeshSurfaceActions, kMeshSurfaceParams,
          "surface_begin: start a SurfaceTool session; add_vertex: append one vertex or a vertex array; add_triangle_fan: append a vertex fan/strip with optional attribute arrays; set_uv/set_normal/set_color: set attributes for following vertices; set_material: assign a Material; commit: build the ArrayMesh and release the session"),
      {},
      {"mesh_surface action=surface_begin name=tri primitive=triangles",
       "mesh_surface action=add_vertex session=tri vertex={\"x\":0,\"y\":0,\"z\":0}",
       "mesh_surface action=add_triangle_fan session=tri vertices=[{\"x\":0,\"y\":0,\"z\":0},{\"x\":1,\"y\":0,\"z\":0},{\"x\":0,\"y\":1,\"z\":0}]",
       "mesh_surface action=commit session=tri name=tri_mesh generate_normals=true"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "mesh_inspect",
      "Inspect a Mesh, derive collision shapes and bake a CSG tree (None side effect; every result stays in memory and is addressable as memory://name). action selects the operation: 'inspect' reports class, surface_count, aabb, per-surface vertex/index counts, format flags and material; 'surface_arrays' returns the named arrays (vertex, normal, tangent, color, tex_uv, tex_uv2, index) of one surface; 'faces' returns the triangle face list of the whole mesh; 'create_trimesh_shape' derives a ConcavePolygonShape3D for static collision; 'create_convex_shape' derives a ConvexPolygonShape3D with clean/simplify control; 'csg_bake' bakes a root CSGShape3D node, selected by node_path in the edited scene or by node object_id, into an ArrayMesh and/or a ConcavePolygonShape3D and is long blocking; 'free' releases a SurfaceTool session started by mesh_surface. Array payloads are cut at 10000 elements and reported through truncated. Bake failures only return an error and register nothing. Example: {\"action\": \"surface_arrays\", \"mesh\": \"memory://tri_mesh\", \"surface_index\": 0} or {\"action\": \"create_convex_shape\", \"mesh\": \"memory://tri_mesh\", \"name\": \"tri_convex\"} or {\"action\": \"csg_bake\", \"node_path\": \"Level/CSGRoot\", \"shape\": \"both\"}.",
      "Resources", {"mesh", "inspect", "arrays", "faces", "collision", "trimesh", "convex", "csg", "bake", "shape", "surface", "release"}, SideEffect::None, tool_flags::kLongBlocking,
      kMeshInspectParams, mesh_ops::handle_inspect_action,
      schema::build_action_schema(
          kMeshInspectActions, kMeshInspectParams,
          "inspect: mesh metadata and per-surface counts; surface_arrays: named surface arrays; faces: triangle face list; create_trimesh_shape: derive a ConcavePolygonShape3D; create_convex_shape: derive a ConvexPolygonShape3D; csg_bake: bake a root CSGShape3D into mesh and/or collision shape; free: release a SurfaceTool session"),
      {},
      {"mesh_inspect action=inspect mesh=memory://tri_mesh",
       "mesh_inspect action=surface_arrays mesh=memory://tri_mesh surface_index=0",
       "mesh_inspect action=create_convex_shape mesh=memory://tri_mesh name=tri_convex clean=true",
       "mesh_inspect action=csg_bake node_path=Level/CSGRoot name=baked shape=both"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "multimesh",
      "Create and fill an in-memory MultiMesh resource (None side effect; addressable as memory://name). action selects the operation: 'create' instantiates a MultiMesh with an optional mesh, transform_format 3d (default) or 2d, use_colors, use_custom_data and instance_count (0-100000; the count is set after the format flags because changing them resets the buffer); 'set_transforms' writes up to instance_count transforms, each {origin:{x,y,z}, rotation:{x,y,z} in degrees, scale:{x,y,z}, basis:{x|y|z:{x,y,z}} or [[3 floats] x3]} for 3d and {origin:{x,y}, rotation degrees, scale:{x,y}} for 2d; 'set_colors' writes per-instance colors when use_colors is enabled; 'set_buffer' writes the flat PackedFloat32Array buffer whose length must equal instance_count x stride, with stride 12 (3d) or 8 (2d) plus 4 for colors and 4 for custom data; 'info' reports instance_count, visible_instance_count, transform_format, use_colors, use_custom_data, mesh, aabb and buffer_length, optionally including the raw buffer cut at 10000 values. Locate the MultiMesh by memory://name, bare name, decimal object_id, res:// path or locator object. Example: {\"action\": \"create\", \"name\": \"scatter\", \"mesh\": \"memory://tri_mesh\", \"instance_count\": 100} then {\"action\": \"set_transforms\", \"multimesh\": \"memory://scatter\", \"transforms\": [{\"origin\": {\"x\": 0, \"y\": 0, \"z\": 0}}]}.",
      "Resources", {"multimesh", "instances", "transforms", "colors", "buffer", "mesh", "scatter"}, SideEffect::None, tool_flags::kNone,
      kMultiMeshParams, mesh_ops::handle_multimesh_action,
      schema::build_action_schema(
          kMultiMeshActions, kMultiMeshParams,
          "create: instantiate and register a MultiMesh; set_transforms: write instance transforms; set_colors: write per-instance colors; set_buffer: write the raw float buffer; info: report metadata"),
      {},
      {"multimesh action=create name=scatter mesh=memory://tri_mesh instance_count=100",
       "multimesh action=set_transforms multimesh=memory://scatter transforms=[{\"origin\":{\"x\":0,\"y\":1,\"z\":0}},{\"origin\":{\"x\":2,\"y\":1,\"z\":0}}]",
       "multimesh action=info multimesh=memory://scatter"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "multimesh_instance",
      "Assign an in-memory MultiMesh to a MultiMeshInstance3D node in the edited scene (WritesScene, no confirmation). action='assign_to_instance' requires node_path (scene-relative such as 'Level/Scatter' or absolute '/root/Level/Scatter') and multimesh (memory://name, bare name, decimal object_id, res:// path or locator object); nodes of any other class are rejected with their actual class. Returns assigned, node_path, class, multimesh_ref and instance_count. Example: {\"action\": \"assign_to_instance\", \"node_path\": \"Level/Scatter\", \"multimesh\": \"memory://scatter\"}.",
      "Resources", {"multimesh", "instance", "scene", "assign"}, SideEffect::WritesScene, tool_flags::kMutating,
      kMultiMeshInstanceParams, mesh_ops::handle_multimesh_instance_action,
      schema::build_action_schema(
          kMultiMeshInstanceActions, kMultiMeshInstanceParams,
          "assign_to_instance: set the MultiMesh on an existing MultiMeshInstance3D in the edited scene"),
      {},
      {"multimesh_instance action=assign_to_instance node_path=Level/Scatter multimesh=memory://scatter"}}));
  return v;
}

} // namespace mesh_tools
} // namespace godot_autopilot

#endif
