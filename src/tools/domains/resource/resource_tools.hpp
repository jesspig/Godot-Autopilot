#ifndef GODOT_AUTOPILOT_RESOURCE_TOOLS_HPP
#define GODOT_AUTOPILOT_RESOURCE_TOOLS_HPP

#include <functional>
#include <initializer_list>
#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "tools/domains/resource/resource_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace resource_tools {

namespace {

const std::vector<ParamSpec> kResourceLoadParams = {
    {"path", "string", "Resource file path, e.g. res://my_resource.tres; required by every action", true},
    {"type_hint", "string", "Only with action='load' or 'threaded_start': expected resource class to guide loading, e.g. PackedScene, Texture2D, Script; default: none", false},
    {"cache_mode", "string", "Only with action='load': loader cache mode: 'reuse' (default), 'replace' or 'ignore'; ignore re-reads the file from disk instead of returning the cached instance", false},
    {"use_sub_threads", "boolean", "Only with action='threaded_start': split the load across worker sub-threads for faster loading; default: false", false},
};

const std::vector<ParamSpec> kResourceQueryParams = {
    {"path", "string", "Resource file or directory path, e.g. res://my_resource.tres; required by action='type', 'exists', 'dir', 'uid' and 'deps', and a match target for action='references' and 'property' locators", false},
    {"type", "string", "Only with action='extensions': resource class name to list extensions for, e.g. Texture2D, AudioStream; optional — when omitted the loader returns extensions for all known types", false},
    {"dependency", "string", "Only with action='deps': optional dependency path to filter for; when present only that exact dependency is returned if found", false},
    {"uid", "string", "Only with action='references': resource UID to search for in uid:// references, e.g. uid://abc123", false},
    {"class_name", "string", "Only with action='references': script class name to search for in script references, e.g. Player", false},
    {"object_id_str", "string", "Only with action='property': object ID as decimal string, alternative to object_id", false},
    {"object_id", "integer", "Only with action='property': object ID of an in-memory resource (from resource_create or resource_load)", false},
    {"name", "string", "With action='property': registration name of an in-memory resource (memory://name); with action='curve_baked': required curve registration name", false},
    {"property", "string", "Only with action='property': property name to read, e.g. curve, texture", false},
    {"type_hint", "string", "Only with action='property': accepted for parity with resource_property action='set'; reading always returns the raw serialized value", false},
};

const std::vector<ParamSpec> kResourceCreateParams = {
    {"type", "string", "Only with action='create' or 'primitive': for create, the Resource class to instantiate (e.g. Resource, Curve2D, PackedScene; engine classes and global script classes are accepted, abstract or invalid scripts rejected); for primitive, one of box, capsule, plane, quad, sphere, torus, tube, text", false},
    {"name", "string", "Registration name so later calls can reference the instance as memory://name and via the returned object_id; required by action='curve' and 'curve_points', optional elsewhere but recommended", false},
    {"path", "string", "Only with action='duplicate': disk path of the resource to load and duplicate, e.g. res://my_resource.tres; in-memory resources are not supported", false},
    {"deep", "boolean", "Only with action='duplicate': deep duplicate copies sub-resources (true), shallow keeps shared references (false, default)", false},
    {"dimension", "string", "Only with action='curve': '2d' creates a Curve2D (default), '3d' creates a Curve3D", false},
    {"bake_interval", "number", "Only with action='curve': distance between baked points in pixels; must be greater than 0 when provided", false},
    {"points", "array", "Only with action='curve_points': array of 1-256 point objects, each {pos, in?, out?} where pos is required and in/out are optional control handles; Curve2D entries use {x, y}, Curve3D entries use {x, y, z}; unknown fields are rejected and the whole call fails without modifying the curve", false},
    {"params", "object", "Only with action='primitive': optional property overrides applied to the new primitive, e.g. size, radius, height; unknown property names are rejected and nothing is registered", false},
    {"vertices", "array", "Only with action='array': triangle vertex positions as 1-65536 {x, y, z} objects; without indices every 3 vertices form one triangle", false},
    {"normals", "array", "Only with action='array': optional per-vertex normals as {x, y, z} objects; must match vertices in length when provided", false},
    {"indices", "array", "Only with action='array': optional triangle indices as integers in [0, vertex_count); each entry must reference a valid vertex", false},
};

const std::vector<ParamSpec> kResourceMeshParams = {
    {"mesh_ref", "object", "Mesh locator: memory://name string, bare registration name, res:// path, decimal object_id string, or {name|path|object_id|resource} object", true},
    {"surface_idx", "integer", "Zero-based surface index to assign the material to; must be below the mesh surface count", true},
    {"material_path", "string", "Material locator: res:// path, memory://name or bare registration name of an in-memory Material", true},
};

const std::vector<ParamSpec> kResourcePropertyParams = {
    {"object_id_str", "string", "Object ID as decimal string, alternative to object_id", false},
    {"object_id", "integer", "Object ID of an in-memory resource (from resource_create or resource_load)", false},
    {"name", "string", "Registration name of an in-memory resource (memory://name)", false},
    {"path", "string", "Resource path; loads the file from disk when no object_id or name matches", false},
    {"property", "string", "Property name to get or set, e.g. curve, texture; must exist on the resource", true},
    {"value", "json", "Only with action='set': serialized JSON value; resource-typed properties accept {'path': 'res://xxx.tres'} or {'resource': 'memory://name'} references", false},
    {"type_hint", "string", "Only with action='set': Variant type hint for deserialization, e.g. Vector2, Color, int, float; auto-detected from the property metadata when omitted", false},
};

const std::vector<ParamSpec> kResourceFileParams = {
    {"path", "string", "Source resource file or directory path inside res://, e.g. res://assets/icon.png; required by every action", true},
    {"new_path", "string", "Only with action='rename': new resource path, e.g. res://new_name.tres; 'to' is accepted as an alias; optional when new_directory is used", false},
    {"new_directory", "string", "Only with action='rename' (alternative to new_path) or 'move': destination directory inside res:// for a move that keeps the file or directory name; directories move recursively", false},
    {"dest_path", "string", "Only with action='copy': destination file path to write, e.g. res://assets/icon_copy.png; missing parent directories are created recursively; an existing destination is rejected unless overwrite=true and confirm=true are both supplied", false},
    {"overwrite", "boolean", "Only with action='copy': allow replacing an existing destination (default: false); replacing also requires confirm=true", false},
    {"confirm", "boolean", "Only with action='copy' when overwrite=true and dest_path exists: explicit confirmation for replacing the destination", false},
    {"force", "boolean", "Only with action='remove': two-stage safety switch (default false); false returns would_delete plus dependents and deletes nothing, true performs the deletion", false},
};

const std::vector<ParamSpec> kSaveResourceParams = {
    {"path", "string", "Source path to load from, or destination path if object_id is provided; required to identify the resource", true},
    {"dest_path", "string", "Destination file path to write, e.g. res://out/my_resource.tres; default: path. Missing parent directories are created recursively; when dest_path differs from path, the resource is duplicated first so the source cached instance is not modified; the response then includes copy_on_write and source_path", false},
    {"name", "string", "Name of an in-memory resource (as passed to resource_create) to save", false},
    {"class_type", "string", "Resource class to instantiate when no resource resolves, e.g. Curve2D; requires name to register the new instance", false},
    {"object_id", "integer", "Object ID of an in-memory resource (from resource_create) to save; mutually exclusive with object_id_str", false},
    {"object_id_str", "string", "Object ID as decimal string, alternative to object_id", false},
    {"flags", "integer", "ResourceSaver.SaverFlags bitfield, e.g. 1 = RELATIVE_PATHS, 8 = REPLACE_SUBRESOURCE_PATHS; default: 0", false},
};

const std::vector<ParamSpec> kSetResourceUidParams = {
    {"path", "string", "Resource file path to assign the UID to, e.g. res://my_resource.tres", true},
    {"uid", "integer", "UID value to assign; when omitted a fresh UID is auto-generated (contract gap: required in schema but optional in the implementation)", true},
};

const std::vector<ParamSpec> kResourceImportParams = {
    {"path", "string", "Resource file path to reimport (action='reimport') or to change import options (action='import_options'); for reimport this is an alternative to files", false},
    {"files", "array", "Only with action='reimport': array of resource file paths to reimport; alternative to a single path", false},
    {"options", "object", "Only with action='import_options': non-empty object mapping whitelisted importer option keys to new values (audio loop/compression and texture filter/mipmap families, e.g. loop, loop_begin, loop_end, compression, compress/mode, mipmaps/generate, filter); any other key is rejected", false},
};

const std::vector<ParamSpec> kAssignMeshToInstanceParams = {
    {"node_path", "string", "Scene-relative path to a MeshInstance2D or MeshInstance3D node, e.g. 'Level/Enemy' or absolute '/root/Level/Enemy'", true},
    {"mesh_ref", "object", "Mesh locator: memory://name string, bare registration name, res:// path, decimal object_id string, or {name|path|object_id|resource} object", true},
};

const std::vector<std::string> kResourceLoadActions = {
    "load", "reload", "threaded_start", "threaded_status", "threaded_get"};

const std::vector<std::string> kResourceQueryActions = {
    "type", "exists", "types", "extensions", "dir", "uid", "deps",
    "references", "property", "curve_baked"};

const std::vector<std::string> kResourceCreateActions = {
    "create", "duplicate", "curve", "curve_points", "primitive", "array"};

const std::vector<std::string> kResourceMeshActions = {"surface_material"};

const std::vector<std::string> kResourcePropertyActions = {"get", "set"};

const std::vector<std::string> kResourceFileActions = {
    "copy", "rename", "move", "remove", "create_dir"};

const std::vector<std::string> kResourceSaveActions = {"save"};

const std::vector<std::string> kResourceUidSetActions = {"set_uid"};

const std::vector<std::string> kResourceImportActions = {"reimport",
                                                         "import_options"};

inline mcp::JsonValue load_script_args(const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *path = args.Find("path");
  if (path != nullptr) out["path"] = *path;
  out["type_hint"] = mcp::JsonValue("Script");
  bool fresh = false;
  const mcp::JsonValue *fresh_p = args.Find("fresh");
  if (fresh_p != nullptr && fresh_p->IsBool()) {
    fresh = fresh_p->GetBool();
  }
  out["cache_mode"] = mcp::JsonValue(fresh ? "ignore" : "reuse");
  return out;
}

inline mcp::JsonValue validate_load_script(const mcp::JsonValue &args) {
  const mcp::JsonValue *path = args.Find("path");
  const mcp::JsonValue *fresh = args.Find("fresh");
  if (path == nullptr || !path->IsString()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue("missing required parameter: path");
    return error;
  }
  if (fresh != nullptr && !fresh->IsBool()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] =
        mcp::JsonValue("invalid parameter: fresh must be a boolean");
    return error;
  }
  return mcp::JsonValue();
}

