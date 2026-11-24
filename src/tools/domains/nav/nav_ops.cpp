#include "tools/domains/nav/nav_ops.hpp"
#include "tools/domains/nav/nav_budget.hpp"
#include "core/observability/log_system.hpp"
#include "util/error_util.hpp"
#include "util/json_godot.hpp"
#include "util/scene_path.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/a_star2d.hpp>
#include <godot_cpp/classes/a_star3d.hpp>
#include <godot_cpp/classes/a_star_grid2d.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/navigation_mesh.hpp>
#include <godot_cpp/classes/navigation_mesh_generator.hpp>
#include <godot_cpp/classes/navigation_path_query_parameters2d.hpp>
#include <godot_cpp/classes/navigation_path_query_parameters3d.hpp>
#include <godot_cpp/classes/navigation_path_query_result2d.hpp>
#include <godot_cpp/classes/navigation_path_query_result3d.hpp>
#include <godot_cpp/classes/navigation_polygon.hpp>
#include <godot_cpp/classes/navigation_server2d.hpp>
#include <godot_cpp/classes/navigation_server3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <cmath>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <mcp/JsonValue.hpp>
#include <chrono>
#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>

namespace godot_autopilot {
namespace nav_ops {

using JV = mcp::JsonValue;

namespace {

constexpr uint32_t kDefaultNavigationLayers = 1;
constexpr int64_t kBakeDefaultTimeoutMs = 30000;
constexpr double kDefaultWeightScale = 1.0;

JV nav_error(const std::string &message, const std::string &code = "") {
  JV error = util::error_json(message);
  if (!code.empty()) {
    error["code"] = JV(code);
  }
  return error;
}

JV nav_mode_error(const std::string &mode, const std::string &parameter) {
  return util::error_json("missing required parameter for mode '" + mode +
                          "': " + parameter);
}

JV nav_action_error(const std::string &action, const std::string &parameter) {
  return util::error_json("missing required parameter for action '" + action +
                          "': " + parameter);
}

bool nav_read_map(const JV &args, const std::string &mode, godot::RID *map,
                  JV *error) {
  const JV *value = args.Find("map_rid");
  if (value == nullptr || !value->IsInt()) {
    *error = nav_mode_error(mode, "map_rid");
    return false;
  }
  *map = util::rid_from_json(*value);
  return true;
}

bool nav_read_object(const JV &args, const char *key, const std::string &mode,
                     const JV **out, JV *error) {
  const JV *value = args.Find(key);
  if (value == nullptr || !value->IsObject()) {
    *error = nav_mode_error(mode, key);
    return false;
  }
  *out = value;
  return true;
}

bool nav_read_rid_array(const JV &args, const char *key,
                        godot::TypedArray<godot::RID> *out,
                        std::string *error) {
  const JV *value = args.Find(key);
  if (value == nullptr) {
    return true;
  }
  if (!value->IsArray()) {
    *error = std::string("parameter '") + key +
             "' must be an array of RID integers";
    return false;
  }
  for (const JV &item : value->GetArray()) {
    if (!item.IsInt()) {
      *error = std::string("parameter '") + key +
               "' must be an array of RID integers";
      return false;
    }
    out->append(util::rid_from_json(item));
  }
  return true;
}

template <typename T>
godot::Ref<T> nav_instantiate(const char *class_name) {
  auto *class_db = godot::ClassDBSingleton::get_singleton();
  if (class_db == nullptr) {
    return godot::Ref<T>();
  }
  godot::Variant value =
      class_db->instantiate(godot::StringName(class_name));
  if (value.get_type() == godot::Variant::NIL) {
    return godot::Ref<T>();
  }
  return godot::Ref<T>(value);
}

JV nav_rid_array_json(const godot::TypedArray<godot::RID> &rids) {
  JV out(JV::array_tag);
  for (int64_t i = 0; i < rids.size(); i++) {
    const godot::RID rid = rids[i];
    out.PushBack(JV(static_cast<int64_t>(rid.get_id())));
  }
  return out;
}

JV vec2i_to_json(const godot::Vector2i &value) {
  return JV(JV::Object{{"x", JV(static_cast<int64_t>(value.x))},
                       {"y", JV(static_cast<int64_t>(value.y))}});
}

JV rect2i_to_json(const godot::Rect2i &value) {
  return JV(JV::Object{{"x", JV(static_cast<int64_t>(value.position.x))},
                       {"y", JV(static_cast<int64_t>(value.position.y))},
                       {"width", JV(static_cast<int64_t>(value.size.x))},
                       {"height", JV(static_cast<int64_t>(value.size.y))}});
}

bool nav_read_int32(const JV *value, int32_t *out) {
  if (value == nullptr || !value->IsNumber()) {
    return false;
  }
  const double raw = util::json_number(value, 0.0);
  if (!std::isfinite(raw) || raw < -2147483648.0 || raw > 2147483647.0 ||
      std::floor(raw) != raw) {
    return false;
  }
  *out = static_cast<int32_t>(raw);
  return true;
}

bool nav_read_bounded_dimension(const JV *value, int32_t *out) {
  if (value == nullptr || !value->IsNumber()) {
    return false;
  }
  const double raw = util::json_number(value, 0.0);
  const nav_budget::GridExtentVerdict verdict =
      nav_budget::validate_grid_extent(raw, 1.0);
  if (verdict.failure != nav_budget::GridExtentFailure::kNone) {
    return false;
  }
  *out = verdict.width;
  return true;
}

bool nav_read_vec2i(const JV &value, godot::Vector2i *out) {
  if (!value.IsObject()) {
    return false;
  }
  return nav_read_int32(value.Find("x"), &out->x) &&
         nav_read_int32(value.Find("y"), &out->y);
}

bool nav_read_rect2i(const JV &value, godot::Rect2i *out) {
  if (!value.IsObject()) {
    return false;
  }
  int32_t width = 0;
  int32_t height = 0;
  if (!nav_read_int32(value.Find("x"), &out->position.x) ||
      !nav_read_int32(value.Find("y"), &out->position.y) ||
      !nav_read_bounded_dimension(value.Find("width"), &width) ||
      !nav_read_bounded_dimension(value.Find("height"), &height)) {
    return false;
  }
  const nav_budget::GridExtentVerdict verdict =
      nav_budget::validate_grid_extent(width, height);
  if (verdict.failure != nav_budget::GridExtentFailure::kNone) {
    return false;
  }
  out->size.x = verdict.width;
  out->size.y = verdict.height;
  return true;
}

bool nav_read_postprocessing(const JV &args, int64_t dimension,
                             godot::Ref<godot::NavigationPathQueryParameters2D>
                                 *parameters_2d,
                             godot::Ref<godot::NavigationPathQueryParameters3D>
                                 *parameters_3d,
                             JV *error) {
  const JV *value = args.Find("path_postprocessing");
  if (value == nullptr) {
    return true;
  }
  if (!value->IsString()) {
    *error = util::error_json(
        "parameter 'path_postprocessing' must be one of: "
        "corridorfunnel, edgecentered, none");
    return false;
  }
  const std::string mode = value->GetString();
  if (dimension == 2) {
    if (mode == "corridorfunnel") {
      (*parameters_2d)->set_path_postprocessing(
          godot::NavigationPathQueryParameters2D::PATH_POSTPROCESSING_CORRIDORFUNNEL);
    } else if (mode == "edgecentered") {
      (*parameters_2d)->set_path_postprocessing(
          godot::NavigationPathQueryParameters2D::PATH_POSTPROCESSING_EDGECENTERED);
    } else if (mode == "none") {
      (*parameters_2d)->set_path_postprocessing(
          godot::NavigationPathQueryParameters2D::PATH_POSTPROCESSING_NONE);
    } else {
      *error = util::error_json(
          "unknown path_postprocessing '" + mode +
          "': expected corridorfunnel, edgecentered or none");
      return false;
    }
    return true;
  }
  if (mode == "corridorfunnel") {
    (*parameters_3d)->set_path_postprocessing(
        godot::NavigationPathQueryParameters3D::PATH_POSTPROCESSING_CORRIDORFUNNEL);
  } else if (mode == "edgecentered") {
    (*parameters_3d)->set_path_postprocessing(
        godot::NavigationPathQueryParameters3D::PATH_POSTPROCESSING_EDGECENTERED);
  } else if (mode == "none") {
    (*parameters_3d)->set_path_postprocessing(
        godot::NavigationPathQueryParameters3D::PATH_POSTPROCESSING_NONE);
  } else {
    *error = util::error_json("unknown path_postprocessing '" + mode +
                              "': expected corridorfunnel, edgecentered or "
                              "none");
    return false;
  }
  return true;
}

struct AstarEntry {
  godot::Ref<godot::RefCounted> object;
  std::string kind;
};

std::unordered_map<int64_t, AstarEntry> &astar_entries() {
  static std::unordered_map<int64_t, AstarEntry> entries;
  return entries;
}

int64_t astar_next_handle() {
  static int64_t next = 1;
  return next++;
}

AstarEntry *astar_lookup(const JV &args, const std::string &action, JV *error) {
  const JV *handle = args.Find("handle");
  if (handle == nullptr || !handle->IsInt()) {
    *error = nav_action_error(action, "handle");
    return nullptr;
  }
  auto it = astar_entries().find(handle->GetInt());
  if (it == astar_entries().end()) {
    *error = util::error_json(
        "unknown astar handle " + std::to_string(handle->GetInt()) +
        " — create one first with action=create");
    return nullptr;
  }
  return &it->second;
}

bool astar_require_kind(const AstarEntry &entry, const std::string &kind,
                        const std::string &action, JV *error) {
  if (entry.kind == kind) {
    return true;
  }
  *error = util::error_json("action '" + action + "' requires kind=" + kind +
                            " but the handle was created with kind=" +
                            entry.kind);
  return false;
}

bool astar_read_cell(const JV &value, godot::Vector2i *out) {
  return nav_read_vec2i(value, out);
}

bool astar_read_point_id(const JV &value, int64_t *out) {
  if (!value.IsInt()) {
    return false;
  }
  *out = value.GetInt();
  return true;
}

JV astar_missing_point(const std::string &action, const char *parameter) {
  return util::error_json("parameter '" + std::string(parameter) +
                          "' must be an integer point id for action '" +
                          action + "'");
}

JV astar_missing_cell(const std::string &action, const char *parameter) {
  return util::error_json("parameter '" + std::string(parameter) +
                          "' must be a {x, y} cell id object for action '" +
                          action + "'");
}

JV astar_path_json(const godot::PackedVector2Array &path) {
  JV points(JV::array_tag);
  for (int64_t i = 0; i < path.size(); i++) {
    points.PushBack(util::vec2_to_json(path[i]));
  }
  JV out(JV::object_tag);
  out["path"] = std::move(points);
  out["count"] = JV(static_cast<int64_t>(path.size()));
  return out;
}

JV astar_path_json(const godot::PackedVector3Array &path) {
  JV points(JV::array_tag);
  for (int64_t i = 0; i < path.size(); i++) {
    points.PushBack(util::vec3_to_json(path[i]));
  }
  JV out(JV::object_tag);
  out["path"] = std::move(points);
  out["count"] = JV(static_cast<int64_t>(path.size()));
  return out;
}

} // namespace

JV handle_2d_map_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_nav_2d_map called");

