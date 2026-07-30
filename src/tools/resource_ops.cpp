#include "resource_ops.hpp"
#include "core/log_system.hpp"
#include "util/variant_json.hpp"
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/resource_uid.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <string>

namespace godot_self_driving {
namespace resource_ops {

namespace {

std::string to_std(const godot::String& s) {
    godot::CharString utf8 = s.utf8();
    return std::string(utf8.ptr());
}

mcp::JsonValue serialize_ref(const godot::Ref<godot::Resource>& res) {
    if (res.is_null()) return mcp::JsonValue(nullptr);
    mcp::JsonValue j(mcp::JsonValue::object_tag);
    j["class"] = mcp::JsonValue(to_std(res->get_class()));
    j["path"] = mcp::JsonValue(to_std(res->get_path()));
    j["object_id"] = mcp::JsonValue(static_cast<int64_t>(res->get_instance_id()));
    j["name"] = mcp::JsonValue(to_std(res->get_name()));
    return j;
}

} // namespace

mcp::JsonValue handle_load(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();

    std::string type_hint;
    auto* th = args.Find("type_hint");
    if (th && th->IsString()) type_hint = th->GetString();

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    godot::Ref<godot::Resource> res = loader->load(
        godot::String(path.c_str()),
        godot::String(type_hint.c_str()));
    if (res.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("failed to load resource: " + path);
        return e;
    }
    if (to_std(res->get_path()).empty()) {
        res->set_path(godot::String(path.c_str()));
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = serialize_ref(res);
    return r;
}

mcp::JsonValue handle_load_threaded(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();

    std::string type_hint;
    auto* th = args.Find("type_hint");
    if (th && th->IsString()) type_hint = th->GetString();

    bool use_sub_threads = false;
    auto* us = args.Find("use_sub_threads");
    if (us && us->IsBool()) use_sub_threads = us->GetBool();

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    godot::Error err = loader->load_threaded_request(
        godot::String(path.c_str()),
        godot::String(type_hint.c_str()),
        use_sub_threads);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(static_cast<int64_t>(err));
    return r;
}

mcp::JsonValue handle_load_threaded_get_status(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    godot::ResourceLoader::ThreadLoadStatus status =
        loader->load_threaded_get_status(godot::String(path.c_str()));
    static const char* names[] = {
        "invalid_resource", "in_progress", "failed", "loaded"};
    int idx = static_cast<int>(status);
    std::string name = (idx >= 0 && idx < 4) ? names[idx] : "unknown";

    mcp::JsonValue j(mcp::JsonValue::object_tag);
    j["status"] = mcp::JsonValue(name);
    j["code"] = mcp::JsonValue(static_cast<int64_t>(idx));
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(j);
    return r;
}

mcp::JsonValue handle_load_threaded_wait(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    godot::Ref<godot::Resource> res = loader->load_threaded_get(
        godot::String(path.c_str()));
    if (res.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("threaded load did not return a resource for: " + path);
        return e;
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = serialize_ref(res);
    return r;
}

mcp::JsonValue handle_save(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();

    std::string dest_path = path;
    auto* dp = args.Find("dest_path");
    if (dp && dp->IsString()) dest_path = dp->GetString();

    int flags = 0;
    auto* fl = args.Find("flags");
    if (fl && fl->IsInt()) flags = static_cast<int>(fl->GetInt());

    std::string name;
    auto* nm = args.Find("name");
    if (nm && nm->IsString()) name = nm->GetString();

    godot::Ref<godot::Resource> res;

    auto* oid_str = args.Find("object_id_str");
    int64_t obj_id = 0;
    bool has_oid = false;
    if (oid_str && oid_str->IsString()) {
        try {
            obj_id = std::stoll(oid_str->GetString());
            has_oid = true;
        } catch (...) {
            mcp::JsonValue e(mcp::JsonValue::object_tag);
            e["error"] = mcp::JsonValue("invalid object_id_str: not a valid 64-bit integer");
            return e;
        }
    } else {
        auto* oid = args.Find("object_id");
        if (oid && oid->IsInt()) {
            obj_id = oid->GetInt();
            has_oid = true;
        }
    }
    if (has_oid) {
        auto* obj = godot::ObjectDB::get_instance(static_cast<uint64_t>(obj_id));
        if (obj) {
            auto* res_obj = godot::Object::cast_to<godot::Resource>(obj);
            if (res_obj) {
                res = godot::Ref<godot::Resource>(res_obj);
            }
        }
    } else {
        auto* loader = godot::ResourceLoader::get_singleton();
        if (loader) {
            res = loader->load(godot::String(path.c_str()));
        }
    }

    if (res.is_null()) {
        auto* ct = args.Find("class_type");
        if (ct && ct->IsString()) {
            std::string class_type = ct->GetString();
            auto* cdbs = godot::ClassDBSingleton::get_singleton();
            if (cdbs) {
                godot::Variant obj_var = cdbs->instantiate(godot::StringName(class_type.c_str()));
                if (obj_var.get_type() != godot::Variant::NIL) {
                    auto* obj = godot::Object::cast_to<godot::Resource>(obj_var);
                    if (obj) {
                        res = godot::Ref<godot::Resource>(obj);
                    }
                }
            }
        }
    }

    if (!res.is_null() && res->get_path().is_empty() && !name.empty()) {
        res->set_path(godot::String(("memory://" + name).c_str()));
    }

    if (res.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("failed to resolve resource: provide object_id (from resource_create) or class_type + name to create a new resource in memory");
        return e;
    }

    auto* saver = godot::ResourceSaver::get_singleton();
    if (!saver) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceSaver not available");
        return e;
    }

    std::string save_dir = dest_path;
    size_t last_slash = save_dir.find_last_of('/');
    if (last_slash != std::string::npos) {
        save_dir = save_dir.substr(0, last_slash);
        auto dir = godot::DirAccess::open(godot::String("res://"));
        if (dir.is_valid()) {
            dir->make_dir_recursive(godot::String(save_dir.c_str()));
        }
    }

    godot::Error err = saver->save(
        res,
        godot::String(dest_path.c_str()),
        static_cast<godot::BitField<godot::ResourceSaver::SaverFlags>>(flags));
    if (err == godot::OK && dest_path != path) {
        auto* editor = godot::EditorInterface::get_singleton();
        if (editor) {
            auto* root = editor->get_edited_scene_root();
            godot::String src_gs(path.c_str());
            godot::String dst_gs(dest_path.c_str());
            if (root && root->get_scene_file_path() == src_gs) {
                root->set_scene_file_path(dst_gs);
            }
            auto* efs = editor->get_resource_filesystem();
            if (efs) {
                efs->update_file(src_gs);
                efs->update_file(dst_gs);
            }
        }
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(static_cast<int64_t>(err));
    return r;
}

mcp::JsonValue handle_create(const mcp::JsonValue& args) {
    auto* it_type = args.Find("type");
    if (!it_type || !it_type->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: type");
        return e;
    }
    std::string type = it_type->GetString();

    std::string name;
    auto* nm = args.Find("name");
    if (nm && nm->IsString()) name = nm->GetString();

    auto* cdbs = godot::ClassDBSingleton::get_singleton();
    if (!cdbs) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ClassDB not available");
        return e;
    }

    if (!cdbs->is_parent_class(godot::StringName(type.c_str()), godot::StringName("Resource"))) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue(type + " is not a Resource subclass");
        return e;
    }

    godot::Variant obj_var = cdbs->instantiate(godot::StringName(type.c_str()));
    if (obj_var.get_type() == godot::Variant::NIL) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("failed to instantiate: " + type);
        return e;
    }

