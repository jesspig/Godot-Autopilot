#include "render_ops.hpp"
#include "core/log_system.hpp"
#include "util/error_util.hpp"
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {
namespace render_ops {

using JV = mcp::JsonValue;

namespace {

// 以下两个辅助复制自 render_ops.cpp 的匿名命名空间，保持两处同步。
godot::RID rid_from_json(const JV &j) {
  return godot::UtilityFunctions::rid_from_int64(j.GetInt());
}

godot::Color parse_color(const JV &j) {
  auto *r = j.Find("r");
  auto *g = j.Find("g");
  auto *b = j.Find("b");
  auto *a = j.Find("a");
  return godot::Color(
      static_cast<float>(
          r && r->IsNumber()
              ? (r->IsInt() ? static_cast<double>(r->GetInt()) : r->GetDouble())
              : 0.0),
      static_cast<float>(
          g && g->IsNumber()
              ? (g->IsInt() ? static_cast<double>(g->GetInt()) : g->GetDouble())
              : 0.0),
      static_cast<float>(
          b && b->IsNumber()
              ? (b->IsInt() ? static_cast<double>(b->GetInt()) : b->GetDouble())
              : 0.0),
      static_cast<float>(
          a && a->IsNumber()
              ? (a->IsInt() ? static_cast<double>(a->GetInt()) : a->GetDouble())
              : 1.0));
}

} // namespace

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

  godot::RID env = rid_from_json(*it_env);
  godot::Color c = parse_color(*it_color);

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

  godot::RID env = rid_from_json(*it_env);
  godot::Color c = parse_color(*it_color);
  auto *src = args.Find("source");
  auto source = static_cast<godot::RenderingServer::EnvironmentAmbientSource>(
      src ? static_cast<int>(src->GetInt()) : 0);
  auto *en = args.Find("energy");
  float energy = static_cast<float>(
      en && en->IsNumber()
          ? (en->IsInt() ? static_cast<double>(en->GetInt()) : en->GetDouble())
          : 1.0);

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
  godot::RID env = rid_from_json(*it_env);
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
  if (it && it->IsNumber())
    intensity = static_cast<float>(
        it->IsInt() ? static_cast<double>(it->GetInt()) : it->GetDouble());
  float strength = 1.0f;
  auto *st = args.Find("strength");
  if (st && st->IsNumber())
    strength = static_cast<float>(
        st->IsInt() ? static_cast<double>(st->GetInt()) : st->GetDouble());
  float mix = 0.05f;
  auto *mx = args.Find("mix");
  if (mx && mx->IsNumber())
    mix = static_cast<float>(mx->IsInt() ? static_cast<double>(mx->GetInt())
                                         : mx->GetDouble());
  float bloom_threshold = 0.0f;
  auto *bt = args.Find("bloom_threshold");
  if (bt && bt->IsNumber())
    bloom_threshold = static_cast<float>(
        bt->IsInt() ? static_cast<double>(bt->GetInt()) : bt->GetDouble());
  int blend_mode = 0;
  auto *bm = args.Find("blend_mode");
  if (bm && bm->IsInt())
    blend_mode = static_cast<int>(bm->GetInt());
  float hdr_bleed_threshold = 0.5f;
  auto *hbt = args.Find("hdr_bleed_threshold");
  if (hbt && hbt->IsNumber())
    hdr_bleed_threshold = static_cast<float>(
        hbt->IsInt() ? static_cast<double>(hbt->GetInt()) : hbt->GetDouble());
  float hdr_bleed_scale = 2.0f;
  auto *hbs = args.Find("hdr_bleed_scale");
  if (hbs && hbs->IsNumber())
    hdr_bleed_scale = static_cast<float>(
        hbs->IsInt() ? static_cast<double>(hbs->GetInt()) : hbs->GetDouble());
  float hdr_luminance_cap = 2.0f;
  auto *hlc = args.Find("hdr_luminance_cap");
  if (hlc && hlc->IsNumber())
    hdr_luminance_cap = static_cast<float>(
        hlc->IsInt() ? static_cast<double>(hlc->GetInt()) : hlc->GetDouble());
  float glow_map_strength = 1.0f;
  auto *gms = args.Find("glow_map_strength");
  if (gms && gms->IsNumber())
    glow_map_strength = static_cast<float>(
        gms->IsInt() ? static_cast<double>(gms->GetInt()) : gms->GetDouble());
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
  godot::RID env = rid_from_json(*it_env);
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
  if (fi && fi->IsNumber())
    fade_in = static_cast<float>(fi->IsInt() ? static_cast<double>(fi->GetInt())
                                             : fi->GetDouble());
  float fade_out = 0.1f;
  auto *fo = args.Find("fade_out");
  if (fo && fo->IsNumber())
    fade_out = static_cast<float>(
        fo->IsInt() ? static_cast<double>(fo->GetInt()) : fo->GetDouble());
  float depth_tolerance = 0.1f;
  auto *dt = args.Find("depth_tolerance");
  if (dt && dt->IsNumber())
    depth_tolerance = static_cast<float>(
        dt->IsInt() ? static_cast<double>(dt->GetInt()) : dt->GetDouble());
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
  godot::RID env = rid_from_json(*it_env);
  int tone_mapper = 0;
  auto *tm = args.Find("tone_mapper");
  if (tm && tm->IsInt())
    tone_mapper = static_cast<int>(tm->GetInt());
  float exposure = 1.0f;
  auto *ex = args.Find("exposure");
  if (ex && ex->IsNumber())
    exposure = static_cast<float>(
        ex->IsInt() ? static_cast<double>(ex->GetInt()) : ex->GetDouble());
  float white = 1.0f;
  auto *wh = args.Find("white");
  if (wh && wh->IsNumber())
    white = static_cast<float>(wh->IsInt() ? static_cast<double>(wh->GetInt())
                                           : wh->GetDouble());
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
  godot::RID env = rid_from_json(*it_env);
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
  if (mcs && mcs->IsNumber())
    min_cell_size = static_cast<float>(
        mcs->IsInt() ? static_cast<double>(mcs->GetInt()) : mcs->GetDouble());
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
  if (bf && bf->IsNumber())
    bounce_feedback = static_cast<float>(
        bf->IsInt() ? static_cast<double>(bf->GetInt()) : bf->GetDouble());
  bool read_sky = true;
  auto *rs_ = args.Find("read_sky");
  if (rs_ && rs_->IsBool())
    read_sky = rs_->GetBool();
  float energy = 1.0f;
  auto *eg = args.Find("energy");
  if (eg && eg->IsNumber())
    energy = static_cast<float>(eg->IsInt() ? static_cast<double>(eg->GetInt())
                                            : eg->GetDouble());
  float normal_bias = 1.0f;
  auto *nb = args.Find("normal_bias");
  if (nb && nb->IsNumber())
    normal_bias = static_cast<float>(
        nb->IsInt() ? static_cast<double>(nb->GetInt()) : nb->GetDouble());
  float probe_bias = 1.0f;
  auto *pb = args.Find("probe_bias");
  if (pb && pb->IsNumber())
    probe_bias = static_cast<float>(
        pb->IsInt() ? static_cast<double>(pb->GetInt()) : pb->GetDouble());
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
  godot::RID env = rid_from_json(*it_env);
  bool enabled = true;
  auto *en = args.Find("enabled");
  if (en && en->IsBool())
    enabled = en->GetBool();
  float density = 0.05f;
  auto *de = args.Find("density");
  if (de && de->IsNumber())
    density = static_cast<float>(de->IsInt() ? static_cast<double>(de->GetInt())
                                             : de->GetDouble());
  JV albedo_col =
      JV::FromObject({{"r", JV(1.0)}, {"g", JV(1.0)}, {"b", JV(1.0)}});
  auto *al = args.Find("albedo");
  if (al && al->IsObject())
    albedo_col = *al;
  godot::Color albedo = parse_color(albedo_col);
  JV emission_col =
      JV::FromObject({{"r", JV(0.0)}, {"g", JV(0.0)}, {"b", JV(0.0)}});
  auto *em = args.Find("emission");
  if (em && em->IsObject())
    emission_col = *em;
  godot::Color emission = parse_color(emission_col);
  float emission_energy = 1.0f;
  auto *ee = args.Find("emission_energy");
  if (ee && ee->IsNumber())
    emission_energy = static_cast<float>(
        ee->IsInt() ? static_cast<double>(ee->GetInt()) : ee->GetDouble());
  float anisotropy = 0.0f;
  auto *an = args.Find("anisotropy");
  if (an && an->IsNumber())
    anisotropy = static_cast<float>(
        an->IsInt() ? static_cast<double>(an->GetInt()) : an->GetDouble());
  float length = 0.0f;
  auto *le = args.Find("length");
  if (le && le->IsNumber())
    length = static_cast<float>(le->IsInt() ? static_cast<double>(le->GetInt())
                                            : le->GetDouble());
  float detail_spread = 0.0f;
  auto *ds = args.Find("detail_spread");
  if (ds && ds->IsNumber())
    detail_spread = static_cast<float>(
        ds->IsInt() ? static_cast<double>(ds->GetInt()) : ds->GetDouble());
  float gi_inject = 0.0f;
  auto *gi = args.Find("gi_inject");
  if (gi && gi->IsNumber())
    gi_inject = static_cast<float>(
        gi->IsInt() ? static_cast<double>(gi->GetInt()) : gi->GetDouble());
  bool temporal_reprojection = false;
  auto *tr = args.Find("temporal_reprojection");
  if (tr && tr->IsBool())
    temporal_reprojection = tr->GetBool();
  float temporal_reprojection_amount = 0.5f;
  auto *tra = args.Find("temporal_reprojection_amount");
  if (tra && tra->IsNumber())
    temporal_reprojection_amount = static_cast<float>(
        tra->IsInt() ? static_cast<double>(tra->GetInt()) : tra->GetDouble());
  float ambient_inject = 0.0f;
  auto *ai = args.Find("ambient_inject");
  if (ai && ai->IsNumber())
    ambient_inject = static_cast<float>(
        ai->IsInt() ? static_cast<double>(ai->GetInt()) : ai->GetDouble());
  float sky_affect = 0.0f;
  auto *sa = args.Find("sky_affect");
  if (sa && sa->IsNumber())
    sky_affect = static_cast<float>(
        sa->IsInt() ? static_cast<double>(sa->GetInt()) : sa->GetDouble());
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
