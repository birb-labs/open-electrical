# open-electrical — Architecture

How the plugin is put together: layers, data flow, lifecycle, and the reasoning
behind the seams. For BricsCAD/BRX platform findings see
[`BRICSCAD-BRX-LINUX.md`](BRICSCAD-BRX-LINUX.md).

---

## 1. Design goals that shaped everything

1. **Cross-platform from one source tree** (Windows `.brx` + Linux `.lrx`), with
   the platform `#ifdef`s confined to as few files as possible.
2. **Testable core** — the domain model and the engineering calculations must
   not depend on `AcDb*`, wxWidgets, or any CAD/UI type, so they can be reasoned
   about (and unit-tested) off-CAD.
3. **English commands & identifiers, translatable UI** — command names and code
   are always English; only user-facing strings are localized (en / pt-BR / es)
   and switchable at runtime.
4. **Data lives in the drawing** — no side-car files; a `.dwg` is self-contained.

These goals produce a layered architecture where dependencies point **inward**:
`commands` and `ui` depend on `services`, which depend on `models`; nothing in
`models`/`services` depends on `ui` or on CAD UI types.

```
        ┌─────────────┐      ┌──────────────┐
        │  commands/   │─────▶│     ui/      │  wxWidgets + DCL + i18n
        │  (EL-*)      │      │ (behind      │
        └──────┬──────┘      │  UiBridge)   │
               │             └──────┬───────┘
               ▼                    ▼
        ┌──────────────────────────────────┐
        │            services/             │  DatabaseService, ProjectContext,
        │                                  │  LightingCalculator, CircuitDistributor,
        │                                  │  Router, ReportGenerator, SymbolFactory
        └──────────────┬───────────────────┘
                       ▼
        ┌──────────────────────────────────┐
        │             models/              │  pure C++, no AcDb / no wx
        └──────────────────────────────────┘
        ┌──────────────────────────────────┐
        │   utilities/ + Platform.h        │  the #ifdef / AcDb bridge seam
        └──────────────────────────────────┘
```

---

## 2. Directory map

```
src/
├── Platform.h              # the Win/Linux seam + global constants (group, dict keys)
├── main.cpp                # AcRxArxApp entry point, command registration, doc reactor
├── commands/
│   ├── Commands.{h,cpp}    # the authoritative EL- command table + handler decls
│   ├── CommandUtil.{h,cpp} # layers, point picking, block/polyline insertion
│   ├── ConfigCommands.cpp  # EL, EL-CONFIG
│   ├── RoomCommands.cpp    # EL-ROOM
│   ├── ElementCommands.cpp # EL-LIGHT/OUTLET/SWITCH/PANEL (+ *-AUTO)
│   ├── CircuitCommands.cpp # EL-CIRCUIT/CONDUIT/WIRE (+ *-AUTO)
│   └── ReportCommands.cpp  # EL-REPORT, EL-UNDO
├── models/                 # ProjectSettings, Room, ElectricalElement + subclasses,
│                           #   Circuit, Conduit, ProjectData, Types, Serialization
├── services/               # DatabaseService, ProjectContext, LightingCalculator,
│                           #   CircuitDistributor, Router, ReportGenerator, SymbolFactory
├── ui/                     # Localization, UiBridge, ConfigDialog, RoomDialog,
│   └── dcl/                #   MainPalette, PaletteHost, + electrical.dcl
└── utilities/              # StringUtil, GeometryHelper, TransactionHelper, Diag
```

---

## 3. Module lifecycle (`main.cpp`)

`CElectricalApp : AcRxArxApp` is the BRX entry point (`IMPLEMENT_ARX_ENTRYPOINT`).

- **`On_kInitAppMsg`** (load): unlock for on-demand unload, register MDI
  awareness, `Localization::loadPacks()` from the module's own directory
  (resolved via `dladdr`/`GetModuleFileName`), **register all 18 commands**
  manually (hyphenated names, see platform notes §5), install the document
  reactor, print a banner. It deliberately **does not** read the drawing DB.