    auto* obj = godot::Object::cast_to<godot::Resource>(obj_var);
    if (!obj) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("instantiated object is not a Resource: " + type);
        return e;
    }
    godot::Ref<godot::Resource> res(obj);

    if (!name.empty()) {
        res->set_name(godot::String(name.c_str()));
        res->set_path(godot::String(("memory://" + name).c_str()));
    }

    auto result = serialize_ref(res);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(result);
    return r;
}

mcp::JsonValue handle_duplicate(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();

    bool deep = false;
    auto* dp = args.Find("deep");
    if (dp && dp->IsBool()) deep = dp->GetBool();

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    godot::Ref<godot::Resource> res = loader->load(godot::String(path.c_str()));
    if (res.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("failed to load resource: " + path);
        return e;
    }

    godot::Ref<godot::Resource> dup = res->duplicate(deep);
    if (dup.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("duplicate returned null");
        return e;
    }

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = serialize_ref(dup);
    return r;
}

mcp::JsonValue handle_get_type(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    godot::Ref<godot::Resource> res = loader->load(godot::String(path.c_str()));
    if (res.is_null()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("failed to load resource: " + path);
        return e;
    }

    mcp::JsonValue j(mcp::JsonValue::object_tag);
    j["class"] = mcp::JsonValue(to_std(res->get_class()));
    j["path"] = mcp::JsonValue(path);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(j);
    return r;
}