  auto *ns = godot::NavigationServer2D::get_singleton();
  if (!ns) {
    JV r(JV::object_tag);
    r["error"] = JV("NavigationServer2D not available");
    return r;
  }

  godot::RID map = ns->map_create();
  auto *act = args.Find("active");
  bool active = act ? act->GetBool() : false;
  ns->map_set_active(map, active);

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_nav_2d_map completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(map);
  return r;
}

JV handle_2d_region_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_nav_2d_region called");

  auto *it_map = args.Find("map_rid");
  if (!it_map || !it_map->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: map_rid");
    return r;
  }

  auto *ns = godot::NavigationServer2D::get_singleton();
  if (!ns) {
    JV r(JV::object_tag);
    r["error"] = JV("NavigationServer2D not available");
    return r;
  }

  godot::RID map = util::rid_from_json(*it_map);
  godot::RID region = ns->region_create();
  ns->region_set_map(region, map);

  if (args.Contains("enabled")) {
    ns->region_set_enabled(region, args["enabled"].GetBool());
  }
  if (args.Contains("navigation_layers")) {
    ns->region_set_navigation_layers(
        region, static_cast<uint32_t>(args["navigation_layers"].GetInt()));
  }

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_nav_2d_region completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(region);
  return r;
}

JV handle_2d_path_query(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "get_nav_2d_map_path called");

  auto *it_map = args.Find("map_rid");
  auto *it_origin = args.Find("origin");
  auto *it_dest = args.Find("destination");
  if (!it_map || !it_map->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: map_rid");
    return r;
  }
  if (!it_origin || !it_origin->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: origin");
    return r;
  }
  if (!it_dest || !it_dest->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: destination");
    return r;
  }

  auto *ns = godot::NavigationServer2D::get_singleton();
  if (!ns) {
    JV r(JV::object_tag);
    r["error"] = JV("NavigationServer2D not available");
    return r;
  }

  godot::RID map = util::rid_from_json(*it_map);
  godot::Vector2 origin = util::json_to_vec2(*it_origin);
  godot::Vector2 dest = util::json_to_vec2(*it_dest);
  auto *opt = args.Find("optimize");
  bool optimize = opt ? opt->GetBool() : true;
  auto *nl = args.Find("navigation_layers");
  uint32_t nav_layers = static_cast<uint32_t>(nl ? nl->GetInt() : 1);

  godot::PackedVector2Array path =
      ns->map_get_path(map, origin, dest, optimize, nav_layers);
  JV arr(JV::array_tag);
  for (int i = 0; i < path.size(); i++) {
    arr.PushBack(util::vec2_to_json(path[i]));
  }

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "get_nav_2d_map_path completed");
  JV r(JV::object_tag);
  r["result"] = JV(JV::Object{{"path", std::move(arr)}});
  return r;
}

JV handle_2d_agent_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_nav_2d_agent called");

  auto *it_map = args.Find("map_rid");
  auto *it_pos = args.Find("position");
  if (!it_map || !it_map->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: map_rid");
    return r;
  }
  if (!it_pos || !it_pos->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: position");
    return r;
  }

  auto *ns = godot::NavigationServer2D::get_singleton();
  if (!ns) {
    JV r(JV::object_tag);
    r["error"] = JV("NavigationServer2D not available");
    return r;
  }

  godot::RID map = util::rid_from_json(*it_map);
  godot::RID agent = ns->agent_create();
  ns->agent_set_map(agent, map);
  ns->agent_set_position(agent, util::json_to_vec2(*it_pos));

  if (args.Contains("radius"))
    ns->agent_set_radius(
        agent,
        static_cast<float>(util::json_number(args.Find("radius"), 0.0)));
  if (args.Contains("max_speed"))
    ns->agent_set_max_speed(
        agent,
        static_cast<float>(util::json_number(args.Find("max_speed"), 0.0)));
  if (args.Contains("avoidance_enabled"))
    ns->agent_set_avoidance_enabled(agent, args["avoidance_enabled"].GetBool());

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_nav_2d_agent completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(agent);
  return r;
}

JV handle_2d_agent_set_target(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_nav_2d_agent_velocity called");

  auto *it_agent = args.Find("agent_rid");
  auto *it_vel = args.Find("velocity");
  if (!it_agent || !it_agent->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: agent_rid");
    return r;
  }
  if (!it_vel || !it_vel->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: velocity");
    return r;
  }

  auto *ns = godot::NavigationServer2D::get_singleton();
  if (!ns) {
    JV r(JV::object_tag);
    r["error"] = JV("NavigationServer2D not available");
    return r;
  }

  ns->agent_set_velocity(util::rid_from_json(*it_agent),
                         util::json_to_vec2(*it_vel));

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_nav_2d_agent_velocity completed");
  JV r(JV::object_tag);
  r["result"] = JV(true);
  return r;
}

