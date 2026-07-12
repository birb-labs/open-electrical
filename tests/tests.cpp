// =============================================================================
//  tests.cpp - Unit tests for the BRX-free core: polygon math, the lumen-method
//  lighting calculation, and Room serialization. Built into the `oe_tests`
//  target and run by `cmake --build build --target check`.
// =============================================================================
#include "test_framework.h"

#include "models/Room.h"
#include "models/Types.h"
#include "models/ProjectData.h"
#include "models/Panel.h"
#include "models/Outlet.h"
#include "models/LightPoint.h"
#include "utilities/PolyMath.h"
#include "utilities/ElementOutline.h"
#include "services/LightingCalculator.h"
#include "services/CircuitDistributor.h"
#include "services/ReportGenerator.h"
#include "services/WireCounts.h"

using namespace electrical;

namespace {

// A closed rectangle [0,w] x [0,h] (drawing units).
std::vector<Point3> rect(double w, double h) {
    return { {0, 0, 0}, {w, 0, 0}, {w, h, 0}, {0, h, 0} };
}

// An L-shape: a wxh rectangle with the top-right quadrant removed.
std::vector<Point3> lShape(double w, double h) {
    return { {0, 0, 0}, {w, 0, 0}, {w, h / 2, 0},
             {w / 2, h / 2, 0}, {w / 2, h, 0}, {0, h, 0} };
}

bool allInside(const std::vector<Point3>& pts, const std::vector<Point3>& poly) {
    for (const auto& p : pts)
        if (!polymath::pointInPolygon(p, poly)) return false;
    return true;
}

} // namespace

// ---- Polygon area ----------------------------------------------------------
TEST_CASE(area_square) {
    CHECK_NEAR(polymath::polygonArea(rect(4, 4)), 16.0, 1e-9);
}

TEST_CASE(area_rectangle) {
    CHECK_NEAR(polymath::polygonArea(rect(10, 6)), 60.0, 1e-9);
}

TEST_CASE(area_lshape) {
    // Full 8x8 (64) minus the removed 4x4 top-right quadrant (16) = 48.
    CHECK_NEAR(polymath::polygonArea(lShape(8, 8)), 48.0, 1e-9);
}

TEST_CASE(area_degenerate) {
    CHECK_NEAR(polymath::polygonArea({ {0, 0, 0}, {1, 1, 0} }), 0.0, 1e-9);
}

TEST_CASE(room_area_meters) {
    Room r;
    r.boundary = rect(5, 4);
    r.recomputeArea(Unit::Meters);
    CHECK_NEAR(r.areaM2, 20.0, 1e-9);
}

// ---- Point in polygon -------------------------------------------------------
TEST_CASE(pip_inside_outside) {
    const auto sq = rect(10, 10);
    CHECK(polymath::pointInPolygon({ 5, 5, 0 }, sq));
    CHECK(!polymath::pointInPolygon({ 15, 5, 0 }, sq));
    CHECK(!polymath::pointInPolygon({ -1, 5, 0 }, sq));
    CHECK(!polymath::pointInPolygon({ 5, 20, 0 }, sq));
}

TEST_CASE(pip_lshape_notch) {
    const auto l = lShape(8, 8);
    CHECK(polymath::pointInPolygon({ 2, 2, 0 }, l));   // solid lower-left
    CHECK(polymath::pointInPolygon({ 2, 6, 0 }, l));   // solid upper-left
    CHECK(!polymath::pointInPolygon({ 6, 6, 0 }, l));  // removed top-right notch
}

// ---- Grid distribution with clipping ---------------------------------------
TEST_CASE(distribute_rectangle_exact_count) {
    const auto poly = rect(10, 6);
    for (int n : { 1, 4, 6, 9, 12 }) {
        const auto pts = polymath::distributeInPolygon(poly, n, 0.0);
        CHECK_EQ(static_cast<int>(pts.size()), n);
        CHECK(allInside(pts, poly));
    }
}

TEST_CASE(distribute_lshape_all_inside) {
    const auto poly = lShape(10, 10);
    for (int n : { 3, 6, 8, 12 }) {
        const auto pts = polymath::distributeInPolygon(poly, n, 0.0);
        CHECK_EQ(static_cast<int>(pts.size()), n);
        CHECK(allInside(pts, poly));   // none may fall in the removed notch
    }
}

