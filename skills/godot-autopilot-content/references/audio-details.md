# Audio Details

Reference for the audio workflow in the content skill: bus layout
manipulation rules, effect chain semantics, and the routing and polyphony
behaviors verified against engine source. Audio bus tools act on the editor
process; a running game keeps its own device and bus layout.

## Reading the bus layout

- `get_audio_bus_layout` returns the complete layout: every bus (the Master
  bus always occupies index 0) with its volume, mute/solo state and effect
  chain. Snapshot it before making changes.
- `get_audio_bus_count` returns the number of buses and bounds the valid
  `bus_index` range; `get_audio_bus_name` maps a zero-based index to a
  name. An out-of-range index returns "audio bus not found at index: N".

## Bus volume, mute, solo and bypass

All four take a zero-based `bus_index` and apply immediately to editor
playback:

- `set_audio_bus_volume_db` - decibels; negative attenuates, positive
  amplifies (typical range -80 to +24).
- `set_audio_bus_mute` - `muted` true silences the bus regardless of
  volume.
- `set_audio_bus_solo` - `solo` true mutes all other buses; soloing several
  buses keeps exactly those audible.
- `set_audio_bus_bypass_effects` - `bypass` true skips the whole effect
  chain without removing it; the chain stays in place for later re-enable.

### bypass vs mute: where the CPU goes

The two switches are NOT equivalent in cost:

- bypass only skips the effect chain - the bus signal still flows, the
  effects are just not applied.
- mute multiplies the bus output by zero AFTER the effect chain - the
  effects keep running on a muted bus and keep spending CPU. To actually
  save effect processing, bypass the effects (or remove them) instead of
  muting.
- solo overrides mute: check both flags when a "muted" bus is still
  audible.

## Bus effects

- `add_audio_bus_effect` - `effect_type` must be the class name of an
  instantiable `AudioEffect` subclass, for example "AudioEffectReverb" or
  "AudioEffectDistortion"; unknown or abstract names return an error.
  `at_position` inserts at a zero-based slot (default -1 appends at the
  end).
- `remove_audio_bus_effect` - `effect_index` within the bus effect chain;
  out-of-range errors. Read the current chain with `get_audio_bus_layout`
  first.

> **Warning - layout replacement is whole-object.** `set_audio_bus_layout`
> replaces the entire layout: bus count, names, volumes and effect chains.
> Never write back a hand-made partial object. The flow is always
> `get_audio_bus_layout` -> edit the returned object -> pass it back with
> only your intended change. The change applies immediately to editor
> playback.

## Bus naming, send routing and the bus cap

Verified against the engine's audio server:

- Renaming a bus to a name that already exists silently appends a numeric
  suffix: the second "Music" becomes "Music 2", the next "Music 3". Code
  that looks the bus up by its requested name will not find it.
- Bus 0 (Master) silently ignores renames to anything other than "Master" -
  the name does not change.
- `get_bus_index` returns -1 for a name that matches no bus; a -1 index
  passed onward is invalid, not "bus 0".
- A playback stream's target `bus` that does not exist silently falls back
  to Master - audio keeps playing, just on the wrong bus. Verify assignments
  by reading `bus` back.
- A bus `send` pointing at a nonexistent bus, or at a bus that would form a
  cycle, silently changes to send to Master instead.
- The bus count is capped at 256 buses per layout.

## Player control

Player tools accept `AudioStreamPlayer`, `AudioStreamPlayer2D` and
`AudioStreamPlayer3D` nodes. `node_path` is a scene-relative path
("Level1/Player") or an absolute "/root/..." path.

- `play_audio_player` - optional `stream_path` loads an audio resource
  before playing; optional `from_position` starts at an offset in seconds.
  If the node has no stream assigned and no `stream_path` is given, an
  error explains it.
- `stop_audio_player` - the stream stays assigned, so the node can be
  played again later.
- `set_audio_player_volume_db` - per-player decibels applied on top of the
  bus volume.
- `set_audio_player_pitch_scale` - `pitch_scale` 1.0 is normal speed; above
  raises pitch and playback speed, below lowers them.
- `get_audio_player_playback_position` - current position in seconds.
- `seek_audio_player` - `to_position` in seconds; works while the node is
  playing or stopped.

### Polyphony: stealing and the max_polyphony floor

- When a player has more simultaneous playbacks than `max_polyphony`, the
  engine steals the OLDEST active playback to start the new one - the new
  sound always plays and the old one is cut. Sound cutting out under rapid
  retriggering is this rule at work, not a bug.
- `max_polyphony` values of 0 or below are silently ignored - the previous
  value stays in effect and no error is raised.

## Audio devices

- `get_audio_device_outputs` / `get_audio_device_inputs` list the hardware
  device names (speakers, headphones, microphones). The list reflects
  attached hardware, not project settings.
- `set_audio_device_output` / `set_audio_device_input` switch the editor
  process's device; `device` must be one of the names returned by the list
  tools.

## Minimal example: a reverb on Master and a shot sound

```json
{"name": "get_audio_bus_layout", "arguments": {}}
{"name": "add_audio_bus_effect",
 "arguments": {"bus_index": 0, "effect_type": "AudioEffectReverb"}}
{"name": "play_audio_player",
 "arguments": {"node_path": "Level1/Player",
               "stream_path": "res://audio/shot.wav"}}
```

Prefer a dedicated bus? Edit the layout object returned by
`get_audio_bus_layout` (append a bus entry), write it back with
`set_audio_bus_layout`, then add the effect at the new bus index and route
the player node to it.

## See also

- The [content skill](../SKILL.md) overview for the condensed audio
  workflow.
- godot-autopilot-scene-system - creating the scenes that host audio player
  nodes.
- godot-autopilot-runtime - play the scene to hear the result and confirm
  bus routing in the running game.
- [UI theme details](ui-theme-details.md) - the other content-domain
  reference in this skill.