JV handle_3d_map_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_nav_3d_map called");

  auto *ns = godot::NavigationServer3D::get_singleton();
  if (!ns) {
    JV r(JV::object_tag);
    r["error"] = JV("NavigationServer3D not available");
    return r;
  }

  godot::RID map = ns->map_create();
  auto *act = args.Find("active");
  bool active = act ? act->GetBool() : false;
  ns->map_set_active(map, active);

  if (args.Contains("cell_size"))
    ns->map_set_cell_size(
        map, static_cast<float>(util::json_number(args.Find("cell_size"),
                                                  0.0)));
  if (args.Contains("cell_height"))
    ns->map_set_cell_height(
        map, static_cast<float>(util::json_number(args.Find("cell_height"),
                                                  0.0)));
  if (args.Contains("up"))
    ns->map_set_up(map, util::json_to_vec3(args["up"]));

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_nav_3d_map completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(map);
  return r;
}

JV handle_3d_region_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_nav_3d_region called");

  auto *it_map = args.Find("map_rid");
  if (!it_map || !it_map->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: map_rid");
    return r;
  }

  auto *ns = godot::NavigationServer3D::get_singleton();
  if (!ns) {
    JV r(JV::object_tag);
    r["error"] = JV("NavigationServer3D not available");
    return r;
  }

  godot::RID map = util::rid_from_json(*it_map);
  godot::RID region = ns->region_create();
  ns->region_set_map(region, map);

  if (args.Contains("enabled"))
    ns->region_set_enabled(region, args["enabled"].GetBool());
  if (args.Contains("navigation_layers"))
    ns->region_set_navigation_layers(
        region, static_cast<uint32_t>(args["navigation_layers"].GetInt()));

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_nav_3d_region completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(region);
  return r;
}

JV handle_3d_path_query(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "get_nav_3d_map_path called");

  auto *it_map = args.Find("map_rid");
  auto *it_origin = args.Find("origin");
  auto *it_dest = args.Find("destination");
  if (!it_map || !it_map->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: map_rid");
    return r;
  }
  if (!it_origin || !it_origin->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: origin");
    return r;
  }
  if (!it_dest || !it_dest->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: destination");
    return r;
  }

  auto *ns = godot::NavigationServer3D::get_singleton();
  if (!ns) {
    JV r(JV::object_tag);
    r["error"] = JV("NavigationServer3D not available");
    return r;
  }

  godot::RID map = util::rid_from_json(*it_map);
  godot::Vector3 origin = util::json_to_vec3(*it_origin);
  godot::Vector3 dest = util::json_to_vec3(*it_dest);
  auto *opt = args.Find("optimize");
  bool optimize = opt ? opt->GetBool() : true;
  auto *nl = args.Find("navigation_layers");
  uint32_t nav_layers = static_cast<uint32_t>(nl ? nl->GetInt() : 1);

  godot::PackedVector3Array path =
      ns->map_get_path(map, origin, dest, optimize, nav_layers);
  JV arr(JV::array_tag);
  for (int i = 0; i < path.size(); i++) {
    arr.PushBack(util::vec3_to_json(path[i]));
  }

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "get_nav_3d_map_path completed");
  JV r(JV::object_tag);
  r["result"] = JV(JV::Object{{"path", std::move(arr)}});
  return r;
}

JV handle_3d_path_query_segment(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "get_nav_3d_map_closest_point_to_segment called");

  auto *it_map = args.Find("map_rid");
  auto *it_start = args.Find("start");
  auto *it_end = args.Find("end");
  if (!it_map || !it_map->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: map_rid");
    return r;
  }
  if (!it_start || !it_start->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: start");
    return r;
  }
  if (!it_end || !it_end->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: end");
    return r;
  }

  auto *ns = godot::NavigationServer3D::get_singleton();
  if (!ns) {
    JV r(JV::object_tag);
    r["error"] = JV("NavigationServer3D not available");
    return r;
  }

  godot::RID map = util::rid_from_json(*it_map);
  auto *uc = args.Find("use_collision");
  bool use_collision = uc ? uc->GetBool() : false;
  godot::Vector3 point = ns->map_get_closest_point_to_segment(
      map, util::json_to_vec3(*it_start), util::json_to_vec3(*it_end), use_collision);

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "get_nav_3d_map_closest_point_to_segment completed");
  JV r(JV::object_tag);
  r["result"] = JV(JV::Object{{"point", util::vec3_to_json(point)}});
  return r;
}

JV handle_3d_agent_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_nav_3d_agent called");

  auto *it_map = args.Find("map_rid");
  auto *it_pos = args.Find("position");
  if (!it_map || !it_map->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: map_rid");
    return r;
  }
  if (!it_pos || !it_pos->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: position");
    return r;
  }

  auto *ns = godot::NavigationServer3D::get_singleton();
  if (!ns) {
    JV r(JV::object_tag);
    r["error"] = JV("NavigationServer3D not available");
    return r;
  }

  godot::RID map = util::rid_from_json(*it_map);
  godot::RID agent = ns->agent_create();
  ns->agent_set_map(agent, map);
  ns->agent_set_position(agent, util::json_to_vec3(*it_pos));

  if (args.Contains("radius"))
    ns->agent_set_radius(
        agent,
        static_cast<float>(util::json_number(args.Find("radius"), 0.0)));
  if (args.Contains("height"))
    ns->agent_set_height(
        agent,
        static_cast<float>(util::json_number(args.Find("height"), 0.0)));
  if (args.Contains("max_speed"))
    ns->agent_set_max_speed(
        agent,
        static_cast<float>(util::json_number(args.Find("max_speed"), 0.0)));
  if (args.Contains("use_3d_avoidance"))
    ns->agent_set_use_3d_avoidance(agent, args["use_3d_avoidance"].GetBool());

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_nav_3d_agent completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(agent);
  return r;
}

JV handle_3d_agent_set_velocity(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_nav_3d_agent_velocity called");

  auto *it_agent = args.Find("agent_rid");
  auto *it_vel = args.Find("velocity");
  if (!it_agent || !it_agent->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: agent_rid");
    return r;
  }
  if (!it_vel || !it_vel->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: velocity");
    return r;
  }

  auto *ns = godot::NavigationServer3D::get_singleton();
  if (!ns) {
    JV r(JV::object_tag);
    r["error"] = JV("NavigationServer3D not available");
    return r;
  }

  ns->agent_set_velocity(util::rid_from_json(*it_agent),
                         util::json_to_vec3(*it_vel));

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_nav_3d_agent_velocity completed");
  JV r(JV::object_tag);
  r["result"] = JV(true);
  return r;
}

JV handle_3d_agent_get_next_path(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "get_nav_3d_agent_state called");

  auto *it_agent = args.Find("agent_rid");
  if (!it_agent || !it_agent->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: agent_rid");
    return r;
  }

  auto *ns = godot::NavigationServer3D::get_singleton();
  if (!ns) {
    JV r(JV::object_tag);
    r["error"] = JV("NavigationServer3D not available");
    return r;
  }

  godot::RID agent = util::rid_from_json(*it_agent);
  godot::Vector3 pos = ns->agent_get_position(agent);
  godot::Vector3 vel = ns->agent_get_velocity(agent);

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "get_nav_3d_agent_state completed");
  JV r(JV::object_tag);
  JV result(JV::object_tag);
  result["position"] = util::vec3_to_json(pos);
  result["velocity"] = util::vec3_to_json(vel);
  r["result"] = std::move(result);
  return r;
}

JV handle_3d_map_set_cell_size(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_nav_3d_map_cell_size called");

  auto *it_map = args.Find("map_rid");
  auto *it_cell = args.Find("cell_size");
  if (!it_map || !it_map->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: map_rid");
    return r;
  }
  if (!it_cell || !it_cell->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: cell_size");
    return r;
  }

  auto *ns = godot::NavigationServer3D::get_singleton();
  if (!ns) {
    JV r(JV::object_tag);
    r["error"] = JV("NavigationServer3D not available");
    return r;
  }

  ns->map_set_cell_size(util::rid_from_json(*it_map),
                        static_cast<float>(util::json_number(it_cell, 0.0)));

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_nav_3d_map_cell_size completed");
  JV r(JV::object_tag);
  r["result"] = JV(true);
  return r;
}