inline mcp::JsonValue load_script_action_args(const mcp::JsonValue &args) {
  mcp::JsonValue out = load_script_args(args);
  out["action"] = mcp::JsonValue("load");
  return out;
}

inline mcp::JsonValue dependency_query_args(const mcp::JsonValue &args) {
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  const mcp::JsonValue *path = args.Find("path");
  const mcp::JsonValue *dependency = args.Find("dependency");
  if (path != nullptr) out["path"] = *path;
  if (dependency != nullptr) out["dependency"] = *dependency;
  out["action"] = mcp::JsonValue("deps");
  return out;
}

inline mcp::JsonValue validate_dependency_check(
    const mcp::JsonValue &args) {
  const mcp::JsonValue *path = args.Find("path");
  const mcp::JsonValue *dependency = args.Find("dependency");
  if (path == nullptr || !path->IsString()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue("missing required parameter: path");
    return error;
  }
  if (dependency == nullptr || !dependency->IsString()) {
    mcp::JsonValue error(mcp::JsonValue::object_tag);
    error["error"] = mcp::JsonValue("missing required parameter: dependency");
    return error;
  }
  return mcp::JsonValue();
}

inline mcp::JsonValue project_dependency_has(
    const mcp::JsonValue &result, const mcp::JsonValue &args) {
  const mcp::JsonValue *dependency = args.Find("dependency");
  bool found = false;
  const mcp::JsonValue *inner = result.Find("result");
  if (dependency != nullptr && inner != nullptr && inner->IsArray()) {
    for (const auto &item : inner->GetArray()) {
      if (item.IsString() && item.GetString() == dependency->GetString()) {
        found = true;
        break;
      }
    }
  }
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  out["result"] = mcp::JsonValue(found);
  return out;
}

