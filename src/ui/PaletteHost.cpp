#include "ui/PaletteHost.h"
#include "ui/MainPalette.h"
#include "ui/Localization.h"

#include <wx/wx.h>
#include <wx/frame.h>

namespace electrical {
namespace ui {

PaletteHost& PaletteHost::instance() {
    static PaletteHost h;
    return h;
}

void PaletteHost::toggle() {
    auto* frame = static_cast<wxFrame*>(frame_);

    if (!frame) {
        // Create the host container. On the default build this is a small
        // top-level frame; replace the frame creation below with the platform
        // docking pane when building against the SDK UI headers:
        //
        //   #if defined(_ELECTRICAL_WIN)
        //     // Wrap `panel` in a BcUiPanelMFC docking pane (see brxWxSample).
        //   #else
        //     // Register `panel` as a docked wxWindow with the wx-based host.
        //   #endif
        //
        frame = new wxFrame(nullptr, wxID_ANY,
                            wxString(EL_TRW("palette.title").c_str()),
                            wxDefaultPosition, wxSize(280, 520),
                            wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER |
                            wxFRAME_TOOL_WINDOW | wxFRAME_FLOAT_ON_PARENT);
        auto* panel = new MainPalettePanel(frame);
        auto* s = new wxBoxSizer(wxVERTICAL);
        s->Add(panel, 1, wxEXPAND);
        frame->SetSizer(s);

        // Hide instead of destroy on close so state is preserved.
        frame->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& e) {
            if (auto* f = static_cast<wxFrame*>(frame_)) f->Hide();
            visible_ = false;
            e.Veto();
        });

        frame_ = frame;
        panel_ = panel;
    }

    visible_ = !frame->IsShown();
    frame->Show(visible_);
    if (visible_) frame->Raise();
}

void PaletteHost::refresh() {
    if (auto* panel = static_cast<MainPalettePanel*>(panel_))
        panel->refresh();
}

void PaletteHost::destroy() {
    if (auto* frame = static_cast<wxFrame*>(frame_)) {
        frame->Destroy();
        frame_ = nullptr;
        panel_ = nullptr;
        visible_ = false;
    }
}

} // namespace ui
} // namespace electrical