JV handle_3d_region_set_nav_mesh(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_nav_3d_region_navigation_mesh called");

  auto *it_region = args.Find("region_rid");
  auto *it_mesh = args.Find("mesh_path");
  if (!it_region || !it_region->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: region_rid");
    return r;
  }
  if (!it_mesh || !it_mesh->IsString()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: mesh_path");
    return r;
  }

  auto *ns = godot::NavigationServer3D::get_singleton();
  if (!ns) {
    JV r(JV::object_tag);
    r["error"] = JV("NavigationServer3D not available");
    return r;
  }

  auto *loader = godot::ResourceLoader::get_singleton();
  if (!loader) {
    JV r(JV::object_tag);
    r["error"] = JV("ResourceLoader not available");
    return r;
  }

  std::string mesh_path = it_mesh->GetString();
  godot::Ref<godot::Resource> res =
      loader->load(godot::String(mesh_path.c_str()));
  if (res.is_null()) {
    JV r(JV::object_tag);
    r["error"] = JV("failed to load navigation mesh: " + mesh_path);
    return r;
  }

  godot::Ref<godot::NavigationMesh> mesh = res;
  if (mesh.is_null()) {
    JV r(JV::object_tag);
    r["error"] = JV("resource is not a NavigationMesh: " + mesh_path);
    return r;
  }

  ns->region_set_navigation_mesh(util::rid_from_json(*it_region), mesh);

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "set_nav_3d_region_navigation_mesh completed");
  JV r(JV::object_tag);
  r["result"] = JV(true);
  return r;
}

JV handle_3d_obstacle_create(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_nav_3d_obstacle called");

  auto *it_map = args.Find("map_rid");
  auto *it_pos = args.Find("position");
  if (!it_map || !it_map->IsNumber()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: map_rid");
    return r;
  }
  if (!it_pos || !it_pos->IsObject()) {
    JV r(JV::object_tag);
    r["error"] = JV("missing required parameter: position");
    return r;
  }

  auto *ns = godot::NavigationServer3D::get_singleton();
  if (!ns) {
    JV r(JV::object_tag);
    r["error"] = JV("NavigationServer3D not available");
    return r;
  }

  godot::RID map = util::rid_from_json(*it_map);
  godot::RID obstacle = ns->obstacle_create();
  ns->obstacle_set_map(obstacle, map);
  ns->obstacle_set_position(obstacle, util::json_to_vec3(*it_pos));

  if (args.Contains("radius"))
    ns->obstacle_set_radius(
        obstacle,
        static_cast<float>(util::json_number(args.Find("radius"), 0.0)));
  if (args.Contains("height"))
    ns->obstacle_set_height(
        obstacle,
        static_cast<float>(util::json_number(args.Find("height"), 0.0)));

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "create_nav_3d_obstacle completed");
  JV r(JV::object_tag);
  r["result"] = util::rid_to_json(obstacle);
  return r;
}

