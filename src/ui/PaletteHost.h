// =============================================================================
//  PaletteHost.h - Platform bridge that docks the MainPalettePanel.
//
//  This is the ONE place with a real Windows/Linux #ifdef seam for UI:
//    - Windows: the panel is hosted in a BcUiPanelMFC docking pane (the same
//      mechanism BricsCAD's own palettes use). See brxWxSample in the BRX SDK
//      (API/brx/) for the exact MFC<->wx bridging.
//    - Linux:   the panel is a native wxWindow docked through the wx-based host.
//
//  For portability the default build hosts the panel in a lightweight top-level
//  wxFrame; swap in the platform docking calls where marked once building
//  against the SDK's UI headers.
// =============================================================================
#pragma once

namespace electrical {
namespace ui {

class PaletteHost {
public:
    static PaletteHost& instance();

    void toggle();     // show/hide the docked palette
    void refresh();    // repopulate palette content
    void destroy();    // tear down on unload

private:
    PaletteHost() = default;
    void* frame_ = nullptr;   // wxFrame* (opaque here to keep wx out of headers)
    void* panel_ = nullptr;   // MainPalettePanel*
    bool  visible_ = false;
};

} // namespace ui
} // namespace electrical
