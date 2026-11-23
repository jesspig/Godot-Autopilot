#ifndef GODOT_AUTOPILOT_RENDER_TOOLS_HPP
#define GODOT_AUTOPILOT_RENDER_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/render/render_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace render_tools {

namespace {

const std::vector<ParamSpec> kCreateRenderScenarioParams = {};

const std::vector<ParamSpec> kSetRenderScenarioEnvironmentParams = {
    {"scenario_rid", "integer", "RID of the render scenario, from create_render_scenario", true},
    {"environment_rid", "integer", "RID of the environment to attach, from render_environment action=create or an external source", true},
};

const std::vector<ParamSpec> kGetRenderCanvasItemRidParams = {
    {"path", "string", "Node path of a CanvasItem node in the edited scene; example: '/root/Node2D/Sprite' (leading slash and root/ prefix are tolerated)", true},
};

const std::vector<ParamSpec> kFindRenderNodeFromRidParams = {
    {"rid", "integer", "RID to resolve, e.g. from get_render_canvas_item_rid; returns is_valid and matching nodes for debugging", true},
};

const std::vector<ParamSpec> kRenderEnvironmentParams = {
    {"environment_rid", "integer", "RID of the environment; required for set_params, set_sky and get, from action=create or an external source", false},
    {"params", "object", "Only with action=set_params: nested groups bg {mode (0=clear_color, 1=color, 2=sky, 3=canvas, 4=keep, 5=camera_feed), color, energy, exposure}, ambient {color, source (0=bg, 1=disabled, 2=color, 3=sky), energy, sky_contribution, reflection_source (0=bg, 1=disabled, 2=sky)}, glow {enabled, level, intensity, strength, mix, bloom_threshold, blend_mode, hdr_bleed_threshold, hdr_bleed_scale, hdr_luminance_cap, glow_map_strength}, ssr {enabled, max_steps, fade_in, fade_out, depth_tolerance}, tonemap {tone_mapper (0=linear, 1=reinhard, 2=filmic, 3=aces), exposure, white}, sdfgi {enabled, cascades, min_cell_size, y_scale, use_occlusion, bounce_feedback, read_sky, energy, normal_bias, probe_bias} and volumetric_fog {enabled, density, albedo, emission, emission_energy, anisotropy, length, detail_spread, gi_inject, temporal_reprojection, temporal_reprojection_amount, ambient_inject, sky_affect}; groups omitted from params keep their stored values", false},
    {"sky_rid", "integer", "Only with action=set_sky: RID of the sky to attach, from render_sky action=create", false},
};

const std::vector<ParamSpec> kRenderInstanceParams = {
    {"instance_rid", "integer", "RID of the instance; required for attach, set_transform, set_visible, set_layer_mask and free, from action=create or an external source", false},
    {"base_rid", "integer", "Only with action=create: RID of the resource to instance, e.g. a mesh from render_mesh action=create or a particles RID", false},
    {"scenario_rid", "integer", "Only with action=create or attach: RID of the scenario; optional on create (an empty RID leaves the instance unattached)", false},
    {"transform", "object", "Optional Transform3D object {origin: {x,y,z}, basis: {x: {x,y,z}, y: ..., z: ...}} or {origin, rotation: {x,y,z} in degrees}", false},
    {"origin", "object", "Transform origin {x, y, z}; alternative to origin_x/origin_y/origin_z", false},
    {"basis", "object", "Transform basis {x: {x,y,z}, y: {x,y,z}, z: {x,y,z}}; alternative to rotation", false},
    {"rotation", "object", "Euler rotation {x, y, z} in degrees; converted to a basis", false},
    {"origin_x", "number", "Origin X (default: 0.0)", false},
    {"origin_y", "number", "Origin Y (default: 0.0)", false},
    {"origin_z", "number", "Origin Z (default: 0.0)", false},
    {"rotation_x", "number", "Euler rotation X in degrees (default: 0.0)", false},
    {"rotation_y", "number", "Euler rotation Y in degrees (default: 0.0)", false},
    {"rotation_z", "number", "Euler rotation Z in degrees (default: 0.0)", false},
    {"visible", "boolean", "Only with action=set_visible: visibility state, true shows and false hides (default: true)", false},
    {"mask", "integer", "Only with action=set_layer_mask: layer mask bitfield; bit n selects layer n+1, example: 1 for layer 1, 3 for layers 1 and 2", false},
};

const std::vector<ParamSpec> kRenderViewportParams = {
    {"viewport_rid", "integer", "RID of the viewport; required for every action except create, from action=create or an external source", false},
    {"camera_rid", "integer", "Only with action=attach_camera: RID of a camera from render_camera action=create", false},
    {"scenario_rid", "integer", "Only with action=set_scenario: RID of a scenario from create_render_scenario", false},
    {"canvas_rid", "integer", "Only with action=attach_canvas: RID of a canvas", false},
    {"rect", "object", "Only with action=attach_to_screen: target rectangle {x, y, width, height}; omitted or zero size maps the whole screen", false},
    {"screen", "integer", "Only with action=attach_to_screen: screen index (default: 0)", false},
    {"width", "integer", "Only with action=set_size: viewport width in pixels (default: 640)", false},
    {"height", "integer", "Only with action=set_size: viewport height in pixels (default: 480)", false},
    {"clear_mode", "number", "Only with action=set_clear_mode: 0=always, 1=never, 2=only_next_frame (default: 0)", false},
};

const std::vector<ParamSpec> kRenderCameraParams = {
    {"camera_rid", "integer", "RID of the render camera; required for transform, perspective and orthogonal, from action=create", false},
    {"transform", "object", "Only with action=transform: Transform3D object {origin: {x,y,z}, basis: {x: {x,y,z}, y: ..., z: ...}} or {origin, rotation: {x,y,z} in degrees}", false},
    {"origin", "object", "Only with action=transform: camera origin {x, y, z}; alternative to origin_x/origin_y/origin_z", false},
    {"basis", "object", "Only with action=transform: basis {x: {x,y,z}, y: {x,y,z}, z: {x,y,z}}; alternative to rotation", false},
    {"rotation", "object", "Only with action=transform: Euler rotation {x, y, z} in degrees; converted to a basis", false},
    {"origin_x", "number", "Only with action=transform: camera origin X (default: 0.0); example: 1.5", false},
    {"origin_y", "number", "Only with action=transform: camera origin Y (default: 0.0); example: 2.0", false},
    {"origin_z", "number", "Only with action=transform: camera origin Z (default: 0.0); example: -10.0", false},
    {"rotation_x", "number", "Only with action=transform: Euler rotation X in degrees (default: 0.0)", false},
    {"rotation_y", "number", "Only with action=transform: Euler rotation Y in degrees (default: 0.0)", false},
    {"rotation_z", "number", "Only with action=transform: Euler rotation Z in degrees (default: 0.0)", false},
    {"fovy_degrees", "number", "Only with action=perspective: vertical field of view in degrees (default: 75); example: 60", false},
    {"size", "number", "Only with action=orthogonal: orthogonal viewport size in units (default: 10)", false},
    {"z_near", "number", "Near clip distance, keep small but positive (default: 0.01)", false},
    {"z_far", "number", "Far clip distance, objects beyond it are culled (default: 4000)", false},
};

const std::vector<ParamSpec> kRenderCanvasItemParams = {
    {"canvas_item_rid", "integer", "RID of the canvas item; required for every action except create, from action=create", false},
    {"rect", "object", "Rectangle {position: {x, y}, size: {x, y}}; used by add_rect and add_texture_rect", false},
    {"color", "object", "Color {r, g, b, a}, each in 0.0-1.0; used by add_rect, add_circle and add_line", false},
    {"antialiased", "boolean", "Antialias the shape edges (default: false)", false},
    {"position", "object", "Only with action=add_circle: circle center {x, y}", false},
    {"radius", "number", "Only with action=add_circle: circle radius in pixels (default: 1.0)", false},
    {"texture_rid", "integer", "Only with action=add_texture_rect or add_polygon: texture RID from render_texture action=create_from_image", false},
    {"tile", "boolean", "Only with action=add_texture_rect: tile the texture inside the rect (default: false)", false},
    {"modulate", "object", "Only with action=add_texture_rect: tint color {r, g, b, a}; white leaves the texture unchanged", false},
    {"transpose", "boolean", "Only with action=add_texture_rect: swap the texture's x and y axes (default: false)", false},
    {"from", "object", "Only with action=add_line: line start point {x, y}", false},
    {"to", "object", "Only with action=add_line: line end point {x, y}", false},
    {"width", "number", "Only with action=add_line: line width in pixels (default: -1.0, use project setting)", false},
    {"points", "array", "Only with action=add_polygon: convex polygon points as an array of {x, y} objects", false},
    {"colors", "array", "Only with action=add_polygon: per-point colors as an array of {r, g, b, a} objects", false},
    {"uvs", "array", "Only with action=add_polygon: optional per-point UVs as an array of {x, y} objects", false},
    {"x", "number", "Only with action=set_transform: local X axis scale (default: 1.0)", false},
    {"y", "number", "Only with action=set_transform: local Y axis scale (default: 1.0)", false},
    {"origin_x", "number", "Only with action=set_transform: origin X offset in pixels (default: 0.0)", false},
    {"origin_y", "number", "Only with action=set_transform: origin Y offset in pixels (default: 0.0)", false},
    {"visible", "boolean", "Only with action=set_visible: visibility state, true shows and false hides (default: true)", false},
};

const std::vector<ParamSpec> kRenderLightParams = {
    {"light_rid", "integer", "RID of the render light; required for set_param and set_color, from action=create", false},
    {"type", "string", "Only with action=create: light type directional (sun-like, parallel rays), omni (point light) or spot (cone light); default: directional", false},
    {"param", "number", "Only with action=set_param: light parameter index 0=energy, 1=specular, 2=range, 3=size, 4=attenuation", false},
    {"value", "number", "Only with action=set_param: parameter value; units depend on param, e.g. range is in meters", false},
    {"color", "object", "Only with action=set_color: light color {r, g, b, a}; alpha is ignored, example: {r: 1, g: 0.9, b: 0.7, a: 1}", false},
};

const std::vector<ParamSpec> kRenderParticlesParams = {
    {"particles_rid", "integer", "RID of a server-side particle system; locator for set_emitting, restart and set_lifetime, from action=create", false},
    {"path", "string", "Node path of a GPUParticles2D, GPUParticles3D, CPUParticles2D or CPUParticles3D node in the edited scene; alternative locator for set_emitting, restart, set_params and get_state; example: 'Level1/Sparks'", false},
    {"mode", "integer", "Only with action=create: particle mode 0=2D, 1=3D (default: 1)", false},
    {"emitting", "boolean", "Emit particles, true starts and false stops emission (default: true)", false},
    {"lifetime", "number", "Particle lifetime in seconds; controls how long each particle stays alive (default: 1.0)", false},
    {"amount", "integer", "Only with action=set_params: particle amount, number of particles in the system", false},
    {"one_shot", "boolean", "Only with action=set_params: one-shot emission, true emits once then stops (default: false)", false},
    {"speed_scale", "number", "Only with action=set_params: speed scale multiplier for particle playback (default: 1.0)", false},
    {"explosiveness", "number", "Only with action=set_params: explosiveness ratio 0.0-1.0, higher values emit particles together at the start (default: 0.0)", false},
};

const std::vector<ParamSpec> kRenderMaterialParams = {
    {"material_rid", "integer", "RID of the render material; required for set_param, from action=create", false},
    {"parameter", "string", "Only with action=set_param: material parameter name; example: 'albedo_color'", false},
    {"value", "json", "Only with action=set_param: parameter value as JSON; plain numbers/strings/bools also work", false},
    {"type_hint", "string", "Only with action=set_param: type hint (e.g. Vector2, Color, int, float)", false},
};

const std::vector<ParamSpec> kRenderShaderParams = {
    {"shader_rid", "integer", "RID of the shader; required for set_code, from action=create", false},
    {"code", "string", "Shader source code; optional on create and required for set_code; must be a full shader file, not a snippet", false},
    {"name", "string", "Only with action=set_global_param: global shader parameter name; the shader must declare a matching parameter with the global keyword", false},
    {"value", "json", "Only with action=set_global_param: parameter value as JSON; plain numbers/strings/bools also work", false},
    {"type_hint", "string", "Only with action=set_global_param: type hint (e.g. Vector2, Color, int, float)", false},
};

const std::vector<ParamSpec> kRenderSkyParams = {
    {"sky_rid", "integer", "RID of the sky; required for set_material, from action=create", false},
    {"material_rid", "integer", "Only with action=set_material: RID of the material to assign, from render_material action=create", false},
};

const std::vector<ParamSpec> kRenderFogVolumeParams = {
    {"fog_rid", "integer", "RID of the fog volume; required for set_shape, from action=create", false},
    {"shape", "number", "Fog volume shape 0=ellipsoid, 1=cone, 2=cylinder, 3=box, 4=world; optional on create and required for set_shape", false},
    {"size", "object", "Only with action=create: fog volume size {x, y, z}; example: {x: 4, y: 2, z: 4}", false},
};

const std::vector<ParamSpec> kRenderDecalParams = {};

const std::vector<ParamSpec> kRenderReflectionProbeParams = {};

const std::vector<ParamSpec> kRenderMeshParams = {
    {"mesh_rid", "integer", "RID of the render mesh; required for add_surface and set_surface_material, from action=create", false},
    {"primitive", "integer", "Only with action=add_surface: primitive type 0=points, 1=lines, 2=line_strip, 3=triangle_strip, 4=triangle_fan, 5=triangles (default: 5)", false},
    {"arrays", "object", "Only with action=add_surface: surface arrays; keys vertices and normals (arrays of {x, y, z} objects), tangents (flat array of floats, 4 per vertex), colors (array of {r, g, b, a} objects), uvs (array of {x, y} objects) and indices (array of integers)", false},
    {"surface", "integer", "Only with action=set_surface_material: surface index, 0-based in add_surface call order", false},
    {"material_rid", "integer", "Only with action=set_surface_material: RID of the material to assign, from render_material action=create", false},
};

const std::vector<ParamSpec> kRenderTextureParams = {
    {"image_path", "string", "Only with action=create_from_image: image file path; accepts res://, user:// or absolute paths (relative paths are not supported); example: 'res://icon.svg'", false},
};

const std::vector<ParamSpec> kRenderInspectParams = {
    {"material_rid", "integer", "Only with action=material_get_param: RID of the material to inspect", false},
    {"parameter", "string", "Only with action=material_get_param: parameter name; example: 'albedo_color'", false},
    {"shader_rid", "integer", "Only with action=shader_get_code or shader_get_param_list: RID of the shader to inspect", false},
    {"mesh_rid", "integer", "Only with action=mesh_surface_get_arrays: RID of the mesh to inspect", false},
    {"surface", "integer", "Only with action=mesh_surface_get_arrays: 0-based surface index", false},
    {"texture_rid", "integer", "Only with action=texture_2d_get: RID of the texture to read back as PNG", false},
    {"name", "string", "Only with action=global_shader_parameter_get or global_shader_parameter_remove: global parameter name", false},
};

const std::vector<ParamSpec> kFreeRenderRidParams = {
    {"rid", "integer", "RID to free, from any render create action; example: 123", true},
};

const std::vector<std::string> kRenderEnvironmentActions = {
    "create", "set_params", "set_sky", "get"};
const std::vector<std::string> kRenderInstanceActions = {
    "create", "attach", "set_transform", "set_visible", "set_layer_mask", "free"};
const std::vector<std::string> kRenderViewportActions = {
    "create", "attach_camera", "set_scenario", "attach_canvas",
    "attach_to_screen", "set_size", "set_clear_mode", "capture_texture"};
const std::vector<std::string> kRenderCameraActions = {
    "create", "transform", "perspective", "orthogonal"};
const std::vector<std::string> kRenderCanvasItemActions = {
    "create", "add_rect", "add_circle", "add_line", "add_texture_rect",
    "add_polygon", "set_transform", "set_visible"};
const std::vector<std::string> kRenderLightActions = {
    "create", "set_param", "set_color"};
const std::vector<std::string> kRenderParticlesActions = {
    "create", "set_emitting", "restart", "set_lifetime", "set_params",
    "get_state"};
const std::vector<std::string> kRenderMaterialActions = {"create", "set_param"};
const std::vector<std::string> kRenderShaderActions = {
    "create", "set_code", "set_global_param"};
const std::vector<std::string> kRenderSkyActions = {"create", "set_material"};
const std::vector<std::string> kRenderFogVolumeActions = {"create", "set_shape"};
const std::vector<std::string> kRenderDecalActions = {"create"};
const std::vector<std::string> kRenderReflectionProbeActions = {"create"};
const std::vector<std::string> kRenderMeshActions = {
    "create", "add_surface", "set_surface_material"};
const std::vector<std::string> kRenderTextureActions = {"create_from_image"};
const std::vector<std::string> kRenderInspectActions = {
    "material_get_param", "shader_get_code", "shader_get_param_list",
    "mesh_surface_get_arrays", "texture_2d_get",
    "global_shader_parameter_get", "global_shader_parameter_list",
    "global_shader_parameter_remove"};

inline mcp::JsonValue render_action_args(const mcp::JsonValue &args,
                                         const char *action) {
  mcp::JsonValue out = args.IsObject()
                           ? args
                           : mcp::JsonValue(mcp::JsonValue::object_tag);
  out["action"] = mcp::JsonValue(action);
  return out;
}

inline mcp::JsonValue render_environment_group_args(const mcp::JsonValue &args,
                                                    const char *group) {
  mcp::JsonValue group_values(mcp::JsonValue::object_tag);
  if (args.IsObject()) {
    for (const auto &entry : args.GetObject()) {
      if (entry.first == "environment_rid" || entry.first == "action" ||
          entry.first == "params" || entry.first == "confirm") {
        continue;
      }
      group_values[entry.first] = entry.second;
    }
  }
  mcp::JsonValue params(mcp::JsonValue::object_tag);
  params[group] = std::move(group_values);
  mcp::JsonValue out(mcp::JsonValue::object_tag);
  if (const mcp::JsonValue *environment = args.Find("environment_rid")) {
    out["environment_rid"] = *environment;
  }
  out["action"] = mcp::JsonValue("set_params");
  out["params"] = std::move(params);
  return out;
}

inline ToolAlias render_alias(const char *name, const char *canonical,
                              const char *action) {
  ToolAlias alias;
  alias.name = name;
  alias.canonical = canonical;
  alias.adapt_args = [action](const mcp::JsonValue &args) {
    return render_action_args(args, action);
  };
  alias.deprecated_since = "0.4.0";
  alias.removal_version = "0.6.0";
  return alias;
}

inline ToolAlias render_environment_alias(const char *name,
                                          const char *group) {
  ToolAlias alias;
  alias.name = name;
  alias.canonical = "render_environment";
  alias.adapt_args = [group](const mcp::JsonValue &args) {
    return render_environment_group_args(args, group);
  };
  alias.deprecated_since = "0.4.0";
  alias.removal_version = "0.6.0";
  return alias;
}

const std::vector<ParamSpec> kCreateRenderMaterialParams = {};
const std::vector<ParamSpec> kCreateRenderSkyParams = {};
const std::vector<ParamSpec> kCreateRenderReflectionProbeParams = {};
const std::vector<ParamSpec> kCreateRenderDecalParams = {};

const std::vector<ParamSpec> kCreateRenderShaderParams = {
    {"code", "string", "Shader source code (optional); can be replaced later with set_render_shader_code", false},
};

const std::vector<ParamSpec> kRestartParticlesParams = {
    {"path", "string", "Node path of a GPUParticles2D, GPUParticles3D, CPUParticles2D or CPUParticles3D node in the edited scene; example: 'Level1/Sparks' (leading slash and root/ prefix are tolerated)", true},
};

const std::vector<ParamSpec> kSetParticlesParamsParams = {
    {"path", "string", "Node path of a GPUParticles2D, GPUParticles3D, CPUParticles2D or CPUParticles3D node in the edited scene; example: 'Level1/Sparks' (leading slash and root/ prefix are tolerated)", true},
    {"emitting", "boolean", "Emit particles, true starts and false stops emission", false},
    {"amount", "integer", "Particle amount, number of particles in the system", false},
    {"lifetime", "number", "Particle lifetime in seconds; controls how long each particle stays alive (default: 1.0)", false},
    {"one_shot", "boolean", "One-shot emission, true emits once then stops (default: false)", false},
    {"speed_scale", "number", "Speed scale multiplier for particle playback (default: 1.0)", false},
    {"explosiveness", "number", "Explosiveness ratio 0.0-1.0, higher values emit particles together at the start (default: 0.0)", false},
};

const std::vector<ParamSpec> kGetParticlesStateParams = {
    {"path", "string", "Node path of a GPUParticles2D, GPUParticles3D, CPUParticles2D or CPUParticles3D node in the edited scene; example: 'Level1/Sparks' (leading slash and root/ prefix are tolerated)", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(29);
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "render_environment",
      "Create and configure a server-side Environment RID for background, ambient light, glow, SSR, tone mapping, SDFGI and volumetric fog (None side effect). action selects the operation: 'create' calls environment_create and returns result.rid; 'set_params' applies a nested params object whose optional bg, ambient, glow, ssr, tonemap, sdfgi and volumetric_fog groups use the same fields as the former dedicated environment tools, and groups omitted from params keep their stored values; 'set_sky' attaches sky_rid with environment_set_sky; 'get' returns the stored params. Attach the environment to a scenario with set_render_scenario_environment. Example: {\"action\": \"create\"} then {\"action\": \"set_params\", \"environment_rid\": 7, \"params\": {\"bg\": {\"mode\": 1, \"color\": {\"r\": 0.1, \"g\": 0.2, \"b\": 0.4, \"a\": 1}}, \"glow\": {\"enabled\": true, \"level\": 0.85}}}.",
      "Render", {"render", "environment", "bg", "ambient", "glow", "ssr", "tonemap", "sdfgi", "volumetric_fog", "set_render_environment_bg_color", "set_render_environment_ambient_light", "set_render_environment_glow", "set_render_environment_ssr", "set_render_environment_tonemap", "set_render_environment_sdfgi", "set_render_environment_volumetric_fog"},
      SideEffect::None, tool_flags::kNone,
      kRenderEnvironmentParams, render_ops::handle_environment_action,
      schema::build_action_schema(
          kRenderEnvironmentActions, kRenderEnvironmentParams,
          "create: create an environment RID; set_params: apply nested bg/ambient/glow/ssr/tonemap/sdfgi/volumetric_fog groups; set_sky: attach a sky RID; get: return the stored params"),
      {},
      {"render_environment action=create",
       "render_environment action=set_params environment_rid=7 params={\"bg\":{\"mode\":1,\"color\":{\"r\":0.1,\"g\":0.2,\"b\":0.4,\"a\":1}},\"glow\":{\"enabled\":true}}",
       "render_environment action=set_sky environment_rid=7 sky_rid=8"}},
      {
          render_environment_alias("set_render_environment_bg_color", "bg"),
          render_environment_alias("set_render_environment_ambient_light",
                                   "ambient"),
          render_environment_alias("set_render_environment_glow", "glow"),
          render_environment_alias("set_render_environment_ssr", "ssr"),
          render_environment_alias("set_render_environment_tonemap",
                                   "tonemap"),
          render_environment_alias("set_render_environment_sdfgi", "sdfgi"),
          render_environment_alias("set_render_environment_volumetric_fog",
                                   "volumetric_fog"),
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "render_instance",
      "Create and control a RenderingServer instance that makes a mesh or another RID renderable (None side effect). action selects the operation: 'create' calls instance_create2(base_rid, scenario_rid) and returns result.rid, with scenario_rid optional; 'attach' moves the instance to scenario_rid; 'set_transform' builds a Transform3D from optional transform/origin/basis objects or rotation/origin_x/y/z with Euler degrees; 'set_visible' toggles visibility; 'set_layer_mask' sets the mask bitfield; 'free' calls free_rid and invalidates the RID in the store. Example: {\"action\": \"create\", \"base_rid\": 12, \"scenario_rid\": 3} or {\"action\": \"set_transform\", \"instance_rid\": 9, \"origin\": {\"x\": 0, \"y\": 1, \"z\": -4}, \"rotation\": {\"x\": 0, \"y\": 180, \"z\": 0}}.",
      "Render", {"render", "instance", "set_render_instance_visible", "set_render_instance_layer_mask"},
      SideEffect::None, tool_flags::kNone,
      kRenderInstanceParams, render_ops::handle_instance_action,
      schema::build_action_schema(
          kRenderInstanceActions, kRenderInstanceParams,
          "create: instance a base RID into a scenario; attach: move the instance to another scenario; set_transform: set origin, basis or Euler rotation; set_visible: toggle visibility; set_layer_mask: set the layer mask; free: free the instance RID"),
      {},
      {"render_instance action=create base_rid=12 scenario_rid=3",
       "render_instance action=set_transform instance_rid=9 origin={\"x\":0,\"y\":1,\"z\":-4} rotation={\"x\":0,\"y\":180,\"z\":0}}",
       "render_instance action=free instance_rid=9"}},
      {
          render_alias("set_render_instance_visible", "render_instance",
                       "set_visible"),
          render_alias("set_render_instance_layer_mask", "render_instance",
                       "set_layer_mask"),
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "render_viewport",
      "Create and wire a server-side Viewport RID and optionally capture its texture as PNG (None side effect). action selects the operation: 'create' returns result.rid; 'attach_camera' binds camera_rid; 'set_scenario' binds scenario_rid; 'attach_canvas' binds canvas_rid; 'attach_to_screen' maps the viewport to the screen with optional rect {x, y, width, height} and screen; 'set_size' sets width and height; 'set_clear_mode' sets 0=always, 1=never, 2=only_next_frame; 'capture_texture' forces a draw, reads viewport_get_texture plus texture_2d_get, retries after one more rendered frame when the image is still empty, and returns result {format: 'png', data, width, height}; a still-empty texture returns code viewport_texture_not_ready. Example: {\"action\": \"create\"} then {\"action\": \"capture_texture\", \"viewport_rid\": 11}.",
      "Render", {"render", "viewport", "create_render_viewport", "set_render_viewport_size", "set_render_viewport_clear_mode", "capture"},
      SideEffect::None, tool_flags::kCaptureImage,
      kRenderViewportParams, render_ops::handle_viewport_action,
      schema::build_action_schema(
          kRenderViewportActions, kRenderViewportParams,
          "create: create a viewport RID; attach_camera: bind a camera; set_scenario: bind a scenario; attach_canvas: bind a canvas; attach_to_screen: map to a screen rectangle; set_size: resize the render target; set_clear_mode: select the clear mode; capture_texture: read the viewport texture back as PNG"),
      {},
      {"render_viewport action=create",
       "render_viewport action=set_size viewport_rid=11 width=640 height=480",
       "render_viewport action=capture_texture viewport_rid=11"}},
      {
          render_alias("create_render_viewport", "render_viewport", "create"),
          render_alias("set_render_viewport_size", "render_viewport",
                       "set_size"),
          render_alias("set_render_viewport_clear_mode", "render_viewport",
                       "set_clear_mode"),
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "render_camera",
      "Create and configure a server-side 3D camera (None side effect). action selects the operation: 'create' returns result.rid; 'transform' accepts a Transform3D via transform/origin/basis objects or rotation/origin_x/y/z Euler degrees; 'perspective' sets fovy_degrees, z_near and z_far; 'orthogonal' sets size, z_near and z_far. Example: {\"action\": \"transform\", \"camera_rid\": 4, \"origin\": {\"x\": 0, \"y\": 2, \"z\": -8}, \"rotation\": {\"x\": -10, \"y\": 0, \"z\": 0}}.",
      "Render", {"render", "camera", "create_render_camera", "set_render_camera_transform", "set_render_camera_perspective", "set_render_camera_orthogonal"},
      SideEffect::None, tool_flags::kNone,
      kRenderCameraParams, render_ops::handle_camera_action,
      schema::build_action_schema(
          kRenderCameraActions, kRenderCameraParams,
          "create: create a camera RID; transform: set origin, basis or Euler rotation; perspective: set a frustum projection; orthogonal: set an orthogonal projection"),
      {},
      {"render_camera action=create",
       "render_camera action=transform camera_rid=4 origin={\"x\":0,\"y\":2,\"z\":-8} rotation={\"x\":-10,\"y\":0,\"z\":0}",
       "render_camera action=perspective camera_rid=4 fovy_degrees=60"}},
      {
          render_alias("create_render_camera", "render_camera", "create"),
          render_alias("set_render_camera_transform", "render_camera",
                       "transform"),
          render_alias("set_render_camera_perspective", "render_camera",
                       "perspective"),
          render_alias("set_render_camera_orthogonal", "render_camera",
                       "orthogonal"),
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "render_canvas_item",
      "Create and draw on a server-side 2D canvas item (None side effect). action selects the operation: 'create' returns result.rid; 'add_rect', 'add_circle', 'add_line', 'add_texture_rect' and 'add_polygon' append draw commands (points and colors arrays for polygons); 'set_transform' sets the 2D transform; 'set_visible' toggles visibility. Use get_render_canvas_item_rid to obtain the RID of an edited-scene CanvasItem node. Example: {\"action\": \"create\"} then {\"action\": \"add_rect\", \"canvas_item_rid\": 5, \"rect\": {\"position\": {\"x\": 0, \"y\": 0}, \"size\": {\"x\": 64, \"y\": 32}}, \"color\": {\"r\": 1, \"g\": 0.5, \"b\": 0, \"a\": 1}}.",
      "Render", {"render", "canvas", "create_render_canvas_item", "add_render_canvas_item_rect", "add_render_canvas_item_circle", "add_render_canvas_item_line", "add_render_canvas_item_texture_rect", "set_render_canvas_item_transform", "set_render_canvas_item_visible"},
      SideEffect::None, tool_flags::kNone,
      kRenderCanvasItemParams, render_ops::handle_canvas_item_action,
      schema::build_action_schema(
          kRenderCanvasItemActions, kRenderCanvasItemParams,
          "create: create a canvas item RID; add_rect/add_circle/add_line/add_texture_rect/add_polygon: append draw commands; set_transform: set the 2D transform; set_visible: toggle visibility"),
      {},
      {"render_canvas_item action=create",
       "render_canvas_item action=add_polygon canvas_item_rid=5 points=[{\"x\":0,\"y\":0},{\"x\":32,\"y\":0},{\"x\":16,\"y\":32}] colors=[{\"r\":1,\"g\":0,\"b\":0,\"a\":1}]",
       "render_canvas_item action=set_visible canvas_item_rid=5 visible=false"}},
      {
          render_alias("create_render_canvas_item", "render_canvas_item",
                       "create"),
          render_alias("add_render_canvas_item_rect", "render_canvas_item",
                       "add_rect"),
          render_alias("add_render_canvas_item_circle", "render_canvas_item",
                       "add_circle"),
          render_alias("add_render_canvas_item_line", "render_canvas_item",
                       "add_line"),
          render_alias("add_render_canvas_item_texture_rect",
                       "render_canvas_item", "add_texture_rect"),
          render_alias("set_render_canvas_item_transform", "render_canvas_item",
                       "set_transform"),
          render_alias("set_render_canvas_item_visible", "render_canvas_item",
                       "set_visible"),
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "render_light",
      "Create and configure a server-side light (None side effect). action selects the operation: 'create' creates a directional, omni or spot light and returns result.rid; 'set_param' sets a numeric parameter (0=energy, 1=specular, 2=range, 3=size, 4=attenuation); 'set_color' sets the light color, alpha is ignored. Example: {\"action\": \"create\", \"type\": \"omni\"} then {\"action\": \"set_param\", \"light_rid\": 6, \"param\": 0, \"value\": 2.0}.",
      "Render", {"render", "light", "create_render_light", "set_render_light_param", "set_render_light_color"},
      SideEffect::None, tool_flags::kNone,
      kRenderLightParams, render_ops::handle_light_action,
      schema::build_action_schema(
          kRenderLightActions, kRenderLightParams,
          "create: create a directional/omni/spot light RID; set_param: set a numeric light parameter; set_color: set the light color"),
      {},
      {"render_light action=create type=omni",
       "render_light action=set_param light_rid=6 param=0 value=2.0"}},
      {
          render_alias("create_render_light", "render_light", "create"),
          render_alias("set_render_light_param", "render_light", "set_param"),
          render_alias("set_render_light_color", "render_light", "set_color"),
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "render_particles",
      "Create and control server-side particle systems or edited-scene particle nodes (None side effect except scene writes for node actions). action selects the operation: 'create' creates a server particle system (mode 0=2D, 1=3D) and returns result.rid; 'set_emitting' starts or stops emission; 'restart' restarts the emission cycle; 'set_lifetime' sets particle lifetime in seconds; 'set_params' sets emitting, amount, lifetime, one_shot, speed_scale and explosiveness on a particle node; 'get_state' reads the playback state of a particle node. Server actions use particles_rid, node actions use path. Example: {\"action\": \"create\", \"mode\": 1} or {\"action\": \"get_state\", \"path\": \"Level1/Sparks\"}.",
      "Render", {"render", "particles", "create_render_particles", "set_render_particles_emitting", "restart_render_particles", "set_render_particles_lifetime", "restart_particles", "set_particles_params", "get_particles_state"},
      SideEffect::None, tool_flags::kMutating,
      kRenderParticlesParams, render_ops::handle_particles_action,
      schema::build_action_schema(
          kRenderParticlesActions, kRenderParticlesParams,
          "create: create a server particle system RID; set_emitting: toggle emission; restart: restart emission; set_lifetime: set server particle lifetime; set_params: write node playback parameters; get_state: read node playback state"),
      {},
      {"render_particles action=create mode=1",
       "render_particles action=set_emitting particles_rid=8 emitting=false",
       "render_particles action=get_state path=Level1/Sparks"}},
      {
          render_alias("create_render_particles", "render_particles", "create"),
          render_alias("set_render_particles_emitting", "render_particles",
                       "set_emitting"),
          render_alias("restart_render_particles", "render_particles",
                       "restart"),
          render_alias("set_render_particles_lifetime", "render_particles",
                       "set_lifetime"),
          render_alias("restart_particles", "render_particles", "restart"),
          render_alias("set_particles_params", "render_particles", "set_params"),
          render_alias("get_particles_state", "render_particles", "get_state"),
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "render_material",
      "Create a server-side material and set its parameters (None side effect). action selects the operation: 'create' returns result.rid; 'set_param' writes a named parameter from a JSON value, with type_hint controlling Variant deserialization (e.g. Vector2, Color, int, float) and the value matching the shader's declared parameter type. Example: {\"action\": \"create\"} then {\"action\": \"set_param\", \"material_rid\": 3, \"parameter\": \"albedo_color\", \"value\": {\"r\": 1, \"g\": 0, \"b\": 0, \"a\": 1}, \"type_hint\": \"Color\"}.",
      "Render", {"render", "material", "create_render_material", "set_render_material_param"},
      SideEffect::None, tool_flags::kNone,
      kRenderMaterialParams, render_ops::handle_material_action,
      schema::build_action_schema(
          kRenderMaterialActions, kRenderMaterialParams,
          "create: create a material RID; set_param: write a named material parameter"),
      {},
      {"render_material action=create",
       "render_material action=set_param material_rid=3 parameter=albedo_color value={\"r\":1,\"g\":0,\"b\":0,\"a\":1} type_hint=Color"}},
      {
          render_alias("create_render_material", "render_material", "create"),
          render_alias("set_render_material_param", "render_material",
                       "set_param"),
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "render_shader",
      "Create a server-side shader, replace its code and set global shader parameters (None side effect). action selects the operation: 'create' creates a shader and optionally applies code, returning result.rid; 'set_code' replaces the shader source, which must be a full shader file; 'set_global_param' writes a global shader parameter declared with the global keyword, with type_hint controlling Variant deserialization. Example: {\"action\": \"create\", \"code\": \"shader_type canvas_item;\"} or {\"action\": \"set_global_param\", \"name\": \"wind\", \"value\": {\"x\": 1, \"y\": 0}}.",
      "Render", {"render", "shader", "create_render_shader", "set_render_shader_code", "set_render_shader_parameter_global"},
      SideEffect::None, tool_flags::kNone,
      kRenderShaderParams, render_ops::handle_shader_action,
      schema::build_action_schema(
          kRenderShaderActions, kRenderShaderParams,
          "create: create a shader RID with optional code; set_code: replace the shader source; set_global_param: write a global shader parameter"),
      {},
      {"render_shader action=create code=shader_type canvas_item;",
       "render_shader action=set_global_param name=wind value={\"x\":1,\"y\":0}"}},
      {
          render_alias("create_render_shader", "render_shader", "create"),
          render_alias("set_render_shader_code", "render_shader", "set_code"),
          render_alias("set_render_shader_parameter_global", "render_shader",
                       "set_global_param"),
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "render_sky",
      "Create a server-side sky and assign its material (None side effect). action selects the operation: 'create' returns result.rid; 'set_material' assigns a material RID from render_material action=create to define the sky appearance. Attach the sky to an environment with render_environment action=set_sky. Example: {\"action\": \"create\"} then {\"action\": \"set_material\", \"sky_rid\": 8, \"material_rid\": 3}.",
      "Render", {"render", "sky", "create_render_sky", "set_render_sky_material"},
      SideEffect::None, tool_flags::kNone,
      kRenderSkyParams, render_ops::handle_sky_action,
      schema::build_action_schema(
          kRenderSkyActions, kRenderSkyParams,
          "create: create a sky RID; set_material: assign a material to the sky"),
      {},
      {"render_sky action=create",
       "render_sky action=set_material sky_rid=8 material_rid=3"}},
      {
          render_alias("create_render_sky", "render_sky", "create"),
          render_alias("set_render_sky_material", "render_sky", "set_material"),
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "render_fog_volume",
      "Create a server-side fog volume and change its shape (None side effect). action selects the operation: 'create' creates a fog volume with optional shape and size and returns result.rid; 'set_shape' changes the shape later (0=ellipsoid, 1=cone, 2=cylinder, 3=box, 4=world). Combine with an environment that has volumetric fog enabled. Example: {\"action\": \"create\", \"shape\": 3, \"size\": {\"x\": 4, \"y\": 2, \"z\": 4}}.",
      "Render", {"render", "fog", "create_render_fog_volume", "set_render_fog_volume_shape"},
      SideEffect::None, tool_flags::kNone,
      kRenderFogVolumeParams, render_ops::handle_fog_volume_action,
      schema::build_action_schema(
          kRenderFogVolumeActions, kRenderFogVolumeParams,
          "create: create a fog volume RID with optional shape and size; set_shape: change the fog volume shape"),
      {},
      {"render_fog_volume action=create shape=3 size={\"x\":4,\"y\":2,\"z\":4}",
       "render_fog_volume action=set_shape fog_rid=10 shape=0"}},
      {
          render_alias("create_render_fog_volume", "render_fog_volume",
                       "create"),
          render_alias("set_render_fog_volume_shape", "render_fog_volume",
                       "set_shape"),
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "render_decal",
      "Create a server-side decal that projects a texture onto nearby surfaces (None side effect). action='create' returns result.rid; instance the decal with render_instance action=create to place it in a scenario. Example: {\"action\": \"create\"}.",
      "Render", {"render", "decal", "create_render_decal"},
      SideEffect::None, tool_flags::kNone,
      kRenderDecalParams, render_ops::handle_decal_action,
      schema::build_action_schema(kRenderDecalActions, kRenderDecalParams,
                                  "create: create a decal RID"),
      {},
      {"render_decal action=create"}},
      {
          render_alias("create_render_decal", "render_decal", "create"),
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "render_reflection_probe",
      "Create a server-side reflection probe that captures surrounding geometry and lights for reflective materials (None side effect). action='create' returns result.rid; instance the probe with render_instance action=create to place it in a scenario. Example: {\"action\": \"create\"}.",
      "Render", {"render", "reflection_probe", "create_render_reflection_probe"},
      SideEffect::None, tool_flags::kNone,
      kRenderReflectionProbeParams, render_ops::handle_reflection_probe_action,
      schema::build_action_schema(kRenderReflectionProbeActions,
                                  kRenderReflectionProbeParams,
                                  "create: create a reflection probe RID"),
      {},
      {"render_reflection_probe action=create"}},
      {
          render_alias("create_render_reflection_probe",
                       "render_reflection_probe", "create"),
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "render_mesh",
      "Create a server-side mesh, add surfaces from vertex arrays and assign surface materials (None side effect). action selects the operation: 'create' returns result.rid; 'add_surface' appends a surface built from vertices, normals, tangents, colors, uvs and indices with an optional primitive type; 'set_surface_material' assigns a material RID to a 0-based surface. A mesh alone is not visible; instance it with render_instance action=create. Example: {\"action\": \"create\"} then {\"action\": \"add_surface\", \"mesh_rid\": 12, \"arrays\": {\"vertices\": [{\"x\": 0, \"y\": 0, \"z\": 0}]}}.",
      "Render", {"render", "mesh", "create_render_mesh", "add_render_mesh_surface", "set_render_mesh_surface_material"},
      SideEffect::None, tool_flags::kNone,
      kRenderMeshParams, render_ops::handle_mesh_action,
      schema::build_action_schema(
          kRenderMeshActions, kRenderMeshParams,
          "create: create a mesh RID; add_surface: append a surface from vertex arrays; set_surface_material: assign a material to a surface"),
      {},
      {"render_mesh action=create",
       "render_mesh action=add_surface mesh_rid=12 arrays={\"vertices\":[{\"x\":0,\"y\":0,\"z\":0},{\"x\":1,\"y\":0,\"z\":0},{\"x\":0,\"y\":1,\"z\":0}]}",
       "render_mesh action=set_surface_material mesh_rid=12 surface=0 material_rid=3"}},
      {
          render_alias("create_render_mesh", "render_mesh", "create"),
          render_alias("add_render_mesh_surface", "render_mesh", "add_surface"),
          render_alias("set_render_mesh_surface_material", "render_mesh",
                       "set_surface_material"),
      })));
  v.push_back(make_spec_tool(with_aliases(ToolSpec{
      "render_texture",
      "Create a server-side 2D texture from an image file (None side effect). action='create_from_image' loads image_path (res://, user:// or absolute; relative paths are not supported) and returns result.rid for use with render_canvas_item action=add_texture_rect or add_polygon. Example: {\"action\": \"create_from_image\", \"image_path\": \"res://icon.svg\"}.",
      "Render", {"render", "texture", "create_render_texture_from_image"},
      SideEffect::None, tool_flags::kNone,
      kRenderTextureParams, render_ops::handle_texture_action,
      schema::build_action_schema(kRenderTextureActions, kRenderTextureParams,
                                  "create_from_image: create a 2D texture RID from an image file"),
      {},
      {"render_texture action=create_from_image image_path=res://icon.svg"}},
      {
          render_alias("create_render_texture_from_image", "render_texture",
                       "create_from_image"),
      })));
  v.push_back(make_spec_tool(ToolSpec{
      "render_inspect",
      "Read back RenderingServer objects without mutating them (None side effect). action selects the operation: 'material_get_param' reads a named material parameter (material_rid, parameter); 'shader_get_code' returns the shader source (shader_rid); 'shader_get_param_list' parses uniform declarations from the shader source and returns parameters with name, type, hint and global (shader_rid); 'mesh_surface_get_arrays' returns the vertex arrays of a mesh surface (mesh_rid, surface); 'texture_2d_get' reads a texture back as PNG (texture_rid); 'global_shader_parameter_get' reads a global parameter by name; 'global_shader_parameter_list' lists every global parameter; 'global_shader_parameter_remove' removes one by name. Example: {\"action\": \"texture_2d_get\", \"texture_rid\": 14} or {\"action\": \"shader_get_param_list\", \"shader_rid\": 8}.",
      "Render", {"render", "inspect", "material", "shader", "mesh", "texture", "readback", "global_shader_parameter"},
      SideEffect::None, tool_flags::kCaptureImage,
      kRenderInspectParams, render_ops::handle_inspect_action,
      schema::build_action_schema(
          kRenderInspectActions, kRenderInspectParams,
          "material_get_param: read a material parameter; shader_get_code: read shader source; shader_get_param_list: list shader uniforms; mesh_surface_get_arrays: read mesh surface arrays; texture_2d_get: read a texture back as PNG; global_shader_parameter_get/list/remove: manage global shader parameters"),
      {},
      {"render_inspect action=material_get_param material_rid=3 parameter=albedo_color",
       "render_inspect action=global_shader_parameter_list",
       "render_inspect action=texture_2d_get texture_rid=14"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "free_render_rid",
      "Free any RenderingServer RID and invalidate it in the render RID store (Destructive, requires confirm=true). Pass rid from any render create action; the call runs RenderingServer::free_rid, erases the RID from the render store and drops stored environment params. Example: {\"rid\": 123, \"confirm\": true}.",
      "Render", {"render", "free", "rid", "release"},
      SideEffect::Destructive, tool_flags::kNone,
      kFreeRenderRidParams, render_ops::handle_free_rid,
      {},
      {},
      {"free_render_rid rid=123 confirm=true"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_scenario",
      "Create a server-side 3D scenario and return its RID as result.rid. A scenario hosts 3D instances, cameras, lights and fog; attach an environment with set_render_scenario_environment and instance meshes with render_instance action=create. Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "scenario", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderScenarioParams, render_ops::handle_scenario_create,
      {},
      {},
      {"create_render_scenario"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_scenario_environment",
      "Attach an environment to a 3D scenario to drive background, ambient light, fog and post-processing. Pass the scenario RID from create_render_scenario and an environment RID from render_environment action=create (external RIDs are also accepted). Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "scenario", "environment"}, SideEffect::None, tool_flags::kNone,
      kSetRenderScenarioEnvironmentParams, render_ops::handle_scenario_set_environment,
      {},
      {},
      {"set_render_scenario_environment scenario_rid=3 environment_rid=7"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_render_canvas_item_rid",
      "Get the canvas item RID of a CanvasItem node in the edited scene, the reverse of find_render_node_from_rid. Pass a node path (leading slash and root/ prefix are tolerated). Returns result.rid for use with render_canvas_item actions. Errors if the node is missing or is not a CanvasItem.",
      "Render", {"render", "rid", "bridge"}, SideEffect::None, tool_flags::kNone,
      kGetRenderCanvasItemRidParams, render_ops::handle_canvas_item_get_rid,
      {},
      {},
      {"get_render_canvas_item_rid path=/root/Node2D/Sprite"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "find_render_node_from_rid",
      "Check whether an RID is valid and find edited-scene nodes that use it, mainly for debugging render RIDs. Returns rid, is_valid and, when matching nodes exist, a nodes array with node_path and type entries. It is the reverse of get_render_canvas_item_rid. Returns 'ok' even for invalid RIDs.",
      "Render", {"utility", "debug", "render"}, SideEffect::None, tool_flags::kNone,
      kFindRenderNodeFromRidParams, render_ops::handle_resolve_rid,
      {},
      {},
      {"find_render_node_from_rid rid=123"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_material",
      "Create a server-side material and return its RID as result.rid. Set parameters with render_material action=set_param and assign via set_render_mesh_surface_material. Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "material", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderMaterialParams, render_ops::handle_material_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_shader",
      "Create a server-side shader and return its RID as result.rid. Optional code sets the source immediately; set_render_shader_code replaces it later. Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "shader", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderShaderParams, render_ops::handle_shader_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_sky",
      "Create a server-side sky and return its RID as result.rid. Assign a material with set_render_sky_material. Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "sky"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderSkyParams, render_ops::handle_sky_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_reflection_probe",
      "Create a server-side reflection probe and return its RID as result.rid. Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "reflection_probe"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderReflectionProbeParams, render_ops::handle_reflection_probe_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_decal",
      "Create a server-side decal and return its RID as result.rid. Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "decal"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderDecalParams, render_ops::handle_decal_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "restart_particles",
      "Restart a particle node in the edited scene from the beginning, even while emitting. Pass the node path of a GPUParticles2D, GPUParticles3D, CPUParticles2D or CPUParticles3D node. Returns 'ok'. Errors if the node is missing or is not a particle node.",
      "Render", {"render", "particles", "restart"}, SideEffect::WritesScene, tool_flags::kMutating,
      kRestartParticlesParams, render_ops::handle_node_particles_restart}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_particles_params",
      "Set playback parameters on a particle node in the edited scene. Optional fields: emitting, amount, lifetime, one_shot, speed_scale and explosiveness; only supplied fields are applied. Returns 'ok'. Errors if the node is missing, not a particle node, or a value has the wrong type.",
      "Render", {"render", "particles", "params"}, SideEffect::WritesScene, tool_flags::kMutating,
      kSetParticlesParamsParams, render_ops::handle_node_particles_set_params}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_particles_state",
      "Get the playback state of a particle node in the edited scene. Returns type plus emitting, amount, lifetime, one_shot, speed_scale and explosiveness in result. Read-only.",
      "Render", {"render", "particles", "state"}, SideEffect::None, tool_flags::kNone,
      kGetParticlesStateParams, render_ops::handle_node_particles_get_state}));
  return v;
}

} // namespace render_tools
} // namespace godot_autopilot

#endif