namespace {

JV nav_query_path_2d(const JV &args) {
  auto *ns = godot::NavigationServer2D::get_singleton();
  if (ns == nullptr) {
    return util::error_json("NavigationServer2D not available");
  }
  godot::RID map;
  JV error;
  if (!nav_read_map(args, "path_query", &map, &error)) {
    return error;
  }
  const JV *origin = nullptr;
  const JV *destination = nullptr;
  if (!nav_read_object(args, "origin", "path_query", &origin, &error)) {
    return error;
  }
  if (!nav_read_object(args, "destination", "path_query", &destination,
                       &error)) {
    return error;
  }
  godot::Ref<godot::NavigationPathQueryParameters2D> parameters =
      nav_instantiate<godot::NavigationPathQueryParameters2D>(
          "NavigationPathQueryParameters2D");
  godot::Ref<godot::NavigationPathQueryResult2D> query_result =
      nav_instantiate<godot::NavigationPathQueryResult2D>(
          "NavigationPathQueryResult2D");
  if (parameters.is_null() || query_result.is_null()) {
    return util::error_json(
        "failed to instantiate NavigationPathQueryParameters2D or "
        "NavigationPathQueryResult2D");
  }
  parameters->set_map(map);
  parameters->set_start_position(util::json_to_vec2(*origin));
  parameters->set_target_position(util::json_to_vec2(*destination));
  const JV *layers = args.Find("navigation_layers");
  if (layers != nullptr && !layers->IsInt()) {
    return util::error_json(
        "parameter 'navigation_layers' must be an integer");
  }
  parameters->set_navigation_layers(
      layers != nullptr ? static_cast<uint32_t>(layers->GetInt())
                        : kDefaultNavigationLayers);
  const JV *simplify = args.Find("simplify");
  if (simplify != nullptr && !simplify->IsBool()) {
    return util::error_json("parameter 'simplify' must be a boolean");
  }
  parameters->set_simplify_path(simplify == nullptr || simplify->GetBool());
  const JV *epsilon = args.Find("simplify_epsilon");
  if (epsilon != nullptr && !epsilon->IsNumber()) {
    return util::error_json("parameter 'simplify_epsilon' must be a number");
  }
  if (epsilon != nullptr) {
    parameters->set_simplify_epsilon(
        static_cast<float>(util::json_number(epsilon, 0.0)));
  }
  const JV *metadata = args.Find("metadata");
  if (metadata != nullptr && !metadata->IsBool()) {
    return util::error_json("parameter 'metadata' must be a boolean");
  }
  const bool include_metadata =
      metadata != nullptr && metadata->GetBool();
  parameters->set_metadata_flags(
      include_metadata
          ? godot::NavigationPathQueryParameters2D::PATH_METADATA_INCLUDE_ALL
          : godot::NavigationPathQueryParameters2D::PATH_METADATA_INCLUDE_NONE);
  godot::TypedArray<godot::RID> excluded;
  std::string rid_error;
  if (!nav_read_rid_array(args, "excluded_regions", &excluded, &rid_error)) {
    return util::error_json(rid_error);
  }
  parameters->set_excluded_regions(excluded);
  if (!nav_read_postprocessing(args, 2, &parameters, nullptr, &error)) {
    return error;
  }
  ns->query_path(parameters, query_result);
  godot::PackedVector2Array points = query_result->get_path();
  JV path(JV::array_tag);
  for (int64_t i = 0; i < points.size(); i++) {
    path.PushBack(util::vec2_to_json(points[i]));
  }
  JV out(JV::object_tag);
  out["path"] = std::move(path);
  out["path_length"] =
      JV(static_cast<double>(query_result->get_path_length()));
  if (include_metadata) {
    godot::PackedInt32Array types = query_result->get_path_types();
    JV types_json(JV::array_tag);
    for (int64_t i = 0; i < types.size(); i++) {
      types_json.PushBack(JV(static_cast<int64_t>(types[i])));
    }
    out["path_types"] = std::move(types_json);
    out["path_rids"] =
        nav_rid_array_json(query_result->get_path_rids());
    godot::PackedInt64Array owners = query_result->get_path_owner_ids();
    JV owners_json(JV::array_tag);
    for (int64_t i = 0; i < owners.size(); i++) {
      owners_json.PushBack(JV(owners[i]));
    }
    out["path_owner_ids"] = std::move(owners_json);
  }
  JV r(JV::object_tag);
  r["result"] = std::move(out);
  return r;
}

JV nav_query_path_3d(const JV &args) {
  auto *ns = godot::NavigationServer3D::get_singleton();
  if (ns == nullptr) {
    return util::error_json("NavigationServer3D not available");
  }
  godot::RID map;
  JV error;
  if (!nav_read_map(args, "path_query", &map, &error)) {
    return error;
  }
  const JV *origin = nullptr;
  const JV *destination = nullptr;
  if (!nav_read_object(args, "origin", "path_query", &origin, &error)) {
    return error;
  }
  if (!nav_read_object(args, "destination", "path_query", &destination,
                       &error)) {
    return error;
  }
  godot::Ref<godot::NavigationPathQueryParameters3D> parameters =
      nav_instantiate<godot::NavigationPathQueryParameters3D>(
          "NavigationPathQueryParameters3D");
  godot::Ref<godot::NavigationPathQueryResult3D> query_result =
      nav_instantiate<godot::NavigationPathQueryResult3D>(
          "NavigationPathQueryResult3D");
  if (parameters.is_null() || query_result.is_null()) {
    return util::error_json(
        "failed to instantiate NavigationPathQueryParameters3D or "
        "NavigationPathQueryResult3D");
  }
  parameters->set_map(map);
  parameters->set_start_position(util::json_to_vec3(*origin));
  parameters->set_target_position(util::json_to_vec3(*destination));
  const JV *layers = args.Find("navigation_layers");
  if (layers != nullptr && !layers->IsInt()) {
    return util::error_json(
        "parameter 'navigation_layers' must be an integer");
  }
  parameters->set_navigation_layers(
      layers != nullptr ? static_cast<uint32_t>(layers->GetInt())
                        : kDefaultNavigationLayers);
  const JV *simplify = args.Find("simplify");
  if (simplify != nullptr && !simplify->IsBool()) {
    return util::error_json("parameter 'simplify' must be a boolean");
  }
  parameters->set_simplify_path(simplify == nullptr || simplify->GetBool());
  const JV *epsilon = args.Find("simplify_epsilon");
  if (epsilon != nullptr && !epsilon->IsNumber()) {
    return util::error_json("parameter 'simplify_epsilon' must be a number");
  }
  if (epsilon != nullptr) {
    parameters->set_simplify_epsilon(
        static_cast<float>(util::json_number(epsilon, 0.0)));
  }
  const JV *metadata = args.Find("metadata");
  if (metadata != nullptr && !metadata->IsBool()) {
    return util::error_json("parameter 'metadata' must be a boolean");
  }
  const bool include_metadata =
      metadata != nullptr && metadata->GetBool();
  parameters->set_metadata_flags(
      include_metadata
          ? godot::NavigationPathQueryParameters3D::PATH_METADATA_INCLUDE_ALL
          : godot::NavigationPathQueryParameters3D::PATH_METADATA_INCLUDE_NONE);
  godot::TypedArray<godot::RID> excluded;
  std::string rid_error;
  if (!nav_read_rid_array(args, "excluded_regions", &excluded, &rid_error)) {
    return util::error_json(rid_error);
  }
  parameters->set_excluded_regions(excluded);
  if (!nav_read_postprocessing(args, 3, nullptr, &parameters, &error)) {
    return error;
  }
  ns->query_path(parameters, query_result);
  godot::PackedVector3Array points = query_result->get_path();
  JV path(JV::array_tag);
  for (int64_t i = 0; i < points.size(); i++) {
    path.PushBack(util::vec3_to_json(points[i]));
  }
  JV out(JV::object_tag);
  out["path"] = std::move(path);
  out["path_length"] =
      JV(static_cast<double>(query_result->get_path_length()));
  if (include_metadata) {
    godot::PackedInt32Array types = query_result->get_path_types();
    JV types_json(JV::array_tag);
    for (int64_t i = 0; i < types.size(); i++) {
      types_json.PushBack(JV(static_cast<int64_t>(types[i])));
    }
    out["path_types"] = std::move(types_json);
    out["path_rids"] =
        nav_rid_array_json(query_result->get_path_rids());
    godot::PackedInt64Array owners = query_result->get_path_owner_ids();
    JV owners_json(JV::array_tag);
    for (int64_t i = 0; i < owners.size(); i++) {
      owners_json.PushBack(JV(owners[i]));
    }
    out["path_owner_ids"] = std::move(owners_json);
  }
  JV r(JV::object_tag);
  r["result"] = std::move(out);
  return r;
}

JV nav_query_closest_point(const JV &args, int64_t dimension,
                           bool want_normal) {
  const std::string mode = want_normal ? "closest_point_normal" : "closest_point";
  godot::RID map;
  JV error;
  if (!nav_read_map(args, mode, &map, &error)) {
    return error;
  }
  const JV *point = nullptr;
  if (!nav_read_object(args, "point", mode, &point, &error)) {
    return error;
  }
  JV out(JV::object_tag);
  if (dimension == 2) {
    if (want_normal) {
      return util::error_json(
          "mode 'closest_point_normal' requires dimension=3 — "
          "NavigationServer2D has no closest_point_normal query");
    }
    auto *ns = godot::NavigationServer2D::get_singleton();
    if (ns == nullptr) {
      return util::error_json("NavigationServer2D not available");
    }
    out["point"] =
        util::vec2_to_json(ns->map_get_closest_point(map, util::json_to_vec2(*point)));
  } else {
    auto *ns = godot::NavigationServer3D::get_singleton();
    if (ns == nullptr) {
      return util::error_json("NavigationServer3D not available");
    }
    if (want_normal) {
      out["normal"] = util::vec3_to_json(
          ns->map_get_closest_point_normal(map, util::json_to_vec3(*point)));
    } else {
      out["point"] = util::vec3_to_json(
          ns->map_get_closest_point(map, util::json_to_vec3(*point)));
    }
  }
  JV r(JV::object_tag);
  r["result"] = std::move(out);
  return r;
}

JV nav_query_closest_point_owner(const JV &args, int64_t dimension) {
  godot::RID map;
  JV error;
  if (!nav_read_map(args, "closest_point_owner", &map, &error)) {
    return error;
  }
  const JV *point = nullptr;
  if (!nav_read_object(args, "point", "closest_point_owner", &point, &error)) {
    return error;
  }
  godot::RID owner;
  if (dimension == 2) {
    auto *ns = godot::NavigationServer2D::get_singleton();
    if (ns == nullptr) {
      return util::error_json("NavigationServer2D not available");
    }
    owner = ns->map_get_closest_point_owner(map, util::json_to_vec2(*point));
  } else {
    auto *ns = godot::NavigationServer3D::get_singleton();
    if (ns == nullptr) {
      return util::error_json("NavigationServer3D not available");
    }
    owner = ns->map_get_closest_point_owner(map, util::json_to_vec3(*point));
  }
  JV out(JV::object_tag);
  out["owner_rid"] = JV(static_cast<int64_t>(owner.get_id()));
  out["valid"] = JV(owner.is_valid());
  JV r(JV::object_tag);
  r["result"] = std::move(out);
  return r;
}

JV nav_query_closest_point_to_segment(const JV &args, int64_t dimension) {
  if (dimension == 2) {
    return util::error_json(
        "mode 'closest_point_to_segment' requires dimension=3 — "
        "NavigationServer2D has no segment query");
  }
  auto *ns = godot::NavigationServer3D::get_singleton();
  if (ns == nullptr) {
    return util::error_json("NavigationServer3D not available");
  }
  godot::RID map;
  JV error;
  if (!nav_read_map(args, "closest_point_to_segment", &map, &error)) {
    return error;
  }
  const JV *start = nullptr;
  const JV *end = nullptr;
  if (!nav_read_object(args, "start", "closest_point_to_segment", &start,
                       &error)) {
    return error;
  }
  if (!nav_read_object(args, "end", "closest_point_to_segment", &end, &error)) {
    return error;
  }
  const JV *use_collision = args.Find("use_collision");
  const bool collision =
      use_collision != nullptr && use_collision->IsBool() &&
      use_collision->GetBool();
  godot::Vector3 point = ns->map_get_closest_point_to_segment(
      map, util::json_to_vec3(*start), util::json_to_vec3(*end), collision);
  JV r(JV::object_tag);
  r["result"] = JV(JV::Object{{"point", util::vec3_to_json(point)}});
  return r;
}

JV nav_query_rid_list(const JV &args, int64_t dimension,
                      const std::string &kind) {
  godot::RID map;
  JV error;
  if (!nav_read_map(args, kind, &map, &error)) {
    return error;
  }
  godot::TypedArray<godot::RID> rids;
  if (dimension == 2) {
    auto *ns = godot::NavigationServer2D::get_singleton();
    if (ns == nullptr) {
      return util::error_json("NavigationServer2D not available");
    }
    if (kind == "regions") {
      rids = ns->map_get_regions(map);
    } else if (kind == "agents") {
      rids = ns->map_get_agents(map);
    } else {
      rids = ns->map_get_links(map);
    }
  } else {
    auto *ns = godot::NavigationServer3D::get_singleton();
    if (ns == nullptr) {
      return util::error_json("NavigationServer3D not available");
    }
    if (kind == "regions") {
      rids = ns->map_get_regions(map);
    } else if (kind == "agents") {
      rids = ns->map_get_agents(map);
    } else {
      rids = ns->map_get_links(map);
    }
  }
  JV out(JV::object_tag);
  out[kind] = nav_rid_array_json(rids);
  out["count"] = JV(static_cast<int64_t>(rids.size()));
  JV r(JV::object_tag);
  r["result"] = std::move(out);
  return r;
}

JV nav_query_iteration(const JV &args, int64_t dimension) {
  godot::RID map;
  JV error;
  if (!nav_read_map(args, "iteration", &map, &error)) {
    return error;
  }
  uint32_t iteration = 0;
  if (dimension == 2) {
    auto *ns = godot::NavigationServer2D::get_singleton();
    if (ns == nullptr) {
      return util::error_json("NavigationServer2D not available");
    }
    iteration = ns->map_get_iteration_id(map);
  } else {
    auto *ns = godot::NavigationServer3D::get_singleton();
    if (ns == nullptr) {
      return util::error_json("NavigationServer3D not available");
    }
    iteration = ns->map_get_iteration_id(map);
  }
  JV r(JV::object_tag);
  r["result"] = JV(JV::Object{
      {"iteration_id", JV(static_cast<int64_t>(iteration))}});
  return r;
}

JV nav_query_force_update(const JV &args, int64_t dimension) {
  godot::RID map;
  JV error;
  if (!nav_read_map(args, "force_update", &map, &error)) {
    return error;
  }
  if (dimension == 2) {
    auto *ns = godot::NavigationServer2D::get_singleton();
    if (ns == nullptr) {
      return util::error_json("NavigationServer2D not available");
    }
    ns->map_force_update(map);
  } else {
    auto *ns = godot::NavigationServer3D::get_singleton();
    if (ns == nullptr) {
      return util::error_json("NavigationServer3D not available");
    }
    ns->map_force_update(map);
  }
  JV r(JV::object_tag);
  r["result"] = JV(true);
  return r;
}

JV nav_query_agent_state(const JV &args, int64_t dimension) {
  const JV *agent = args.Find("agent_rid");
  if (agent == nullptr || !agent->IsInt()) {
    return nav_mode_error("agent_state", "agent_rid");
  }
  const godot::RID agent_rid = util::rid_from_json(*agent);
  JV out(JV::object_tag);
  if (dimension == 2) {
    auto *ns = godot::NavigationServer2D::get_singleton();
    if (ns == nullptr) {
      return util::error_json("NavigationServer2D not available");
    }
    out["position"] =
        util::vec2_to_json(ns->agent_get_position(agent_rid));
    out["velocity"] =
        util::vec2_to_json(ns->agent_get_velocity(agent_rid));
  } else {
    auto *ns = godot::NavigationServer3D::get_singleton();
    if (ns == nullptr) {
      return util::error_json("NavigationServer3D not available");
    }
    out["position"] =
        util::vec3_to_json(ns->agent_get_position(agent_rid));
    out["velocity"] =
        util::vec3_to_json(ns->agent_get_velocity(agent_rid));
  }
  JV r(JV::object_tag);
  r["result"] = std::move(out);
  return r;
}

} // namespace

