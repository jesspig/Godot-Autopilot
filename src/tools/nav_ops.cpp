#include "nav_ops.hpp"
#include "core/log_system.hpp"
#include "util/error_util.hpp"
#include "util/json_godot.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/navigation_mesh.hpp>
#include <godot_cpp/classes/navigation_polygon.hpp>
#include <godot_cpp/classes/navigation_server2d.hpp>
#include <godot_cpp/classes/navigation_server3d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <mcp/JsonValue.hpp>
#include <string>

namespace godot_autopilot {
namespace nav_ops {

using JV = mcp::JsonValue;

namespace {

JV vec2_to_json(const godot::Vector2 &v) {
  return JV::FromObject({{"x", JV(v.x)}, {"y", JV(v.y)}});
}

JV vec3_to_json(const godot::Vector3 &v) {
  return JV::FromObject({{"x", JV(v.x)}, {"y", JV(v.y)}, {"z", JV(v.z)}});
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
    arr.PushBack(vec2_to_json(path[i]));
  }

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "get_nav_2d_map_path completed");
  JV r(JV::object_tag);
  r["result"] = JV::FromObject({{"path", std::move(arr)}});
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
    arr.PushBack(vec3_to_json(path[i]));
  }

  LogSystem::instance().log(LogLevel::Debug, LogCategory::Tools,
                            "get_nav_3d_map_path completed");
  JV r(JV::object_tag);
  r["result"] = JV::FromObject({{"path", std::move(arr)}});
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
  r["result"] = JV::FromObject({{"point", vec3_to_json(point)}});
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
  result["position"] = vec3_to_json(pos);
  result["velocity"] = vec3_to_json(vel);
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

} // namespace nav_ops
} // namespace godot_autopilot