TEST_CASE(distribute_carries_z) {
    const auto pts = polymath::distributeInPolygon(rect(4, 4), 1, 2.8);
    CHECK_EQ(static_cast<int>(pts.size()), 1);
    if (!pts.empty()) CHECK_NEAR(pts[0].z, 2.8, 1e-9);
}

TEST_CASE(distribute_degenerate_empty) {
    CHECK(polymath::distributeInPolygon({ {0, 0, 0}, {1, 1, 0} }, 4).empty());
    CHECK(polymath::distributeInPolygon(rect(4, 4), 0).empty());
}

// ---- Target illuminance (NBR 5413 range + contrast) -------------------------
TEST_CASE(target_lux_contrast) {
    CHECK_NEAR(LightingCalculator::targetLux(RoomUsage::Office, TaskContrast::Low),    300.0, 1e-9);
    CHECK_NEAR(LightingCalculator::targetLux(RoomUsage::Office, TaskContrast::Medium), 500.0, 1e-9);
    CHECK_NEAR(LightingCalculator::targetLux(RoomUsage::Office, TaskContrast::High),   750.0, 1e-9);
    // Single-arg overload = mid of the range.
    CHECK_NEAR(LightingCalculator::targetLux(RoomUsage::Office), 500.0, 1e-9);
}

// ---- Lumen method -----------------------------------------------------------
TEST_CASE(lumen_method_known_count) {
    Room r;
    r.boundary = rect(5, 4);          // 20 m2 in meters
    r.usage    = RoomUsage::Office;   // E range mid = 500 lux
    r.contrast = TaskContrast::Medium;

    LightingInput in;
    in.lumensPerLuminaire = 2000.0;
    in.wattsPerLuminaire  = 20.0;
    in.maintenanceFactor  = 0.80;
    in.cuOverride         = 0.50;     // deterministic (skip room-index estimate)
    in.contrast           = TaskContrast::Medium;

    const LightingResult res = LightingCalculator::calculate(r, Unit::Meters, in);
    // phi = 500 * 20 / (0.5 * 0.8) = 25000; N = ceil(25000 / 2000) = 13.
    CHECK_NEAR(res.requiredLux, 500.0, 1e-9);
    CHECK_NEAR(res.areaM2, 20.0, 1e-9);
    CHECK_EQ(res.luminaireCount, 13);
    CHECK_EQ(static_cast<int>(res.positions.size()), 13);
}

TEST_CASE(lumen_method_contrast_raises_count) {
    Room r;
    r.boundary = rect(5, 4);
    r.usage    = RoomUsage::Office;

    LightingInput in;
    in.lumensPerLuminaire = 2000.0;
    in.maintenanceFactor  = 0.80;
    in.cuOverride         = 0.50;

    in.contrast = TaskContrast::Low;      // E = 300
    const int nLow = LightingCalculator::calculate(r, Unit::Meters, in).luminaireCount;
    in.contrast = TaskContrast::High;     // E = 750
    const int nHigh = LightingCalculator::calculate(r, Unit::Meters, in).luminaireCount;
    CHECK(nHigh > nLow);
}

// ---- Room serialization round-trip -----------------------------------------
TEST_CASE(room_serialization_roundtrip) {
    Room a;
    a.id = 7;
    a.name = "Sala 1";
    a.usage = RoomUsage::Office;
    a.usageDetail = "Leitura";
    a.function = RoomFunction::Commercial;
    a.ceilingHeight = 3.10;
    a.workPlaneHeight = 0.80;
    a.ceilingReflectance = 0.80;
    a.wallReflectance = 0.60;
    a.floorReflectance = 0.30;
    a.contrast = TaskContrast::High;
    a.maintenanceFactor = 0.70;
    a.utilizationFactor = 0.55;
    a.boundary = rect(5, 4);

    PropertyBag bag;
    a.serialize(bag);

    Room b;
    b.deserialize(bag);

    CHECK_EQ(b.id, 7);
    CHECK(b.name == "Sala 1");
    CHECK(b.usage == RoomUsage::Office);
    CHECK(b.usageDetail == "Leitura");
    CHECK(b.function == RoomFunction::Commercial);
    CHECK_NEAR(b.ceilingHeight, 3.10, 1e-9);
    CHECK_NEAR(b.workPlaneHeight, 0.80, 1e-9);
    CHECK_NEAR(b.ceilingReflectance, 0.80, 1e-9);
    CHECK_NEAR(b.wallReflectance, 0.60, 1e-9);
    CHECK_NEAR(b.floorReflectance, 0.30, 1e-9);
    CHECK(b.contrast == TaskContrast::High);
    CHECK_NEAR(b.maintenanceFactor, 0.70, 1e-9);
    CHECK_NEAR(b.utilizationFactor, 0.55, 1e-9);
    CHECK_EQ(b.boundary.size(), a.boundary.size());
}