JV handle_nav_query(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "nav_query called");
  const JV *mode_value = args.Find("mode");
  if (mode_value == nullptr || !mode_value->IsString()) {
    return util::error_json("missing required parameter: mode");
  }
  const std::string mode = mode_value->GetString();
  const JV *dimension_value = args.Find("dimension");
  if (dimension_value == nullptr || !dimension_value->IsInt()) {
    return util::error_json(
        "missing required parameter: dimension (2 or 3)");
  }
  const int64_t dimension = dimension_value->GetInt();
  if (dimension != 2 && dimension != 3) {
    return util::error_json("parameter 'dimension' must be 2 or 3");
  }
  JV result;
  if (mode == "path_query") {
    result = dimension == 2 ? nav_query_path_2d(args) : nav_query_path_3d(args);
  } else if (mode == "closest_point") {
    result = nav_query_closest_point(args, dimension, false);
  } else if (mode == "closest_point_normal") {
    result = nav_query_closest_point(args, dimension, true);
  } else if (mode == "closest_point_owner") {
    result = nav_query_closest_point_owner(args, dimension);
  } else if (mode == "closest_point_to_segment") {
    result = nav_query_closest_point_to_segment(args, dimension);
  } else if (mode == "regions" || mode == "agents" || mode == "links") {
    result = nav_query_rid_list(args, dimension, mode);
  } else if (mode == "iteration") {
    result = nav_query_iteration(args, dimension);
  } else if (mode == "force_update") {
    result = nav_query_force_update(args, dimension);
  } else if (mode == "agent_state") {
    result = nav_query_agent_state(args, dimension);
  } else {
    return util::error_json(
        "unknown nav_query mode '" + mode +
        "': expected path_query, closest_point, closest_point_normal, "
        "closest_point_owner, closest_point_to_segment, regions, agents, "
        "links, iteration, force_update or agent_state");
  }
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "nav_query completed: " + mode);
  return result;
}

JV handle_bake_navigation_mesh(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "bake_navigation_mesh called");
  const JV *navmesh_value = args.Find("navmesh_path");
  if (navmesh_value == nullptr || !navmesh_value->IsString() ||
      navmesh_value->GetString().empty()) {
    return util::error_json("missing required parameter: navmesh_path");
  }
  const std::string navmesh_path = navmesh_value->GetString();
  int64_t timeout_ms = kBakeDefaultTimeoutMs;
  const JV *timeout_value = args.Find("timeout_ms");
  if (timeout_value != nullptr) {
    if (!timeout_value->IsInt()) {
      return util::error_json(
          "parameter 'timeout_ms' must be a positive integer");
    }
    timeout_ms = timeout_value->GetInt();
    if (timeout_ms <= 0) {
      return util::error_json(
          "parameter 'timeout_ms' must be a positive integer");
    }
  }
  auto *loader = godot::ResourceLoader::get_singleton();
  if (loader == nullptr) {
    return util::error_json("ResourceLoader not available");
  }
  godot::Ref<godot::Resource> resource =
      loader->load(godot::String::utf8(navmesh_path.c_str()));
  if (resource.is_null()) {
    return nav_error("failed to load navigation mesh: " + navmesh_path,
                     "navigation_mesh_load_failed");
  }
  godot::Ref<godot::NavigationMesh> mesh = resource;
  if (mesh.is_null()) {
    return nav_error("resource is not a NavigationMesh: " + navmesh_path,
                     "navigation_mesh_invalid");
  }
  auto *editor = godot::EditorInterface::get_singleton();
  godot::Node *scene_root =
      editor != nullptr ? editor->get_edited_scene_root() : nullptr;
  if (scene_root == nullptr) {
    return util::error_json(
        "no edited scene root is available — open a scene before baking a "
        "navigation mesh");
  }
  godot::Node *root = scene_root;
  const JV *root_path_value = args.Find("root_path");
  if (root_path_value != nullptr) {
    if (!root_path_value->IsString()) {
      return util::error_json("parameter 'root_path' must be a string");
    }
    std::string hint;
    root = util::resolve_scene_node(root_path_value->GetString(), scene_root,
                                    &hint);
    if (root == nullptr) {
      return util::error_json("root_path '" + root_path_value->GetString() +
                              "' was not found: " + hint);
    }
  }
  const std::string root_path = util::to_std(godot::String(root->get_path()));
  auto *generator = godot::NavigationMeshGenerator::get_singleton();
  if (generator == nullptr) {
    return util::error_json(
        "NavigationMeshGenerator singleton not available");
  }
  const auto started = std::chrono::steady_clock::now();
  generator->bake(mesh, root);
  const int64_t elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - started)
          .count();
  const int64_t vertices = mesh->get_vertices().size();
  const int64_t polygons = mesh->get_polygon_count();
  if (elapsed_ms > timeout_ms) {
    JV details(JV::object_tag);
    details["code"] = JV("bake_timeout");
    details["navmesh_path"] = JV(navmesh_path);
    details["root_path"] = JV(root_path);
    details["elapsed_ms"] = JV(elapsed_ms);
    details["timeout_ms"] = JV(timeout_ms);
    details["vertices"] = JV(vertices);
    details["polygons"] = JV(polygons);
    details["baked"] = JV(true);
    JV error = nav_error(
        "navigation mesh bake exceeded timeout_ms (" +
            std::to_string(elapsed_ms) + " > " + std::to_string(timeout_ms) +
            " ms) — the synchronous engine bake cannot be interrupted and the "
            "mesh was already updated",
        "bake_timeout");
    error["elapsed_ms"] = JV(elapsed_ms);
    error["timeout_ms"] = JV(timeout_ms);
    error["retryable"] = JV(false);
    error["structured_error"] = std::move(details);
    return error;
  }
  JV out(JV::object_tag);
  out["navmesh_path"] = JV(navmesh_path);
  out["root_path"] = JV(root_path);
  out["vertices"] = JV(vertices);
  out["polygons"] = JV(polygons);
  out["elapsed_ms"] = JV(elapsed_ms);
  out["timeout_ms"] = JV(timeout_ms);
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "bake_navigation_mesh completed: " + navmesh_path);
  JV r(JV::object_tag);
  r["result"] = std::move(out);
  return r;
}