inline mcp::JsonValue resource_args_with_action(const mcp::JsonValue &args,
                                                const char *action) {
  mcp::JsonValue out = args.IsObject()
                           ? args
                           : mcp::JsonValue(mcp::JsonValue::object_tag);
  out["action"] = mcp::JsonValue(action);
  return out;
}

inline std::function<mcp::JsonValue(const mcp::JsonValue &)>
resource_required_validator(std::initializer_list<const char *> required) {
  std::vector<std::string> names;
  names.reserve(required.size());
  for (const char *name : required) {
    names.emplace_back(name);
  }
  return [names](const mcp::JsonValue &args) {
    for (const std::string &name : names) {
      if (args.Find(name.c_str()) == nullptr) {
        mcp::JsonValue error(mcp::JsonValue::object_tag);
        error["error"] = mcp::JsonValue("missing required parameter: " + name);
        return error;
      }
    }
    return mcp::JsonValue();
  };
}

inline ToolAlias resource_alias(const char *name, const char *canonical,
                                const char *action) {
  ToolAlias alias;
  alias.name = name;
  alias.canonical = canonical;
  alias.adapt_args = [action](const mcp::JsonValue &args) {
    return resource_args_with_action(args, action);
  };
  alias.deprecated_since = "0.4.0";
  alias.removal_version = "0.6.0";
  return alias;
}

