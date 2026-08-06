#include "spriteframes_ops.hpp"
#include "core/log_system.hpp"
#include "resource_ops.hpp"
#include "util/error_util.hpp"
#include <godot_cpp/classes/atlas_texture.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sprite_frames.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <string>

namespace godot_self_driving {
namespace spriteframes_ops {

using JV = mcp::JsonValue;

namespace {

JV error(const std::string &msg) {
  JV e(JV::object_tag);
  e["error"] = JV(msg);
  return e;
}

godot::Ref<godot::SpriteFrames> resolve_spriteframes(const std::string &name,
                                                     std::string &out_error) {
  out_error.clear();
  godot::Ref<godot::Resource> res = resource_ops::resolve_memory_resource(name);
  if (res.is_null()) {
    out_error = "memory resource not found: " + name +
                " (create it with spriteframes_create first)";
    return godot::Ref<godot::SpriteFrames>();
  }
  godot::Ref<godot::SpriteFrames> sf(res);
  if (sf.is_null()) {
    out_error = "memory resource is not a SpriteFrames: " + name;
    return godot::Ref<godot::SpriteFrames>();
  }
  return sf;
}

} // namespace

JV handle_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "spriteframes_create called");

  auto *n = args.Find("name");
  if (!n || !n->IsString() || n->GetString().empty())
    return error("missing required parameter: name");
  std::string name = n->GetString();

  auto *cdbs = godot::ClassDBSingleton::get_singleton();
  if (!cdbs)
    return error("ClassDB singleton not available");

  godot::Variant obj_var = cdbs->instantiate(godot::StringName("SpriteFrames"));
  if (obj_var.get_type() == godot::Variant::NIL)
    return error("failed to instantiate SpriteFrames");

  godot::Ref<godot::SpriteFrames> sf = obj_var;
  if (sf.is_null())
    return error("instantiated object is not a SpriteFrames");

  sf->remove_animation(godot::StringName("default"));

  resource_ops::register_memory_resource(sf, name);
  sf->set_path(godot::String(("memory://" + name).c_str()));

  JV r(JV::object_tag);
  JV info(JV::object_tag);
  info["class"] = JV("SpriteFrames");
  info["name"] = JV(name);
  info["path"] = JV("memory://" + name);
  info["object_id"] = JV(static_cast<int64_t>(sf->get_instance_id()));
  info["default_animation_removed"] = JV(true);
  r["result"] = std::move(info);
  LogSystem::instance().log(
      LogLevel::Info, LogCategory::Tools,
      "spriteframes_create completed (default animation removed)");
  return r;
}

JV handle_add_animation(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "spriteframes_add_animation called");

  auto *n = args.Find("name");
  if (!n || !n->IsString() || n->GetString().empty())
    return error("missing required parameter: name");
  std::string name = n->GetString();

  auto *a = args.Find("animation");
  if (!a || !a->IsString() || a->GetString().empty())
    return error("missing required parameter: animation");
  std::string animation = a->GetString();

  double fps = 5.0;
  auto *f = args.Find("fps");
  if (f && f->IsInt())
    fps = static_cast<double>(f->GetInt());
  else if (f && f->IsDouble())
    fps = f->GetDouble();

  bool loop = true;
  auto *l = args.Find("loop");
  if (l && l->IsBool())
    loop = l->GetBool();

  std::string resolve_err;
  godot::Ref<godot::SpriteFrames> sf = resolve_spriteframes(name, resolve_err);
  if (sf.is_null())
    return error(resolve_err);

  godot::StringName anim(animation.c_str());
  sf->add_animation(anim);
  sf->set_animation_speed(anim, fps);
  sf->set_animation_loop(anim, loop);

  JV r(JV::object_tag);
  r["result"] = JV("ok");
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "spriteframes_add_animation completed");
  return r;
}

JV handle_add_frame(const JV &args) {
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "spriteframes_add_frame called");

  auto *n = args.Find("name");
  if (!n || !n->IsString() || n->GetString().empty())
    return error("missing required parameter: name");
  std::string name = n->GetString();

  auto *a = args.Find("animation");
  if (!a || !a->IsString() || a->GetString().empty())
    return error("missing required parameter: animation");
  std::string animation = a->GetString();

  auto *t = args.Find("texture");
  if (!t || !t->IsString() || t->GetString().empty())
    return error("missing required parameter: texture");
  std::string texture = t->GetString();

  float duration = 1.0f;
  auto *d = args.Find("duration");
  if (d && d->IsInt())
    duration = static_cast<float>(d->GetInt());
  else if (d && d->IsDouble())
    duration = static_cast<float>(d->GetDouble());

  int hframes = 1;
  auto *hf = args.Find("hframes");
  if (hf && hf->IsInt())
    hframes = static_cast<int>(hf->GetInt());
  else if (hf)
    return error("invalid parameter: hframes must be a positive integer");
  if (hframes < 1)
    return error("invalid parameter: hframes must be a positive integer");

  int vframes = 1;
  auto *vf = args.Find("vframes");
  if (vf && vf->IsInt())
    vframes = static_cast<int>(vf->GetInt());
  else if (vf)
    return error("invalid parameter: vframes must be a positive integer");
  if (vframes < 1)
    return error("invalid parameter: vframes must be a positive integer");

  std::string resolve_err;
  godot::Ref<godot::SpriteFrames> sf = resolve_spriteframes(name, resolve_err);
  if (sf.is_null())
    return error(resolve_err);

  godot::String tex_path(texture.c_str());
  if (!godot::FileAccess::file_exists(tex_path))
    return util::error_detail("file does not exist", texture,
                              "an existing file path",
                              "check the disk directory structure; docs paths "
                              "may be relative to the wrong folder");
  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader)
    return error("ResourceLoader not available");
  godot::Ref<godot::Resource> tex_res = loader->load(tex_path);
  if (tex_res.is_null())
    return util::error_detail(
        "file exists but failed to load (not imported or wrong type)", texture,
        "an importable resource of type Texture2D",
        "reimport the file or check the file format");
  godot::Ref<godot::Texture2D> tex = tex_res;
  if (tex.is_null())
    return error("loaded resource is not a Texture2D: " + texture);

  godot::StringName anim(animation.c_str());
  if (!sf->has_animation(anim))
    return error("animation not found: " + animation +
                 " (add it with spriteframes_add_animation first)");

  int added_frames = 0;
  if (hframes * vframes > 1) {
    int fr_w = tex->get_width() / hframes;
    int fr_h = tex->get_height() / vframes;
    for (int y = 0; y < vframes; ++y) {
      for (int x = 0; x < hframes; ++x) {
        godot::Ref<godot::AtlasTexture> atlas;
        atlas.instantiate();
        atlas->set_atlas(tex);
        atlas->set_region(godot::Rect2(x * fr_w, y * fr_h, fr_w, fr_h));
        sf->add_frame(anim, atlas, duration);
        ++added_frames;
      }
    }
  } else {
    sf->add_frame(anim, tex, duration);
    added_frames = 1;
  }

  JV r(JV::object_tag);
  r["result"] = JV("ok");
  r["frames"] = JV(static_cast<int64_t>(sf->get_frame_count(anim)));
  r["added_frames"] = JV(static_cast<int64_t>(added_frames));
  LogSystem::instance().log(LogLevel::Info, LogCategory::Tools,
                            "spriteframes_add_frame completed");
  return r;
}

} // namespace spriteframes_ops
} // namespace godot_self_driving
