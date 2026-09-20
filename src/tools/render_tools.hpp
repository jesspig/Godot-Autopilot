#ifndef GODOT_AUTOPILOT_RENDER_TOOLS_HPP
#define GODOT_AUTOPILOT_RENDER_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/render_ops.hpp"
#include <tools/tool_spec.hpp>

namespace godot_autopilot {
namespace render_tools {

namespace {

const std::vector<ParamSpec> kCreateRenderCanvasItemParams = {};

const std::vector<ParamSpec> kAddRenderCanvasItemRectParams = {
    {"canvas_item_rid", "integer", "RID of the canvas item, from create_render_canvas_item", true},
    {"rect", "object", "Rectangle as {position: {x, y}, size: {x, y}}; example: {position: {x: 10, y: 20}, size: {x: 100, y: 50}}", true},
    {"color", "object", "Fill color {r, g, b, a}, each in 0.0-1.0; example: {r: 1, g: 0.5, b: 0, a: 1}", true},
    {"antialiased", "boolean", "Antialias the rectangle edges (default: false)", false},
};

const std::vector<ParamSpec> kAddRenderCanvasItemCircleParams = {
    {"canvas_item_rid", "integer", "RID of the canvas item, from create_render_canvas_item", true},
    {"position", "object", "Circle center {x, y}; example: {x: 50, y: 50}", true},
    {"color", "object", "Fill color {r, g, b, a}, each in 0.0-1.0; example: {r: 1, g: 0, b: 0, a: 1}", true},
    {"radius", "number", "Circle radius in pixels (default: 1.0)", false},
    {"antialiased", "boolean", "Antialias the circle edges (default: false)", false},
};

const std::vector<ParamSpec> kAddRenderCanvasItemTextureRectParams = {
    {"canvas_item_rid", "integer", "RID of the canvas item, from create_render_canvas_item", true},
    {"texture_rid", "integer", "RID of the texture to draw, from create_render_texture_from_image", true},
    {"rect", "object", "Destination rectangle {position: {x, y}, size: {x, y}}; example: {position: {x: 0, y: 0}, size: {x: 100, y: 100}}", true},
    {"tile", "boolean", "Tile the texture inside the rect (default: false)", false},
    {"modulate", "object", "Tint color {r, g, b, a}, each in 0.0-1.0; white leaves the texture unchanged", false},
    {"transpose", "boolean", "Swap the texture's x and y axes (default: false)", false},
};

const std::vector<ParamSpec> kAddRenderCanvasItemLineParams = {
    {"canvas_item_rid", "integer", "RID of the canvas item, from create_render_canvas_item", true},
    {"from", "object", "Line start point {x, y}; example: {x: 0, y: 0}", true},
    {"to", "object", "Line end point {x, y}; example: {x: 100, y: 100}", true},
    {"color", "object", "Line color {r, g, b, a}, each in 0.0-1.0", true},
    {"width", "number", "Line width in pixels (default: -1.0, use project setting)", false},
    {"antialiased", "boolean", "Antialias the line (default: false)", false},
};

const std::vector<ParamSpec> kSetRenderCanvasItemTransformParams = {
    {"canvas_item_rid", "integer", "RID of the canvas item, from create_render_canvas_item", true},
    {"x", "number", "Local X axis scale (default: 1.0)", false},
    {"y", "number", "Local Y axis scale (default: 1.0)", false},
    {"origin_x", "number", "Origin X offset in pixels (default: 0.0)", false},
    {"origin_y", "number", "Origin Y offset in pixels (default: 0.0)", false},
};

const std::vector<ParamSpec> kSetRenderCanvasItemVisibleParams = {
    {"canvas_item_rid", "integer", "RID of the canvas item, from create_render_canvas_item", true},
    {"visible", "boolean", "Visibility state, true shows and false hides (default: true)", false},
};

const std::vector<ParamSpec> kGetRenderCanvasItemRidParams = {
    {"path", "string", "Node path of a CanvasItem node in the edited scene; example: '/root/Node2D/Sprite' (leading slash and root/ prefix are tolerated)", true},
};

const std::vector<ParamSpec> kCreateRenderScenarioParams = {};

const std::vector<ParamSpec> kSetRenderScenarioEnvironmentParams = {
    {"scenario_rid", "integer", "RID of the render scenario, from create_render_scenario", true},
    {"environment_rid", "integer", "RID of the environment to attach; must come from an external source (e.g. code_execute), as no create tool exists", true},
};

const std::vector<ParamSpec> kCreateRenderCameraParams = {};

const std::vector<ParamSpec> kSetRenderCameraTransformParams = {
    {"camera_rid", "integer", "RID of the render camera, from create_render_camera", true},
    {"origin_x", "number", "Camera origin X (default: 0.0); example: 1.5", false},
    {"origin_y", "number", "Camera origin Y (default: 0.0); example: 2.0", false},
    {"origin_z", "number", "Camera origin Z (default: 0.0); example: -10.0", false},
};

const std::vector<ParamSpec> kSetRenderCameraPerspectiveParams = {
    {"camera_rid", "integer", "RID of the render camera, from create_render_camera", true},
    {"fovy_degrees", "number", "Vertical field of view in degrees (default: 75); example: 60", false},
    {"z_near", "number", "Near clip distance, keep small but positive (default: 0.01)", false},
    {"z_far", "number", "Far clip distance, objects beyond it are culled (default: 4000)", false},
};

const std::vector<ParamSpec> kSetRenderCameraOrthogonalParams = {
    {"camera_rid", "integer", "RID of the render camera, from create_render_camera", true},
    {"size", "number", "Orthogonal viewport size in units (default: 10)", false},
    {"z_near", "number", "Near clip distance, keep small but positive (default: 0.01)", false},
    {"z_far", "number", "Far clip distance, objects beyond it are culled (default: 4000)", false},
};

const std::vector<ParamSpec> kCreateRenderLightParams = {
    {"type", "string", "Light type: directional (sun-like, parallel rays), omni (point light), or spot (cone light); default: directional", false},
};

const std::vector<ParamSpec> kSetRenderLightParamParams = {
    {"light_rid", "integer", "RID of the render light, from create_render_light", true},
    {"param", "number", "Light parameter index: 0=energy, 1=specular, 2=range, 3=size, 4=attenuation", true},
    {"value", "number", "Parameter value; units depend on param, e.g. range is in meters", true},
};

const std::vector<ParamSpec> kSetRenderLightColorParams = {
    {"light_rid", "integer", "RID of the render light, from create_render_light", true},
    {"color", "object", "Light color {r, g, b, a}, each in 0.0-1.0; alpha is ignored, example: {r: 1, g: 0.9, b: 0.7, a: 1}", true},
};

const std::vector<ParamSpec> kCreateRenderMeshParams = {};

const std::vector<ParamSpec> kAddRenderMeshSurfaceParams = {
    {"mesh_rid", "integer", "RID of the render mesh, from create_render_mesh", true},
    {"primitive", "integer", "Primitive type: 0=points, 1=lines, 2=line_strip, 3=triangle_strip, 4=triangle_fan, 5=triangles (default: 5)", false},
    {"arrays", "object", "Surface arrays; keys: vertices and normals (arrays of {x, y, z} objects), tangents (flat array of floats, 4 per vertex), colors (array of {r, g, b, a} objects), uvs (array of {x, y} objects) and indices (array of integers)", false},
};

const std::vector<ParamSpec> kSetRenderMeshSurfaceMaterialParams = {
    {"mesh_rid", "integer", "RID of the render mesh, from create_render_mesh", true},
    {"surface", "integer", "Surface index, 0-based in add_render_mesh_surface call order; example: 0 for the first surface", true},
    {"material_rid", "integer", "RID of the material to assign, from create_render_material", true},
};

const std::vector<ParamSpec> kCreateRenderMaterialParams = {};

const std::vector<ParamSpec> kSetRenderMaterialParamParams = {
    {"material_rid", "integer", "RID of the render material, from create_render_material", true},
    {"parameter", "string", "Material parameter name; example: 'albedo_color'", true},
    {"value", "object", "Parameter value as JSON; plain numbers/strings/bools also work", true},
    {"type_hint", "string", "Type hint (e.g. Vector2, Color, int, float)", false},
};

const std::vector<ParamSpec> kCreateRenderViewportParams = {};

const std::vector<ParamSpec> kSetRenderViewportSizeParams = {
    {"viewport_rid", "integer", "RID of the render viewport, from create_render_viewport", true},
    {"width", "integer", "Viewport width in pixels (default: 640)", false},
    {"height", "integer", "Viewport height in pixels (default: 480)", false},
};

const std::vector<ParamSpec> kSetRenderViewportClearModeParams = {
    {"viewport_rid", "integer", "RID of the render viewport, from create_render_viewport", true},
    {"clear_mode", "number", "Clear mode: 0=always, 1=never, 2=only_next_frame (default: 0)", false},
};

const std::vector<ParamSpec> kCreateRenderParticlesParams = {
    {"mode", "integer", "Particle mode: 0=2D, 1=3D (default: 1)", false},
};

const std::vector<ParamSpec> kSetRenderEnvironmentBgColorParams = {
    {"environment_rid", "integer", "RID of the render environment; must come from an external source (e.g. code_execute), as no create tool exists", true},
    {"color", "object", "Background color {r, g, b, a}, each in 0.0-1.0; example: {r: 0.1, g: 0.2, b: 0.4, a: 1}", true},
};

const std::vector<ParamSpec> kSetRenderEnvironmentAmbientLightParams = {
    {"environment_rid", "integer", "RID of the render environment; must come from an external source (e.g. code_execute), as no create tool exists", true},
    {"color", "object", "Ambient light color {r, g, b, a}, each in 0.0-1.0", true},
    {"source", "number", "Ambient source: 0=bg, 1=disabled, 2=color, 3=sky (default: 0)", false},
    {"energy", "number", "Ambient light energy, scales brightness (default: 1.0)", false},
};

const std::vector<ParamSpec> kCreateRenderFogVolumeParams = {
    {"shape", "number", "Fog volume shape: 0=ellipsoid, 1=cone, 2=cylinder, 3=box, 4=world (default: 0); can be changed later with set_render_fog_volume_shape", false},
    {"size", "object", "Fog volume size {x, y, z}; example: {x: 4, y: 2, z: 4}", false},
};

const std::vector<ParamSpec> kCreateRenderShaderParams = {
    {"code", "string", "Shader source code (optional); can be replaced later with set_render_shader_code", false},
};

const std::vector<ParamSpec> kCreateRenderTextureFromImageParams = {
    {"image_path", "string", "Image file path; accepts res://, user:// or absolute paths (relative paths are not supported); example: 'res://icon.svg'", true},
};

const std::vector<ParamSpec> kSetRenderShaderCodeParams = {
    {"shader_rid", "integer", "RID of the shader, from create_render_shader", true},
    {"code", "string", "Shader source code; must be a full shader file, not a snippet", true},
};

const std::vector<ParamSpec> kSetRenderEnvironmentGlowParams = {
    {"environment_rid", "integer", "RID of the render environment; must come from an external source (e.g. code_execute), as no create tool exists", true},
    {"enabled", "boolean", "Enable glow (default: true)", false},
    {"level", "number", "Glow level strength (default: 0.85)", false},
    {"intensity", "number", "Glow intensity (default: 0.8)", false},
    {"strength", "number", "Glow strength (default: 1.0)", false},
    {"mix", "number", "Glow mix with scene (default: 0.05)", false},
    {"bloom_threshold", "number", "Bloom threshold (default: 0.0)", false},
    {"blend_mode", "integer", "Glow blend mode: 0=additive, 1=screen, 2=softlight, 3=replace, 4=mix", false},
    {"hdr_bleed_threshold", "number", "HDR bleed threshold (default: 0.5)", false},
    {"hdr_bleed_scale", "number", "HDR bleed scale (default: 2.0)", false},
    {"hdr_luminance_cap", "number", "HDR luminance cap (default: 2.0)", false},
    {"glow_map_strength", "number", "Glow map strength (default: 1.0)", false},
};

const std::vector<ParamSpec> kSetRenderEnvironmentSsrParams = {
    {"environment_rid", "integer", "RID of the render environment; must come from an external source (e.g. code_execute), as no create tool exists", true},
    {"enabled", "boolean", "Enable screen-space reflections (default: true)", false},
    {"max_steps", "integer", "Maximum reflection steps (default: 64)", false},
    {"fade_in", "number", "Fade-in distance (default: 0.1)", false},
    {"fade_out", "number", "Fade-out distance (default: 0.1)", false},
    {"depth_tolerance", "number", "Depth tolerance (default: 0.1)", false},
};

const std::vector<ParamSpec> kSetRenderEnvironmentTonemapParams = {
    {"environment_rid", "integer", "RID of the render environment; must come from an external source (e.g. code_execute), as no create tool exists", true},
    {"tone_mapper", "integer", "Tone mapper: 0=linear, 1=reinhard, 2=filmic, 3=aces (default: 0)", false},
    {"exposure", "number", "Exposure, higher values brighten the image (default: 1.0)", false},
    {"white", "number", "White reference in ev; higher values darken highlights (default: 1.0)", false},
};

const std::vector<ParamSpec> kSetRenderEnvironmentSdfgiParams = {
    {"environment_rid", "integer", "RID of the render environment; must come from an external source (e.g. code_execute), as no create tool exists", true},
    {"enabled", "boolean", "Enable SDFGI (default: true)", false},
    {"cascades", "integer", "Number of cascades, 1-8 (default: 4)", false},
    {"min_cell_size", "number", "Minimum cell size in meters (default: 0.1)", false},
    {"y_scale", "integer", "Y scale mode: 0=50%, 1=25%, 2=12.5% (default: 0)", false},
    {"use_occlusion", "boolean", "Use occlusion culling (default: true)", false},
    {"bounce_feedback", "number", "Bounce feedback (default: 0.5)", false},
    {"read_sky", "boolean", "Read sky for lighting (default: true)", false},
    {"energy", "number", "GI energy (default: 1.0)", false},
    {"normal_bias", "number", "Normal bias (default: 1.0)", false},
    {"probe_bias", "number", "Probe bias (default: 1.0)", false},
};

const std::vector<ParamSpec> kSetRenderEnvironmentVolumetricFogParams = {
    {"environment_rid", "integer", "RID of the render environment; must come from an external source (e.g. code_execute), as no create tool exists", true},
    {"enabled", "boolean", "Enable volumetric fog (default: true)", false},
    {"density", "number", "Fog density, higher makes fog thicker (default: 0.05)", false},
    {"albedo", "object", "Fog albedo color {r, g, b, a}, each in 0.0-1.0", false},
    {"emission", "object", "Fog emission color {r, g, b, a}, each in 0.0-1.0", false},
    {"emission_energy", "number", "Emission energy (default: 1.0)", false},
    {"anisotropy", "number", "Light anisotropy, 0.0 scatters evenly (default: 0.0)", false},
    {"length", "number", "Fog length, controls spatial scale (default: 0.0)", false},
    {"detail_spread", "number", "Detail spread (default: 0.0)", false},
    {"gi_inject", "number", "GI injection (default: 0.0)", false},
    {"temporal_reprojection", "boolean", "Temporal reprojection smooths flickering (default: false)", false},
    {"temporal_reprojection_amount", "number", "Temporal reprojection amount (default: 0.5)", false},
    {"ambient_inject", "number", "Ambient injection (default: 0.0)", false},
    {"sky_affect", "number", "Sky affect (default: 0.0)", false},
};

const std::vector<ParamSpec> kCreateRenderSkyParams = {};

const std::vector<ParamSpec> kSetRenderSkyMaterialParams = {
    {"sky_rid", "integer", "RID of the sky, from create_render_sky", true},
    {"material_rid", "integer", "RID of the material to assign, from create_render_material", true},
};

const std::vector<ParamSpec> kSetRenderParticlesEmittingParams = {
    {"particles_rid", "integer", "RID of the particle system, from create_render_particles", true},
    {"emitting", "boolean", "Emit particles, true starts and false stops emission (default: true)", false},
};

const std::vector<ParamSpec> kRestartRenderParticlesParams = {
    {"particles_rid", "integer", "RID of the particle system, from create_render_particles", true},
};

const std::vector<ParamSpec> kSetRenderParticlesLifetimeParams = {
    {"particles_rid", "integer", "RID of the particle system, from create_render_particles", true},
    {"lifetime", "number", "Particle lifetime in seconds; controls how long each particle stays alive (default: 1.0)", false},
};

const std::vector<ParamSpec> kCreateRenderReflectionProbeParams = {};

const std::vector<ParamSpec> kCreateRenderDecalParams = {};

const std::vector<ParamSpec> kSetRenderFogVolumeShapeParams = {
    {"fog_rid", "integer", "RID of the fog volume, from create_render_fog_volume", true},
    {"shape", "integer", "Fog volume shape: 0=ellipsoid, 1=cone, 2=cylinder, 3=box, 4=world", true},
};

const std::vector<ParamSpec> kSetRenderInstanceVisibleParams = {
    {"instance_rid", "integer", "RID of the render instance; must come from an external source (e.g. code_execute), as no create tool exists", true},
    {"visible", "boolean", "Visibility state, true shows and false hides (default: true)", false},
};

const std::vector<ParamSpec> kSetRenderInstanceLayerMaskParams = {
    {"instance_rid", "integer", "RID of the render instance; must come from an external source (e.g. code_execute), as no create tool exists", true},
    {"mask", "integer", "Layer mask bitfield; bit n selects layer n+1, example: 1 for layer 1, 3 for layers 1 and 2", true},
};

const std::vector<ParamSpec> kSetRenderShaderParameterGlobalParams = {
    {"name", "string", "Global shader parameter name; the shader must declare a matching parameter with the global keyword", true},
    {"value", "object", "Parameter value as JSON; plain numbers/strings/bools also work", true},
    {"type_hint", "string", "Type hint (e.g. Vector2, Color, int, float)", false},
};

const std::vector<ParamSpec> kFindRenderNodeFromRidParams = {
    {"rid", "integer", "RID to resolve, e.g. from get_render_canvas_item_rid; returns is_valid and matching nodes for debugging", true},
};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(49);
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_canvas_item",
      "Create a server-side 2D canvas item and return its RID as result.rid. Use this to build custom 2D drawing independent of scene nodes, then draw shapes with add_render_canvas_item_rect, add_render_canvas_item_circle, add_render_canvas_item_texture_rect or add_render_canvas_item_line and control visibility with set_render_canvas_item_visible. Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "canvas", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderCanvasItemParams, render_ops::handle_canvas_item_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_render_canvas_item_rect",
      "Add a filled rectangle draw command to a canvas item. Pass the RID from create_render_canvas_item, a rect object {position: {x, y}, size: {x, y}} and a color object {r, g, b, a}; antialiased is optional and defaults to false. Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "canvas", "draw", "rect"}, SideEffect::None, tool_flags::kNone,
      kAddRenderCanvasItemRectParams, render_ops::handle_canvas_item_draw_rect}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_render_canvas_item_circle",
      "Add a filled circle draw command to a canvas item. Pass the RID from create_render_canvas_item, a center position {x, y} and a color {r, g, b, a}; radius defaults to 1.0 and antialiased to false. Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "canvas", "draw", "circle"}, SideEffect::None, tool_flags::kNone,
      kAddRenderCanvasItemCircleParams, render_ops::handle_canvas_item_draw_circle}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_render_canvas_item_texture_rect",
      "Draw a texture inside a rectangle on a canvas item. Pass the RID from create_render_canvas_item, the texture RID from create_render_texture_from_image and a destination rect; tile, modulate and transpose are optional. Modulate tints the drawn texture. Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "canvas", "draw", "texture"}, SideEffect::None, tool_flags::kNone,
      kAddRenderCanvasItemTextureRectParams, render_ops::handle_canvas_item_draw_texture}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_render_canvas_item_line",
      "Add a line draw command to a canvas item. Pass the RID from create_render_canvas_item, start point from, end point to and a color; width defaults to -1 (project setting) and antialiased to false. Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "canvas", "draw", "line"}, SideEffect::None, tool_flags::kNone,
      kAddRenderCanvasItemLineParams, render_ops::handle_canvas_item_draw_line}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_canvas_item_transform",
      "Set the 2D transform of a canvas item: x and y scale the local axes, origin_x and origin_y offset the position. Pass the RID from create_render_canvas_item. All parameters are optional and default to identity. Returns 'ok'. Errors if canvas_item_rid is missing.",
      "Render", {"render", "canvas", "transform"}, SideEffect::None, tool_flags::kNone,
      kSetRenderCanvasItemTransformParams, render_ops::handle_canvas_item_set_transform}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_canvas_item_visible",
      "Set the visibility state of a canvas item, not a toggle. Pass the RID from create_render_canvas_item; visible defaults to true. Hide the item to remove its draw commands from the canvas temporarily; set it back to true to restore drawing. Returns 'ok'. Errors if canvas_item_rid is missing.",
      "Render", {"render", "canvas", "visible"}, SideEffect::None, tool_flags::kNone,
      kSetRenderCanvasItemVisibleParams, render_ops::handle_canvas_item_set_visible}));
  v.push_back(make_spec_tool(ToolSpec{
      "get_render_canvas_item_rid",
      "Get the canvas item RID of a CanvasItem node in the edited scene, the reverse of find_render_node_from_rid. Pass a node path (leading slash and root/ prefix are tolerated). Returns result.rid for use with the add_render_canvas_item_* draw tools. Errors if the node is missing or is not a CanvasItem.",
      "Render", {"render", "rid", "bridge"}, SideEffect::None, tool_flags::kNone,
      kGetRenderCanvasItemRidParams, render_ops::handle_canvas_item_get_rid}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_scenario",
      "Create a server-side 3D scenario and return its RID as result.rid. A scenario hosts 3D cameras, lights, meshes and fog; use it to assemble a custom render scene outside the node tree. Pass the RID to set_render_scenario_environment to attach an environment. Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "scenario", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderScenarioParams, render_ops::handle_scenario_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_scenario_environment",
      "Attach an environment to a 3D scenario to drive background, ambient light, fog and post-processing. Pass the scenario RID from create_render_scenario; the environment RID must come from an external source (e.g. code_execute), as no create tool exists. Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "scenario", "environment"}, SideEffect::None, tool_flags::kNone,
      kSetRenderScenarioEnvironmentParams, render_ops::handle_scenario_set_environment}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_camera",
      "Create a server-side 3D camera and return its RID as result.rid. Configure its projection with set_render_camera_perspective or set_render_camera_orthogonal and place it with set_render_camera_transform. Cameras work with a scenario RID from create_render_scenario to define the viewpoint of a custom render scene. Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "camera", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderCameraParams, render_ops::handle_camera_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_camera_transform",
      "Set the 3D camera position. Pass the RID from create_render_camera; only the origin is applied, rotation and scale stay at identity, so the camera always looks down -Z. Combine with set_render_camera_perspective or set_render_camera_orthogonal to define the view frustum. Returns 'ok'. Errors if camera_rid is missing.",
      "Render", {"render", "camera", "transform"}, SideEffect::None, tool_flags::kNone,
      kSetRenderCameraTransformParams, render_ops::handle_camera_set_transform}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_camera_perspective",
      "Set a perspective (frustum) projection on a camera. Pass the RID from create_render_camera; fovy_degrees defaults to 75, z_near to 0.01 and z_far to 4000. Use it for natural 3D scenes where objects shrink with distance. Returns 'ok'. Errors if camera_rid is missing.",
      "Render", {"render", "camera", "perspective"}, SideEffect::None, tool_flags::kNone,
      kSetRenderCameraPerspectiveParams, render_ops::handle_camera_set_perspective}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_camera_orthogonal",
      "Set an orthogonal projection on a camera, typically for 2D-like or UI rendering. Pass the RID from create_render_camera; size defaults to 10, z_near to 0.01 and z_far to 4000. Objects keep their size regardless of distance. Returns 'ok'. Errors if camera_rid is missing.",
      "Render", {"render", "camera", "orthogonal"}, SideEffect::None, tool_flags::kNone,
      kSetRenderCameraOrthogonalParams, render_ops::handle_camera_set_orthogonal}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_light",
      "Create a server-side light and return its RID as result.rid. type selects directional, omni or spot and defaults to directional. Pair with set_render_light_param and set_render_light_color. Use it to light custom 3D scenes assembled from server-side RIDs. Returns an error on unknown light types or if the RenderingServer is unavailable.",
      "Render", {"render", "light", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderLightParams, render_ops::handle_light_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_light_param",
      "Set a numeric parameter on a light. Pass the RID from create_render_light, a param index (0=energy, 1=specular, 2=range, 3=size, 4=attenuation) and a float value. Which parameters apply depends on the light type, e.g. range only affects omni and spot lights. Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "light", "param"}, SideEffect::None, tool_flags::kNone,
      kSetRenderLightParamParams, render_ops::handle_light_set_param}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_light_color",
      "Set the color of a light. Pass the RID from create_render_light and a color {r, g, b, a}; alpha is ignored. Combine with set_render_light_param to control energy and range. Lights tint the surfaces they illuminate. Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "light", "color"}, SideEffect::None, tool_flags::kNone,
      kSetRenderLightColorParams, render_ops::handle_light_set_color}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_mesh",
      "Create a server-side mesh to hold geometry, returning its RID as result.rid. A mesh owns one or more surfaces built with add_render_mesh_surface; assign per-surface materials with set_render_mesh_surface_material. A mesh alone is not visible; it needs to be instanced into a scenario. Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "mesh", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderMeshParams, render_ops::handle_mesh_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "add_render_mesh_surface",
      "Add a surface built from vertex arrays to a mesh. Pass the RID from create_render_mesh; primitive defaults to triangles and arrays may hold vertices, normals, tangents, colors, uvs and indices. Surfaces are indexed from 0 in call order. Returns 'ok'. Errors if mesh_rid is missing.",
      "Render", {"render", "mesh", "surface"}, SideEffect::None, tool_flags::kNone,
      kAddRenderMeshSurfaceParams, render_ops::handle_mesh_add_surface}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_mesh_surface_material",
      "Assign a material to one surface of a mesh. Pass the RID from create_render_mesh, a 0-based surface index (in add_render_mesh_surface call order) and a material RID from create_render_material. Each surface can have its own material. Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "mesh", "material"}, SideEffect::None, tool_flags::kNone,
      kSetRenderMeshSurfaceMaterialParams, render_ops::handle_mesh_set_material}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_material",
      "Create a server-side material and return its RID as result.rid. Materials feed shader parameters into surfaces; set them with set_render_material_param and assign via set_render_mesh_surface_material. Note that no tool attaches a shader to a material, so the material only takes effect with external shader setup (e.g. code_execute). Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "material", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderMaterialParams, render_ops::handle_material_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_material_param",
      "Set a named parameter on a material. Pass the RID from create_render_material, the parameter name and a JSON value; type_hint controls Variant deserialization (e.g. Vector2, Color, int, float). The value must match the shader's declared parameter type. Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "material", "param"}, SideEffect::None, tool_flags::kNone,
      kSetRenderMaterialParamParams, render_ops::handle_material_set_param}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_viewport",
      "Create a server-side viewport and return its RID as result.rid. Configure it with set_render_viewport_size and set_render_viewport_clear_mode. Note that no tool can attach a camera or canvas to a viewport, so standalone viewports cannot be rendered; the RID is useful mainly for external composition. Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "viewport", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderViewportParams, render_ops::handle_viewport_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_viewport_size",
      "Set the pixel size of a viewport. Pass the RID from create_render_viewport; width defaults to 640 and height to 480. It determines the resolution of the viewport's render target. Change it at runtime to resize the target. Returns 'ok'. Errors if viewport_rid is missing.",
      "Render", {"render", "viewport", "size"}, SideEffect::None, tool_flags::kNone,
      kSetRenderViewportSizeParams, render_ops::handle_viewport_set_size}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_viewport_clear_mode",
      "Set how a viewport clears its background each frame. Pass the RID from create_render_viewport; clear_mode is 0=always, 1=never, 2=only_next_frame and defaults to 0. Never clearing keeps the previous frame, useful for persistent drawing. Returns 'ok'. Errors if viewport_rid is missing.",
      "Render", {"render", "viewport", "clear"}, SideEffect::None, tool_flags::kNone,
      kSetRenderViewportClearModeParams, render_ops::handle_viewport_set_clear_mode}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_particles",
      "Create a server-side particle system and return its RID as result.rid. mode selects 2D (0) or 3D (1) and defaults to 3D. Pair with set_render_particles_emitting, set_render_particles_lifetime and restart_render_particles. Note that no tool instantiates the system into a scenario, so visible emission requires external setup (e.g. code_execute). Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "particle", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderParticlesParams, render_ops::handle_particle_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_environment_bg_color",
      "Set the background color of an environment. Pass the environment RID from an external source (e.g. code_execute); no create tool exists. Requires a color {r, g, b, a}. It takes effect when the environment background mode is set to color. Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "environment", "bg"}, SideEffect::None, tool_flags::kNone,
      kSetRenderEnvironmentBgColorParams, render_ops::handle_environment_set_bg_color}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_environment_ambient_light",
      "Set the ambient light of an environment. Pass the environment RID from an external source (e.g. code_execute); source selects 0=bg, 1=disabled, 2=color, 3=sky and energy scales brightness. Ambient light fills shadows so unlit surfaces stay visible. Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "environment", "ambient"}, SideEffect::None, tool_flags::kNone,
      kSetRenderEnvironmentAmbientLightParams, render_ops::handle_environment_set_ambient}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_fog_volume",
      "Create a server-side volumetric fog volume and return its RID as result.rid. shape and size may be set at creation, or later with set_render_fog_volume_shape. Fog volumes add localized volumetric fog in 3D scenes when combined with an environment that has volumetric fog enabled. Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "fog", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderFogVolumeParams, render_ops::handle_fog_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_shader",
      "Create a server-side shader and return its RID as result.rid. Optional code sets the shader source immediately; set_render_shader_code replaces it later. Note that no tool can attach a shader to a material, so the shader RID is mainly useful for testing. Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "shader", "create"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderShaderParams, render_ops::handle_shader_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_texture_from_image",
      "Create a server-side 2D texture from an image file and return its RID as result.rid. image_path accepts res://, user:// or absolute paths; relative paths are not supported. Pass the RID to add_render_canvas_item_texture_rect. Returns an error if the image fails to load.",
      "Render", {"render", "texture"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderTextureFromImageParams, render_ops::handle_texture_create_2d}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_shader_code",
      "Set the source code of a shader. Pass the RID from create_render_shader and the shader code as a string. It replaces any code set at creation time; compilation happens on the server side. Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "shader"}, SideEffect::None, tool_flags::kNone,
      kSetRenderShaderCodeParams, render_ops::handle_shader_set_code}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_environment_glow",
      "Configure glow (bloom) post-processing on an environment. Pass the environment RID from an external source (e.g. code_execute); all parameters are optional, see schema for meanings and defaults. Glow makes bright areas bleed into their surroundings. Returns 'ok'. Errors if environment_rid is missing.",
      "Render", {"render", "environment"}, SideEffect::None, tool_flags::kNone,
      kSetRenderEnvironmentGlowParams, render_ops::handle_environment_set_glow}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_environment_ssr",
      "Configure screen-space reflections on an environment. Pass the environment RID from an external source (e.g. code_execute); all parameters are optional, see schema for meanings and defaults. SSR reflects visible geometry onto reflective surfaces without cubemaps. Returns 'ok'. Errors if environment_rid is missing.",
      "Render", {"render", "environment"}, SideEffect::None, tool_flags::kNone,
      kSetRenderEnvironmentSsrParams, render_ops::handle_environment_set_ssr}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_environment_tonemap",
      "Configure tone mapping and exposure on an environment. Pass the environment RID from an external source (e.g. code_execute); tone_mapper, exposure and white are optional, see schema for meanings. Tone mapping maps HDR scene colors to the display range. Returns 'ok'. Errors if environment_rid is missing.",
      "Render", {"render", "environment"}, SideEffect::None, tool_flags::kNone,
      kSetRenderEnvironmentTonemapParams, render_ops::handle_environment_set_tonemap}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_environment_sdfgi",
      "Configure SDFGI global illumination on an environment. Pass the environment RID from an external source (e.g. code_execute); all parameters are optional, see schema for meanings and defaults. SDFGI provides GI for large outdoor scenes. Returns 'ok'. Errors if environment_rid is missing.",
      "Render", {"render", "environment"}, SideEffect::None, tool_flags::kNone,
      kSetRenderEnvironmentSdfgiParams, render_ops::handle_environment_set_sdfgi}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_environment_volumetric_fog",
      "Configure volumetric fog on an environment. Pass the environment RID from an external source (e.g. code_execute); all parameters are optional, see schema for meanings and defaults. It fills the scene with light-scattering fog, e.g. mist or smoke. Returns 'ok'. Errors if environment_rid is missing.",
      "Render", {"render", "environment"}, SideEffect::None, tool_flags::kNone,
      kSetRenderEnvironmentVolumetricFogParams, render_ops::handle_environment_set_volumetric_fog}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_sky",
      "Create a server-side sky and return its RID as result.rid. Assign a material with set_render_sky_material. Note that no tool can attach a sky to an environment, so the sky RID is mainly useful for testing. Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "sky"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderSkyParams, render_ops::handle_sky_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_sky_material",
      "Set the material of a sky. Pass the sky RID from create_render_sky and a material RID from create_render_material. The material defines the sky appearance, e.g. a procedural sky or panorama texture. Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "sky"}, SideEffect::None, tool_flags::kNone,
      kSetRenderSkyMaterialParams, render_ops::handle_sky_set_material}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_particles_emitting",
      "Set whether a particle system is emitting. Pass the RID from create_render_particles; emitting defaults to true. To restart playback from the beginning, use restart_render_particles instead. Emitting alone does not restart a finished emission; combine with set_render_particles_lifetime to control duration. Returns 'ok'. Errors if particles_rid is missing.",
      "Render", {"render", "particles"}, SideEffect::None, tool_flags::kNone,
      kSetRenderParticlesEmittingParams, render_ops::handle_particles_set_emitting}));
  v.push_back(make_spec_tool(ToolSpec{
      "restart_render_particles",
      "Restart a particle system from the beginning, even if it is currently emitting. Pass the RID from create_render_particles. Unlike set_render_particles_emitting, this forces a fresh emission cycle. Use it to replay bursts, e.g. explosions or one-shot effects. Returns 'ok'. Errors if particles_rid is missing.",
      "Render", {"render", "particles"}, SideEffect::None, tool_flags::kNone,
      kRestartRenderParticlesParams, render_ops::handle_particles_restart}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_particles_lifetime",
      "Set the lifetime of a particle system in seconds, controlling how long each emitted particle stays alive. Pass the RID from create_render_particles; lifetime defaults to 1.0. Pair with set_render_particles_emitting and restart_render_particles to control playback. Returns 'ok'. Errors if particles_rid is missing.",
      "Render", {"render", "particles"}, SideEffect::None, tool_flags::kNone,
      kSetRenderParticlesLifetimeParams, render_ops::handle_particles_set_lifetime}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_reflection_probe",
      "Create a server-side reflection probe and return its RID as result.rid. A reflection probe captures surrounding geometry and lights to feed reflective materials. Use it to add local reflections to custom 3D scenes, though no tool instantiates the probe into a scenario. Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "reflection_probe"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderReflectionProbeParams, render_ops::handle_reflection_probe_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "create_render_decal",
      "Create a server-side decal and return its RID as result.rid. A decal projects a texture onto nearby surfaces, e.g. bullet holes or signs. Note that no tool instantiates the decal into a scenario, so its use is currently limited. Returns an error if the RenderingServer is unavailable.",
      "Render", {"render", "decal"}, SideEffect::None, tool_flags::kNone,
      kCreateRenderDecalParams, render_ops::handle_decal_create}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_fog_volume_shape",
      "Set the shape of a fog volume after creation. Pass the fog RID from create_render_fog_volume; the same shape can also be supplied at creation. Different shapes suit different fog layouts, e.g. ellipsoid for clouds. Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "fog"}, SideEffect::None, tool_flags::kNone,
      kSetRenderFogVolumeShapeParams, render_ops::handle_fog_volume_set_shape}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_instance_visible",
      "Set the visibility state of a rendering instance, not a toggle. The instance RID must come from an external source (e.g. code_execute), as no create tool exists; visible defaults to true. Use it to show or hide a server-side object in the rendered scene. Returns 'ok'. Errors if instance_rid is missing.",
      "Render", {"render", "instance"}, SideEffect::None, tool_flags::kNone,
      kSetRenderInstanceVisibleParams, render_ops::handle_instance_set_visible}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_instance_layer_mask",
      "Set the layer mask of a rendering instance, controlling which camera layers see it. The instance RID must come from an external source (e.g. code_execute), as no create tool exists; mask is a bitfield. Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "instance"}, SideEffect::None, tool_flags::kNone,
      kSetRenderInstanceLayerMaskParams, render_ops::handle_instance_set_layer_mask}));
  v.push_back(make_spec_tool(ToolSpec{
      "set_render_shader_parameter_global",
      "Set a global shader parameter available to all shaders. The shader must declare a parameter with the same name using the global keyword. type_hint controls Variant deserialization (e.g. Vector2, Color, int, float). Returns 'ok'. Errors if a required parameter is missing.",
      "Render", {"render", "shader"}, SideEffect::None, tool_flags::kNone,
      kSetRenderShaderParameterGlobalParams, render_ops::handle_global_shader_parameter_set}));
  v.push_back(make_spec_tool(ToolSpec{
      "find_render_node_from_rid",
      "Check whether an RID is valid and find edited-scene nodes that use it, mainly for debugging render RIDs. Returns rid, is_valid and, when matching nodes exist, a nodes array with node_path and type entries. It is the reverse of get_render_canvas_item_rid. Returns 'ok' even for invalid RIDs.",
      "Render", {"utility", "debug", "render"}, SideEffect::None, tool_flags::kNone,
      kFindRenderNodeFromRidParams, render_ops::handle_resolve_rid}));
  return v;
}

} // namespace render_tools
} // namespace godot_autopilot

#endif