TEST_CASE(room_deserialize_defaults_for_old_drawing) {
    // A bag without the new keys must yield the documented defaults.
    PropertyBag bag;
    bag.putText("name", "Old");
    Room r;
    r.deserialize(bag);
    CHECK_NEAR(r.ceilingReflectance, 0.70, 1e-9);
    CHECK_NEAR(r.wallReflectance, 0.50, 1e-9);
    CHECK_NEAR(r.floorReflectance, 0.20, 1e-9);
    CHECK(r.contrast == TaskContrast::Medium);
    CHECK_NEAR(r.maintenanceFactor, 0.80, 1e-9);
}

// ---- Regression: the user's bedroom scenario (was over-dimensioned to 3) ----
// Bedroom ~8 m2, E = 150 lux, CU = 0.6, MF = 0.8. Hand calc:
//   phi = 150 * 8 / (0.6 * 0.8) = 2500 lm.
//   2400 lm luminaire -> ceil(2500/2400) = 2.
//   2600 lm luminaire -> ceil(2500/2600) = 1.
TEST_CASE(lumen_method_bedroom_2400_gives_two) {
    Room r;
    r.boundary = rect(4, 2);          // 8 m2 in meters
    LightingInput in;
    in.targetLuxOverride  = 150.0;    // explicit E
    in.cuOverride         = 0.60;     // explicit CU
    in.maintenanceFactor  = 0.80;
    in.lumensPerLuminaire = 2400.0;
    const LightingResult res = LightingCalculator::calculate(r, Unit::Meters, in);
    CHECK_NEAR(res.totalLumensNeeded, 2500.0, 1e-6);
    CHECK_EQ(res.luminaireCount, 2);
    CHECK_EQ(static_cast<int>(res.positions.size()), 2);
}

TEST_CASE(lumen_method_bedroom_2600_gives_one) {
    Room r;
    r.boundary = rect(4, 2);
    LightingInput in;
    in.targetLuxOverride  = 150.0;
    in.cuOverride         = 0.60;
    in.maintenanceFactor  = 0.80;
    in.lumensPerLuminaire = 2600.0;
    const LightingResult res = LightingCalculator::calculate(r, Unit::Meters, in);
    CHECK_EQ(res.luminaireCount, 1);
}

// The formula must match ceil((E*A)/(CU*MF*flux)) exactly across a sweep.
TEST_CASE(lumen_method_formula_matches_ceil) {
    const double E = 150.0, A = 8.0, CU = 0.6, MF = 0.8;
    Room r; r.boundary = rect(4, 2);   // A = 8 m2
    for (double flux : { 1500.0, 2000.0, 2400.0, 2600.0, 3200.0 }) {
        LightingInput in;
        in.targetLuxOverride  = E;
        in.cuOverride         = CU;
        in.maintenanceFactor  = MF;
        in.lumensPerLuminaire = flux;
        const int expected = static_cast<int>(std::ceil((E * A) / (CU * MF * flux)));
        CHECK_EQ(LightingCalculator::calculate(r, Unit::Meters, in).luminaireCount,
                 std::max(1, expected));
    }
}

// Auto CU (cuOverride = 0) must land in a realistic band for a small room, not
// the old ~0.29 that doubled the count.
TEST_CASE(auto_cu_is_realistic) {
    Room r;
    r.boundary = rect(4, 2);
    r.usage    = RoomUsage::Bedroom;   // E = 150 (medium)
    LightingInput in;
    in.cuOverride         = 0.0;       // auto
    in.ceilingReflectance = 0.70;
    in.maintenanceFactor  = 0.80;
    in.lumensPerLuminaire = 2400.0;
    const LightingResult res = LightingCalculator::calculate(r, Unit::Meters, in);
    CHECK(res.utilizationFactor > 0.45);
    CHECK(res.utilizationFactor < 0.85);
    CHECK(res.luminaireCount <= 2);    // was 3 before the fix
}

