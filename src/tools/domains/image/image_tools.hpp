#ifndef GODOT_AUTOPILOT_IMAGE_TOOLS_HPP
#define GODOT_AUTOPILOT_IMAGE_TOOLS_HPP

#include "util/json_value.hpp"
#include <memory>
#include <string>
#include <vector>

#include "tools/domains/image/image_ops.hpp"
#include <tools/base/tool_spec.hpp>

namespace godot_autopilot {
namespace image_tools {

namespace {

const std::vector<ParamSpec> kImageEditParams = {
    {"image", "object", "Image locator for actions info/resize/crop/flip/rotate/fill: memory://name string, bare registration name, decimal object_id string, res:// image path, or {name|path|object_id|object_id_str} object", false},
    {"path", "string", "Only with action='load': source image path inside res://, e.g. res://assets/icon.png; PNG, JPG, WebP, TGA and BMP are supported", false},
    {"name", "string", "Only with action='create' or 'load': registration name so later calls can reference the image as memory://name; when omitted the image is addressable by the returned object_id", false},
    {"width", "integer", "With action='create', 'resize' or 'crop': target width in pixels, 1-4096; crop requires the requested region to fit inside the image", false},
    {"height", "integer", "With action='create', 'resize' or 'crop': target height in pixels, 1-4096; crop requires the requested region to fit inside the image", false},
    {"format", "string", "Only with action='create': uncompressed pixel format: l8, la8, r8, rg8, rgb8, rgba8 (default), rgba4444, rgb565, rf, rgf, rgbf, rgbaf, rh, rgh, rgbh, rgbah or rgbe9995", false},
    {"color", "object", "With action='create' or 'fill': RGBA color object {r, g, b, a} with 0-1 components; create defaults to fully transparent black", false},
    {"interpolation", "string", "Only with action='resize': nearest, bilinear (default), cubic, trilinear or lanczos", false},
    {"x", "integer", "Only with action='crop': left offset of the kept region (default: 0)", false},
    {"y", "integer", "Only with action='crop': top offset of the kept region (default: 0)", false},
    {"axis", "string", "Only with action='flip': x, y or both (default: y)", false},
    {"degrees", "integer", "Only with action='rotate': rotation angle, one of 90, 180 or 270", false},
    {"direction", "string", "Only with action='rotate' when degrees is 90 or 270: clockwise (default) or counterclockwise", false},
};

const std::vector<ParamSpec> kImagePixelParams = {
    {"image", "object", "Target Image locator, required by every action: memory://name string, bare registration name, decimal object_id string, res:// image path, or {name|path|object_id|object_id_str} object", true},
    {"src", "object", "Only with action='blit' or 'blend': source Image locator in the same formats as image", false},
    {"x", "integer", "With action='blit' or 'blend': destination left offset (default: 0); with get_pixel/set_pixel: pixel x coordinate", false},
    {"y", "integer", "With action='blit' or 'blend': destination top offset (default: 0); with get_pixel/set_pixel: pixel y coordinate", false},
    {"src_x", "integer", "Only with action='blit' or 'blend': source region left offset (default: 0)", false},
    {"src_y", "integer", "Only with action='blit' or 'blend': source region top offset (default: 0)", false},
    {"src_width", "integer", "Only with action='blit' or 'blend': source region width (default: full source width)", false},
    {"src_height", "integer", "Only with action='blit' or 'blend': source region height (default: full source height)", false},
    {"color", "object", "Only with action='set_pixel': RGBA color object {r, g, b, a} with 0-1 components", false},
    {"mode", "string", "Only with action='compress': s3tc (default), etc, etc2, bptc or astc", false},
    {"source", "string", "Only with action='compress': generic (default), srgb or normal", false},
    {"astc_format", "string", "Only with action='compress' and mode='astc': 4x4 (default) or 8x8", false},
    {"name", "string", "Only with action='to_texture': registration name for the ImageTexture so later calls can reference it as memory://name", false},
};

const std::vector<ParamSpec> kImageSaveParams = {
    {"image", "object", "Image locator: memory://name string, bare registration name, decimal object_id string, res:// image path, or {name|path|object_id|object_id_str} object", true},
    {"path", "string", "Destination PNG path inside res://, e.g. res://assets/generated.png; must end with .png; missing parent directories are created recursively and an existing file is overwritten", true},
};

const std::vector<ParamSpec> kTextureGenParams = {
    {"width", "integer", "Output width in pixels, 1-4096 (default: 256)", false},
    {"height", "integer", "Output height in pixels, 1-4096 (default: 256; ignored by gradient_texture dimension='1d')", false},
    {"name", "string", "Registration name for the generated Image (noise/gradient) or Texture resource (noise_texture/gradient_texture); addressable later as memory://name", false},
    {"image_name", "string", "Only with action='noise_texture' or 'gradient_texture': registration name for the synchronously rendered preview Image; default: '<name>_image' when name is set, otherwise object_id only", false},
    {"seed", "integer", "Only with action='noise' or 'noise_texture': FastNoiseLite seed (default: 0)", false},
    {"frequency", "number", "Only with action='noise' or 'noise_texture': noise frequency (default: engine FastNoiseLite default)", false},
    {"noise_type", "string", "Only with action='noise' or 'noise_texture': simplex, simplex_smooth, cellular, perlin, value_cubic or value", false},
    {"fractal_type", "string", "Only with action='noise' or 'noise_texture': none, fbm, ridged or ping_pong", false},
    {"fractal_octaves", "integer", "Only with action='noise' or 'noise_texture': fractal octave count, at least 1", false},
    {"gradient", "array", "Gradient control points as [{offset: 0..1, color: {r, g, b, a}}, ...]; required shape for action='gradient' and 'gradient_texture', optional color_ramp for 'noise_texture'; defaults to black to white when omitted", false},
    {"interpolation", "string", "Gradient interpolation: linear (default), constant or cubic", false},
    {"seamless", "boolean", "Only with action='noise' or 'noise_texture': generate a tileable seamless image (default: false)", false},
    {"normalize", "boolean", "Only with action='noise' or 'noise_texture': normalize noise output to 0-1 (default: true)", false},
    {"invert", "boolean", "Only with action='noise' or 'noise_texture': invert the noise output (default: false)", false},
    {"skirt", "number", "Only with seamless noise: seamless blend skirt fraction (default: 0.1)", false},
    {"axis", "string", "Only with action='gradient': gradient direction horizontal (default) or vertical", false},
    {"dimension", "string", "Only with action='gradient_texture': 2d (default) creates GradientTexture2D, 1d creates GradientTexture1D", false},
    {"fill", "string", "Only with gradient_texture dimension='2d': linear (default), radial, square or conic", false},
    {"repeat", "string", "Only with gradient_texture dimension='2d': none (default), repeat or mirror", false},
    {"fill_from", "object", "Only with gradient_texture dimension='2d': normalized start point {x, y} (default: {x: 0, y: 0})", false},
    {"fill_to", "object", "Only with gradient_texture dimension='2d': normalized end point {x, y} (default: {x: 1, y: 0})", false},
};

const std::vector<std::string> kImageEditActions = {
    "create", "load", "info", "resize", "crop", "flip", "rotate", "fill"};

const std::vector<std::string> kImagePixelActions = {
    "blit", "blend", "get_pixel", "set_pixel", "compress", "decompress",
    "to_texture"};

const std::vector<std::string> kImageSaveActions = {"save"};

const std::vector<std::string> kTextureGenActions = {
    "noise", "gradient", "noise_texture", "gradient_texture"};

} // namespace

inline std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> make_tools() {
  std::vector<std::unique_ptr<::godot_autopilot::ToolBase>> v;
  v.reserve(4);
  v.push_back(make_spec_tool(ToolSpec{
      "image_edit",
      "Create, load and edit in-memory Image resources (None side effect; nothing is written to disk). action selects the operation: 'create' makes a blank width x height image (format default rgba8, optional color fill) and registers it; 'load' reads a PNG/JPG/WebP/TGA/BMP file inside res:// and registers it; 'info' reports class, width, height, format, used_rect, data_size, is_empty and is_compressed; 'resize' scales to width x height with optional interpolation; 'crop' keeps the x/y/width/height region; 'flip' mirrors along axis (x, y or both); 'rotate' rotates by degrees (90/180/270) with optional direction; 'fill' replaces every pixel with color. Locate images by the returned name (memory://name or bare name), decimal object_id, res:// path, or a {name|path|object_id} object. Images are capped at 4096 x 4096. Chain with image_pixel (pixel edits, compression, to_texture) and image_save. Example: {\"action\": \"create\", \"width\": 64, \"height\": 64, \"name\": \"icon\"} then {\"action\": \"fill\", \"image\": \"memory://icon\", \"color\": {\"r\": 1, \"g\": 0, \"b\": 0, \"a\": 1}}.",
      "Resources", {"image", "create", "load", "resize", "crop", "flip", "rotate", "fill", "info", "pixel", "png"}, SideEffect::None, tool_flags::kNone,
      kImageEditParams, image_ops::handle_edit_action,
      schema::build_action_schema(
          kImageEditActions, kImageEditParams,
          "create: blank image; load: read an image file from res://; info: report image metadata; resize: scale; crop: keep a region; flip: mirror; rotate: 90/180/270 degrees; fill: replace all pixels with one color"),
      {},
      {"image_edit action=create width=64 height=64 name=icon format=rgba8",
       "image_edit action=resize image=memory://icon width=32 height=32 interpolation=nearest",
       "image_edit action=fill image=memory://icon color={\"r\":1,\"g\":0,\"b\":0,\"a\":1}"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "image_pixel",
      "Read or modify pixels of in-memory Image resources and convert them to textures (None side effect). action selects the operation: 'blit' copies src onto image at x/y (replaces destination pixels), 'blend' alpha-blends src onto image, both accepting an optional source region via src_x/src_y/src_width/src_height; 'get_pixel' returns the color at x/y; 'set_pixel' writes color at x/y; 'compress' encodes with mode s3tc/etc/etc2/bptc/astc and source generic/srgb/normal; 'decompress' restores an uncompressed image; 'to_texture' wraps the image in an ImageTexture registered as a Resource. image is required for every action; locate it by memory://name, bare registration name, decimal object_id, res:// path or {name|path|object_id}. Example: {\"action\": \"set_pixel\", \"image\": \"memory://icon\", \"x\": 0, \"y\": 0, \"color\": {\"r\": 0, \"g\": 1, \"b\": 0, \"a\": 1}} or {\"action\": \"to_texture\", \"image\": \"memory://icon\", \"name\": \"icon_tex\"}.",
      "Resources", {"image", "pixel", "blit", "blend", "compress", "decompress", "texture", "image_texture"}, SideEffect::None, tool_flags::kNone,
      kImagePixelParams, image_ops::handle_pixel_action,
      schema::build_action_schema(
          kImagePixelActions, kImagePixelParams,
          "blit: copy src pixels onto image; blend: alpha-blend src onto image; get_pixel: read one color; set_pixel: write one color; compress: encode with an import compression mode; decompress: decode to an uncompressed format; to_texture: create an ImageTexture"),
      {},
      {"image_pixel action=blit image=memory://base src=memory://overlay x=0 y=0",
       "image_pixel action=get_pixel image=memory://icon x=0 y=0",
       "image_pixel action=to_texture image=memory://icon name=icon_tex"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "image_save",
      "Save an in-memory Image to a PNG file inside res:// (WritesFile, no confirmation). action='save' writes image to path, creating missing parent directories recursively and overwriting an existing file; the response reports path, bytes, width, height and verified (true when the file exists after saving). The image handle comes from image_edit, image_pixel or texture_gen. Example: {\"action\": \"save\", \"image\": \"memory://icon\", \"path\": \"res://assets/icon.png\"}.",
      "Resources", {"image", "save", "png", "export", "write"}, SideEffect::WritesFile, tool_flags::kMutating,
      kImageSaveParams, image_ops::handle_save_action,
      schema::build_action_schema(
          kImageSaveActions, kImageSaveParams,
          "save: write the image as PNG and verify the file"),
      {},
      {"image_save action=save image=memory://icon path=res://assets/icon.png"}}));
  v.push_back(make_spec_tool(ToolSpec{
      "texture_gen",
      "Generate noise and gradient images and textures (None side effect; nothing is written to disk). action selects the operation: 'noise' renders a FastNoiseLite image (width, height, seed, frequency, noise_type, fractal_type, fractal_octaves, seamless, normalize, invert) and registers an Image; 'gradient' renders a Gradient image along axis horizontal/vertical and registers an Image; 'noise_texture' creates a NoiseTexture2D resource (same noise options plus optional gradient color_ramp) and also registers a synchronously rendered preview Image so it can be saved immediately; 'gradient_texture' creates a GradientTexture1D (dimension='1d') or GradientTexture2D (dimension='2d', fill linear/radial/square/conic, repeat none/repeat/mirror, fill_from/fill_to) and also registers a preview Image. gradient points are [{\"offset\": 0..1, \"color\": {\"r\", \"g\", \"b\", \"a\"}}] and default to black to white. Sizes are capped at 4096. Textures register under name, preview images under image_name (default: '<name>_image'); save either with image_save. NoiseTexture2D renders on demand in the editor, so the preview Image is generated synchronously and is never read back as an empty texture. Example: {\"action\": \"noise_texture\", \"name\": \"clouds\", \"width\": 256, \"height\": 256, \"seed\": 42, \"seamless\": true} then {\"action\": \"save\", \"image\": \"memory://clouds_image\", \"path\": \"res://assets/clouds.png\"}.",
      "Resources", {"noise", "gradient", "texture", "generate", "fast_noise_lite", "noise_texture", "gradient_texture", "seamless"}, SideEffect::None, tool_flags::kNone,
      kTextureGenParams, image_ops::handle_texture_gen_action,
      schema::build_action_schema(
          kTextureGenActions, kTextureGenParams,
          "noise: render a FastNoiseLite image; gradient: render a Gradient image; noise_texture: create a NoiseTexture2D plus a preview image; gradient_texture: create a GradientTexture1D/2D plus a preview image"),
      {},
      {"texture_gen action=noise width=256 height=256 seed=7 frequency=0.02 seamless=true name=noise_img",
       "texture_gen action=gradient width=128 height=16 gradient=[{\"offset\":0,\"color\":{\"r\":0,\"g\":0,\"b\":0,\"a\":1}},{\"offset\":1,\"color\":{\"r\":1,\"g\":1,\"b\":1,\"a\":1}}] name=ramp",
       "texture_gen action=gradient_texture name=radial dimension=2d fill=radial width=256 height=256"}}));
  return v;
}

} // namespace image_tools
} // namespace godot_autopilot

#endif