- **`ElDocReactor::documentActivated`**: resets the in-memory project for the
  now-active drawing and applies that drawing's saved UI language. This is the
  safe moment for the first DB read.
- **`On_kUnloadAppMsg`**: `removeGroup`, tear down wx (`ui::shutdownWx()`),
  remove the reactor.

`Platform.h` centralizes the few global constants: the command group
(`ELECTRICAL`), the root dictionary key (`EL_ELECTRICAL_PROJECT`), and the XData
app name (`EL_ELECTRICAL`).

---

## 4. The model layer (`models/`)

Plain value types, **no CAD dependency**. Highlights:

- **`Types.h`** — domain enums (units, phases, grounding, room usage/function,
  outlet/switch kinds, circuit types) with helpers like `toMeters(Unit)`.
- **`Serialization.h`** — a `PropertyBag`: a typed key→value map
  (`int`/`real`/`text`/`point`) plus an `ISerializable` interface
  (`serialize`/`deserialize`/`typeTag`). **This is the boundary that keeps the
  model CAD-free**: models serialize into a `PropertyBag`; only
  `DatabaseService` knows how to turn a bag into an `AcDbXrecord`.
- **`ElectricalElement`** — base for `LightPoint`, `Outlet`, `Switch`, `Panel`.
  Each element stores the **handle** (hex) of its backing block reference so the
  model↔drawing link survives save/reload. `Outlet` models per-module mounting
  heights (single/duplex/triplex).
- **`Room`** — boundary loop + openings; computes area (shoelace) and perimeter.
- **`Circuit` / `Conduit`** — sizing results and routed geometry.
- **`ProjectData`** — the in-memory aggregate (settings + rooms + elements +
  circuits + conduits) with id allocators and a polymorphic element factory
  (`makeElement(typeTag)`) used on load.

---

## 5. Services (`services/`)

### Persistence — `DatabaseService`
The only place that touches `AcDb`. It maps `PropertyBag ⇄ AcDbResbuf` chains and
reads/writes the named-dictionary tree described in the platform notes §8. It
also registers the RegApp and can stamp/read an XData key on a block reference so
a selected entity can be resolved back to its model object.

### Session state — `ProjectContext`
A process-wide singleton holding the active `ProjectData`. Lazy-loads from the
drawing on first access, is reset by the document reactor on drawing switch, and
writes back on `save()`. Commands operate on `ProjectContext::instance().project()`.

### Engineering — `LightingCalculator`
The one fully-worked calculation: the **lumen method** (NBR 5413 / ISO 8995
illuminance targets, NBR 5410 for the electrical side).
`Φ_total = (E·A)/(CU·MF)`, `N = ⌈Φ_total/Φ_luminaire⌉`, then a near-square grid
layout clipped to the room polygon, and a back-calculated achieved lux. The
coefficient of utilization `CU` is a compact monotonic approximation of
manufacturer tables (flagged for refinement).

### Distribution / routing / reports (first-pass)
- **`CircuitDistributor`** — groups elements into circuits by NBR 5410 rules
  (lighting and outlets never mixed; specific outlets get dedicated circuits;
  conductor/breaker from the standard ladder; round-robin phase balancing).
- **`Router`** — connects a circuit's devices; supports **vertical-drops-only**
  (omit horizontal conduits) vs. full orthogonal ceiling runs; then pulls wires.
- **`ReportGenerator`** — Legend, Load Schedule, Bill of Materials, and a textual
  Single-line Diagram (netlist) that `EL-REPORT` drops as MText.

### Symbols — `SymbolFactory`
Generates the NBR 5444 block definitions in code (see platform notes §9).

> These three (distributor/router/reports) are deliberately **functional first
> passes**, meant to be refined against full NBR tables — see the backlog in the
> README.

---

## 6. Commands (`commands/`)

