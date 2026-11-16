#include "tools/domains/render/render_ops.hpp"
#include "core/observability/log_system.hpp"
#include "util/error_util.hpp"
#include "util/json_godot.hpp"
#include "util/rid_registry.hpp"
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <cstdint>
#include <mcp/JsonValue.hpp>
#include <string>
#include <unordered_map>

namespace godot_autopilot {
namespace render_ops {

using JV = mcp::JsonValue;

namespace {

std::unordered_map<int64_t, JV> &environment_states() {
  static std::unordered_map<int64_t, JV> states;
  return states;
}

JV &environment_state(int64_t id) {
  JV &state = environment_states()[id];
  if (!state.IsObject()) {
    state = JV(JV::object_tag);
  }
  return state;
}

bool is_environment_group(const std::string &name) {
  return name == "bg" || name == "ambient" || name == "glow" ||
         name == "ssr" || name == "tonemap" || name == "sdfgi" ||
         name == "volumetric_fog";
}

JV environment_group_or_empty(int64_t id, const char *group) {
  auto it = environment_states().find(id);
  if (it == environment_states().end()) {
    return JV(JV::object_tag);
  }
  const JV *value = it->second.Find(group);
  if (!value || !value->IsObject()) {
    return JV(JV::object_tag);
  }
  return *value;
}

void merge_environment_group(JV &target, const JV &incoming) {
  for (const auto &entry : incoming.GetObject()) {
    target[entry.first] = entry.second;
  }
}

JV delegated_environment_call(const JV &group, const JV &rid_value) {
  JV call = group;
  call["environment_rid"] = rid_value;
  return call;
}

JV apply_environment_bg(godot::RenderingServer *rs, const godot::RID &env,
                       const JV &group) {
  if (const JV *mode = group.Find("mode"); mode && mode->IsInt()) {
    rs->environment_set_background(
        env, static_cast<godot::RenderingServer::EnvironmentBG>(
                 static_cast<int>(mode->GetInt())));
  }
  if (const JV *color = group.Find("color"); color && color->IsObject()) {
    rs->environment_set_bg_color(env, util::json_to_color(*color));
  }
  const bool has_energy = group.Find("energy") != nullptr;
  const bool has_exposure = group.Find("exposure") != nullptr;
  if (has_energy || has_exposure) {
    float energy =
        static_cast<float>(util::json_number(group.Find("energy"), 1.0));
    float exposure =
        static_cast<float>(util::json_number(group.Find("exposure"), 1.0));
    rs->environment_set_bg_energy(env, energy, exposure);
  }
  return JV();
}

JV apply_environment_ambient(godot::RenderingServer *rs, const godot::RID &env,
                             const JV &group) {
  godot::Color color(1.0f, 1.0f, 1.0f, 1.0f);
  if (const JV *value = group.Find("color"); value && value->IsObject()) {
    color = util::json_to_color(*value);
  }
  int source = 0;
  if (const JV *value = group.Find("source"); value && value->IsInt()) {
    source = static_cast<int>(value->GetInt());
  }
  float energy =
      static_cast<float>(util::json_number(group.Find("energy"), 1.0));
  float sky_contribution =
      static_cast<float>(util::json_number(group.Find("sky_contribution"), 0.0));
  int reflection_source = 0;
  if (const JV *value = group.Find("reflection_source");
      value && value->IsInt()) {
    reflection_source = static_cast<int>(value->GetInt());
  }
  rs->environment_set_ambient_light(
      env, color,
      static_cast<godot::RenderingServer::EnvironmentAmbientSource>(source),
      energy, sky_contribution,
      static_cast<godot::RenderingServer::EnvironmentReflectionSource>(
          reflection_source));
  return JV();
}

JV apply_environment_group(godot::RenderingServer *rs, const godot::RID &env,
                           const JV &rid_value, const std::string &name,
                           const JV &group) {
  JV call = delegated_environment_call(group, rid_value);
  if (name == "bg") {
    return apply_environment_bg(rs, env, group);
  }
  if (name == "ambient") {
    return apply_environment_ambient(rs, env, group);
  }
  if (name == "glow") {
    return handle_environment_set_glow(call);
  }
  if (name == "ssr") {
    return handle_environment_set_ssr(call);
  }
  if (name == "tonemap") {
    return handle_environment_set_tonemap(call);
  }
  if (name == "sdfgi") {
    return handle_environment_set_sdfgi(call);
  }
  return handle_environment_set_volumetric_fog(call);
}

} // namespace

JV handle_environment_action(const JV &args) {
  const JV *action_p = args.Find("action");
  if (!action_p || !action_p->IsString()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: action");
    return r;
  }
  const std::string action = action_p->GetString();
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "render_environment action=" + action);

  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  if (action == "create") {
    godot::RID rid = rs->environment_create();
    int64_t id = util::rid_store<RenderRidDomain>().store(rid);
    environment_state(id);
    return util::ok_result(util::rid_to_json(rid));
  }

