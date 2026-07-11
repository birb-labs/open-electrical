#include "commands/Commands.h"

namespace electrical {
namespace commands {

// The authoritative list of commands the plugin exposes. Order here is the
// order they register. All flags are MODAL unless a command must run in the
// application/session context.
static const CommandDef kCommands[] = {
    { _T("EL"),             _T("EL"),             &showPalette,            ACRX_CMD_MODAL | ACRX_CMD_SESSION },

    { _T("EL-CONFIG"),      _T("EL-CONFIG"),      &configProject,          ACRX_CMD_MODAL },

    { _T("EL-ROOM"),        _T("EL-ROOM"),        &insertRoom,             ACRX_CMD_MODAL | ACRX_CMD_USEPICKSET },

    { _T("EL-LIGHT"),       _T("EL-LIGHT"),       &insertLight,            ACRX_CMD_MODAL },
    { _T("EL-OUTLET"),      _T("EL-OUTLET"),      &insertOutlet,           ACRX_CMD_MODAL },
    { _T("EL-OUTLET-AUTO"), _T("EL-OUTLET-AUTO"), &insertOutletAuto,       ACRX_CMD_MODAL },
    { _T("EL-SWITCH"),      _T("EL-SWITCH"),      &insertSwitch,           ACRX_CMD_MODAL },
    { _T("EL-SWITCH-AUTO"), _T("EL-SWITCH-AUTO"), &insertSwitchAuto,       ACRX_CMD_MODAL },
    { _T("EL-PANEL"),       _T("EL-PANEL"),       &insertPanel,            ACRX_CMD_MODAL },
    { _T("EL-CALC-LIGHT"),  _T("EL-CALC-LIGHT"),  &calcLightingAuto,       ACRX_CMD_MODAL },
    { _T("EL-EDIT"),        _T("EL-EDIT"),        &editElement,            ACRX_CMD_MODAL | ACRX_CMD_USEPICKSET },

    { _T("EL-CIRCUIT"),     _T("EL-CIRCUIT"),     &distributeCircuits,     ACRX_CMD_MODAL },
    { _T("EL-CIRCUIT-AUTO"),_T("EL-CIRCUIT-AUTO"),&distributeCircuitsAuto, ACRX_CMD_MODAL },
    { _T("EL-CONDUIT"),     _T("EL-CONDUIT"),     &routeConduit,           ACRX_CMD_MODAL },
    { _T("EL-CONDUIT-AUTO"),_T("EL-CONDUIT-AUTO"),&routeConduitAuto,       ACRX_CMD_MODAL },
    { _T("EL-CONDUIT-EDIT"),_T("EL-CONDUIT-EDIT"),&editConduit,            ACRX_CMD_MODAL },
    { _T("EL-ROUTE-AUTO"),  _T("EL-ROUTE-AUTO"),  &routeAllAuto,           ACRX_CMD_MODAL },
    { _T("EL-WIRE"),        _T("EL-WIRE"),        &routeWire,              ACRX_CMD_MODAL },
    { _T("EL-WIRE-AUTO"),   _T("EL-WIRE-AUTO"),   &routeWireAuto,          ACRX_CMD_MODAL },
    { _T("EL-WIRE-FLIP"),   _T("EL-WIRE-FLIP"),   &flipWiring,             ACRX_CMD_MODAL | ACRX_CMD_USEPICKSET },

    { _T("EL-DEL"),         _T("EL-DEL"),         &deleteSelected,         ACRX_CMD_MODAL | ACRX_CMD_USEPICKSET },
    { _T("EL-DEL-ROOM"),    _T("EL-DEL-ROOM"),    &deleteRoom,             ACRX_CMD_MODAL },

    { _T("EL-REPORT"),      _T("EL-REPORT"),      &generateReport,         ACRX_CMD_MODAL },
    { _T("EL-LOAD-SCHEDULE"),_T("EL-LOAD-SCHEDULE"),&generateLoadSchedule, ACRX_CMD_MODAL },
    { _T("EL-SINGLE-LINE"), _T("EL-SINGLE-LINE"), &generateSingleLine,     ACRX_CMD_MODAL },
    { _T("EL-UNDO"),        _T("EL-UNDO"),        &undoLast,               ACRX_CMD_MODAL },
};

const CommandDef* commandTable(size_t& count) {
    count = sizeof(kCommands) / sizeof(kCommands[0]);
    return kCommands;
}

} // namespace commands
} // namespace electrical