`Commands.cpp` holds the **single source of truth**: a table mapping each global
name (`EL`, `EL-CONFIG`, …) to its handler and flags. `main.cpp` iterates it to
register/unregister. Handlers are thin: they gather input via `aced*` prompts or
a dialog, mutate `ProjectData` through the services, draw via `CommandUtil`
(layers, block insertion, polylines), and `save()`.

- **Undo** — `EL-UNDO` and every mutating command wrap their work in an
  `UndoGroup` (`utilities/TransactionHelper`) that emits `UNDO Begin/End`
  markers, so one plugin action is one undo step; `EL-UNDO` then reloads the
  model from the restored drawing.

---

## 7. UI (`ui/`) — the second `#ifdef` seam

Everything wxWidgets is hidden behind **`UiBridge.h`** (free functions:
`showConfigDialog`, `showRoomDialog`, `togglePalette`, …). Commands/services
never include a wx header.

- **`Localization`** — flat-JSON language packs from `resources/lang/*.json`,
  with English compiled in as the fallback. `EL_TR("key")` / `EL_TRW("key")`.
- **`ConfigDialog` / `RoomDialog`** — wxWidgets dialogs (tabbed settings, room
  attributes) whose every label comes from `Localization`.
- **`MainPalettePanel`** — the dockable palette content (command buttons +
  project summary), which queues commands on the active document.
- **`PaletteHost`** — the **one real Win/Linux UI `#ifdef`**: on Windows the
  panel is meant to live in a `BcUiPanelMFC` docking pane; on Linux it docks as a
  native `wxWindow`. The default build hosts it in a lightweight top-level
  `wxFrame` (a documented TODO is full native docking via `brxWxSample`).
- **wx bootstrap** — `UiBridge` brings wx up in **embedded mode**
  (`wxInitialize()` + `wxIMPLEMENT_APP_NO_MAIN`, **not** `wxEntryStart`). This
  was the subject of a hard-won crash fix; see platform notes §6.2.

Simple option pickers use **DCL** (`ui/dcl/electrical.dcl`) rather than wx, per
the "wx for complex, DCL for simple" split.

---

## 8. Build system

- **`CMakeLists.txt`** + **`cmake/FindBRX.cmake`** encode the SDK-vs-runtime
  split, the whole-archive entry point, the platform defines, the `.lrx`/`.brx`
  suffix, and (Linux) the wx **version script** (`src/exports.map`).
- **`scripts/setup.sh`** (Linux) builds the private static wxWidgets on first run
  (`scripts/build-wxstatic.sh`), configures, builds, and installs to
  `~/.local/share/open-electrical/`. **`scripts/setup.ps1`** is the Windows
  equivalent.
- **`scripts/debug-load.sh`** reproduces a load/UI crash under the sandbox and
  captures diagnostics (see platform notes §7).

---

## 9. Diagnostics

`utilities/Diag.h` provides `EL_DIAG_LOG(...)`, an opt-in
(`EL_DIAG=1`) flushed file logger to `~/oe-init.log`. It exists because the CAD
engine runs in a bubblewrap sandbox that defeats gdb and core dumps; the log's
last line localizes a crash. Silent by default; leave it in place — it is the
practical way to diagnose issues on a user's machine.

---

## 10. Data-flow example: `EL-LIGHT-AUTO`

1. `insertLightAuto()` reads `ProjectContext::instance().project()`.
2. For each `Room`, `LightingCalculator::calculate()` returns luminaire count +
   grid positions (lumen method).
3. For each position, `SymbolFactory::ensure()` guarantees the `EL_LIGHT` block
   exists, `CommandUtil::insertSymbol()` inserts a block reference and returns
   its handle, and a `LightPoint` model is created and linked to the room.
4. `ProjectContext::save()` serializes every model into the drawing's dictionary
   tree via `DatabaseService`.
5. All of it is wrapped in one `UndoGroup`, so `EL-UNDO` reverts the whole batch.

---

See also: [`BRICSCAD-BRX-LINUX.md`](BRICSCAD-BRX-LINUX.md) (platform findings),
the root [`README.md`](../README.md) (build/install/user guide), and
`resources/symbols/README.md` (symbol blocks).
