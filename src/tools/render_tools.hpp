#ifndef GODOT_AUTOPILOT_RENDER_TOOLS_HPP
#define GODOT_AUTOPILOT_RENDER_TOOLS_HPP

#include <mcp/JsonValue.hpp>
#include <memory>
#include <string>
#include <vector>

#include "tools/render_ops.hpp"
#include "tools/tool_decl.hpp"

namespace godot_autopilot {
namespace render_tools {

GDA_TOOL_CLASS(CreateRenderCanvasItemTool, "create_render_canvas_item",
               "Create a server-side 2D canvas item and return its RID as result.rid. Use this to build custom 2D drawing independent of scene nodes, then draw shapes with add_render_canvas_item_rect, add_render_canvas_item_circle, add_render_canvas_item_texture_rect or add_render_canvas_item_line and control visibility with set_render_canvas_item_visible. Returns an error if the RenderingServer is unavailable.",
               "Render", std::vector<std::string>({"render", "canvas", "create"}), render_ops::handle_canvas_item_create, false)

GDA_TOOL_CLASS(AddRenderCanvasItemRectTool, "add_render_canvas_item_rect",
               "Add a filled rectangle draw command to a canvas item. Pass the RID from create_render_canvas_item, a rect object {position: {x, y}, size: {x, y}} and a color object {r, g, b, a}; antialiased is optional and defaults to false. Returns 'ok'. Errors if a required parameter is missing.",
               "Render", std::vector<std::string>({"render", "canvas", "draw", "rect"}), render_ops::handle_canvas_item_draw_rect, false)

GDA_TOOL_CLASS(AddRenderCanvasItemCircleTool, "add_render_canvas_item_circle",
               "Add a filled circle draw command to a canvas item. Pass the RID from create_render_canvas_item, a center position {x, y} and a color {r, g, b, a}; radius defaults to 1.0 and antialiased to false. Returns 'ok'. Errors if a required parameter is missing.",
               "Render", std::vector<std::string>({"render", "canvas", "draw", "circle"}), render_ops::handle_canvas_item_draw_circle, false)

GDA_TOOL_CLASS(AddRenderCanvasItemTextureRectTool, "add_render_canvas_item_texture_rect",
               "Draw a texture inside a rectangle on a canvas item. Pass the RID from create_render_canvas_item, the texture RID from create_render_texture_from_image and a destination rect; tile, modulate and transpose are optional. Modulate tints the drawn texture. Returns 'ok'. Errors if a required parameter is missing.",
               "Render", std::vector<std::string>({"render", "canvas", "draw", "texture"}), render_ops::handle_canvas_item_draw_texture, false)

GDA_TOOL_CLASS(AddRenderCanvasItemLineTool, "add_render_canvas_item_line",
               "Add a line draw command to a canvas item. Pass the RID from create_render_canvas_item, start point from, end point to and a color; width defaults to -1 (project setting) and antialiased to false. Returns 'ok'. Errors if a required parameter is missing.",
               "Render", std::vector<std::string>({"render", "canvas", "draw", "line"}), render_ops::handle_canvas_item_draw_line, false)

GDA_TOOL_CLASS(SetRenderCanvasItemTransformTool, "set_render_canvas_item_transform",
               "Set the 2D transform of a canvas item: x and y scale the local axes, origin_x and origin_y offset the position. Pass the RID from create_render_canvas_item. All parameters are optional and default to identity. Returns 'ok'. Errors if canvas_item_rid is missing.",
               "Render", std::vector<std::string>({"render", "canvas", "transform"}), render_ops::handle_canvas_item_set_transform, false)

GDA_TOOL_CLASS(SetRenderCanvasItemVisibleTool, "set_render_canvas_item_visible",
               "Set the visibility state of a canvas item, not a toggle. Pass the RID from create_render_canvas_item; visible defaults to true. Hide the item to remove its draw commands from the canvas temporarily; set it back to true to restore drawing. Returns 'ok'. Errors if canvas_item_rid is missing.",
               "Render", std::vector<std::string>({"render", "canvas", "visible"}), render_ops::handle_canvas_item_set_visible, false)

GDA_TOOL_CLASS(GetRenderCanvasItemRidTool, "get_render_canvas_item_rid",
               "Get the canvas item RID of a CanvasItem node in the edited scene, the reverse of find_render_node_from_rid. Pass a node path (leading slash and root/ prefix are tolerated). Returns result.rid for use with the add_render_canvas_item_* draw tools. Errors if the node is missing or is not a CanvasItem.",
               "Render", std::vector<std::string>({"render", "rid", "bridge"}), render_ops::handle_canvas_item_get_rid, false)

GDA_TOOL_CLASS(CreateRenderScenarioTool, "create_render_scenario",
               "Create a server-side 3D scenario and return its RID as result.rid. A scenario hosts 3D cameras, lights, meshes and fog; use it to assemble a custom render scene outside the node tree. Pass the RID to set_render_scenario_environment to attach an environment. Returns an error if the RenderingServer is unavailable.",
               "Render", std::vector<std::string>({"render", "scenario", "create"}), render_ops::handle_scenario_create, false)

GDA_TOOL_CLASS(SetRenderScenarioEnvironmentTool, "set_render_scenario_environment",
               "Attach an environment to a 3D scenario to drive background, ambient light, fog and post-processing. Pass the scenario RID from create_render_scenario; the environment RID must come from an external source (e.g. code_execute), as no create tool exists. Returns 'ok'. Errors if a required parameter is missing.",
               "Render", std::vector<std::string>({"render", "scenario", "environment"}), render_ops::handle_scenario_set_environment, false)

GDA_TOOL_CLASS(CreateRenderCameraTool, "create_render_camera",
               "Create a server-side 3D camera and return its RID as result.rid. Configure its projection with set_render_camera_perspective or set_render_camera_orthogonal and place it with set_render_camera_transform. Cameras work with a scenario RID from create_render_scenario to define the viewpoint of a custom render scene. Returns an error if the RenderingServer is unavailable.",
               "Render", std::vector<std::string>({"render", "camera", "create"}), render_ops::handle_camera_create, false)

GDA_TOOL_CLASS(SetRenderCameraTransformTool, "set_render_camera_transform",
               "Set the 3D camera position. Pass the RID from create_render_camera; only the origin is applied, rotation and scale stay at identity, so the camera always looks down -Z. Combine with set_render_camera_perspective or set_render_camera_orthogonal to define the view frustum. Returns 'ok'. Errors if camera_rid is missing.",
               "Render", std::vector<std::string>({"render", "camera", "transform"}), render_ops::handle_camera_set_transform, false)

GDA_TOOL_CLASS(SetRenderCameraPerspectiveTool, "set_render_camera_perspective",
               "Set a perspective (frustum) projection on a camera. Pass the RID from create_render_camera; fovy_degrees defaults to 75, z_near to 0.01 and z_far to 4000. Use it for natural 3D scenes where objects shrink with distance. Returns 'ok'. Errors if camera_rid is missing.",
               "Render", std::vector<std::string>({"render", "camera", "perspective"}), render_ops::handle_camera_set_perspective, false)

GDA_TOOL_CLASS(SetRenderCameraOrthogonalTool, "set_render_camera_orthogonal",
               "Set an orthogonal projection on a camera, typically for 2D-like or UI rendering. Pass the RID from create_render_camera; size defaults to 10, z_near to 0.01 and z_far to 4000. Objects keep their size regardless of distance. Returns 'ok'. Errors if camera_rid is missing.",
               "Render", std::vector<std::string>({"render", "camera", "orthogonal"}), render_ops::handle_camera_set_orthogonal, false)

GDA_TOOL_CLASS(CreateRenderLightTool, "create_render_light",
               "Create a server-side light and return its RID as result.rid. type selects directional, omni or spot and defaults to directional. Pair with set_render_light_param and set_render_light_color. Use it to light custom 3D scenes assembled from server-side RIDs. Returns an error on unknown light types or if the RenderingServer is unavailable.",
               "Render", std::vector<std::string>({"render", "light", "create"}), render_ops::handle_light_create, false)

GDA_TOOL_CLASS(SetRenderLightParamTool, "set_render_light_param",
               "Set a numeric parameter on a light. Pass the RID from create_render_light, a param index (0=energy, 1=specular, 2=range, 3=size, 4=attenuation) and a float value. Which parameters apply depends on the light type, e.g. range only affects omni and spot lights. Returns 'ok'. Errors if a required parameter is missing.",
               "Render", std::vector<std::string>({"render", "light", "param"}), render_ops::handle_light_set_param, false)

GDA_TOOL_CLASS(SetRenderLightColorTool, "set_render_light_color",
               "Set the color of a light. Pass the RID from create_render_light and a color {r, g, b, a}; alpha is ignored. Combine with set_render_light_param to control energy and range. Lights tint the surfaces they illuminate. Returns 'ok'. Errors if a required parameter is missing.",
               "Render", std::vector<std::string>({"render", "light", "color"}), render_ops::handle_light_set_color, false)

GDA_TOOL_CLASS(CreateRenderMeshTool, "create_render_mesh",
               "Create a server-side mesh to hold geometry, returning its RID as result.rid. A mesh owns one or more surfaces built with add_render_mesh_surface; assign per-surface materials with set_render_mesh_surface_material. A mesh alone is not visible; it needs to be instanced into a scenario. Returns an error if the RenderingServer is unavailable.",
               "Render", std::vector<std::string>({"render", "mesh", "create"}), render_ops::handle_mesh_create, false)

GDA_TOOL_CLASS(AddRenderMeshSurfaceTool, "add_render_mesh_surface",
               "Add a surface built from vertex arrays to a mesh. Pass the RID from create_render_mesh; primitive defaults to triangles and arrays may hold vertices, normals, tangents, colors, uvs and indices. Surfaces are indexed from 0 in call order. Returns 'ok'. Errors if mesh_rid is missing.",
               "Render", std::vector<std::string>({"render", "mesh", "surface"}), render_ops::handle_mesh_add_surface, false)

GDA_TOOL_CLASS(SetRenderMeshSurfaceMaterialTool, "set_render_mesh_surface_material",
               "Assign a material to one surface of a mesh. Pass the RID from create_render_mesh, a 0-based surface index (in add_render_mesh_surface call order) and a material RID from create_render_material. Each surface can have its own material. Returns 'ok'. Errors if a required parameter is missing.",
               "Render", std::vector<std::string>({"render", "mesh", "material"}), render_ops::handle_mesh_set_material, false)

GDA_TOOL_CLASS(CreateRenderMaterialTool, "create_render_material",
               "Create a server-side material and return its RID as result.rid. Materials feed shader parameters into surfaces; set them with set_render_material_param and assign via set_render_mesh_surface_material. Note that no tool attaches a shader to a material, so the material only takes effect with external shader setup (e.g. code_execute). Returns an error if the RenderingServer is unavailable.",
               "Render", std::vector<std::string>({"render", "material", "create"}), render_ops::handle_material_create, false)

GDA_TOOL_CLASS(SetRenderMaterialParamTool, "set_render_material_param",
               "Set a named parameter on a material. Pass the RID from create_render_material, the parameter name and a JSON value; type_hint controls Variant deserialization (e.g. Vector2, Color, int, float). The value must match the shader's declared parameter type. Returns 'ok'. Errors if a required parameter is missing.",
               "Render", std::vector<std::string>({"render", "material", "param"}), render_ops::handle_material_set_param, false)

GDA_TOOL_CLASS(CreateRenderViewportTool, "create_render_viewport",
               "Create a server-side viewport and return its RID as result.rid. Configure it with set_render_viewport_size and set_render_viewport_clear_mode. Note that no tool can attach a camera or canvas to a viewport, so standalone viewports cannot be rendered; the RID is useful mainly for external composition. Returns an error if the RenderingServer is unavailable.",
               "Render", std::vector<std::string>({"render", "viewport", "create"}), render_ops::handle_viewport_create, false)

GDA_TOOL_CLASS(SetRenderViewportSizeTool, "set_render_viewport_size",
               "Set the pixel size of a viewport. Pass the RID from create_render_viewport; width defaults to 640 and height to 480. It determines the resolution of the viewport's render target. Change it at runtime to resize the target. Returns 'ok'. Errors if viewport_rid is missing.",
               "Render", std::vector<std::string>({"render", "viewport", "size"}), render_ops::handle_viewport_set_size, false)

GDA_TOOL_CLASS(SetRenderViewportClearModeTool, "set_render_viewport_clear_mode",
               "Set how a viewport clears its background each frame. Pass the RID from create_render_viewport; clear_mode is 0=always, 1=never, 2=only_next_frame and defaults to 0. Never clearing keeps the previous frame, useful for persistent drawing. Returns 'ok'. Errors if viewport_rid is missing.",
               "Render", std::vector<std::string>({"render", "viewport", "clear"}), render_ops::handle_viewport_set_clear_mode, false)

GDA_TOOL_CLASS(CreateRenderParticlesTool, "create_render_particles",
               "Create a server-side particle system and return its RID as result.rid. mode selects 2D (0) or 3D (1) and defaults to 3D. Pair with set_render_particles_emitting, set_render_particles_lifetime and restart_render_particles. Note that no tool instantiates the system into a scenario, so visible emission requires external setup (e.g. code_execute). Returns an error if the RenderingServer is unavailable.",
               "Render", std::vector<std::string>({"render", "particle", "create"}), render_ops::handle_particle_create, false)

GDA_TOOL_CLASS(SetRenderEnvironmentBgColorTool, "set_render_environment_bg_color",
               "Set the background color of an environment. Pass the environment RID from an external source (e.g. code_execute); no create tool exists. Requires a color {r, g, b, a}. It takes effect when the environment background mode is set to color. Returns 'ok'. Errors if a required parameter is missing.",
               "Render", std::vector<std::string>({"render", "environment", "bg"}), render_ops::handle_environment_set_bg_color, false)

GDA_TOOL_CLASS(SetRenderEnvironmentAmbientLightTool, "set_render_environment_ambient_light",
               "Set the ambient light of an environment. Pass the environment RID from an external source (e.g. code_execute); source selects 0=bg, 1=disabled, 2=color, 3=sky and energy scales brightness. Ambient light fills shadows so unlit surfaces stay visible. Returns 'ok'. Errors if a required parameter is missing.",
               "Render", std::vector<std::string>({"render", "environment", "ambient"}), render_ops::handle_environment_set_ambient, false)

GDA_TOOL_CLASS(CreateRenderFogVolumeTool, "create_render_fog_volume",
               "Create a server-side volumetric fog volume and return its RID as result.rid. shape and size may be set at creation, or later with set_render_fog_volume_shape. Fog volumes add localized volumetric fog in 3D scenes when combined with an environment that has volumetric fog enabled. Returns an error if the RenderingServer is unavailable.",
               "Render", std::vector<std::string>({"render", "fog", "create"}), render_ops::handle_fog_create, false)

GDA_TOOL_CLASS(CreateRenderShaderTool, "create_render_shader",
               "Create a server-side shader and return its RID as result.rid. Optional code sets the shader source immediately; set_render_shader_code replaces it later. Note that no tool can attach a shader to a material, so the shader RID is mainly useful for testing. Returns an error if the RenderingServer is unavailable.",
               "Render", std::vector<std::string>({"render", "shader", "create"}), render_ops::handle_shader_create, false)

GDA_TOOL_CLASS(CreateRenderTextureFromImageTool, "create_render_texture_from_image",
               "Create a server-side 2D texture from an image file and return its RID as result.rid. image_path accepts res://, user:// or absolute paths; relative paths are not supported. Pass the RID to add_render_canvas_item_texture_rect. Returns an error if the image fails to load.",
               "Render", std::vector<std::string>({"render", "texture"}), render_ops::handle_texture_create_2d, false)

GDA_TOOL_CLASS(SetRenderShaderCodeTool, "set_render_shader_code",
               "Set the source code of a shader. Pass the RID from create_render_shader and the shader code as a string. It replaces any code set at creation time; compilation happens on the server side. Returns 'ok'. Errors if a required parameter is missing.",
               "Render", std::vector<std::string>({"render", "shader"}), render_ops::handle_shader_set_code, false)

GDA_TOOL_CLASS(SetRenderEnvironmentGlowTool, "set_render_environment_glow",
               "Configure glow (bloom) post-processing on an environment. Pass the environment RID from an external source (e.g. code_execute); all parameters are optional, see schema for meanings and defaults. Glow makes bright areas bleed into their surroundings. Returns 'ok'. Errors if environment_rid is missing.",
               "Render", std::vector<std::string>({"render", "environment"}), render_ops::handle_environment_set_glow, false)

GDA_TOOL_CLASS(SetRenderEnvironmentSsrTool, "set_render_environment_ssr",
               "Configure screen-space reflections on an environment. Pass the environment RID from an external source (e.g. code_execute); all parameters are optional, see schema for meanings and defaults. SSR reflects visible geometry onto reflective surfaces without cubemaps. Returns 'ok'. Errors if environment_rid is missing.",
               "Render", std::vector<std::string>({"render", "environment"}), render_ops::handle_environment_set_ssr, false)

GDA_TOOL_CLASS(SetRenderEnvironmentTonemapTool, "set_render_environment_tonemap",
               "Configure tone mapping and exposure on an environment. Pass the environment RID from an external source (e.g. code_execute); tone_mapper, exposure and white are optional, see schema for meanings. Tone mapping maps HDR scene colors to the display range. Returns 'ok'. Errors if environment_rid is missing.",
               "Render", std::vector<std::string>({"render", "environment"}), render_ops::handle_environment_set_tonemap, false)

GDA_TOOL_CLASS(SetRenderEnvironmentSdfgiTool, "set_render_environment_sdfgi",
               "Configure SDFGI global illumination on an environment. Pass the environment RID from an external source (e.g. code_execute); all parameters are optional, see schema for meanings and defaults. SDFGI provides GI for large outdoor scenes. Returns 'ok'. Errors if environment_rid is missing.",
               "Render", std::vector<std::string>({"render", "environment"}), render_ops::handle_environment_set_sdfgi, false)

GDA_TOOL_CLASS(SetRenderEnvironmentVolumetricFogTool, "set_render_environment_volumetric_fog",
               "Configure volumetric fog on an environment. Pass the environment RID from an external source (e.g. code_execute); all parameters are optional, see schema for meanings and defaults. It fills the scene with light-scattering fog, e.g. mist or smoke. Returns 'ok'. Errors if environment_rid is missing.",
               "Render", std::vector<std::string>({"render", "environment"}), render_ops::handle_environment_set_volumetric_fog, false)

GDA_TOOL_CLASS(CreateRenderSkyTool, "create_render_sky",
               "Create a server-side sky and return its RID as result.rid. Assign a material with set_render_sky_material. Note that no tool can attach a sky to an environment, so the sky RID is mainly useful for testing. Returns an error if the RenderingServer is unavailable.",
               "Render", std::vector<std::string>({"render", "sky"}), render_ops::handle_sky_create, false)

GDA_TOOL_CLASS(SetRenderSkyMaterialTool, "set_render_sky_material",
               "Set the material of a sky. Pass the sky RID from create_render_sky and a material RID from create_render_material. The material defines the sky appearance, e.g. a procedural sky or panorama texture. Returns 'ok'. Errors if a required parameter is missing.",
               "Render", std::vector<std::string>({"render", "sky"}), render_ops::handle_sky_set_material, false)

GDA_TOOL_CLASS(SetRenderParticlesEmittingTool, "set_render_particles_emitting",
               "Set whether a particle system is emitting. Pass the RID from create_render_particles; emitting defaults to true. To restart playback from the beginning, use restart_render_particles instead. Emitting alone does not restart a finished emission; combine with set_render_particles_lifetime to control duration. Returns 'ok'. Errors if particles_rid is missing.",
               "Render", std::vector<std::string>({"render", "particles"}), render_ops::handle_particles_set_emitting, false)

GDA_TOOL_CLASS(RestartRenderParticlesTool, "restart_render_particles",
               "Restart a particle system from the beginning, even if it is currently emitting. Pass the RID from create_render_particles. Unlike set_render_particles_emitting, this forces a fresh emission cycle. Use it to replay bursts, e.g. explosions or one-shot effects. Returns 'ok'. Errors if particles_rid is missing.",
               "Render", std::vector<std::string>({"render", "particles"}), render_ops::handle_particles_restart, false)

GDA_TOOL_CLASS(SetRenderParticlesLifetimeTool, "set_render_particles_lifetime",
               "Set the lifetime of a particle system in seconds, controlling how long each emitted particle stays alive. Pass the RID from create_render_particles; lifetime defaults to 1.0. Pair with set_render_particles_emitting and restart_render_particles to control playback. Returns 'ok'. Errors if particles_rid is missing.",
               "Render", std::vector<std::string>({"render", "particles"}), render_ops::handle_particles_set_lifetime, false)

GDA_TOOL_CLASS(CreateRenderReflectionProbeTool, "create_render_reflection_probe",
               "Create a server-side reflection probe and return its RID as result.rid. A reflection probe captures surrounding geometry and lights to feed reflective materials. Use it to add local reflections to custom 3D scenes, though no tool instantiates the probe into a scenario. Returns an error if the RenderingServer is unavailable.",
               "Render", std::vector<std::string>({"render", "reflection_probe"}), render_ops::handle_reflection_probe_create, false)

GDA_TOOL_CLASS(CreateRenderDecalTool, "create_render_decal",
               "Create a server-side decal and return its RID as result.rid. A decal projects a texture onto nearby surfaces, e.g. bullet holes or signs. Note that no tool instantiates the decal into a scenario, so its use is currently limited. Returns an error if the RenderingServer is unavailable.",
               "Render", std::vector<std::string>({"render", "decal"}), render_ops::handle_decal_create, false)

GDA_TOOL_CLASS(SetRenderFogVolumeShapeTool, "set_render_fog_volume_shape",
               "Set the shape of a fog volume after creation. Pass the fog RID from create_render_fog_volume; the same shape can also be supplied at creation. Different shapes suit different fog layouts, e.g. ellipsoid for clouds. Returns 'ok'. Errors if a required parameter is missing.",
               "Render", std::vector<std::string>({"render", "fog"}), render_ops::handle_fog_volume_set_shape, false)

GDA_TOOL_CLASS(SetRenderInstanceVisibleTool, "set_render_instance_visible",
               "Set the visibility state of a rendering instance, not a toggle. The instance RID must come from an external source (e.g. code_execute), as no create tool exists; visible defaults to true. Use it to show or hide a server-side object in the rendered scene. Returns 'ok'. Errors if instance_rid is missing.",
               "Render", std::vector<std::string>({"render", "instance"}), render_ops::handle_instance_set_visible, false)

GDA_TOOL_CLASS(SetRenderInstanceLayerMaskTool, "set_render_instance_layer_mask",
               "Set the layer mask of a rendering instance, controlling which camera layers see it. The instance RID must come from an external source (e.g. code_execute), as no create tool exists; mask is a bitfield. Returns 'ok'. Errors if a required parameter is missing.",
               "Render", std::vector<std::string>({"render", "instance"}), render_ops::handle_instance_set_layer_mask, false)

GDA_TOOL_CLASS(SetRenderShaderParameterGlobalTool, "set_render_shader_parameter_global",
               "Set a global shader parameter available to all shaders. The shader must declare a parameter with the same name using the global keyword. type_hint controls Variant deserialization (e.g. Vector2, Color, int, float). Returns 'ok'. Errors if a required parameter is missing.",
               "Render", std::vector<std::string>({"render", "shader"}), render_ops::handle_global_shader_parameter_set, false)

GDA_TOOL_CLASS(FindRenderNodeFromRidTool, "find_render_node_from_rid",
               "Check whether an RID is valid and find edited-scene nodes that use it, mainly for debugging render RIDs. Returns rid, is_valid and, when matching nodes exist, a nodes array with node_path and type entries. It is the reverse of get_render_canvas_item_rid. Returns 'ok' even for invalid RIDs.",
               "Render", std::vector<std::string>({"utility", "debug", "render"}), render_ops::handle_resolve_rid, false)

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(49);
  v.push_back(std::make_unique<CreateRenderCanvasItemTool>());
  v.push_back(std::make_unique<AddRenderCanvasItemRectTool>());
  v.push_back(std::make_unique<AddRenderCanvasItemCircleTool>());
  v.push_back(std::make_unique<AddRenderCanvasItemTextureRectTool>());
  v.push_back(std::make_unique<AddRenderCanvasItemLineTool>());
  v.push_back(std::make_unique<SetRenderCanvasItemTransformTool>());
  v.push_back(std::make_unique<SetRenderCanvasItemVisibleTool>());
  v.push_back(std::make_unique<GetRenderCanvasItemRidTool>());
  v.push_back(std::make_unique<CreateRenderScenarioTool>());
  v.push_back(std::make_unique<SetRenderScenarioEnvironmentTool>());
  v.push_back(std::make_unique<CreateRenderCameraTool>());
  v.push_back(std::make_unique<SetRenderCameraTransformTool>());
  v.push_back(std::make_unique<SetRenderCameraPerspectiveTool>());
  v.push_back(std::make_unique<SetRenderCameraOrthogonalTool>());
  v.push_back(std::make_unique<CreateRenderLightTool>());
  v.push_back(std::make_unique<SetRenderLightParamTool>());
  v.push_back(std::make_unique<SetRenderLightColorTool>());
  v.push_back(std::make_unique<CreateRenderMeshTool>());
  v.push_back(std::make_unique<AddRenderMeshSurfaceTool>());
  v.push_back(std::make_unique<SetRenderMeshSurfaceMaterialTool>());
  v.push_back(std::make_unique<CreateRenderMaterialTool>());
  v.push_back(std::make_unique<SetRenderMaterialParamTool>());
  v.push_back(std::make_unique<CreateRenderViewportTool>());
  v.push_back(std::make_unique<SetRenderViewportSizeTool>());
  v.push_back(std::make_unique<SetRenderViewportClearModeTool>());
  v.push_back(std::make_unique<CreateRenderParticlesTool>());
  v.push_back(std::make_unique<SetRenderEnvironmentBgColorTool>());
  v.push_back(std::make_unique<SetRenderEnvironmentAmbientLightTool>());
  v.push_back(std::make_unique<CreateRenderFogVolumeTool>());
  v.push_back(std::make_unique<CreateRenderShaderTool>());
  v.push_back(std::make_unique<CreateRenderTextureFromImageTool>());
  v.push_back(std::make_unique<SetRenderShaderCodeTool>());
  v.push_back(std::make_unique<SetRenderEnvironmentGlowTool>());
  v.push_back(std::make_unique<SetRenderEnvironmentSsrTool>());
  v.push_back(std::make_unique<SetRenderEnvironmentTonemapTool>());
  v.push_back(std::make_unique<SetRenderEnvironmentSdfgiTool>());
  v.push_back(std::make_unique<SetRenderEnvironmentVolumetricFogTool>());
  v.push_back(std::make_unique<CreateRenderSkyTool>());
  v.push_back(std::make_unique<SetRenderSkyMaterialTool>());
  v.push_back(std::make_unique<SetRenderParticlesEmittingTool>());
  v.push_back(std::make_unique<RestartRenderParticlesTool>());
  v.push_back(std::make_unique<SetRenderParticlesLifetimeTool>());
  v.push_back(std::make_unique<CreateRenderReflectionProbeTool>());
  v.push_back(std::make_unique<CreateRenderDecalTool>());
  v.push_back(std::make_unique<SetRenderFogVolumeShapeTool>());
  v.push_back(std::make_unique<SetRenderInstanceVisibleTool>());
  v.push_back(std::make_unique<SetRenderInstanceLayerMaskTool>());
  v.push_back(std::make_unique<SetRenderShaderParameterGlobalTool>());
  v.push_back(std::make_unique<FindRenderNodeFromRidTool>());
  return v;
}

} // namespace render_tools
} // namespace godot_autopilot

#endif