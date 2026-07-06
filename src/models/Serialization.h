// =============================================================================
//  Serialization.h - Platform-agnostic property bag.
//
//  Models serialize into a PropertyBag (a typed key/value map). The persistence
//  layer (DatabaseService) is the only place that knows how to turn a
//  PropertyBag into an AcDbResbuf chain for an AcDbXrecord, keeping the model
//  layer free of any AcDb dependency and unit-testable off-CAD.
// =============================================================================
#pragma once

#include <map>
#include <string>
#include <vector>
#include <cstdint>

namespace electrical {

struct Point3 {
    double x = 0.0, y = 0.0, z = 0.0;
    Point3() = default;
    Point3(double x_, double y_, double z_ = 0.0) : x(x_), y(y_), z(z_) {}
};

// A single stored value. Kept deliberately small - covers everything the models
// need without dragging in a variant library.
struct Property {
    enum class Kind : uint8_t { Int, Real, Text, Point } kind = Kind::Int;
    int64_t     i = 0;
    double      r = 0.0;
    std::string s;      // stored UTF-8; converted to ACHAR at the resbuf edge
    Point3      p;
};

class PropertyBag {
public:
    void putInt(const std::string& k, int64_t v)       { Property pr; pr.kind = Property::Kind::Int;  pr.i = v; map_[k] = pr; }
    void putReal(const std::string& k, double v)        { Property pr; pr.kind = Property::Kind::Real; pr.r = v; map_[k] = pr; }
    void putText(const std::string& k, std::string v)   { Property pr; pr.kind = Property::Kind::Text; pr.s = std::move(v); map_[k] = pr; }
    void putPoint(const std::string& k, const Point3& v){ Property pr; pr.kind = Property::Kind::Point; pr.p = v; map_[k] = pr; }

    template <typename E> void putEnum(const std::string& k, E v) {
        putInt(k, static_cast<int64_t>(v));
    }

    int64_t     getInt (const std::string& k, int64_t def = 0)             const { auto it = map_.find(k); return it != map_.end() ? it->second.i : def; }
    double      getReal(const std::string& k, double def = 0.0)            const { auto it = map_.find(k); return it != map_.end() ? it->second.r : def; }
    std::string getText(const std::string& k, std::string def = {})       const { auto it = map_.find(k); return it != map_.end() ? it->second.s : def; }
    Point3      getPoint(const std::string& k, const Point3& def = {})     const { auto it = map_.find(k); return it != map_.end() ? it->second.p : def; }

    template <typename E> E getEnum(const std::string& k, E def) const {
        auto it = map_.find(k);
        return it != map_.end() ? static_cast<E>(it->second.i) : def;
    }

    bool has(const std::string& k) const { return map_.count(k) != 0; }

    const std::map<std::string, Property>& entries() const { return map_; }
    std::map<std::string, Property>&       entries()       { return map_; }

private:
    std::map<std::string, Property> map_;
};

// Every persisted model implements this contract.
class ISerializable {
public:
    virtual ~ISerializable() = default;
    virtual void serialize(PropertyBag& bag) const = 0;
    virtual void deserialize(const PropertyBag& bag) = 0;
    // Stable type tag used to reconstruct the right subclass on load.
    virtual std::string typeTag() const = 0;
};

} // namespace electrical
