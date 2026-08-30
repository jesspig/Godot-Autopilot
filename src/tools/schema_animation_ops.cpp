#include "tools/schema_fills.hpp"
#include "tools/schema_builder.hpp"

namespace godot_autopilot {

void fill_schema_animation(std::unordered_map<std::string, mcp::JsonValue>& m) {

        m["create_scene_animation_player"] = schema::build_schema({
            {"parent_path", "string", "Parent node path in the edited scene (e.g. 'Root' or 'Root/Actors')", true},
            {"name", "string", "Node name for the new AnimationPlayer (string, default: AnimationPlayer)", false},
        });
        m["create_animation"] = schema::build_schema({
            {"player_path", "string", "Path of the AnimationPlayer node in the edited scene (e.g. 'AnimationPlayer')", true},
            {"name", "string", "Animation name to create inside the default ('') library, e.g. 'walk'; must not already exist", true},
            {"length_sec", "number", "Animation length in seconds (number, default: 1.0; must be > 0)", false},
            {"loop_mode", "integer", "Loop mode: 0 none, 1 linear loop, 2 pingpong (integer, default: 0)", false},
        });
        m["remove_animation"] = schema::build_schema({
            {"player_path", "string", "Path of the AnimationPlayer node in the edited scene", true},
            {"name", "string", "Animation name to remove; searched across all of the player's libraries", true},
        });
        m["get_animation_list"] = schema::build_schema({
            {"player_path", "string", "Path of the AnimationPlayer node in the edited scene", true},
        });
        m["create_animation_track"] = schema::build_schema({
            {"player_path", "string", "Path of the AnimationPlayer node in the edited scene", true},
            {"anim_name", "string", "Animation name to add the track to; must already exist", true},
            {"track_type", "string", "Track kind: 'value' (animates a property) or 'method' (invokes a method per key)", true},
            {"node_path", "string", "Scene-relative path of the target node the track drives, e.g. 'Sprite2D'", true},
            {"property", "string", "Property to animate for value tracks, e.g. 'position:x' or 'modulate:a'; required when track_type is 'value', ignored for method tracks — the track path becomes <node_path>:<property>", false},
        });
        m["insert_animation_keyframe"] = schema::build_schema({
            {"player_path", "string", "Path of the AnimationPlayer node in the edited scene", true},
            {"anim_name", "string", "Animation name containing the target track", true},
            {"track_index", "integer", "Index of the track within the animation, as returned by create_animation_track", true},
            {"time", "number", "Key time in seconds since animation start (number, >= 0)", true},
            {"value", "object", "Key value as JSON, deserialized to a Variant (e.g. 1.5 or {\"x\":0,\"y\":1}); method tracks require an object like {\"method\":\"jump\",\"args\":[42]}", true},
        });
        m["remove_animation_track"] = schema::build_schema({
            {"player_path", "string", "Path of the AnimationPlayer node in the edited scene", true},
            {"anim_name", "string", "Animation name containing the track to remove", true},
            {"track_index", "integer", "Index of the track to remove; must be within [0, track_count)", true},
        });
        m["create_scene_animation_tree"] = schema::build_schema({
            {"parent_path", "string", "Parent node path in the edited scene (e.g. 'Root')", true},
            {"name", "string", "Node name for the new AnimationTree (string, default: AnimationTree)", false},
            {"anim_player", "string", "Scene-relative AnimationPlayer path to wire into the tree's animation_player property, e.g. 'Root/AnimationPlayer'", false},
        });
        m["add_animation_machine_state"] = schema::build_schema({
            {"animation_tree_path", "string", "Path of the AnimationTree whose state machine root receives the state", true},
            {"state_name", "string", "Unique state name within the state machine, e.g. 'idle'", true},
            {"animation", "string", "Animation name assigned to the state's AnimationNodeAnimation, e.g. 'walk'", false},
        });
        m["connect_animation_states"] = schema::build_schema({
            {"animation_tree_path", "string", "Path of the AnimationTree owning both states", true},
            {"from_state", "string", "Source state name of the transition", true},
            {"to_state", "string", "Target state name of the transition", true},
            {"condition", "string", "Bool parameter name gating the transition, e.g. 'is_running'; sets advance_mode AUTO with that advance_condition", false},
        });
}

} // namespace godot_autopilot