  const JV *rid_p = args.Find("environment_rid");
  if (!rid_p || !rid_p->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter for action '" + action +
                    "': environment_rid");
    return r;
  }
  godot::RID env = util::rid_from_json(*rid_p);
  if (!env.is_valid()) {
    JV r(JV::object_tag);
    r["error"] = JV("environment_rid is not a valid RID for action '" + action +
                    "': " + std::to_string(rid_p->GetInt()));
    return r;
  }
  const int64_t env_id = static_cast<int64_t>(rid_p->GetInt());

  if (action == "set_params") {
    const JV *params_p = args.Find("params");
    if (!params_p || !params_p->IsObject()) {
      JV r(JV::object_tag);
      r["error"] = JV("missing required parameter for action 'set_params': "
                      "params (object with optional bg, ambient, glow, ssr, "
                      "tonemap, sdfgi and volumetric_fog groups)");
      return r;
    }
    for (const auto &entry : params_p->GetObject()) {
      if (!is_environment_group(entry.first)) {
        JV r(JV::object_tag);
        r["error"] = JV("unknown environment params group: " + entry.first);
        return r;
      }
      if (!entry.second.IsObject()) {
        JV r(JV::object_tag);
        r["error"] =
            JV("environment params group must be an object: " + entry.first);
        return r;
      }
    }
    for (const auto &entry : params_p->GetObject()) {
      JV merged = environment_group_or_empty(env_id, entry.first.c_str());
      merge_environment_group(merged, entry.second);
      JV applied =
          apply_environment_group(rs, env, *rid_p, entry.first, merged);
      if (applied.IsObject() && applied.Find("error") != nullptr) {
        return applied;
      }
      environment_state(env_id)[entry.first] = std::move(merged);
    }
    JV r(JV::object_tag);
    r["result"] = JV("ok");
    return r;
  }

  if (action == "set_sky") {
    const JV *sky_p = args.Find("sky_rid");
    if (!sky_p || !sky_p->IsNumber()) {
      JV r(JV::object_tag);
      r["error"] =
          JV("missing required parameter for action 'set_sky': sky_rid");
      return r;
    }
    godot::RID sky = util::rid_from_json(*sky_p);
    if (!sky.is_valid()) {
      JV r(JV::object_tag);
      r["error"] = JV("sky_rid is not a valid RID: " +
                      std::to_string(sky_p->GetInt()));
      return r;
    }
    rs->environment_set_sky(env, sky);
    environment_state(env_id)["sky_rid"] = *sky_p;
    JV r(JV::object_tag);
    r["result"] = JV("ok");
    return r;
  }

  if (action == "get") {
    JV params(JV::object_tag);
    auto it = environment_states().find(env_id);
    if (it != environment_states().end()) {
      params = it->second;
    }
    JV inner(JV::object_tag);
    inner["rid"] = JV(env_id);
    inner["params"] = std::move(params);
    return util::ok_result(std::move(inner));
  }

  JV r(JV::object_tag);
  r["error"] = JV("unknown action for render_environment: " + action);
  return r;
}

void forget_environment_state(int64_t rid_id) {
  environment_states().erase(rid_id);
}