// ---- Conduit path trimming (router: leave/enter element footprints) ---------
static double dist2d(const Point3& a, const Point3& b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

TEST_CASE(trim_straight_run_leaves_both_ends) {
    // A -> B along +X, length 10; trim 0.2 at A and 0.3 at B.
    const std::vector<Point3> path = { {0, 0, 0}, {10, 0, 0} };
    const auto out = polymath::trimPathEnds(path, 0.2, 0.3);
    CHECK_EQ(static_cast<int>(out.size()), 2);
    // New start sits 0.2 from A toward B; new end 0.3 from B toward A.
    CHECK_NEAR(dist2d(out.front(), path.front()), 0.2, 1e-9);
    CHECK_NEAR(dist2d(out.back(),  path.back()),  0.3, 1e-9);
    CHECK_NEAR(out.front().x, 0.2, 1e-9);
    CHECK_NEAR(out.back().x,  9.7, 1e-9);
}

TEST_CASE(trim_L_run_keeps_corner) {
    // L-shaped run A(0,0) -> corner(10,0) -> B(10,5).
    const std::vector<Point3> path = { {0, 0, 0}, {10, 0, 0}, {10, 5, 0} };
    const auto out = polymath::trimPathEnds(path, 0.25, 0.33);
    CHECK_EQ(static_cast<int>(out.size()), 3);         // corner preserved
    CHECK_NEAR(out[1].x, 10.0, 1e-9);                  // corner unchanged
    CHECK_NEAR(out[1].y, 0.0, 1e-9);
    // Start trimmed along the first (horizontal) segment; end along the last one.
    CHECK_NEAR(out.front().x, 0.25, 1e-9);
    CHECK_NEAR(out.front().y, 0.0, 1e-9);
    CHECK_NEAR(out.back().x, 10.0, 1e-9);
    CHECK_NEAR(out.back().y, 5.0 - 0.33, 1e-9);
}

TEST_CASE(trim_overlapping_elements_keeps_run) {
    // Total length 0.4 < rStart+rEnd (0.3+0.3): path returned unchanged.
    const std::vector<Point3> path = { {0, 0, 0}, {0.4, 0, 0} };
    const auto out = polymath::trimPathEnds(path, 0.3, 0.3);
    CHECK_EQ(static_cast<int>(out.size()), 2);
    CHECK_NEAR(out.front().x, 0.0, 1e-9);
    CHECK_NEAR(out.back().x, 0.4, 1e-9);
}

TEST_CASE(trim_preserves_z) {
    // Ceiling run carries a constant Z; trimming must keep it.
    const std::vector<Point3> path = { {0, 0, 2.8}, {10, 0, 2.8} };
    const auto out = polymath::trimPathEnds(path, 0.2, 0.2);
    CHECK_NEAR(out.front().z, 2.8, 1e-9);
    CHECK_NEAR(out.back().z, 2.8, 1e-9);
}

// ---- Element outline connection rule ----------------------------------------
TEST_CASE(outline_light_attaches_on_circle) {
    // A light at the origin (no rotation); a far +X target attaches on its circle
    // rim at radius kLightR (= 1.6 * 0.09 = 0.144), on the +X side.
    const Point3 p = outline::nearestAttachPoint(outline::Kind::Light, 1,
                                                 {0, 0, 0}, 0.0, {10, 0, 0});
    CHECK_NEAR(p.x, 0.144, 1e-6);
    CHECK_NEAR(p.y, 0.0, 1e-6);
}

TEST_CASE(outline_panel_attaches_on_right_edge) {
    // 3:1 panel: width = 6*0.09 = 0.54, so the right edge sits at x = 0.27.
    const Point3 p = outline::nearestAttachPoint(outline::Kind::Panel, 1,
                                                 {0, 0, 0}, 0.0, {10, 0.09, 0});
    CHECK_NEAR(p.x, 0.27, 1e-6);
}

TEST_CASE(outline_rotation_is_applied) {
    // Light placed at (5,5), rotated 90 deg; a far +Y target attaches at radius
    // kLightR straight above the origin.
    const double halfPi = 1.5707963267948966;
    const Point3 p = outline::nearestAttachPoint(outline::Kind::Light, 1,
                                                 {5, 5, 0}, halfPi, {5, 100, 0});
    CHECK_NEAR(p.x, 5.0, 1e-6);
    CHECK_NEAR(p.y, 5.144, 1e-6);
}

TEST_CASE(outline_outlet_modules_stack) {
    // More modules -> the apex (a far +Y target) attaches higher up.
    const Point3 p1 = outline::nearestAttachPoint(outline::Kind::Outlet, 1,
                                                  {0, 0, 0}, 0.0, {0, 100, 0});
    const Point3 p3 = outline::nearestAttachPoint(outline::Kind::Outlet, 3,
                                                  {0, 0, 0}, 0.0, {0, 100, 0});
    CHECK(p3.y > p1.y + 0.2);
}

TEST_CASE(outline_outlet_apex_is_rounded) {
    // A far +Y target attaches at the ROUNDED tip, which sits below the sharp
    // apex (the fillet clips the point) - proving the outline honours the arcs of
    // the outlet block instead of a sharp triangle.
    const double th = 0.09 * 1.7320508075688772;   // kOutletBW * sqrt(3)
    const double sharpApexY = 0.045 + th;          // kWallGap + th
    const Point3 p = outline::nearestAttachPoint(outline::Kind::Outlet, 1,
                                                 {0, 0, 0}, 0.0, {0, 100, 0});
    CHECK(p.y < sharpApexY - 0.005);   // clipped below the sharp tip
    CHECK(p.y > sharpApexY - 0.05);    // but only slightly (levemente arredondado)
}

// ---- Wiring distribution: conductors per circuit (NBR 5410) -----------------
TEST_CASE(wire_counts_per_circuit_type) {
    CHECK_EQ(conductorsFor(CircuitType::GeneralOutlets).phase,   1);
    CHECK_EQ(conductorsFor(CircuitType::GeneralOutlets).neutral, 1);
    CHECK_EQ(conductorsFor(CircuitType::GeneralOutlets).ground,  1);
    CHECK_EQ(conductorsFor(CircuitType::GeneralOutlets).retorno, 0);
    CHECK_EQ(conductorsFor(CircuitType::Lighting).ground,  0);
    CHECK_EQ(conductorsFor(CircuitType::Lighting).retorno, 1);
}

TEST_CASE(wire_counts_sum_across_shared_circuits) {
    // Two outlet circuits sharing one conduit -> "2F 2N 2PE" (devices don't
    // multiply conductors; distinct circuits do).
    ConductorSet s;
    s += conductorsFor(CircuitType::GeneralOutlets);
    s += conductorsFor(CircuitType::GeneralOutlets);
    CHECK_EQ(s.phase,   2);
    CHECK_EQ(s.neutral, 2);
    CHECK_EQ(s.ground,  2);
    CHECK_EQ(s.retorno, 0);
}

// ---- Panel <-> circuit linkage & reports (regression) ----------------------
namespace {

// Counts non-overlapping occurrences of `needle` in `hay`.
int countOccurrences(const std::string& hay, const std::string& needle) {
    int n = 0;
    for (size_t pos = 0; (pos = hay.find(needle, pos)) != std::string::npos; pos += needle.size())
        ++n;
    return n;
}

// A panel with an allocated id, appended to the project. Returns its id.
int addPanel(ProjectData& p, const std::string& name, bool isMain) {
    auto panel = std::make_unique<Panel>();
    panel->id = p.allocPanelId();
    panel->name = name;
    panel->isMain = isMain;
    panel->handle = "PNL" + std::to_string(panel->id);
    const int id = panel->id;
    p.elements.push_back(std::move(panel));
    return id;
}

// A lighting point with a handle and load, unassigned to a circuit.
void addLight(ProjectData& p, const std::string& handle, double va) {
    auto l = std::make_unique<LightPoint>();
    l->handle = handle;
    l->powerVA = va;
    p.elements.push_back(std::move(l));
}

} // namespace

TEST_CASE(distribute_populates_panel_circuit_list) {
    ProjectData p;
    const int mainId = addPanel(p, "QGBT", true);
    addLight(p, "L1", 100.0);
    addLight(p, "L2", 100.0);

    const DistributionResult r = CircuitDistributor::distribute(p);
    CHECK(r.circuitsCreated >= 1);

    // Every created circuit points at the main panel...
    for (const auto& c : p.circuits) CHECK_EQ(c.panelId, mainId);

    // ...and the panel's circuit list mirrors exactly those circuits.
    Panel* main = p.findPanel(mainId);
    CHECK(main != nullptr);
    CHECK_EQ(static_cast<int>(main->circuitIds.size()),
             static_cast<int>(p.circuits.size()));
}

TEST_CASE(single_line_lists_each_circuit_once_under_its_panel) {
    ProjectData p;
    const int aId = addPanel(p, "QGBT", true);
    const int bId = addPanel(p, "QD-1", false);

    Circuit c1; c1.id = p.allocCircuitId(); c1.name = "CKT-A"; c1.panelId = aId;
    Circuit c2; c2.id = p.allocCircuitId(); c2.name = "CKT-B"; c2.panelId = bId;
    p.circuits.push_back(c1);
    p.circuits.push_back(c2);

    const std::string sld = ReportGenerator::singleLineDiagram(p);
    // Before the fix every circuit was printed under EVERY panel (2x here).
    CHECK_EQ(countOccurrences(sld, "CKT-A"), 1);
    CHECK_EQ(countOccurrences(sld, "CKT-B"), 1);
}

TEST_CASE(single_line_orphan_circuit_falls_under_main) {
    ProjectData p;
    addPanel(p, "QGBT", true);
    Circuit c; c.id = p.allocCircuitId(); c.name = "CKT-X"; c.panelId = 999;  // no such panel
    p.circuits.push_back(c);

    const std::string sld = ReportGenerator::singleLineDiagram(p);
    CHECK_EQ(countOccurrences(sld, "CKT-X"), 1);   // shown once, attributed to main
}

// ---- Voltage drop (simplified NBR 5410) ------------------------------------
TEST_CASE(voltage_drop_scales_with_length) {
    ProjectData p;
    p.settings.phases = Phases::SinglePhase;
    p.settings.voltageLineToNeutral = 220.0;

    Circuit c; c.id = p.allocCircuitId(); c.panelId = 1;
    c.connectedLoadVA = 2200.0;        // I = 10 A at 220 V
    c.phaseConductorMM2 = 2.5;
    p.circuits.push_back(c);

    // Two 10 m conduits carrying the circuit -> 20 m one-way length.
    for (int i = 0; i < 2; ++i) {
        Conduit cd; cd.id = p.allocConduitId(); cd.circuitIds = { c.id }; cd.lengthM = 10.0;
        p.conduits.push_back(cd);
    }

    CircuitDistributor::computeVoltageDrops(p);
    // dV = 2 * 0.0172 * 20 * 10 / 2.5 = 2.752 V -> 1.251 % of 220 V.
    CHECK_NEAR(p.circuits[0].voltageDropPct, 1.251, 0.05);

    // Doubling the length doubles the drop.
    for (auto& cd : p.conduits) cd.lengthM = 20.0;
    CircuitDistributor::computeVoltageDrops(p);
    CHECK_NEAR(p.circuits[0].voltageDropPct, 2.502, 0.05);
}

TEST_CASE(voltage_drop_zero_without_conduits) {
    ProjectData p;
    Circuit c; c.id = p.allocCircuitId(); c.connectedLoadVA = 1000.0; c.phaseConductorMM2 = 2.5;
    p.circuits.push_back(c);
    CircuitDistributor::computeVoltageDrops(p);
    CHECK_NEAR(p.circuits[0].voltageDropPct, 0.0, 1e-9);
}

// ---- Flush panel outline is recessed --------------------------------------
TEST_CASE(flush_panel_outline_is_recessed) {
    const double kSwR = 0.09, h = 2.0 * kSwR;         // 3:1 panel height = 0.18
    // Attach from far above: surface panel's top edge sits at y = h; a flush panel
    // is recessed 0.75h, so its top edge sits at 0.25h - strictly lower.
    const Point3 surf = outline::nearestAttachPoint(outline::Kind::Panel, 1,
                            {0, 0, 0}, 0.0, {0, 100, 0}, /*flushPanel=*/false);
    const Point3 flush = outline::nearestAttachPoint(outline::Kind::Panel, 1,
                            {0, 0, 0}, 0.0, {0, 100, 0}, /*flushPanel=*/true);
    CHECK_NEAR(surf.y,  h,         1e-6);
    CHECK_NEAR(flush.y, 0.25 * h,  1e-6);
    CHECK(flush.y < surf.y);
}

int main() {
    return tf::runAll();
}
