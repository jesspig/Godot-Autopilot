#include "tools/schema_fills.hpp"
#include "tools/schema_builder.hpp"

namespace godot_autopilot {

void fill_schema_content(std::unordered_map<std::string, mcp::JsonValue>& m) {

        m["create_text_font"] = schema::build_schema({});
        m["create_shaped_text"] = schema::build_schema({
            {"direction", "integer", "Text direction: 0=auto, 1=ltr, 2=rtl (default: 0)", false},
            {"orientation", "integer", "Text orientation: 0=horizontal, 1=vertical (default: 0)", false},
        });
        m["set_text_font_antialiasing"] = schema::build_schema({
            {"font_rid", "integer", "RID of the font (from text_create_font)", true},
            {"antialiasing", "integer", "Font antialiasing mode (0=none, 1=gray, 2=grayscale, 3=subpixel)", true},
        });
        m["set_text_font_data"] = schema::build_schema({
            {"font_rid", "integer", "RID of the font (from text_create_font)", true},
            {"data", "string", "Path to a font file (.ttf, .otf, .woff2)", true},
        });
        m["set_text_font_hinting"] = schema::build_schema({
            {"font_rid", "integer", "RID of the font (from text_create_font)", true},
            {"hinting", "integer", "Font hinting mode (0=none, 1=light, 2=normal)", true},
        });
        m["get_text_font_system_path"] = schema::build_schema({
            {"font_name", "string", "System font name (e.g. Arial)", true},
            {"weight", "integer", "Font weight (default: 400)", false},
            {"stretch", "integer", "Font stretch percentage (default: 100)", false},
            {"italic", "boolean", "Request italic variant (default: false)", false},
        });
        m["has_text_feature"] = schema::build_schema({
            {"feature", "integer", "TextServer feature flag (TextServer.Feature enum: 1=simple_layout, 2=bidi_layout, 4=shaped, 8=kerning, 16=ligatures, 32=font_lcd_subpixel, 64=font_autohinter, 128=font_subpixel_positioning, 256=font_system, 512=font_variable, 1024=context_sensitive_cleartype, 2048=fast_path, 4096=shaping_fallback, 8192=unicode_security, 16384=has_rid)", true},
        });
        m["is_text_locale_right_to_left"] = schema::build_schema({
            {"locale", "string", "Locale code (e.g. ar, he, en)", true},
        });
        m["add_shaped_text_string"] = schema::build_schema({
            {"shaped_rid", "integer", "RID of the shaped text (from text_create_shaped_text)", true},
            {"text", "string", "Text string to add", true},
            {"font_rid", "integer", "RID of the font (from text_create_font)", true},
            {"size", "integer", "Font size in pixels", true},
            {"language", "string", "Text language code (optional)", false},
        });
        m["get_shaped_text_size"] = schema::build_schema({
            {"shaped_rid", "integer", "RID of the shaped text (from text_create_shaped_text)", true},
        });

        m["create_tilemap"] = schema::build_schema({
            {"name", "string", "TileMap node name (default: TileMap)", false},
            {"tile_size", "integer", "Tile size in pixels (default: 16)", false},
            {"format", "integer", "Tile map cell format (0=square, 1=isometric, default: 0)", false},
            {"parent_path", "string", "Parent node path (omit to add to root)", false},
        });
        m["set_tilemap_cell"] = schema::build_schema({
            {"path", "string", "Path to the TileMap node", true},
            {"x", "integer", "Cell X coordinate", true},
            {"y", "integer", "Cell Y coordinate", true},
            {"layer", "integer", "Tile layer index (default: 0)", false},
            {"source_id", "integer", "TileSet source ID (default: 0)", false},
            {"atlas_coords", "object", "Atlas coordinates Vector2i (e.g. {\"x\":0,\"y\":0})", false},
        });
        m["set_tilemap_cells"] = schema::build_schema({
            {"node_path", "string", "Path to the TileMap node", true},
            {"cells", "array", "Array of cells, each {\"x\":int,\"y\":int,\"source_id\":int,\"atlas_coords\":{\"x\":int,\"y\":int}} — 单次调用 ≤64 条目（大 payload 可能被客户端截断）；331 格级别的大批量请用 script_execute_gdscript 编程铺设（TileMap.set_cell 循环）", true},
            {"layer", "integer", "Tile layer index (default: 0)", false},
        });
        m["create_tilemap_tileset"] = schema::build_schema({
            {"name", "string", "Resource name used as memory:// reference", false},
            {"tile_size", "integer", "Base tile size in pixels (default 16)", false},
        });
        m["add_tilemap_atlas_source"] = schema::build_schema({
            {"name", "string", "TileSet resource name (memory:// reference)", true},
            {"source_id", "integer", "Source ID to assign (0-255)", true},
            {"texture", "string", "Texture file path (e.g. res://tiles.png)", true},
            {"tile_size", "object", "Tile size in pixels (e.g. {\"x\":16,\"y\":16})", true},
            {"margin", "integer", "Margin in pixels around the atlas texture (default: 0)", false},
            {"spacing", "integer", "Spacing between tiles in pixels (default: 0)", false},
        });
        m["add_tilemap_physics_layer"] = schema::build_schema({
            {"name", "string", "TileSet resource name (memory:// reference)", true},
            {"layer_id", "integer", "Physics layer index to add", true},
            {"collision_layer", "integer", "Collision layer bitmask (default: 1)", false},
            {"collision_mask", "integer", "Collision mask bitmask (default: 1)", false},
        });
        m["set_tilemap_tile_collision"] = schema::build_schema({
            {"name", "string", "TileSet resource name (memory:// reference)", true},
            {"source_id", "integer", "Source ID of the atlas source", true},
            {"atlas_coords", "object", "Atlas coordinates of the tile (e.g. {\"x\":0,\"y\":0})", true},
            {"physics_layer", "integer", "Physics layer index to set collision on", true},
            {"polygon", "array", "Collision polygon points (Array of {x,y}), each point is relative to the TILE CENTER (e.g. for tile_size=16 use (-8,-8)-(8,8) for full-tile collision; (0,0)-(16,16) starts at the center and overhangs)", true},
        });

        m["create_spriteframes"] = schema::build_schema({
            {"name", "string", "Resource name used as memory:// reference", true},
        });
        m["add_spriteframes_animation"] = schema::build_schema({
            {"name", "string", "SpriteFrames resource name (memory:// reference)", true},
            {"animation", "string", "Animation name to add", true},
            {"fps", "number", "Animation playback speed in frames per second (default: 5)", false},
            {"loop", "boolean", "Loop the animation (default: true)", false},
        });
        m["add_spriteframes_frame"] = schema::build_schema({
            {"name", "string", "SpriteFrames resource name (memory:// reference)", true},
            {"animation", "string", "Animation name to add the frame to", true},
            {"texture", "string", "Texture file path (e.g. res://frame.png)", true},
            {"duration", "number", "Frame duration in seconds (default: 1.0)", false},
            {"hframes", "integer", "Horizontal frame count for spritesheet splitting (default: 1)", false},
            {"vframes", "integer", "Vertical frame count for spritesheet splitting (default: 1)", false},
        });
}

} // namespace godot_autopilot
