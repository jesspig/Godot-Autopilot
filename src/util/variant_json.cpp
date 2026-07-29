#include "variant_json.hpp"
#include <cctype>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/core/object.hpp>

namespace godot_self_driving {

namespace {

std::string str_from_variant(const godot::Variant& v) {
    godot::String s = v.operator godot::String();
    godot::CharString utf8 = s.utf8();
    return std::string(utf8.ptr());
}

std::string to_std_string(const godot::String& s) {
    godot::CharString utf8 = s.utf8();
    return std::string(utf8.ptr());
}

std::string to_lower(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) {
        r.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return r;
}

std::optional<godot::Variant::Type> parse_type_hint(const std::string& hint) {
    static const std::unordered_map<std::string, godot::Variant::Type> map = {
        {"nil", godot::Variant::NIL},
        {"bool", godot::Variant::BOOL},
        {"int", godot::Variant::INT},
        {"float", godot::Variant::FLOAT},
        {"string", godot::Variant::STRING},
        {"vector2", godot::Variant::VECTOR2},
        {"vector2i", godot::Variant::VECTOR2I},
        {"rect2", godot::Variant::RECT2},
        {"rect2i", godot::Variant::RECT2I},
        {"vector3", godot::Variant::VECTOR3},
        {"vector3i", godot::Variant::VECTOR3I},
        {"transform2d", godot::Variant::TRANSFORM2D},
        {"vector4", godot::Variant::VECTOR4},
        {"vector4i", godot::Variant::VECTOR4I},
        {"plane", godot::Variant::PLANE},
        {"quaternion", godot::Variant::QUATERNION},
        {"aabb", godot::Variant::AABB},
        {"basis", godot::Variant::BASIS},
        {"transform3d", godot::Variant::TRANSFORM3D},
        {"projection", godot::Variant::PROJECTION},
        {"color", godot::Variant::COLOR},
        {"string_name", godot::Variant::STRING_NAME},
        {"stringname", godot::Variant::STRING_NAME},
        {"node_path", godot::Variant::NODE_PATH},
        {"nodepath", godot::Variant::NODE_PATH},
        {"rid", godot::Variant::RID},
        {"object", godot::Variant::OBJECT},
        {"callable", godot::Variant::CALLABLE},
        {"signal", godot::Variant::SIGNAL},
        {"dictionary", godot::Variant::DICTIONARY},
        {"dict", godot::Variant::DICTIONARY},
        {"array", godot::Variant::ARRAY},
        {"packed_byte_array", godot::Variant::PACKED_BYTE_ARRAY},
        {"packedbytearray", godot::Variant::PACKED_BYTE_ARRAY},
        {"packed_int32_array", godot::Variant::PACKED_INT32_ARRAY},
        {"packedint32array", godot::Variant::PACKED_INT32_ARRAY},
        {"packed_int64_array", godot::Variant::PACKED_INT64_ARRAY},
        {"packedint64array", godot::Variant::PACKED_INT64_ARRAY},
        {"packed_float32_array", godot::Variant::PACKED_FLOAT32_ARRAY},
        {"packedfloat32array", godot::Variant::PACKED_FLOAT32_ARRAY},
        {"packed_float64_array", godot::Variant::PACKED_FLOAT64_ARRAY},
        {"packedfloat64array", godot::Variant::PACKED_FLOAT64_ARRAY},
        {"packed_string_array", godot::Variant::PACKED_STRING_ARRAY},
        {"packedstringarray", godot::Variant::PACKED_STRING_ARRAY},
        {"packed_vector2_array", godot::Variant::PACKED_VECTOR2_ARRAY},
        {"packedvector2array", godot::Variant::PACKED_VECTOR2_ARRAY},
        {"packed_vector3_array", godot::Variant::PACKED_VECTOR3_ARRAY},
        {"packedvector3array", godot::Variant::PACKED_VECTOR3_ARRAY},
        {"packed_color_array", godot::Variant::PACKED_COLOR_ARRAY},
        {"packedcolorarray", godot::Variant::PACKED_COLOR_ARRAY},
        {"packed_vector4_array", godot::Variant::PACKED_VECTOR4_ARRAY},
        {"packedvector4array", godot::Variant::PACKED_VECTOR4_ARRAY},
    };
    auto it = map.find(to_lower(hint));
    if (it != map.end()) return it->second;
    return std::nullopt;
}

double as_double(const mcp::JsonValue& j, double def = 0.0) {
    if (j.IsInt()) return static_cast<double>(j.GetInt());
    if (j.IsDouble()) return j.GetDouble();
    return def;
}

int64_t as_int64(const mcp::JsonValue& j, int64_t def = 0) {
    if (j.IsInt()) return j.GetInt();
    if (j.IsDouble()) return static_cast<int64_t>(j.GetDouble());
    return def;
}

double field_double(const mcp::JsonValue& j, const char* key, double def = 0.0) {
    auto* v = j.Find(key);
    if (!v) return def;
    return as_double(*v, def);
}

int64_t field_int64(const mcp::JsonValue& j, const char* key, int64_t def = 0) {
    auto* v = j.Find(key);
    if (!v) return def;
    return as_int64(*v, def);
}

double nested_double(const mcp::JsonValue& j, const char* outer, const char* inner, double def = 0.0) {
    auto* o = j.Find(outer);
    if (!o || !o->IsObject()) return def;
    return field_double(*o, inner, def);
}

int64_t nested_int64(const mcp::JsonValue& j, const char* outer, const char* inner, int64_t def = 0) {
    auto* o = j.Find(outer);
    if (!o || !o->IsObject()) return def;
    return field_int64(*o, inner, def);
}

std::string get_string_or_dump(const mcp::JsonValue& j) {
    if (j.IsString()) return j.GetString();
    return j.Dump();
}

godot::Variant deserialize_inferred(const mcp::JsonValue& j) {
    if (j.IsNull()) return godot::Variant();
    if (j.IsBool()) return godot::Variant(j.GetBool());
    if (j.IsInt()) return godot::Variant(j.GetInt());
    if (j.IsDouble()) return godot::Variant(j.GetDouble());
    if (j.IsString()) return godot::Variant(godot::String(j.GetString().c_str()));
    if (j.IsArray()) {
        godot::Array arr;
        for (const auto& elem : j.GetArray()) {
            arr.append(deserialize_inferred(elem));
        }
        return godot::Variant(arr);
    }
    if (j.IsObject()) {
        godot::Dictionary d;
        for (const auto& [key, val] : j.GetObject()) {
            d[godot::String(key.c_str())] = deserialize_inferred(val);
        }
        return godot::Variant(d);
    }
    return godot::Variant();
}

godot::Variant deserialize_typed(const mcp::JsonValue& j, godot::Variant::Type type) {
    using namespace godot;
    switch (type) {
    case Variant::NIL:
        return Variant();

    case Variant::BOOL:
        return Variant(j.IsBool() ? j.GetBool() : false);

    case Variant::INT:
        return Variant(j.IsNumber() ? as_int64(j) : 0);

    case Variant::FLOAT:
        return Variant(j.IsNumber() ? as_double(j) : 0.0);

    case Variant::STRING: {
        auto s = get_string_or_dump(j);
        return Variant(String(s.c_str()));
    }

    case Variant::VECTOR2:
        return Variant(Vector2(
            field_double(j, "x"),
            field_double(j, "y")));

    case Variant::VECTOR2I:
        return Variant(Vector2i(
            field_int64(j, "x"),
            field_int64(j, "y")));

    case Variant::RECT2:
        return Variant(Rect2(
            Vector2(nested_double(j, "position", "x"),
                    nested_double(j, "position", "y")),
            Vector2(nested_double(j, "size", "x"),
                    nested_double(j, "size", "y"))));

    case Variant::RECT2I:
        return Variant(Rect2i(
            Vector2i(nested_int64(j, "position", "x"),
                     nested_int64(j, "position", "y")),
            Vector2i(nested_int64(j, "size", "x"),
                     nested_int64(j, "size", "y"))));

    case Variant::VECTOR3:
        return Variant(Vector3(
            field_double(j, "x"),
            field_double(j, "y"),
            field_double(j, "z")));

    case Variant::VECTOR3I:
        return Variant(Vector3i(
            field_int64(j, "x"),
            field_int64(j, "y"),
            field_int64(j, "z")));

    case Variant::TRANSFORM2D: {
        Vector2 cols[3];
        auto* cols_j = j.Find("columns");
        if (cols_j && cols_j->IsArray()) {
            const auto& arr = cols_j->GetArray();
            for (int i = 0; i < 3 && i < static_cast<int>(arr.size()); ++i) {
                const auto& c = arr[i];
                if (c.IsArray()) {
                    const auto& ca = c.GetArray();
                    cols[i] = Vector2(
                        ca.size() > 0 ? as_double(ca[0]) : 0.0,
                        ca.size() > 1 ? as_double(ca[1]) : 0.0);
                }
            }
        }
        return Variant(Transform2D(cols[0], cols[1], cols[2]));
    }

    case Variant::VECTOR4:
        return Variant(Vector4(
            field_double(j, "x"),
            field_double(j, "y"),
            field_double(j, "z"),
            field_double(j, "w")));

    case Variant::VECTOR4I:
        return Variant(Vector4i(
            field_int64(j, "x"),
            field_int64(j, "y"),
            field_int64(j, "z"),
            field_int64(j, "w")));

    case Variant::PLANE: {
        Vector3 normal(nested_double(j, "normal", "x"),
                       nested_double(j, "normal", "y"),
                       nested_double(j, "normal", "z"));
        return Variant(Plane(normal, field_double(j, "d")));
    }

    case Variant::QUATERNION:
        return Variant(Quaternion(
            field_double(j, "x"),
            field_double(j, "y"),
            field_double(j, "z"),
            field_double(j, "w")));

    case Variant::AABB:
        return Variant(::godot::AABB(
            Vector3(nested_double(j, "position", "x"),
                    nested_double(j, "position", "y"),
                    nested_double(j, "position", "z")),
            Vector3(nested_double(j, "size", "x"),
                    nested_double(j, "size", "y"),
                    nested_double(j, "size", "z"))));

    case Variant::BASIS: {
        Basis b;
        auto* rows_j = j.Find("rows");
        if (rows_j && rows_j->IsArray()) {
            const auto& arr = rows_j->GetArray();
            for (int i = 0; i < 3 && i < static_cast<int>(arr.size()); ++i) {
                const auto& r = arr[i];
                if (r.IsArray()) {
                    const auto& ra = r.GetArray();
                    b.rows[i] = Vector3(
                        ra.size() > 0 ? as_double(ra[0]) : 0.0,
                        ra.size() > 1 ? as_double(ra[1]) : 0.0,
                        ra.size() > 2 ? as_double(ra[2]) : 0.0);
                }
            }
        }
        return Variant(b);
    }

    case Variant::TRANSFORM3D: {
        Basis b;
        auto* basis_j = j.Find("basis");
        if (basis_j && basis_j->IsObject()) {
            auto* rows_j = basis_j->Find("rows");
            if (rows_j && rows_j->IsArray()) {
                const auto& arr = rows_j->GetArray();
                for (int i = 0; i < 3 && i < static_cast<int>(arr.size()); ++i) {
                    const auto& r = arr[i];
                    if (r.IsArray()) {
                        const auto& ra = r.GetArray();
                        b.rows[i] = Vector3(
                            ra.size() > 0 ? as_double(ra[0]) : 0.0,
                            ra.size() > 1 ? as_double(ra[1]) : 0.0,
                            ra.size() > 2 ? as_double(ra[2]) : 0.0);
                    }
                }
            }
        }
        return Variant(Transform3D(b, Vector3(
            nested_double(j, "origin", "x"),
            nested_double(j, "origin", "y"),
            nested_double(j, "origin", "z"))));
    }

    case Variant::PROJECTION: {
        Vector4 cols[4];
        auto* cols_j = j.Find("columns");
        if (cols_j && cols_j->IsArray()) {
            const auto& arr = cols_j->GetArray();
            for (int i = 0; i < 4 && i < static_cast<int>(arr.size()); ++i) {
                const auto& c = arr[i];
                if (c.IsArray()) {
                    const auto& ca = c.GetArray();
                    cols[i] = Vector4(
                        ca.size() > 0 ? as_double(ca[0]) : 0.0,
                        ca.size() > 1 ? as_double(ca[1]) : 0.0,
                        ca.size() > 2 ? as_double(ca[2]) : 0.0,
                        ca.size() > 3 ? as_double(ca[3]) : 0.0);
                }
            }
        }
        return Variant(Projection(cols[0], cols[1], cols[2], cols[3]));
    }

    case Variant::COLOR:
        return Variant(Color(
            static_cast<float>(field_double(j, "r")),
            static_cast<float>(field_double(j, "g")),
            static_cast<float>(field_double(j, "b")),
            static_cast<float>(field_double(j, "a", 1.0))));

    case Variant::STRING_NAME: {
        auto s = get_string_or_dump(j);
        return Variant(StringName(String(s.c_str())));
    }

    case Variant::NODE_PATH: {
        auto s = get_string_or_dump(j);
        return Variant(NodePath(String(s.c_str())));
    }

    case Variant::DICTIONARY: {
        Dictionary d;
        if (j.IsObject()) {
            for (const auto& [key, val] : j.GetObject()) {
                d[String(key.c_str())] = deserialize_inferred(val);
            }
        }
        return Variant(d);
    }

    case Variant::ARRAY: {
        Array arr;
        if (j.IsArray()) {
            for (const auto& elem : j.GetArray()) {
                arr.append(deserialize_inferred(elem));
            }
        }
        return Variant(arr);
    }

    case Variant::PACKED_BYTE_ARRAY: {
        PackedByteArray a;
        if (j.IsArray()) {
            a.resize(static_cast<int>(j.Size()));
            for (int i = 0; i < static_cast<int>(j.Size()); ++i) {
                a[i] = static_cast<uint8_t>(as_int64(j[i]));
            }
        }
        return Variant(a);
    }

    case Variant::PACKED_INT32_ARRAY: {
        PackedInt32Array a;
        if (j.IsArray()) {
            a.resize(static_cast<int>(j.Size()));
            for (int i = 0; i < static_cast<int>(j.Size()); ++i) {
                a[i] = static_cast<int32_t>(as_int64(j[i]));
            }
        }
        return Variant(a);
    }

    case Variant::PACKED_INT64_ARRAY: {
        PackedInt64Array a;
        if (j.IsArray()) {
            a.resize(static_cast<int>(j.Size()));
            for (int i = 0; i < static_cast<int>(j.Size()); ++i) {
                a[i] = as_int64(j[i]);
            }
        }
        return Variant(a);
    }

    case Variant::PACKED_FLOAT32_ARRAY: {
        PackedFloat32Array a;
        if (j.IsArray()) {
            a.resize(static_cast<int>(j.Size()));
            for (int i = 0; i < static_cast<int>(j.Size()); ++i) {
                a[i] = static_cast<float>(as_double(j[i]));
            }
        }
        return Variant(a);
    }

    case Variant::PACKED_FLOAT64_ARRAY: {
        PackedFloat64Array a;
        if (j.IsArray()) {
            a.resize(static_cast<int>(j.Size()));
            for (int i = 0; i < static_cast<int>(j.Size()); ++i) {
                a[i] = as_double(j[i]);
            }
        }
        return Variant(a);
    }

    case Variant::PACKED_STRING_ARRAY: {
        PackedStringArray a;
        if (j.IsArray()) {
            for (const auto& elem : j.GetArray()) {
                auto es = get_string_or_dump(elem);
                a.append(String(es.c_str()));
            }
        }
        return Variant(a);
    }

    case Variant::PACKED_VECTOR2_ARRAY: {
        PackedVector2Array a;
        if (j.IsArray()) {
            for (const auto& elem : j.GetArray()) {
                a.append(Vector2(
                    field_double(elem, "x"),
                    field_double(elem, "y")));
            }
        }
        return Variant(a);
    }

    case Variant::PACKED_VECTOR3_ARRAY: {
        PackedVector3Array a;
        if (j.IsArray()) {
            for (const auto& elem : j.GetArray()) {
                a.append(Vector3(
                    field_double(elem, "x"),
                    field_double(elem, "y"),
                    field_double(elem, "z")));
            }
        }
        return Variant(a);
    }

    case Variant::PACKED_COLOR_ARRAY: {
        PackedColorArray a;
        if (j.IsArray()) {
            for (const auto& elem : j.GetArray()) {
                a.append(Color(
                    static_cast<float>(field_double(elem, "r")),
                    static_cast<float>(field_double(elem, "g")),
                    static_cast<float>(field_double(elem, "b")),
                    static_cast<float>(field_double(elem, "a", 1.0))));
            }
        }
        return Variant(a);
    }

    case Variant::PACKED_VECTOR4_ARRAY: {
        PackedVector4Array a;
        if (j.IsArray()) {
            for (const auto& elem : j.GetArray()) {
                a.append(Vector4(
                    field_double(elem, "x"),
                    field_double(elem, "y"),
                    field_double(elem, "z"),
                    field_double(elem, "w")));
            }
        }
        return Variant(a);
    }

    default:
        return Variant();
    }
}

godot::Variant deserialize_as_object(const mcp::JsonValue& j, const std::string& class_name) {
    if (!j.IsObject()) return godot::Variant();

    auto* cdbs = godot::ClassDBSingleton::get_singleton();
    if (!cdbs) return godot::Variant();

    std::string resolved_class = class_name;
    auto* class_field = j.Find("class");
    if (class_field && class_field->IsString()) {
        resolved_class = class_field->GetString();
    }

    godot::StringName sn(resolved_class.c_str());
    godot::Variant obj_var = cdbs->instantiate(sn);
    if (obj_var.get_type() == godot::Variant::NIL) return obj_var;

    godot::Object* obj = obj_var.operator godot::Object*();
    if (!obj) return godot::Variant();

    godot::TypedArray<godot::Dictionary> props = obj->get_property_list();
    std::unordered_set<std::string> valid_props;
    for (int64_t i = 0; i < props.size(); i++) {
        godot::Dictionary prop = props[i];
        std::string pname = to_std_string(godot::String(prop["name"]));
        valid_props.insert(std::move(pname));
    }

    for (const auto& [key, val] : j.GetObject()) {
        if (key == "class") continue;
        if (valid_props.count(key) == 0) continue;
        godot::StringName prop_name(key.c_str());
        godot::Variant prop_val = deserialize_inferred(val);
        obj->set(prop_name, prop_val);
    }

    return obj_var;
}

} // anonymous namespace

mcp::JsonValue VariantJson::serialize(const godot::Variant& v) {
    using namespace godot;
    switch (v.get_type()) {
    case Variant::NIL:
        return mcp::JsonValue();

    case Variant::BOOL:
        return mcp::JsonValue(static_cast<bool>(v));

    case Variant::INT:
        return mcp::JsonValue(static_cast<int64_t>(v));

    case Variant::FLOAT:
        return mcp::JsonValue(static_cast<double>(v));

    case Variant::STRING: {
        godot::String s = v.operator godot::String();
        return mcp::JsonValue(to_std_string(s));
    }

    case Variant::VECTOR2: {
        auto vec = static_cast<Vector2>(v);
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["x"] = mcp::JsonValue(vec.x);
        j["y"] = mcp::JsonValue(vec.y);
        return j;
    }

    case Variant::VECTOR2I: {
        auto vec = static_cast<Vector2i>(v);
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["x"] = mcp::JsonValue(vec.x);
        j["y"] = mcp::JsonValue(vec.y);
        return j;
    }

    case Variant::RECT2: {
        auto r = static_cast<Rect2>(v);
        mcp::JsonValue pos(mcp::JsonValue::object_tag);
        pos["x"] = mcp::JsonValue(r.position.x);
        pos["y"] = mcp::JsonValue(r.position.y);
        mcp::JsonValue sz(mcp::JsonValue::object_tag);
        sz["x"] = mcp::JsonValue(r.size.x);
        sz["y"] = mcp::JsonValue(r.size.y);
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["position"] = std::move(pos);
        j["size"] = std::move(sz);
        return j;
    }

    case Variant::RECT2I: {
        auto r = static_cast<Rect2i>(v);
        mcp::JsonValue pos(mcp::JsonValue::object_tag);
        pos["x"] = mcp::JsonValue(r.position.x);
        pos["y"] = mcp::JsonValue(r.position.y);
        mcp::JsonValue sz(mcp::JsonValue::object_tag);
        sz["x"] = mcp::JsonValue(r.size.x);
        sz["y"] = mcp::JsonValue(r.size.y);
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["position"] = std::move(pos);
        j["size"] = std::move(sz);
        return j;
    }

    case Variant::VECTOR3: {
        auto vec = static_cast<Vector3>(v);
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["x"] = mcp::JsonValue(vec.x);
        j["y"] = mcp::JsonValue(vec.y);
        j["z"] = mcp::JsonValue(vec.z);
        return j;
    }

    case Variant::VECTOR3I: {
        auto vec = static_cast<Vector3i>(v);
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["x"] = mcp::JsonValue(vec.x);
        j["y"] = mcp::JsonValue(vec.y);
        j["z"] = mcp::JsonValue(vec.z);
        return j;
    }

    case Variant::TRANSFORM2D: {
        auto t = static_cast<Transform2D>(v);
        mcp::JsonValue cols(mcp::JsonValue::array_tag);
        for (int i = 0; i < 3; i++) {
            mcp::JsonValue::Array col_arr;
            col_arr.push_back(mcp::JsonValue(t.columns[i].x));
            col_arr.push_back(mcp::JsonValue(t.columns[i].y));
            cols.PushBack(mcp::JsonValue(std::move(col_arr)));
        }
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["columns"] = std::move(cols);
        return j;
    }

    case Variant::VECTOR4: {
        auto vec = static_cast<Vector4>(v);
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["x"] = mcp::JsonValue(vec.x);
        j["y"] = mcp::JsonValue(vec.y);
        j["z"] = mcp::JsonValue(vec.z);
        j["w"] = mcp::JsonValue(vec.w);
        return j;
    }

    case Variant::VECTOR4I: {
        auto vec = static_cast<Vector4i>(v);
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["x"] = mcp::JsonValue(vec.x);
        j["y"] = mcp::JsonValue(vec.y);
        j["z"] = mcp::JsonValue(vec.z);
        j["w"] = mcp::JsonValue(vec.w);
        return j;
    }

    case Variant::PLANE: {
        auto p = static_cast<Plane>(v);
        mcp::JsonValue norm(mcp::JsonValue::object_tag);
        norm["x"] = mcp::JsonValue(p.normal.x);
        norm["y"] = mcp::JsonValue(p.normal.y);
        norm["z"] = mcp::JsonValue(p.normal.z);
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["normal"] = std::move(norm);
        j["d"] = mcp::JsonValue(p.d);
        return j;
    }

    case Variant::QUATERNION: {
        auto q = static_cast<Quaternion>(v);
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["x"] = mcp::JsonValue(q.x);
        j["y"] = mcp::JsonValue(q.y);
        j["z"] = mcp::JsonValue(q.z);
        j["w"] = mcp::JsonValue(q.w);
        return j;
    }

    case Variant::AABB: {
        auto aabb = static_cast<godot::AABB>(v);
        mcp::JsonValue pos(mcp::JsonValue::object_tag);
        pos["x"] = mcp::JsonValue(aabb.position.x);
        pos["y"] = mcp::JsonValue(aabb.position.y);
        pos["z"] = mcp::JsonValue(aabb.position.z);
        mcp::JsonValue sz(mcp::JsonValue::object_tag);
        sz["x"] = mcp::JsonValue(aabb.size.x);
        sz["y"] = mcp::JsonValue(aabb.size.y);
        sz["z"] = mcp::JsonValue(aabb.size.z);
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["position"] = std::move(pos);
        j["size"] = std::move(sz);
        return j;
    }

    case Variant::BASIS: {
        auto b = static_cast<Basis>(v);
        mcp::JsonValue rows(mcp::JsonValue::array_tag);
        for (int i = 0; i < 3; i++) {
            mcp::JsonValue::Array row_arr;
            row_arr.push_back(mcp::JsonValue(b.rows[i].x));
            row_arr.push_back(mcp::JsonValue(b.rows[i].y));
            row_arr.push_back(mcp::JsonValue(b.rows[i].z));
            rows.PushBack(mcp::JsonValue(std::move(row_arr)));
        }
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["rows"] = std::move(rows);
        return j;
    }

    case Variant::TRANSFORM3D: {
        auto t3 = static_cast<Transform3D>(v);
        mcp::JsonValue rows(mcp::JsonValue::array_tag);
        for (int i = 0; i < 3; i++) {
            mcp::JsonValue::Array row_arr;
            row_arr.push_back(mcp::JsonValue(t3.basis.rows[i].x));
            row_arr.push_back(mcp::JsonValue(t3.basis.rows[i].y));
            row_arr.push_back(mcp::JsonValue(t3.basis.rows[i].z));
            rows.PushBack(mcp::JsonValue(std::move(row_arr)));
        }
        mcp::JsonValue basis_j(mcp::JsonValue::object_tag);
        basis_j["rows"] = std::move(rows);
        mcp::JsonValue orig(mcp::JsonValue::object_tag);
        orig["x"] = mcp::JsonValue(t3.origin.x);
        orig["y"] = mcp::JsonValue(t3.origin.y);
        orig["z"] = mcp::JsonValue(t3.origin.z);
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["basis"] = std::move(basis_j);
        j["origin"] = std::move(orig);
        return j;
    }

    case Variant::PROJECTION: {
        auto p = static_cast<Projection>(v);
        mcp::JsonValue cols(mcp::JsonValue::array_tag);
        for (int i = 0; i < 4; i++) {
            mcp::JsonValue::Array col_arr;
            col_arr.push_back(mcp::JsonValue(p.columns[i].x));
            col_arr.push_back(mcp::JsonValue(p.columns[i].y));
            col_arr.push_back(mcp::JsonValue(p.columns[i].z));
            col_arr.push_back(mcp::JsonValue(p.columns[i].w));
            cols.PushBack(mcp::JsonValue(std::move(col_arr)));
        }
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["columns"] = std::move(cols);
        return j;
    }

    case Variant::COLOR: {
        auto c = static_cast<Color>(v);
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["r"] = mcp::JsonValue(static_cast<double>(c.r));
        j["g"] = mcp::JsonValue(static_cast<double>(c.g));
        j["b"] = mcp::JsonValue(static_cast<double>(c.b));
        j["a"] = mcp::JsonValue(static_cast<double>(c.a));
        return j;
    }

    case Variant::STRING_NAME: {
        godot::StringName sn = v.operator godot::StringName();
        godot::String s(sn);
        return mcp::JsonValue(to_std_string(s));
    }

    case Variant::NODE_PATH: {
        godot::NodePath np = v.operator godot::NodePath();
        godot::String s(np);
        return mcp::JsonValue(to_std_string(s));
    }

    case Variant::RID: {
        godot::RID rid = v.operator godot::RID();
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        j["id"] = mcp::JsonValue(static_cast<int64_t>(rid.get_id()));
        return j;
    }

    case Variant::OBJECT: {
        godot::Object* obj = v.operator godot::Object*();
        if (obj) {
            mcp::JsonValue j(mcp::JsonValue::object_tag);
            j["class"] = mcp::JsonValue(to_std_string(obj->get_class()));

            auto props = obj->get_property_list();
            for (int64_t i = 0; i < props.size(); i++) {
                godot::Dictionary prop = props[i];
                godot::String prop_name = prop["name"];
                std::string name_std = to_std_string(prop_name);

                if (name_std.empty() || name_std[0] == '_' || name_std == "script") continue;

                int64_t usage = static_cast<int64_t>(prop["usage"]);
                if (!(usage & PROPERTY_USAGE_STORAGE)) continue;
                if (usage & (PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP | PROPERTY_USAGE_CATEGORY)) continue;

                godot::Variant val = obj->get(prop_name);
                j[name_std] = serialize(val);
            }

            return j;
        }
        return mcp::JsonValue();
    }

    case Variant::CALLABLE: {
        godot::Callable c = v.operator godot::Callable();
        godot::Object* obj = c.get_object();
        if (obj) {
            mcp::JsonValue j(mcp::JsonValue::object_tag);
            j["object_id"] = mcp::JsonValue(static_cast<int64_t>(obj->get_instance_id()));
            j["method"] = mcp::JsonValue(to_std_string(godot::String(c.get_method())));
            return j;
        }
        return mcp::JsonValue();
    }

    case Variant::SIGNAL: {
        godot::Signal sig = v.operator godot::Signal();
        godot::Object* obj = sig.get_object();
        if (obj) {
            mcp::JsonValue j(mcp::JsonValue::object_tag);
            j["object_id"] = mcp::JsonValue(static_cast<int64_t>(obj->get_instance_id()));
            j["signal"] = mcp::JsonValue(to_std_string(godot::String(sig.get_name())));
            return j;
        }
        return mcp::JsonValue();
    }

    case Variant::DICTIONARY: {
        godot::Dictionary d = v.operator godot::Dictionary();
        auto keys = d.keys();
        mcp::JsonValue j(mcp::JsonValue::object_tag);
        for (int i = 0; i < keys.size(); i++) {
            auto key = keys[i];
            j[to_std_string(key.operator godot::String())] = serialize(d[key]);
        }
        return j;
    }

    case Variant::ARRAY: {
        godot::Array a = v.operator godot::Array();
        mcp::JsonValue j(mcp::JsonValue::array_tag);
        for (int i = 0; i < a.size(); i++) {
            j.PushBack(serialize(a[i]));
        }
        return j;
    }

    case Variant::PACKED_BYTE_ARRAY: {
        auto a = static_cast<PackedByteArray>(v);
        mcp::JsonValue j(mcp::JsonValue::array_tag);
        for (int i = 0; i < a.size(); i++) {
            j.PushBack(mcp::JsonValue(static_cast<int>(a[i])));
        }
        return j;
    }

    case Variant::PACKED_INT32_ARRAY: {
        auto a = static_cast<PackedInt32Array>(v);
        mcp::JsonValue j(mcp::JsonValue::array_tag);
        for (int i = 0; i < a.size(); i++) {
            j.PushBack(mcp::JsonValue(a[i]));
        }
        return j;
    }

    case Variant::PACKED_INT64_ARRAY: {
        auto a = static_cast<PackedInt64Array>(v);
        mcp::JsonValue j(mcp::JsonValue::array_tag);
        for (int i = 0; i < a.size(); i++) {
            j.PushBack(mcp::JsonValue(a[i]));
        }
        return j;
    }

    case Variant::PACKED_FLOAT32_ARRAY: {
        auto a = static_cast<PackedFloat32Array>(v);
        mcp::JsonValue j(mcp::JsonValue::array_tag);
        for (int i = 0; i < a.size(); i++) {
            j.PushBack(mcp::JsonValue(static_cast<double>(a[i])));
        }
        return j;
    }

    case Variant::PACKED_FLOAT64_ARRAY: {
        auto a = static_cast<PackedFloat64Array>(v);
        mcp::JsonValue j(mcp::JsonValue::array_tag);
        for (int i = 0; i < a.size(); i++) {
            j.PushBack(mcp::JsonValue(a[i]));
        }
        return j;
    }

    case Variant::PACKED_STRING_ARRAY: {
        auto a = static_cast<PackedStringArray>(v);
        mcp::JsonValue j(mcp::JsonValue::array_tag);
        for (int i = 0; i < a.size(); i++) {
            j.PushBack(mcp::JsonValue(to_std_string(a[i])));
        }
        return j;
    }

    case Variant::PACKED_VECTOR2_ARRAY: {
        auto a = static_cast<PackedVector2Array>(v);
        mcp::JsonValue j(mcp::JsonValue::array_tag);
        for (int i = 0; i < a.size(); i++) {
            auto vec = a[i];
            mcp::JsonValue elem(mcp::JsonValue::object_tag);
            elem["x"] = mcp::JsonValue(vec.x);
            elem["y"] = mcp::JsonValue(vec.y);
            j.PushBack(std::move(elem));
        }
        return j;
    }

    case Variant::PACKED_VECTOR3_ARRAY: {
        auto a = static_cast<PackedVector3Array>(v);
        mcp::JsonValue j(mcp::JsonValue::array_tag);
        for (int i = 0; i < a.size(); i++) {
            auto vec = a[i];
            mcp::JsonValue elem(mcp::JsonValue::object_tag);
            elem["x"] = mcp::JsonValue(vec.x);
            elem["y"] = mcp::JsonValue(vec.y);
            elem["z"] = mcp::JsonValue(vec.z);
            j.PushBack(std::move(elem));
        }
        return j;
    }

    case Variant::PACKED_COLOR_ARRAY: {
        auto a = static_cast<PackedColorArray>(v);
        mcp::JsonValue j(mcp::JsonValue::array_tag);
        for (int i = 0; i < a.size(); i++) {
            auto c = a[i];
            mcp::JsonValue elem(mcp::JsonValue::object_tag);
            elem["r"] = mcp::JsonValue(static_cast<double>(c.r));
            elem["g"] = mcp::JsonValue(static_cast<double>(c.g));
            elem["b"] = mcp::JsonValue(static_cast<double>(c.b));
            elem["a"] = mcp::JsonValue(static_cast<double>(c.a));
            j.PushBack(std::move(elem));
        }
        return j;
    }

    case Variant::PACKED_VECTOR4_ARRAY: {
        auto a = static_cast<PackedVector4Array>(v);
        mcp::JsonValue j(mcp::JsonValue::array_tag);
        for (int i = 0; i < a.size(); i++) {
            auto vec = a[i];
            mcp::JsonValue elem(mcp::JsonValue::object_tag);
            elem["x"] = mcp::JsonValue(vec.x);
            elem["y"] = mcp::JsonValue(vec.y);
            elem["z"] = mcp::JsonValue(vec.z);
            elem["w"] = mcp::JsonValue(vec.w);
            j.PushBack(std::move(elem));
        }
        return j;
    }

    default:
        return mcp::JsonValue(str_from_variant(v));
    }
}

godot::Variant VariantJson::deserialize(const mcp::JsonValue& j, const std::string& type_hint) {
    if (!type_hint.empty()) {
        auto type = parse_type_hint(type_hint);
        if (type.has_value()) {
            return deserialize_typed(j, type.value());
        }
        // Not a known Variant type — try as Godot class name (e.g., "InputEventKey")
        if (j.IsObject()) {
            godot::Variant obj = deserialize_as_object(j, type_hint);
            if (obj.get_type() != godot::Variant::NIL) {
                return obj;
            }
        }
    }
    return deserialize_inferred(j);
}

} // namespace godot_self_driving