mcp::JsonValue handle_exists(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    bool exists = loader->exists(godot::String(path.c_str()));
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(exists);
    return r;
}

mcp::JsonValue handle_list_types(const mcp::JsonValue&) {
    auto* cdbs = godot::ClassDBSingleton::get_singleton();
    if (!cdbs) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ClassDB not available");
        return e;
    }

    godot::PackedStringArray all = cdbs->get_class_list();
    mcp::JsonValue types(mcp::JsonValue::array_tag);
    for (int i = 0; i < all.size(); i++) {
        std::string cls = to_std(all[i]);
        if (cdbs->is_parent_class(godot::StringName(cls.c_str()), godot::StringName("Resource")) &&
            cdbs->can_instantiate(godot::StringName(cls.c_str()))) {
            types.PushBack(mcp::JsonValue(cls));
        }
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(types);
    return r;
}

mcp::JsonValue handle_get_extensions(const mcp::JsonValue& args) {
    std::string type;
    auto* tp = args.Find("type");
    if (tp && tp->IsString()) type = tp->GetString();

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    godot::PackedStringArray exts = loader->get_recognized_extensions_for_type(
        godot::String(type.c_str()));
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    for (int i = 0; i < exts.size(); i++) {
        arr.PushBack(mcp::JsonValue(to_std(exts[i])));
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(arr);
    return r;
}

mcp::JsonValue handle_list_dir(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    godot::PackedStringArray entries = loader->list_directory(godot::String(path.c_str()));
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    for (int i = 0; i < entries.size(); i++) {
        arr.PushBack(mcp::JsonValue(to_std(entries[i])));
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(arr);
    return r;
}

mcp::JsonValue handle_get_uid(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    int64_t uid = loader->get_resource_uid(godot::String(path.c_str()));
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(static_cast<int64_t>(uid));
    return r;
}

mcp::JsonValue handle_set_uid(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();

    auto* uid_svc = godot::ResourceUID::get_singleton();
    if (!uid_svc) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceUID not available");
        return e;
    }

    int64_t uid;
    auto* it_uid = args.Find("uid");
    if (it_uid && it_uid->IsNumber()) {
        uid = it_uid->GetInt();
    } else {
        uid = uid_svc->create_id();
    }
    uid_svc->set_id(uid, godot::String(path.c_str()));
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(static_cast<int64_t>(uid));
    return r;
}

mcp::JsonValue handle_remove(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();
    godot::String path_gs(path.c_str());

    auto* loader = godot::ResourceLoader::get_singleton();
    if (loader && loader->has_cached(path_gs)) {
        auto cached = loader->get_cached_ref(path_gs);
        if (cached.is_valid()) {
            cached->set_path("");
        }
    }

    godot::Error err = godot::DirAccess::remove_absolute(path_gs);

    auto* editor = godot::EditorInterface::get_singleton();
    if (editor) {
        auto* efs = editor->get_resource_filesystem();
        if (efs) {
            efs->update_file(path_gs);
        }
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(static_cast<int64_t>(err));
    return r;
}

mcp::JsonValue handle_rename(const mcp::JsonValue& args) {
    auto* it_from = args.Find("from");
    auto* it_to = args.Find("to");
    if (!it_from || !it_from->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: from");
        return e;
    }
    if (!it_to || !it_to->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: to");
        return e;
    }
    std::string from = it_from->GetString();
    std::string to = it_to->GetString();

    godot::Error err = godot::DirAccess::rename_absolute(
        godot::String(from.c_str()), godot::String(to.c_str()));
    if (err != godot::OK) {
        mcp::JsonValue r(mcp::JsonValue::object_tag);
        r["result"] = mcp::JsonValue(static_cast<int64_t>(err));
        return r;
    }

    godot::String from_gs(from.c_str());
    godot::String to_gs(to.c_str());

    auto* loader = godot::ResourceLoader::get_singleton();
    if (loader && loader->has_cached(from_gs)) {
        auto cached = loader->get_cached_ref(from_gs);
        if (cached.is_valid()) {
            cached->set_path(to_gs);
        }
    }

    auto* editor = godot::EditorInterface::get_singleton();
    if (editor) {
        auto* root = editor->get_edited_scene_root();
        if (root && root->get_scene_file_path() == from_gs) {
            root->set_scene_file_path(to_gs);
        }
        auto* efs = editor->get_resource_filesystem();
        if (efs) {
            efs->update_file(from_gs);
            efs->update_file(to_gs);
        }
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(static_cast<int64_t>(err));
    return r;
}

mcp::JsonValue handle_get_dependencies(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    godot::PackedStringArray deps = loader->get_dependencies(godot::String(path.c_str()));
    mcp::JsonValue arr(mcp::JsonValue::array_tag);
    for (int i = 0; i < deps.size(); i++) {
        arr.PushBack(mcp::JsonValue(to_std(deps[i])));
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = std::move(arr);
    return r;
}

mcp::JsonValue handle_has_dependency(const mcp::JsonValue& args) {
    auto* it_path = args.Find("path");
    auto* it_dep = args.Find("dependency");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    if (!it_dep || !it_dep->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: dependency");
        return e;
    }
    std::string path = it_path->GetString();
    std::string dep = it_dep->GetString();

    auto* loader = godot::ResourceLoader::get_singleton();
    if (!loader) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("ResourceLoader not available");
        return e;
    }

    godot::PackedStringArray deps = loader->get_dependencies(godot::String(path.c_str()));
    godot::String dep_gs(dep.c_str());
    bool found = false;
    for (int i = 0; i < deps.size(); i++) {
        if (deps[i] == dep_gs) { found = true; break; }
    }
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue(found);
    return r;
}

mcp::JsonValue handle_import(const mcp::JsonValue& args) {
    auto* engine = godot::Engine::get_singleton();
    if (engine && engine->is_editor_hint() == false) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("resource_import is only available in editor mode");
        return e;
    }
    auto* it_path = args.Find("path");
    if (!it_path || !it_path->IsString()) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("missing required parameter: path");
        return e;
    }
    std::string path = it_path->GetString();

    auto* editor = godot::EditorInterface::get_singleton();
    if (!editor) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("EditorInterface not available");
        return e;
    }

    auto* efs = editor->get_resource_filesystem();
    if (!efs) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("EditorFileSystem not available");
        return e;
    }

    godot::PackedStringArray files;
    files.append(godot::String(path.c_str()));
    efs->reimport_files(files);
    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("import queued for: " + path);
    return r;
}