namespace {

bool astar_read_diagonal_mode(const JV &value,
                              godot::AStarGrid2D::DiagonalMode *out) {
  if (value.IsNumber()) {
    const int64_t mode =
        static_cast<int64_t>(util::json_number(&value, 0.0));
    if (mode < 0 ||
        mode >= static_cast<int64_t>(godot::AStarGrid2D::DIAGONAL_MODE_MAX)) {
      return false;
    }
    *out = static_cast<godot::AStarGrid2D::DiagonalMode>(mode);
    return true;
  }
  if (!value.IsString()) {
    return false;
  }
  const std::string name = value.GetString();
  if (name == "always") {
    *out = godot::AStarGrid2D::DIAGONAL_MODE_ALWAYS;
  } else if (name == "never") {
    *out = godot::AStarGrid2D::DIAGONAL_MODE_NEVER;
  } else if (name == "at_least_one_walkable") {
    *out = godot::AStarGrid2D::DIAGONAL_MODE_AT_LEAST_ONE_WALKABLE;
  } else if (name == "only_if_no_obstacles") {
    *out = godot::AStarGrid2D::DIAGONAL_MODE_ONLY_IF_NO_OBSTACLES;
  } else {
    return false;
  }
  return true;
}

JV astar_create(const JV &args) {
  const JV *kind_value = args.Find("kind");
  if (kind_value == nullptr || !kind_value->IsString()) {
    return nav_action_error("create", "kind");
  }
  const std::string kind = kind_value->GetString();
  std::string class_name;
  if (kind == "grid_2d") {
    class_name = "AStarGrid2D";
  } else if (kind == "2d") {
    class_name = "AStar2D";
  } else if (kind == "3d") {
    class_name = "AStar3D";
  } else {
    return util::error_json("invalid kind '" + kind +
                            "': expected grid_2d, 2d or 3d");
  }
  godot::Ref<godot::RefCounted> object =
      nav_instantiate<godot::RefCounted>(class_name.c_str());
  if (object.is_null()) {
    return util::error_json("failed to instantiate " + class_name);
  }
  JV out(JV::object_tag);
  out["kind"] = JV(kind);
  if (kind == "grid_2d") {
    godot::Ref<godot::AStarGrid2D> grid = object;
    const JV *region_value = args.Find("region");
    const JV *size_value = args.Find("size");
    if (region_value == nullptr && size_value == nullptr) {
      return util::error_json(
          "action 'create' with kind=grid_2d requires 'region' or 'size'");
    }
    if (region_value != nullptr && size_value != nullptr) {
      return util::error_json(
          "action 'create' with kind=grid_2d accepts 'region' or 'size', not "
          "both");
    }
    if (region_value != nullptr) {
      godot::Rect2i region;
      if (!nav_read_rect2i(*region_value, &region)) {
        return util::error_json(
            "parameter 'region' must be an object {x, y, width, height} with "
            "positive width and height");
      }
      grid->set_region(region);
    } else {
      godot::Vector2i size;
      if (!nav_read_vec2i(*size_value, &size) || size.x < 1 || size.y < 1) {
        return util::error_json(
            "parameter 'size' must be an object {x, y} with positive "
            "dimensions");
      }
      const nav_budget::GridExtentVerdict extent =
          nav_budget::validate_grid_extent(size.x, size.y);
      if (extent.failure != nav_budget::GridExtentFailure::kNone) {
        return util::error_json(
            "grid size rejected by budget: each dimension is capped at 2048 "
            "and the cell count at 1048576");
      }
      grid->set_size(size);
    }
    const JV *cell_size_value = args.Find("cell_size");
    if (cell_size_value != nullptr) {
      if (!cell_size_value->IsObject()) {
        return util::error_json(
            "parameter 'cell_size' must be an object {x, y}");
      }
      grid->set_cell_size(util::json_to_vec2(*cell_size_value));
    }
    const JV *offset_value = args.Find("offset");
    if (offset_value != nullptr) {
      if (!offset_value->IsObject()) {
        return util::error_json("parameter 'offset' must be an object {x, y}");
      }
      grid->set_offset(util::json_to_vec2(*offset_value));
    }
    const JV *diagonal_value = args.Find("diagonal_mode");
    if (diagonal_value != nullptr) {
      godot::AStarGrid2D::DiagonalMode diagonal_mode =
          godot::AStarGrid2D::DIAGONAL_MODE_ALWAYS;
      if (!astar_read_diagonal_mode(*diagonal_value, &diagonal_mode)) {
        return util::error_json(
            "parameter 'diagonal_mode' must be always, never, "
            "at_least_one_walkable or only_if_no_obstacles");
      }
      grid->set_diagonal_mode(diagonal_mode);
    }
    const JV *jumping_value = args.Find("jumping_enabled");
    if (jumping_value != nullptr) {
      if (!jumping_value->IsBool()) {
        return util::error_json(
            "parameter 'jumping_enabled' must be a boolean");
      }
      grid->set_jumping_enabled(jumping_value->GetBool());
    }
    grid->update();
    out["region"] = rect2i_to_json(grid->get_region());
    out["size"] = vec2i_to_json(grid->get_size());
    out["cell_size"] = util::vec2_to_json(grid->get_cell_size());
  } else if (kind == "2d") {
    godot::Ref<godot::AStar2D> astar = object;
    out["point_count"] = JV(astar->get_point_count());
  } else {
    godot::Ref<godot::AStar3D> astar = object;
    out["point_count"] = JV(astar->get_point_count());
  }
  const int64_t handle = astar_next_handle();
  astar_entries()[handle] = AstarEntry{object, kind};
  out["handle"] = JV(handle);
  return util::ok_result(std::move(out));
}

JV astar_free(const JV &args) {
  const JV *handle = args.Find("handle");
  if (handle == nullptr || !handle->IsInt()) {
    return nav_action_error("free", "handle");
  }
  const bool freed = astar_entries().erase(handle->GetInt()) > 0;
  JV out(JV::object_tag);
  out["freed"] = JV(freed);
  out["handle"] = JV(handle->GetInt());
  return util::ok_result(std::move(out));
}

JV astar_grid_action(const godot::Ref<godot::AStarGrid2D> &grid,
                     const std::string &action, const JV &args) {
  if (action == "set_region") {
    const JV *region_value = args.Find("region");
    godot::Rect2i region;
    if (region_value == nullptr || !nav_read_rect2i(*region_value, &region)) {
      return nav_action_error(action, "region");
    }
    grid->set_region(region);
    grid->update();
    JV out(JV::object_tag);
    out["region"] = rect2i_to_json(grid->get_region());
    out["size"] = vec2i_to_json(grid->get_size());
    return util::ok_result(std::move(out));
  }
  if (action == "fill_solid_region") {
    const JV *region_value = args.Find("region");
    godot::Rect2i region;
    if (region_value == nullptr || !nav_read_rect2i(*region_value, &region)) {
      return nav_action_error(action, "region");
    }
    const JV *solid_value = args.Find("solid");
    if (solid_value != nullptr && !solid_value->IsBool()) {
      return util::error_json("parameter 'solid' must be a boolean");
    }
    const bool solid =
        solid_value == nullptr || solid_value->GetBool();
    grid->fill_solid_region(region, solid);
    grid->update();
    return util::ok_result(JV(true));
  }
  if (action == "set_point_solid") {
    const JV *point = args.Find("point");
    godot::Vector2i cell;
    if (point == nullptr || !astar_read_cell(*point, &cell)) {
      return nav_action_error(action, "point");
    }
    const JV *solid_value = args.Find("solid");
    if (solid_value != nullptr && !solid_value->IsBool()) {
      return util::error_json("parameter 'solid' must be a boolean");
    }
    const bool solid =
        solid_value == nullptr || solid_value->GetBool();
    grid->set_point_solid(cell, solid);
    grid->update();
    return util::ok_result(JV(true));
  }
  if (action == "get_id_path" || action == "get_point_path") {
    const JV *from_value = args.Find("from");
    const JV *to_value = args.Find("to");
    godot::Vector2i from;
    godot::Vector2i to;
    if (from_value == nullptr || !astar_read_cell(*from_value, &from)) {
      return astar_missing_cell(action, "from");
    }
    if (to_value == nullptr || !astar_read_cell(*to_value, &to)) {
      return astar_missing_cell(action, "to");
    }
    const JV *partial_value = args.Find("allow_partial_path");
    if (partial_value != nullptr && !partial_value->IsBool()) {
      return util::error_json(
          "parameter 'allow_partial_path' must be a boolean");
    }
    const bool allow_partial =
        partial_value != nullptr && partial_value->GetBool();
    if (action == "get_id_path") {
      godot::TypedArray<godot::Vector2i> ids =
          grid->get_id_path(from, to, allow_partial);
      JV path(JV::array_tag);
      for (int64_t i = 0; i < ids.size(); i++) {
        godot::Vector2i id = ids[i];
        path.PushBack(vec2i_to_json(id));
      }
      JV out(JV::object_tag);
      out["path"] = std::move(path);
      out["count"] = JV(static_cast<int64_t>(ids.size()));
      return util::ok_result(std::move(out));
    }
    godot::PackedVector2Array points =
        grid->get_point_path(from, to, allow_partial);
    return util::ok_result(astar_path_json(points));
  }
  return util::error_json("action '" + action +
                          "' is not supported for kind=grid_2d");
}

template <typename T>
JV astar_graph_action(const godot::Ref<T> &astar, const std::string &action,
                      const JV &args) {
  constexpr bool kIs3D = std::is_same_v<T, godot::AStar3D>;
  if (action == "add_point") {
    const JV *id_value = args.Find("id");
    if (id_value == nullptr || !id_value->IsInt()) {
      return nav_action_error(action, "id");
    }
    const JV *position_value = args.Find("position");
    if (position_value == nullptr || !position_value->IsObject()) {
      return nav_action_error(action, "position");
    }
    const JV *weight_value = args.Find("weight_scale");
    if (weight_value != nullptr && !weight_value->IsNumber()) {
      return util::error_json("parameter 'weight_scale' must be a number");
    }
    const double weight_scale =
        weight_value != nullptr
            ? util::json_number(weight_value, kDefaultWeightScale)
            : kDefaultWeightScale;
    if constexpr (kIs3D) {
      astar->add_point(id_value->GetInt(), util::json_to_vec3(*position_value),
                       static_cast<float>(weight_scale));
    } else {
      astar->add_point(id_value->GetInt(), util::json_to_vec2(*position_value),
                       static_cast<float>(weight_scale));
    }
    JV out(JV::object_tag);
    out["id"] = JV(id_value->GetInt());
    out["point_count"] = JV(astar->get_point_count());
    return util::ok_result(std::move(out));
  }
  if (action == "connect" || action == "disconnect") {
    const JV *from_value = args.Find("from");
    const JV *to_value = args.Find("to");
    int64_t from = 0;
    int64_t to = 0;
    if (from_value == nullptr || !astar_read_point_id(*from_value, &from)) {
      return astar_missing_point(action, "from");
    }
    if (to_value == nullptr || !astar_read_point_id(*to_value, &to)) {
      return astar_missing_point(action, "to");
    }
    const JV *bidirectional_value = args.Find("bidirectional");
    if (bidirectional_value != nullptr && !bidirectional_value->IsBool()) {
      return util::error_json(
          "parameter 'bidirectional' must be a boolean");
    }
    const bool bidirectional =
        bidirectional_value == nullptr || bidirectional_value->GetBool();
    if (action == "connect") {
      astar->connect_points(from, to, bidirectional);
    } else {
      astar->disconnect_points(from, to, bidirectional);
    }
    return util::ok_result(JV(true));
  }
  if (action == "get_path") {
    const JV *from_value = args.Find("from");
    const JV *to_value = args.Find("to");
    int64_t from = 0;
    int64_t to = 0;
    if (from_value == nullptr || !astar_read_point_id(*from_value, &from)) {
      return astar_missing_point(action, "from");
    }
    if (to_value == nullptr || !astar_read_point_id(*to_value, &to)) {
      return astar_missing_point(action, "to");
    }
    const JV *partial_value = args.Find("allow_partial_path");
    if (partial_value != nullptr && !partial_value->IsBool()) {
      return util::error_json(
          "parameter 'allow_partial_path' must be a boolean");
    }
    const bool allow_partial =
        partial_value != nullptr && partial_value->GetBool();
    if constexpr (kIs3D) {
      godot::PackedVector3Array points =
          astar->get_point_path(from, to, allow_partial);
      return util::ok_result(astar_path_json(points));
    } else {
      godot::PackedVector2Array points =
          astar->get_point_path(from, to, allow_partial);
      return util::ok_result(astar_path_json(points));
    }
  }
  return util::error_json("action '" + action +
                          "' is not supported for kind=" +
                          (kIs3D ? "3d" : "2d"));
}

} // namespace