JV handle_environment_set_glow(const JV &args) {
  auto *it_env = args.Find("environment_rid");
  if (!it_env || !it_env->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: environment_rid");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_environment_glow called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID env = util::rid_from_json(*it_env);
  bool enabled = true;
  auto *en = args.Find("enabled");
  if (en && en->IsBool())
    enabled = en->GetBool();
  godot::PackedFloat32Array levels;
  levels.append(0.85f);
  auto *lv = args.Find("level");
  if (lv && lv->IsDouble())
    levels[0] = static_cast<float>(lv->GetDouble());
  float intensity = 0.8f;
  auto *it = args.Find("intensity");
  intensity = static_cast<float>(util::json_number(it, intensity));
  float strength = 1.0f;
  auto *st = args.Find("strength");
  strength = static_cast<float>(util::json_number(st, strength));
  float mix = 0.05f;
  auto *mx = args.Find("mix");
  mix = static_cast<float>(util::json_number(mx, mix));
  float bloom_threshold = 0.0f;
  auto *bt = args.Find("bloom_threshold");
  bloom_threshold = static_cast<float>(util::json_number(bt, bloom_threshold));
  int blend_mode = 0;
  auto *bm = args.Find("blend_mode");
  if (bm && bm->IsInt())
    blend_mode = static_cast<int>(bm->GetInt());
  float hdr_bleed_threshold = 0.5f;
  auto *hbt = args.Find("hdr_bleed_threshold");
  hdr_bleed_threshold =
      static_cast<float>(util::json_number(hbt, hdr_bleed_threshold));
  float hdr_bleed_scale = 2.0f;
  auto *hbs = args.Find("hdr_bleed_scale");
  hdr_bleed_scale = static_cast<float>(util::json_number(hbs, hdr_bleed_scale));
  float hdr_luminance_cap = 2.0f;
  auto *hlc = args.Find("hdr_luminance_cap");
  hdr_luminance_cap =
      static_cast<float>(util::json_number(hlc, hdr_luminance_cap));
  float glow_map_strength = 1.0f;
  auto *gms = args.Find("glow_map_strength");
  glow_map_strength =
      static_cast<float>(util::json_number(gms, glow_map_strength));
  rs->environment_set_glow(
      env, enabled, levels, intensity, strength, mix, bloom_threshold,
      static_cast<godot::RenderingServer::EnvironmentGlowBlendMode>(blend_mode),
      hdr_bleed_threshold, hdr_bleed_scale, hdr_luminance_cap,
      glow_map_strength, godot::RID());
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_environment_set_ssr(const JV &args) {
  auto *it_env = args.Find("environment_rid");
  if (!it_env || !it_env->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: environment_rid");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_environment_ssr called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID env = util::rid_from_json(*it_env);
  bool enabled = true;
  auto *en = args.Find("enabled");
  if (en && en->IsBool())
    enabled = en->GetBool();
  int max_steps = 64;
  auto *ms = args.Find("max_steps");
  if (ms && ms->IsInt())
    max_steps = static_cast<int>(ms->GetInt());
  float fade_in = 0.1f;
  auto *fi = args.Find("fade_in");
  fade_in = static_cast<float>(util::json_number(fi, fade_in));
  float fade_out = 0.1f;
  auto *fo = args.Find("fade_out");
  fade_out = static_cast<float>(util::json_number(fo, fade_out));
  float depth_tolerance = 0.1f;
  auto *dt = args.Find("depth_tolerance");
  depth_tolerance = static_cast<float>(util::json_number(dt, depth_tolerance));
  rs->environment_set_ssr(env, enabled, max_steps, fade_in, fade_out,
                          depth_tolerance);
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_environment_set_tonemap(const JV &args) {
  auto *it_env = args.Find("environment_rid");
  if (!it_env || !it_env->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: environment_rid");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_environment_tonemap called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID env = util::rid_from_json(*it_env);
  int tone_mapper = 0;
  auto *tm = args.Find("tone_mapper");
  if (tm && tm->IsInt())
    tone_mapper = static_cast<int>(tm->GetInt());
  float exposure = 1.0f;
  auto *ex = args.Find("exposure");
  exposure = static_cast<float>(util::json_number(ex, exposure));
  float white = 1.0f;
  auto *wh = args.Find("white");
  white = static_cast<float>(util::json_number(wh, white));
  rs->environment_set_tonemap(
      env,
      static_cast<godot::RenderingServer::EnvironmentToneMapper>(tone_mapper),
      exposure, white);
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_environment_set_sdfgi(const JV &args) {
  auto *it_env = args.Find("environment_rid");
  if (!it_env || !it_env->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: environment_rid");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_environment_sdfgi called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID env = util::rid_from_json(*it_env);
  bool enabled = true;
  auto *en = args.Find("enabled");
  if (en && en->IsBool())
    enabled = en->GetBool();
  int cascades = 4;
  auto *ca = args.Find("cascades");
  if (ca && ca->IsInt())
    cascades = static_cast<int>(ca->GetInt());
  float min_cell_size = 0.1f;
  auto *mcs = args.Find("min_cell_size");
  min_cell_size = static_cast<float>(util::json_number(mcs, min_cell_size));
  int y_scale = 0;
  auto *ys = args.Find("y_scale");
  if (ys && ys->IsInt())
    y_scale = static_cast<int>(ys->GetInt());
  bool use_occlusion = true;
  auto *uo = args.Find("use_occlusion");
  if (uo && uo->IsBool())
    use_occlusion = uo->GetBool();
  float bounce_feedback = 0.5f;
  auto *bf = args.Find("bounce_feedback");
  bounce_feedback = static_cast<float>(util::json_number(bf, bounce_feedback));
  bool read_sky = true;
  auto *rs_ = args.Find("read_sky");
  if (rs_ && rs_->IsBool())
    read_sky = rs_->GetBool();
  float energy = 1.0f;
  auto *eg = args.Find("energy");
  energy = static_cast<float>(util::json_number(eg, energy));
  float normal_bias = 1.0f;
  auto *nb = args.Find("normal_bias");
  normal_bias = static_cast<float>(util::json_number(nb, normal_bias));
  float probe_bias = 1.0f;
  auto *pb = args.Find("probe_bias");
  probe_bias = static_cast<float>(util::json_number(pb, probe_bias));
  rs->environment_set_sdfgi(
      env, enabled, cascades, min_cell_size,
      static_cast<godot::RenderingServer::EnvironmentSDFGIYScale>(y_scale),
      use_occlusion, bounce_feedback, read_sky, energy, normal_bias,
      probe_bias);
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_environment_set_volumetric_fog(const JV &args) {
  auto *it_env = args.Find("environment_rid");
  if (!it_env || !it_env->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: environment_rid");
    return r;
  }
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_environment_volumetric_fog called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }
  godot::RID env = util::rid_from_json(*it_env);
  bool enabled = true;
  auto *en = args.Find("enabled");
  if (en && en->IsBool())
    enabled = en->GetBool();
  float density = 0.05f;
  auto *de = args.Find("density");
  density = static_cast<float>(util::json_number(de, density));
  JV albedo_col =
      JV(JV::Object{{"r", JV(1.0)}, {"g", JV(1.0)}, {"b", JV(1.0)}});
  auto *al = args.Find("albedo");
  if (al && al->IsObject())
    albedo_col = *al;
  godot::Color albedo = util::json_to_color(albedo_col);
  JV emission_col =
      JV(JV::Object{{"r", JV(0.0)}, {"g", JV(0.0)}, {"b", JV(0.0)}});
  auto *em = args.Find("emission");
  if (em && em->IsObject())
    emission_col = *em;
  godot::Color emission = util::json_to_color(emission_col);
  float emission_energy = 1.0f;
  auto *ee = args.Find("emission_energy");
  emission_energy = static_cast<float>(util::json_number(ee, emission_energy));
  float anisotropy = 0.0f;
  auto *an = args.Find("anisotropy");
  anisotropy = static_cast<float>(util::json_number(an, anisotropy));
  float length = 0.0f;
  auto *le = args.Find("length");
  length = static_cast<float>(util::json_number(le, length));
  float detail_spread = 0.0f;
  auto *ds = args.Find("detail_spread");
  detail_spread = static_cast<float>(util::json_number(ds, detail_spread));
  float gi_inject = 0.0f;
  auto *gi = args.Find("gi_inject");
  gi_inject = static_cast<float>(util::json_number(gi, gi_inject));
  bool temporal_reprojection = false;
  auto *tr = args.Find("temporal_reprojection");
  if (tr && tr->IsBool())
    temporal_reprojection = tr->GetBool();
  float temporal_reprojection_amount = 0.5f;
  auto *tra = args.Find("temporal_reprojection_amount");
  temporal_reprojection_amount =
      static_cast<float>(util::json_number(tra, temporal_reprojection_amount));
  float ambient_inject = 0.0f;
  auto *ai = args.Find("ambient_inject");
  ambient_inject = static_cast<float>(util::json_number(ai, ambient_inject));
  float sky_affect = 0.0f;
  auto *sa = args.Find("sky_affect");
  sky_affect = static_cast<float>(util::json_number(sa, sky_affect));
  rs->environment_set_volumetric_fog(
      env, enabled, density, albedo, emission, emission_energy, anisotropy,
      length, detail_spread, gi_inject, temporal_reprojection,
      temporal_reprojection_amount, ambient_inject, sky_affect);
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

} // namespace render_ops
} // namespace godot_autopilot
