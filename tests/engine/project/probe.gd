extends Node2D

const SMOKE_MARKER := "gda-engine-smoke-v1"

var frames_seen: int = 0
var input_events: int = 0

func _ready() -> void:
	set_meta("gda_smoke", SMOKE_MARKER)
	add_to_group("gda_smoke")

func _process(_delta: float) -> void:
	frames_seen += 1

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventKey:
		input_events += 1

func probe_snapshot() -> Dictionary:
	var scene := get_tree().current_scene
	return {
		"marker": get_meta("gda_smoke", ""),
		"frames_seen": frames_seen,
		"input_events": input_events,
		"process_frames": Engine.get_process_frames(),
		"node_count": get_tree().get_nodes_in_group("gda_smoke").size(),
		"scene_path": scene.scene_file_path if scene != null else "",
		"autopilot_singleton": Engine.has_singleton("AutopilotTools"),
		"godot_version": Engine.get_version_info().get("string", ""),
	}
