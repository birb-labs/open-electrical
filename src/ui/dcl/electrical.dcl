// =============================================================================
//  electrical.dcl - Simple modal dialogs (DCL) for quick option pickers.
//
//  Complex dialogs (project settings, room editor, reports) use wxWidgets. DCL
//  is used only for lightweight option prompts, invoked from BRX via the
//  acedDcl* / ads DCL API. All labels are English.
//
//  Load from BRX with the standard sequence:
//     load_dialog("electrical.dcl", &dclId);
//     new_dialog("el_outlet_type", dclId, ...);
//     ... set tiles / action_tile ...
//     start_dialog();
// =============================================================================

el_outlet_type : dialog {
    label = "Insert Outlet";
    : boxed_radio_column {
        label = "Outlet type";
        : radio_button { key = "single";  label = "Single";  value = "1"; }
        : radio_button { key = "duplex";  label = "Duplex"; }
        : radio_button { key = "triplex"; label = "Triplex"; }
    }
    : boxed_column {
        label = "Purpose";
        : radio_button { key = "tug"; label = "General use (TUG)"; value = "1"; }
        : radio_button { key = "tue"; label = "Specific use (TUE)"; }
    }
    : edit_box { key = "height"; label = "Mounting height (m):"; edit_width = 8; }
    spacer;
    ok_cancel;
}

el_switch_type : dialog {
    label = "Insert Switch";
    : boxed_radio_column {
        label = "Switch type";
        : radio_button { key = "single_pole"; label = "Single-pole"; value = "1"; }
        : radio_button { key = "double_pole"; label = "Double-pole"; }
        : radio_button { key = "three_way";   label = "Three-way"; }
        : radio_button { key = "four_way";     label = "Four-way"; }
    }
    : edit_box { key = "gangs"; label = "Number of keys:"; edit_width = 6; }
    spacer;
    ok_cancel;
}

el_report_pick : dialog {
    label = "Generate Report";
    : boxed_column {
        label = "Documents";
        : toggle { key = "legend"; label = "Legend"; value = "1"; }
        : toggle { key = "loads";  label = "Load Schedule"; value = "1"; }
        : toggle { key = "bom";    label = "Bill of Materials"; value = "1"; }
        : toggle { key = "sld";    label = "Single-line Diagram"; value = "1"; }
    }
    spacer;
    ok_cancel;
}
