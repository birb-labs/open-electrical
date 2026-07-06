// =============================================================================
//  ReportGenerator.h - Produces the four project documents.
//
//  Each generator returns the document as plain text/CSV so it can be inserted
//  into the drawing as an MText table, written to a file, or shown in the
//  wxWidgets report viewer. Table drawing into model space is done by the
//  ReportCommands layer using these strings.
// =============================================================================
#pragma once

#include "models/ProjectData.h"
#include <string>
#include <vector>

namespace electrical {

struct BomLine {
    std::string item;
    std::string unit;
    double      quantity = 0.0;
};

class ReportGenerator {
public:
    // Legend: symbol -> description of every element type used in the project.
    static std::string legend(const ProjectData& p);

    // Load schedule (quadro de cargas): one row per circuit with sizing.
    static std::string loadSchedule(const ProjectData& p);

    // Bill of materials: aggregated quantities of devices, conduit and wire.
    static std::vector<BomLine> billOfMaterials(const ProjectData& p);
    static std::string billOfMaterialsText(const ProjectData& p);

    // Single-line diagram description (textual netlist the SLD command draws).
    static std::string singleLineDiagram(const ProjectData& p);
};

} // namespace electrical