JV handle_astar_path(const JV &args) {
  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "astar_path called");
  const JV *action_value = args.Find("action");
  if (action_value == nullptr || !action_value->IsString()) {
    return util::error_json("missing required parameter: action");
  }
  const std::string action = action_value->GetString();
  if (action == "create") {
    return astar_create(args);
  }
  if (action == "free") {
    return astar_free(args);
  }
  JV error;
  AstarEntry *entry = astar_lookup(args, action, &error);
  if (entry == nullptr) {
    return error;
  }
  if (action == "set_region" || action == "fill_solid_region" ||
      action == "set_point_solid" || action == "get_id_path" ||
      action == "get_point_path") {
    if (!astar_require_kind(*entry, "grid_2d", action, &error)) {
      return error;
    }
    return astar_grid_action(entry->object, action, args);
  }
  if (action == "add_point" || action == "connect" || action == "disconnect" ||
      action == "get_path") {
    if (entry->kind == "2d") {
      return astar_graph_action(godot::Ref<godot::AStar2D>(entry->object),
                                action, args);
    }
    if (entry->kind == "3d") {
      return astar_graph_action(godot::Ref<godot::AStar3D>(entry->object),
                                action, args);
    }
    return util::error_json("action '" + action +
                            "' requires kind=2d or 3d but the handle was "
                            "created with kind=" +
                            entry->kind);
  }
  return util::error_json(
      "unknown astar action '" + action +
      "': expected create, free, set_region, fill_solid_region, "
      "set_point_solid, get_id_path, get_point_path, add_point, connect, "
      "disconnect or get_path");
}

} // namespace nav_ops
} // namespace godot_autopilot