inline ToolAlias resource_alias(const char *name, const char *canonical,
                                const char *action,
                                std::initializer_list<const char *> required) {
  ToolAlias alias = resource_alias(name, canonical, action);
  alias.validate_args = resource_required_validator(required);
  return alias;
}

inline ToolAlias resource_alias(
    const char *name, const char *canonical, const char *action,
    std::function<mcp::JsonValue(const mcp::JsonValue &)> adapt_args,
    std::function<mcp::JsonValue(const mcp::JsonValue &)> validate_args,
    std::function<mcp::JsonValue(const mcp::JsonValue &)> project_result = {},
    std::function<mcp::JsonValue(const mcp::JsonValue &, const mcp::JsonValue &)>
        project_result_with_args = {}) {
  ToolAlias alias;
  alias.name = name;
  alias.canonical = canonical;
  alias.adapt_args = std::move(adapt_args);
  alias.project_result = std::move(project_result);
  alias.project_result_with_args = std::move(project_result_with_args);
  alias.deprecated_since = "0.4.0";
  alias.removal_version = "0.6.0";
  alias.validate_args = std::move(validate_args);
  return alias;
}

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(10);
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "resource_load",
      "Load or reload a resource file, optionally through the threaded loader (None side effect). action selects the operation: 'load' loads path into memory and returns the reference with class, path, object_id, object_id_str and name, with optional type_hint to guide loading and cache_mode (reuse/replace/ignore) to control the loader cache; 'reload' force-reloads path from disk with CACHE_MODE_REPLACE and returns result (reloaded), path, replaced and reused_cached_instance; 'threaded_start' starts a background load and returns the Godot error code, then poll 'threaded_status' (status is invalid_resource, in_progress, failed or loaded) and fetch the resource with 'threaded_get'. Example: {\"action\": \"load\", \"path\": \"res://data/config.tres\"} or {\"action\": \"threaded_start\", \"path\": \"res://big.tscn\", \"use_sub_threads\": true}.",
      "Resources", {"resource", "load", "reload", "cache", "threaded", "status", "wait", "script", "load_resource", "load_script", "reload_resource", "load_resource_threaded", "get_resource_load_threaded", "get_resource_load_threaded_status"}, SideEffect::None, tool_flags::kNone,
      kResourceLoadParams, resource_ops::handle_load_action,
      schema::build_action_schema(
          kResourceLoadActions, kResourceLoadParams,
          "load: load a resource file into memory; reload: force-reload with CACHE_MODE_REPLACE; threaded_start: start a background load; threaded_status: poll the threaded load; threaded_get: fetch the threaded load result"),
      {},
      {"resource_load action=load path=res://data/config.tres type_hint=PackedScene",
       "resource_load action=threaded_start path=res://big.tscn use_sub_threads=true"}},
      {
          resource_alias("load_resource", "resource_load", "load", {"path"}),
          resource_alias("load_script", "resource_load", "load",
                         load_script_action_args, validate_load_script),
          resource_alias("reload_resource", "resource_load", "reload",
                         {"path"}),
          resource_alias("load_resource_threaded", "resource_load",
                         "threaded_start", {"path"}),
          resource_alias("get_resource_load_threaded_status", "resource_load",
                         "threaded_status", {"path"}),
          resource_alias("get_resource_load_threaded", "resource_load",
                         "threaded_get", {"path"}),
      })));
  v.push_back(make_spec_tool(ToolSpec{
      "resource_query",
      "Read-only resource queries (None side effect, no confirmation). action selects the operation: 'type' reports the loaded engine class and path; 'exists' checks loadability through the resource loader (not a plain file-existence test); 'types' lists every instantiable Resource subclass; 'extensions' lists the recognized file extensions for a type; 'dir' lists directory entries; 'uid' reads the numeric UID (-1 when none); 'deps' lists the external dependencies of a resource with an optional dependency filter; 'references' reverse-scans every .tscn/.tres for path, uid or class_name references; 'property' reads one property value from a resource located by object_id/object_id_str, name or path; 'curve_baked' reads the baked polyline of a curve registered as memory://name. Example: {\"action\": \"exists\", \"path\": \"res://icon.svg\"} or {\"action\": \"references\", \"path\": \"res://assets/icon.png\"}.",
      "Resources", {"resource", "query", "type", "exists", "types", "extensions", "directory", "dir", "uid", "deps", "dependencies", "references", "reverse", "property", "curve", "baked", "get_resource_type", "has_resource", "get_resource_types", "get_resource_extensions", "get_resource_dir_files", "get_resource_uid", "get_resource_dependencies", "get_resource_references", "get_curve_baked_points"}, SideEffect::None, tool_flags::kNone,
      kResourceQueryParams, resource_ops::handle_query_action,
      schema::build_action_schema(
          kResourceQueryActions, kResourceQueryParams,
          "type: report the resource class; exists: check loadability; types: list instantiable Resource subclasses; extensions: list recognized extensions; dir: list directory entries; uid: read the numeric UID; deps: list dependencies; references: reverse-reference scan; property: read one property; curve_baked: read a curve's baked polyline"),
{
          resource_alias("get_resource_type", "resource_query", "type",
                         {"path"}),
          resource_alias("has_resource", "resource_query", "exists",
                         {"path"}),
          resource_alias("get_resource_types", "resource_query", "types"),
          resource_alias("get_resource_extensions", "resource_query",
                         "extensions"),
          resource_alias("get_resource_dir_files", "resource_query", "dir",
                         {"path"}),
          resource_alias("get_resource_uid", "resource_query", "uid",
                         {"path"}),
          resource_alias("get_resource_dependencies", "resource_query", "deps",
                         {"path"}),
          resource_alias("has_resource_dependency", "resource_query", "deps",
                         dependency_query_args, validate_dependency_check, {},
                         project_dependency_has),
          resource_alias("get_resource_references", "resource_query",
                         "references"),
          resource_alias("get_curve_baked_points", "resource_query",
                         "curve_baked", {"name"}),
      },
{"resource_query action=exists path=res://icon.svg",
       "resource_query action=deps path=res://scene.tscn",
       "resource_query action=curve_baked name=path"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "resource_create",
      "Create in-memory resources (None side effect; nothing is written to disk). action selects the operation: 'create' instantiates a Resource subclass by type and optionally registers it as memory://name; 'duplicate' loads path and returns a duplicate (deep controls sub-resource copying); 'curve' creates a Curve2D ('2d', default) or Curve3D ('3d') with an optional bake_interval and registers it as memory://name; 'curve_points' replaces all control points of a curve created by action='curve' (all-or-nothing); 'primitive' creates a PrimitiveMesh selected by type (box, capsule, plane, quad, sphere, torus, tube, text) with optional params overrides; 'array' creates an ArrayMesh triangle surface from vertices with optional normals and indices. Returns the reference with class, name, path, object_id and object_id_str; mesh actions also return mesh_ref. Example: {\"action\": \"create\", \"type\": \"Curve2D\", \"name\": \"path\"} or {\"action\": \"array\", \"vertices\": [{\"x\": 0, \"y\": 0, \"z\": 0}]}.",
      "Resources", {"resource", "create", "duplicate", "curve", "points", "primitive", "mesh", "array", "vertices", "create_resource", "duplicate_resource", "create_curve_resource", "set_curve_points", "create_primitive_mesh", "create_array_mesh"}, SideEffect::None, tool_flags::kNone,
      kResourceCreateParams, resource_ops::handle_create_action,
      schema::build_action_schema(
          kResourceCreateActions, kResourceCreateParams,
          "create: instantiate a Resource class in memory; duplicate: duplicate a disk resource; curve: create a Curve2D/Curve3D; curve_points: replace curve control points; primitive: create a PrimitiveMesh; array: create an ArrayMesh surface"),
{
          resource_alias("create_resource", "resource_create", "create",
                         {"type"}),
          resource_alias("duplicate_resource", "resource_create", "duplicate",
                         {"path"}),
          resource_alias("create_curve_resource", "resource_create", "curve",
                         {"name"}),
          resource_alias("set_curve_points", "resource_create",
                         "curve_points", {"name", "points"}),
          resource_alias("create_primitive_mesh", "resource_create",
                         "primitive", {"type"}),
          resource_alias("create_array_mesh", "resource_create", "array",
                         {"vertices"}),
      },
{"resource_create action=create type=Curve2D name=path",
       "resource_create action=curve name=path dimension=3d bake_interval=4",
       "resource_create action=primitive type=box name=crate params={\"size\":{\"x\":2,\"y\":2,\"z\":2}}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "resource_mesh",
      "Assign a Material to one surface of a mesh (None side effect). action='surface_material' requires mesh_ref (memory://name string, bare registration name, res:// path, decimal object_id string, or locator object), surface_idx (zero-based, must be below the mesh surface count) and material_path (res:// path, memory://name or bare registration name of an in-memory Material). Returns ok with class, surface_idx, surface_count and material. Errors when the mesh or material cannot be resolved, the resolved values are not a Mesh/Material, or the surface index is out of range. Example: {\"action\": \"surface_material\", \"mesh_ref\": \"memory://crate\", \"surface_idx\": 0, \"material_path\": \"res://materials/wood.tres\"}.",
      "Resources", {"resource", "mesh", "material", "surface", "set", "set_mesh_surface_material"}, SideEffect::None, tool_flags::kMutating,
      kResourceMeshParams, resource_ops::handle_mesh_action,
      schema::build_action_schema(
          kResourceMeshActions, kResourceMeshParams,
          "surface_material: assign a Material to one surface of a mesh"),
{
          resource_alias("set_mesh_surface_material", "resource_mesh",
                         "surface_material",
                         {"mesh_ref", "surface_idx", "material_path"}),
      },
{"resource_mesh action=surface_material mesh_ref=memory://crate surface_idx=0 material_path=res://materials/wood.tres"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "resource_property",
      "Read or modify a property on a resource (None side effect, no confirmation). action selects the operation: 'get' reads a property value; 'set' writes a serialized JSON value and returns ok with class, path, object_id, resource_attached and possible readback warnings. Locate the resource by object_id/object_id_str (from resource_create or resource_load), name (in-memory) or path (loaded from disk); value on set accepts {'path': 'res://...'} or {'resource': 'memory://name'} references for resource-typed properties, and type_hint guides deserialization. Example: {\"action\": \"get\", \"name\": \"path\", \"property\": \"bake_interval\"} or {\"action\": \"set\", \"name\": \"path\", \"property\": \"bake_interval\", \"value\": 8}.",
      "Resources", {"resource", "property", "get", "set", "readback", "get_resource_property", "set_resource_property"}, SideEffect::None, tool_flags::kNone,
      kResourcePropertyParams, resource_ops::handle_property_action,
      schema::build_action_schema(
          kResourcePropertyActions, kResourcePropertyParams,
          "get: read a property value from a resource; set: write a serialized JSON value to a resource property"),
{
          resource_alias("get_resource_property", "resource_property", "get",
                         {"property"}),
          resource_alias("set_resource_property", "resource_property", "set",
                         {"property", "value"}),
      },
{"resource_property action=get name=path property=bake_interval",
       "resource_property action=set name=path property=bake_interval value=8"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "resource_file",
      "Copy, rename, move, remove or create resource files and directories (WritesFile; copy overwrite requires explicit confirmation). action selects the operation: 'copy' copies path to dest_path inside res:// (missing parent directories are created recursively; an existing dest_path is rejected unless overwrite=true and confirm=true are both supplied, and the staged copy is hash-verified before atomic replacement); 'rename' renames or moves path to new_path (or from/to) or into new_directory while keeping the name — the .uid sidecar travels with the file, open scenes that equal or reference the target are saved and reloaded, path= and uid= tokens in dependent .tscn/.tres files are rewritten and project.godot entries are remapped; 'move' moves path into new_directory and returns moved and failed arrays; 'remove' is a two-stage flow — without force it returns would_delete plus dependents from a reverse-reference scan and deletes nothing, with force=true it moves the file to the OS trash (or permanently removes it) and removes the .uid sidecar; 'create_dir' creates a directory below res:// recursively and reports created plus already_existed. Example: {\"action\": \"rename\", \"path\": \"res://old.tres\", \"new_path\": \"res://new.tres\"} or {\"action\": \"remove\", \"path\": \"res://old.tres\"}.",
      "Resources", {"resource", "file", "copy", "rename", "move", "remove", "delete", "directory", "create", "dry-run", "force", "uid", "copy_resource_file", "rename_resource_file", "move_resource_file", "remove_resource_file", "create_directory"}, SideEffect::WritesFile, tool_flags::kMutating,
      kResourceFileParams, resource_ops::handle_file_action,
      schema::build_action_schema(
          kResourceFileActions, kResourceFileParams,
          "copy: copy a file inside res:// without silent overwrite; rename: rename or move a file/directory with reference rewrite; move: move a file/directory into another directory; remove: two-stage dry-run/force deletion; create_dir: create a directory recursively"),
{
          resource_alias("copy_resource_file", "resource_file", "copy",
                         {"path", "dest_path"}),
          resource_alias("rename_resource_file", "resource_file", "rename"),
          resource_alias("move_resource_file", "resource_file", "move",
                         {"path", "new_directory"}),
          resource_alias("remove_resource_file", "resource_file", "remove",
                         {"path"}),
          resource_alias("create_directory", "resource_file", "create_dir",
                         {"path"}),
      },
{"resource_file action=rename path=res://old.tres new_path=res://new.tres",
       "resource_file action=remove path=res://old.tres",
       "resource_file action=remove path=res://old.tres force=true"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "resource_save",
      "Save a resource to disk at dest_path (defaults to path) (WritesFile, no confirmation). action='save' locates the resource by object_id/object_id_str (from resource_create), name (in-memory) or path (loaded from disk); if none resolves, class_type plus a name creates and registers a new instance. Missing directories are created recursively and the editor file system is refreshed. Returns the Godot error code, verified and directories_created when applicable. verified means the resource was re-read from disk after saving; on Windows, safe-save briefly replaces the file, so a concurrent reader may transiently fail — re-check/retry if that surfaces. Example: {\"action\": \"save\", \"name\": \"path\", \"dest_path\": \"res://curves/path.tres\"}.",
      "Resources", {"resource", "save", "write", "disk", "save_resource"}, SideEffect::WritesFile, tool_flags::kMutating,
      kSaveResourceParams, resource_ops::handle_save_action,
      schema::build_action_schema(
          kResourceSaveActions, kSaveResourceParams,
          "save: save an in-memory or disk resource to dest_path and verify the result"),
{
          resource_alias("save_resource", "resource_save", "save", {"path"}),
      },
{"resource_save action=save name=path dest_path=res://curves/path.tres"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "resource_uid_set",
      "Assign a UID to a resource file so it can be referenced as uid://<id>, the counterpart of resource_query action='uid' (WritesFile, no confirmation). action='set_uid' requires path; uid is optional and auto-generated when omitted. Triggers a filesystem reimport so the assignment persists. Returns the UID that was applied. Example: {\"action\": \"set_uid\", \"path\": \"res://data/config.tres\"}.",
      "Resources", {"resource", "uid", "set", "set_resource_uid"}, SideEffect::WritesFile, tool_flags::kMutating,
      kSetResourceUidParams, resource_ops::handle_uid_set_action,
      schema::build_action_schema(
          kResourceUidSetActions, kSetResourceUidParams,
          "set_uid: assign an explicit or auto-generated UID to a resource file"),
{
          resource_alias("set_resource_uid", "resource_uid_set", "set_uid",
                         {"path"}),
      },
{"resource_uid_set action=set_uid path=res://data/config.tres"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "resource_import",
      "Queue asset reimports and edit .import options (WritesFile, no confirmation; editor only). action='reimport' queues one file (path) or several (files) through the editor file system and returns the queued count, not completion — poll the file system status to know when it finishes; action='import_options' writes whitelisted keys into the .import [params] section of an already-imported asset and queues a reimport, returning applied keys with reimport queued. Only safe importer keys are accepted (audio loop/compression and texture filter/mipmap families). Example: {\"action\": \"reimport\", \"path\": \"res://assets/tex.png\"} or {\"action\": \"import_options\", \"path\": \"res://assets/jump.wav\", \"options\": {\"loop\": true}}.",
      "Resources", {"resource", "reimport", "import", "options", "asset", "reimport_resource_files", "set_resource_import_options"}, SideEffect::WritesFile, tool_flags::kMutating,
      kResourceImportParams, resource_ops::handle_import_action,
      schema::build_action_schema(
          kResourceImportActions, kResourceImportParams,
          "reimport: queue reimport of one or more files; import_options: rewrite whitelisted .import [params] keys and queue a reimport"),
{
          resource_alias("reimport_resource_files", "resource_import",
                         "reimport"),
          resource_alias("set_resource_import_options", "resource_import",
                         "import_options", {"path", "options"}),
      },
{"resource_import action=reimport files=[\"res://assets/tex.png\"]",
       "resource_import action=import_options path=res://assets/jump.wav options={\"loop\":true}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "assign_mesh_to_instance",
      "Assign a mesh to a MeshInstance2D or MeshInstance3D node in the edited scene. Requires node_path (scene-relative path such as 'Level/Enemy' or absolute '/root/Level/Enemy') and mesh_ref (memory://name string, bare registration name, res:// path, decimal object_id string, or locator object). Nodes of any other class are rejected with their actual class name. Returns ok with assigned, node_path, mesh_ref and class; a mesh without normals yields a shading warning. When the instance rejects the in-memory mesh, save it with resource_save first and assign the saved res:// path instead. Example: {\"node_path\": \"Level/Enemy\", \"mesh_ref\": \"memory://crate\"}.",
      "Resources", {"resource", "mesh", "assign", "instance"}, SideEffect::WritesScene, tool_flags::kMutating,
      kAssignMeshToInstanceParams, resource_ops::handle_assign_mesh_to_instance,
      {},
      {},
      {"assign_mesh_to_instance node_path=Level/Enemy mesh_ref=memory://crate"}}));
  return v;
}

} // namespace resource_tools
} // namespace godot_autopilot

#endif