mcp::JsonValue handle_reimport(const mcp::JsonValue& args) {
    auto* engine = godot::Engine::get_singleton();
    if (engine && engine->is_editor_hint() == false) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("resource_reimport is only available in editor mode");
        return e;
    }

    auto* editor = godot::EditorInterface::get_singleton();
    if (!editor) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("EditorInterface not available");
        return e;
    }

    auto* efs = editor->get_resource_filesystem();
    if (!efs) {
        mcp::JsonValue e(mcp::JsonValue::object_tag);
        e["error"] = mcp::JsonValue("EditorFileSystem not available");
        return e;
    }

    int count = 0;
    auto* it_files = args.Find("files");
    if (it_files && it_files->IsArray()) {
        const auto& arr = it_files->GetArray();
        godot::PackedStringArray files;
        for (const auto& f : arr) {
            if (f.IsString()) {
                files.append(godot::String(f.GetString().c_str()));
                count++;
            }
        }
        efs->reimport_files(files);
    }

    if (!args.Contains("files")) {
        auto* it_path = args.Find("path");
        if (it_path && it_path->IsString()) {
            godot::PackedStringArray files;
            files.append(godot::String(it_path->GetString().c_str()));
            efs->reimport_files(files);
            count++;
        }
    }

    mcp::JsonValue r(mcp::JsonValue::object_tag);
    r["result"] = mcp::JsonValue("reimport queued for " + std::to_string(count) + " file(s)");
    return r;
}

} // namespace resource_ops
} // namespace godot_self_driving
