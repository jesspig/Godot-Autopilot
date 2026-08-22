#include "render_ops.hpp"
#include "core/log_system.hpp"
#include "util/error_util.hpp"
#include "util/json_godot.hpp"
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {
namespace render_ops {

using JV = mcp::JsonValue;

JV handle_environment_set_bg_color(const JV &args) {
  auto *it_env = args.Find("environment_rid");
  auto *it_color = args.Find("color");
  if (!it_env || !it_env->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: environment_rid");
    return r;
  }
  if (!it_color || !it_color->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: color");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_environment_bg_color called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID env = util::rid_from_json(*it_env);
  godot::Color c = util::json_to_color(*it_color);

  rs->environment_set_bg_color(env, c);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_render_environment_bg_color completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
}

JV handle_environment_set_ambient(const JV &args) {
  auto *it_env = args.Find("environment_rid");
  auto *it_color = args.Find("color");
  if (!it_env || !it_env->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: environment_rid");
    return r;
  }
  if (!it_color || !it_color->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: color");
    return r;
  }

  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "set_render_environment_ambient_light called");
  auto *rs = godot::RenderingServer::get_singleton();
  if (!rs) {
    JV r(JV::object_tag);
    r["error"] = JV("RenderingServer not available");
    return r;
  }

  godot::RID env = util::rid_from_json(*it_env);
  godot::Color c = util::json_to_color(*it_color);
  auto *src = args.Find("source");
  auto source = static_cast<godot::RenderingServer::EnvironmentAmbientSource>(
      src ? static_cast<int>(src->GetInt()) : 0);
  auto *en = args.Find("energy");
  float energy = static_cast<float>(util::json_number(en, 1.0));

  rs->environment_set_ambient_light(env, c, source, energy);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_render_environment_ambient_light completed");
  JV r(JV::object_tag);
  r["result"] = JV("ok");
  return r;
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
      JV::FromObject({{"r", JV(1.0)}, {"g", JV(1.0)}, {"b", JV(1.0)}});
  auto *al = args.Find("albedo");
  if (al && al->IsObject())
    albedo_col = *al;
  godot::Color albedo = util::json_to_color(albedo_col);
  JV emission_col =
      JV::FromObject({{"r", JV(0.0)}, {"g", JV(0.0)}, {"b", JV(0.0)}});
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
