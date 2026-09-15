#include "tools/schema_fills.hpp"
#include "tools/schema_builder.hpp"

namespace godot_autopilot {

void fill_schema_content(std::unordered_map<std::string, mcp::JsonValue>& m) {

        m["create_text_font"] = schema::build_schema({});
        m["create_shaped_text"] = schema::build_schema({
            {"direction", "integer", "Text layout direction: 0=auto, 1=ltr, 2=rtl (default: 0)", false},
            {"orientation", "integer", "Text orientation: 0=horizontal, 1=vertical (default: 0)", false},
        });
        m["set_text_font_antialiasing"] = schema::build_schema({
            {"font_rid", "integer", "RID id returned by create_text_font", true},
            {"antialiasing", "integer", "Font antialiasing mode: 0=none, 1=gray, 2=grayscale, 3=subpixel", true},
        });
        m["set_text_font_data"] = schema::build_schema({
            {"font_rid", "integer", "RID id returned by create_text_font", true},
            {"data", "string", "Path to a font file (.ttf, .otf, .woff2); get_text_font_system_path returns a usable path", true},
        });
        m["set_text_font_hinting"] = schema::build_schema({
            {"font_rid", "integer", "RID id returned by create_text_font", true},
            {"hinting", "integer", "Font hinting mode: 0=none, 1=light, 2=normal", true},
        });
        m["get_text_font_system_path"] = schema::build_schema({
            {"font_name", "string", "System font name (e.g. 'Arial', 'Segoe UI', 'Noto Sans')", true},
            {"weight", "integer", "Font weight, 100-900 (default: 400)", false},
            {"stretch", "integer", "Font stretch percentage (default: 100)", false},
            {"italic", "boolean", "Request italic variant (default: false)", false},
        });
        m["has_text_feature"] = schema::build_schema({
            {"feature", "integer", "TextServer feature flag (TextServer.Feature enum: 1=simple_layout, 2=bidi_layout, 4=shaped, 8=kerning, 16=ligatures, 32=font_lcd_subpixel, 64=font_autohinter, 128=font_subpixel_positioning, 256=font_system, 512=font_variable, 1024=context_sensitive_cleartype, 2048=fast_path, 4096=shaping_fallback, 8192=unicode_security, 16384=has_rid)", true},
        });
        m["is_text_locale_right_to_left"] = schema::build_schema({
            {"locale", "string", "BCP-47 locale code (e.g. 'ar', 'he', 'fa' for RTL; 'en', 'de' for LTR)", true},
        });
        m["add_shaped_text_string"] = schema::build_schema({
            {"shaped_rid", "integer", "RID id returned by create_shaped_text", true},
            {"text", "string", "Text string to add (may contain Unicode)", true},
            {"font_rid", "integer", "RID id returned by create_text_font (configured with set_text_font_data first)", true},
            {"size", "integer", "Font size in pixels", true},
            {"language", "string", "Language code (e.g. 'en', 'ar') to improve shaping; omit for auto detection", false},
        });
        m["get_shaped_text_size"] = schema::build_schema({
            {"shaped_rid", "integer", "RID id returned by create_shaped_text", true},
        });

        m["create_tilemap"] = schema::build_schema({
            {"name", "string", "TileMap node name (default: TileMap)", false},
            {"tile_size", "integer", "Tile size in pixels, sets the default TileSet tile_size (default: 16)", false},
            {"format", "integer", "Cell quadrant size in pixels (default: 16)", false},
            {"parent_path", "string", "Parent node path; omit to add at the scene root (errors when the scene has no root)", false},
        });
        m["set_tilemap_cell"] = schema::build_schema({
            {"path", "string", "Path to the TileMap or TileMapLayer node", true},
            {"x", "integer", "Cell X coordinate", true},
            {"y", "integer", "Cell Y coordinate", true},
            {"layer", "integer", "Tile layer index (default: 0); TileMapLayer nodes ignore it", false},
            {"source_id", "integer", "TileSet source ID (default: 0); must exist in the node's TileSet", false},
            {"atlas_coords", "object", "Atlas coordinates Vector2i (e.g. {\"x\":0,\"y\":0})", false},
        });
        m["set_tilemap_cells"] = schema::build_schema({
            {"node_path", "string", "Path to the TileMap or TileMapLayer node", true},
            {"cells", "array", "Array of cells, each {\"x\":int,\"y\":int,\"source_id\":int,\"atlas_coords\":{\"x\":int,\"y\":int}} — source_id is required per entry. No fixed server-side entry cap, but keep batches reasonably sized because client or transport layers may limit a single request payload (bigger layouts: loop in execute_script or code_execute). An entry with source_id -1 clears the cell at x, y (atlas_coords is ignored and may be omitted). Invalid entries are skipped and reported in the error plus warnings", true},
            {"layer", "integer", "Tile layer index (default: 0); TileMapLayer nodes ignore it", false},
        });
        m["fill_tilemap_rect"] = schema::build_schema({
            {"node_path", "string", "Path to the TileMap or TileMapLayer node", true},
            {"from", "object", "Inclusive first corner cell {x, y}; the two corners are normalized internally, so from/to order does not matter", true},
            {"to", "object", "Inclusive opposite corner cell {x, y}", true},
            {"source_id", "integer", "TileSet source ID placed in every cell; required and must be >= 0 unless erase is true", false},
            {"atlas_coords", "object", "Atlas coords {x, y} placed in every cell; both must be >= 0 and is required unless erase is true", false},
            {"alternative", "integer", "Alternative tile id placed in every cell (default: 0; must be >= 0)", false},
            {"erase", "boolean", "Clear every cell in the rect instead of placing tiles; source_id and atlas_coords are ignored (default: false)", false},
            {"layer", "integer", "Tile layer index (default: 0); TileMapLayer nodes ignore it", false},
        });
        m["create_tilemap_tileset"] = schema::build_schema({
            {"name", "string", "Resource name used as the memory:// reference for later tools (default: TileSet)", false},
            {"tile_size", "integer", "Base tile size in pixels (default: 16)", false},
        });
        m["add_tilemap_atlas_source"] = schema::build_schema({
            {"name", "string", "TileSet name from create_tilemap_tileset (memory:// reference)", true},
            {"source_id", "integer", "Source ID to assign, 0-255; must not already be used", true},
            {"texture", "string", "Texture file path (e.g. res://tiles.png); must exist on disk", true},
            {"tile_size", "object", "Tile size in pixels (e.g. {\"x\":16,\"y\":16})", true},
            {"margin", "integer", "Margin in pixels around the atlas texture (default: 0)", false},
            {"spacing", "integer", "Spacing between tiles in pixels (default: 0)", false},
        });
        m["add_tilemap_physics_layer"] = schema::build_schema({
            {"name", "string", "TileSet name from create_tilemap_tileset (memory:// reference)", true},
            {"layer_id", "integer", "Physics layer index to add (appended at the end of the layer list)", true},
            {"collision_layer", "integer", "Collision layer bitmask (default: 1)", false},
            {"collision_mask", "integer", "Collision mask bitmask (default: 1)", false},
        });
        m["set_tilemap_tile_collision"] = schema::build_schema({
            {"name", "string", "TileSet name from create_tilemap_tileset (memory:// reference)", true},
            {"source_id", "integer", "Source ID of the atlas source added by add_tilemap_atlas_source", true},
            {"atlas_coords", "object", "Atlas coordinates of the tile (e.g. {\"x\":0,\"y\":0})", true},
            {"physics_layer", "integer", "Physics layer index added by add_tilemap_physics_layer", true},
            {"polygon", "array", "Collision polygon: array of {x,y} points, or array of such arrays for multiple polygons; an empty array clears existing collision. Points are relative to the TILE CENTER (e.g. for tile_size=16 use (-8,-8)-(8,8) for full-tile collision; (0,0)-(16,16) starts at the center and overhangs)", true},
        });

        m["create_spriteframes"] = schema::build_schema({
            {"name", "string", "Resource name used as the memory:// reference for later tools (e.g. 'hero_walk')", true},
            {"default_animation", "string", "Optional animation created immediately as an empty animation (engine-default fps/loop). Use 'default' to match the AnimatedSprite2D default animation property, so a node that never sets animation keeps a deterministic first animation in the editor instead of falling back to an arbitrary list entry; runtime playback still needs an explicit play() call or property_set on the node", false},
        });
        m["add_spriteframes_animation"] = schema::build_schema({
            {"name", "string", "SpriteFrames name from create_spriteframes (memory:// reference)", true},
            {"animation", "string", "Animation name to add (e.g. 'walk', 'idle')", true},
            {"fps", "number", "Animation playback speed in frames per second (default: 5)", false},
            {"loop", "boolean", "Loop the animation (default: true)", false},
        });
        m["add_spriteframes_frame"] = schema::build_schema({
            {"name", "string", "SpriteFrames name from create_spriteframes (memory:// reference)", true},
            {"animation", "string", "Animation name to add the frame to (created with add_spriteframes_animation first)", true},
            {"texture", "string", "Texture file path (e.g. res://frame.png); must exist and import as Texture2D", true},
            {"duration", "number", "Frame duration in seconds (default: 1.0)", false},
            {"hframes", "integer", "Horizontal frame count for spritesheet splitting (default: 1; must be >= 1)", false},
            {"vframes", "integer", "Vertical frame count for spritesheet splitting (default: 1; must be >= 1)", false},
        });
}

} // namespace godot_autopilot
