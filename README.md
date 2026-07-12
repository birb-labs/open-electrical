# Open BricsCAD Electrical

A cross-platform **BRX (C++) plugin** for BricsCAD V26 Ultimate that implements a
complete **low-voltage electrical installation design** workflow (similar in
scope to PRO-Elétrica), following **ABNT NBR 5410** (installations) and
**NBR 5444** (symbology).

- **Language of the plugin:** every command, label, and message is **English**.
  Only the *interface text* can be switched to **pt-BR** or **es** at runtime
  (commands always stay English).
- **Platforms:** Windows (x64) and Linux (Ubuntu 20.04/22.04, Fedora, openSUSE).
- **UI:** wxWidgets for palettes and complex dialogs; DCL for simple option
  pickers.
- **Persistence:** all project data is stored inside the `.dwg` using named
  dictionaries + Xrecords (and XData stamps on the placed blocks).

> **Status: foundation build.** All **26 commands** are registered and functional,
> the data model + drawing persistence are complete, `EL-CONFIG` is wired
> end-to-end through the wxWidgets dialog, and the calculation/report services
> are implemented (the lumen method is fully implemented; the circuit
> distributor/router are functional first-pass algorithms meant to be refined).

---

## Documentation

- **[docs/index.html](docs/index.html)** — the full documentation site (static
  HTML/CSS/JS, no build; in Portuguese). Start with the **first-project guide**,
  then the per-command pages and the **troubleshooting** page.
- **[docs-legacy/ARCHITECTURE.md](docs-legacy/ARCHITECTURE.md)** — how the plugin
  is built: layers, data flow, module lifecycle, persistence, UI seams.
- **[docs-legacy/BRICSCAD-BRX-LINUX.md](docs-legacy/BRICSCAD-BRX-LINUX.md)** —
  field notes on **BricsCAD BRX on Linux**: the SDK-vs-runtime split, the `.lrx`
  module, the whole-archive entry point, the **wxWidgets version conflict +
  embedded-init crash and their fixes**, and how to debug when the CAD engine
  runs inside a `bwrap` sandbox. Read this before doing any BRX/GUI work on Linux.

---

## 1. Requirements

| | Windows | Linux |
|---|---|---|
| BricsCAD | V26 Ultimate (Pro+ required for compiled plugins) | V26 Ultimate |
| Compiler | Visual Studio 2022, toolset **v143**, Windows SDK **10.0.19041.0** | GCC/Clang matching the host libstdc++ |
| BRX SDK | **BRX SDK V26** (must match the BricsCAD major version) | **BRX SDK V26** (Linux) |
| wxWidgets | 3.2.x (Unicode), linked **static** | built **static** by `scripts/build-wxstatic.sh` |
| CMake | 3.20+ | 3.20+ |

> **Why static wxWidgets?** BricsCAD loads its own wxWidgets (3.1 on Linux).
> Linking the plugin against the system's *shared* wxWidgets puts two wx runtimes
> in one process and **crashes BricsCAD on load**. The module therefore links a
> private **static** wxWidgets and localises its symbols (`src/exports.map`), the
> approach the BRX `brxWxSample` documents. `scripts/setup.sh` builds this static
> wx automatically on first run.

> The **BRX SDK is not redistributable**. Register at bricsys.com as an
> *ARX/BRX/TX* developer; Bricsys grants the SDK download and a free developer
> license after manual approval. The toolset/SDK **must** match the target
> BricsCAD version or the module will fail to load.

---

## 2. Project layout

```
open-electrical/
├── CMakeLists.txt              # cross-platform build
├── cmake/FindBRX.cmake         # locates BRX SDK + BricsCAD runtime (BRX::BRX)
├── scripts/                    # setup.sh (Linux) / setup.ps1 (Windows)
├── src/
│   ├── main.cpp                # BRX entry point, command registration
│   ├── Platform.h              # the Win/Linux #ifdef seam
│   ├── commands/               # EL- command handlers (+ Commands table)
│   ├── models/                 # ProjectSettings, Room, elements, Circuit, Conduit
│   ├── services/               # DatabaseService, LightingCalculator, distributor,
│   │                           #   Router, ReportGenerator, SymbolFactory, ProjectContext
│   ├── ui/                     # wxWidgets dialogs/palette + Localization + dcl/
│   └── utilities/              # geometry, transactions, string conversion
└── resources/
    ├── lang/                   # en.json, pt-BR.json, es.json
    └── symbols/                # symbol notes (blocks are generated natively)
```

---

## 3. Building

The build needs two roots (both auto-detected with the defaults below):

- `BRX_SDK_ROOT`  — extracted BRX SDK V26 (headers). Default `/opt/brx_sdk_v26`.
- `BRICSCAD_ROOT` — installed BricsCAD program folder, which ships the runtime
  libraries and `libdrx_entrypoint.a`. Default `/opt/bricsys/bricscad/v26`.

The module carries the platform's BRX extension: **`.brx` on Windows, `.lrx`
(Linux Runtime eXtension) on Linux**.

### 3.1 One-shot scripts (recommended)

```bash
# Linux — builds and installs to ~/.local/share/open-electrical
./scripts/setup.sh

# Windows (PowerShell) — builds and installs to %LOCALAPPDATA%\open-electrical
.\scripts\setup.ps1
```

Override roots via environment variables, e.g.
`BRX_SDK_ROOT=/opt/brx_sdk_v26 BRICSCAD_ROOT=/opt/bricsys/bricscad/v26 ./scripts/setup.sh`.

### 3.2 Manual — Windows (MSVC)

```bat
cmake -S . -B build ^
  -G "Visual Studio 17 2022" -A x64 -T v143 ^
  -DBRX_SDK_ROOT="C:/BRX_SDK_V26"
cmake --build build --config Release
```

Output: `build/Release/open-electrical.brx`.

### 3.3 Manual — Linux (GCC/Clang)

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBRX_SDK_ROOT=/opt/brx_sdk_v26 \
  -DBRICSCAD_ROOT=/opt/bricsys/bricscad/v26
cmake --build build -j
```

Output: `build/open-electrical.lrx` (a shared object with the `.lrx` extension).
The `resources/` folder is copied next to the module automatically. The build
links `libdrx_entrypoint.a` whole-archive so the ARX entry points are re-exported
and BricsCAD recognises the module.

> **Toolchain matching matters.** On both platforms the plugin must share the
> host's CRT/toolset. Do not mix Clang objects into a GCC-built BricsCAD host.

### 3.1 Unit tests

The platform-independent core (polygon math, the lumen method, `Room`
serialization) has unit tests that link **no** BRX/wxWidgets, so they run headless:

```
cmake --build build --target check
```

The `check` target builds the `oe_tests` executable and runs it, exiting non-zero
if any test fails. Tests live in `tests/` and use a small header-only harness
(`tests/test_framework.h`) — GoogleTest/Catch2 are not vendored so the build stays
fully offline. Covered: polygon area, point-in-polygon, the clipped-grid
distribution (rectangle / L-shape, exact count, all points inside), the lumen
method (known-input luminaire count, contrast raising the count) and `Room`
serialization round-trip + old-drawing defaults.

---

## 4. Installing / loading

### 4.1 Manual (APPLOAD)

1. Start BricsCAD V26.
2. Run `APPLOAD` (or `(arxload "…/open-electrical.lrx")` on Linux).
3. Browse to `open-electrical.lrx` (Linux) / `open-electrical.brx` (Windows) and **Load**.
4. Keep `resources/` in the same folder as the module (localization packs).
5. You should see: `open-electrical loaded (Windows|Linux). Type EL for the palette.`

### 4.2 Auto-load on startup (optional)

Add the module to the demand-load registry (Windows) under
`HKCU\Software\Bricsys\BricsCAD\V26x64\en_US\Applications\open-electrical`
with `Loader` = full path to the module and `LoadCtrls` = `2` (load at startup),
or place a path line in an `autoload.rx` on the support path. See the BRX SDK's
deployment docs for the exact registry layout.

---

## 5. User guide

Open the dockable palette with **`EL`**, or run any command directly. All command
names are English and prefixed `EL-`.

### 5.1 Workflow

1. **`EL-CONFIG`** — set scale/unit, network (voltage, frequency, phases,
   grounding), installation (surface/concealed, conduit material, default wire
   gauge), the power utility (NeoEnergia/Enel/CEMIG/COPEL, or add your own), and
   the **interface language** (English / Português / Español).
2. **`EL-ROOM`** — pick the room corners one by one (minimum 3; a rubber-band
   line follows the cursor, `Undo` removes the last corner, Enter closes the
   loop). After the loop closes, a dialog collects everything the lumen method
   needs **per room**: name, type, specific use (free text), function, ceiling &
   work-plane heights, ceiling/wall/floor **reflectances (%)**, **task contrast**
   (Low/Medium/High), **maintenance factor (MF)** and **utilization factor (CU,
   0 = auto)**. Area is computed automatically. These parameters are stored on the
   room — there is no global lighting configuration.
3. Place elements:
   - **`EL-LIGHT`** — insert a lighting point manually (ceiling or wall). The
     automatic, lumen-method placement is **`EL-CALC-LIGHT`** (below).
   - **`EL-OUTLET`** / **`EL-OUTLET-AUTO`** — outlets (single/duplex/triplex, each
     module with its own height; auto spaces them along the perimeter per
     NBR 5410).
   - **`EL-SWITCH`** / **`EL-SWITCH-AUTO`** — switches (auto places one by each
     room's door, linked to that room's lights).
   - **`EL-PANEL`** — load panels (first one is the main board, QGBT).
   - **`EL-CALC-LIGHT`** — lumen method. A dialog lists the rooms so you pick one
     (or **All rooms**) and choose the **luminaire** (catalogue or custom flux/
     power). Everything else is read from each room (see §5.3). Luminaires are laid
     on a grid clipped to the room polygon, so L-, U- and trapezoidal rooms work;
     the grid densifies until N luminaires fit inside. Placed points get
     `COMANDO = A` and `CIRCUITO = ?` as placeholders.
4. **`EL-CIRCUIT`** / **`EL-CIRCUIT-AUTO`** — distribute circuits (lighting and
   outlets never mixed; specific outlets get dedicated circuits; conductor/breaker
   sized from the NBR 5410 ladder; phases balanced). Afterwards each element's
   `CIRCUITO` block attribute is **rewritten in place** with the circuit it landed
   on — the `?` placeholders resolve to real numbers.
5. **`EL-CONDUIT`** / **`EL-CONDUIT-AUTO`** — route conduits. Requires circuits
   (run `EL-CIRCUIT-AUTO` first). For each circuit the run starts **at the panel**,
   threads the circuit's devices in nearest-neighbour order along the ceiling, and
   drops vertically to each device (and to the panel) at its mounting height.
   Conduits are drawn as **3D polylines** on the conduit layer, so the vertical
   drops are visible (they were previously lost on a flat 2D polyline). Auto asks
   whether to **omit horizontal conduits** (vertical drops only). `EL-ROUTE-AUTO`
   does routing **and** wiring in one step.
6. **`EL-WIRE`** / **`EL-WIRE-AUTO`** — pull conductors through the conduits.
7. **`EL-REPORT`** — generate the **Legend**, **Load Schedule**, **Bill of
   Materials**, and **Single-line Diagram** (placed as MText at a picked point).
8. **`EL-UNDO`** — roll back the last plugin action (uses the drawing's undo
   group markers) and reload the model from the restored drawing.

### 5.2 Command reference

| Command | Function |
|---|---|
| `EL` | Open/close the main palette |
| `EL-CONFIG` | Project settings |
| `EL-ROOM` | Insert/edit room |
| `EL-LIGHT` | Lighting point — manual insertion |
| `EL-OUTLET` / `EL-OUTLET-AUTO` | Outlet — manual / automatic |
| `EL-SWITCH` / `EL-SWITCH-AUTO` | Switch — manual / automatic |
| `EL-PANEL` | Load panel |
| `EL-CALC-LIGHT` | Lumen-method calculation + automatic luminaire placement |
| `EL-EDIT` | Edit a placed component (re-opens its dialog, updates the symbol) |
| `EL-CIRCUIT` / `EL-CIRCUIT-AUTO` | Circuit distribution — manual / automatic |
| `EL-CONDUIT` / `EL-CONDUIT-AUTO` | Conduit routing — manual / automatic |
| `EL-CONDUIT-EDIT` | Reshape a manual conduit run (endpoint / curve) |
| `EL-ROUTE-AUTO` | Route conduits **and** pull wires in one step |
| `EL-WIRE` / `EL-WIRE-AUTO` | Wiring — manual / automatic |
| `EL-WIRE-FLIP` | Mirror a conduit's wiring balloon to the other side |
| `EL-REPORT` | Generate all reports (Legend / Loads / BOM / SLD) |
| `EL-LOAD-SCHEDULE` | Insert only the load schedule (quadro de cargas) |
| `EL-SINGLE-LINE` | Insert only the single-line diagram |
| `EL-DEL` | Delete selected components/conduits (model + drawing) |
| `EL-DEL-ROOM` | Delete only a room boundary (components kept) |
| `EL-UNDO` | Undo the last plugin action |

> All 26 registered commands are listed in `src/commands/Commands.cpp`. Note
> there is **no** `EL-LIGHT-AUTO` command — automatic lighting is `EL-CALC-LIGHT`.

### 5.3 Lighting calculation (lumen method)

`EL-CALC-LIGHT` runs:

```
phi_total = (E x A) / (CU x MF)        N = ceil(phi_total / phi_luminaire)
```

All the parameters below live **on the room** (set in the `EL-ROOM` dialog), not
in any global configuration:

- **A** — the room's floor area, from the boundary polygon (shoelace).
- **E** — the target illuminance, chosen automatically from the room's **type**
  and **task contrast**, per the NBR 5413 / ISO 8995 range (low / mid / high):

  | Type | E lo–mid–hi (lux) | | Type | E lo–mid–hi (lux) |
  |---|---|---|---|---|
  | Office | 300 / 500 / 750 | | Living / Bedroom / Closet | 100 / 150 / (300) |
  | Kitchen | 200 / 300 / 500 | | Garage / Balcony | 50 / 100 / 150 |
  | Service area | 150 / 200 / 300 | | Bathroom / Hallway / Dining | 100 / 150 / 200 |

  The **contrast** field picks the bound: Low → lower, Medium → mid, High → upper.
- **CU** — the room's utilization factor; `0` derives it from the room index *K*
  and the ceiling reflectance.
- **MF** — the room's maintenance factor (default `0.80`).
- **phi_luminaire** — chosen in the `EL-CALC-LIGHT` dialog: a catalogue model
  (LED 18W/24W/36W, fluorescent 2x16W/2x32W) or a custom flux/power.
- Work-plane and ceiling heights come from the room.

So the workflow is: `EL-ROOM` for each room, **setting its type + contrast** →
`EL-CALC-LIGHT` (pick room(s) + luminaire) → `EL-CIRCUIT-AUTO`. (There is no
separate `EL-LIGHT-AUTO` command; `EL-CALC-LIGHT` is the automatic placement.)

Rooms without a boundary polygon are skipped with a message; if no room has one,
the command says so and does nothing.

### 5.4 Known limitations

- **`EL-EDIT` only works on blocks the plugin inserted.** It resolves the picked
  entity's handle against the project dictionary, so a block that was copy-pasted
  or inserted manually has no model behind it and is rejected. Reconstructing a
  model from a block's attribute values is not implemented.
- **No live preview for the perpendicular wall snap.** The angle *is* constrained
  to the wall's two normals, but the cursor is not highlighted during the drag
  (that needs an `AcEdJig`).
- Luminaire distribution uses an adaptive clipped grid. For strongly concave rooms
  the densified grid can overshoot N and is then sub-sampled with a uniform stride,
  which is spread-preserving but not a true optimal layout.
- Conduit routing is nearest-neighbour from the panel with orthogonal ceiling runs
  and vertical drops — no corridor/wall-following awareness. `Router::pullWires()`
  tags each conduit with the conductor set of **every distinct circuit** travelling
  through it (`Router::conduitCircuits`), so a shared trunk accumulates correctly;
  what is still missing is graph-based fill/derating per segment.

### 5.5 Symbols

The plugin **generates its NBR 5444 symbol blocks natively** the first time each
is needed (`services/SymbolFactory`), so no external symbol library is required.
Every symbol is built from the norm's four base shapes — **circle** (light points,
switches), **equilateral triangle** (outlets), **rectangle** (panels), **line**
(conduits) — with fills and dividers encoding sections/height, exactly as
described by NBR 5444. The block chosen on insertion reflects what you set in
the element dialog:

| Block(s) | Element | Glyph (NBR 5444) |
|----------|---------|------------------|
| `EL_LIGHT` | Ceiling lighting point | Circle (bigger than a switch) split by a horizontal chord, stem to the south pole |
| `EL_LIGHT_WALL` | Wall luminaire (arandela) | Half-disc, flat side on the wall, split wall-centre to apex |
| `EL_OUTLET_L` / `_M` / `_H` | Outlet — low / medium / high | Triangle + wall stem; fill = **empty / right-half / full** |
| `EL_OUTLET_<h1>[_h2][_h3]` | Multi-gang outlet (1-3 modules) | Triangles chained apex-to-base, each module's own height |
| `EL_OUTLET_SW2` | Outlet + 2-section switch | Outlet triangle (medium) with a 2-section switch circle at its tip |
| `EL_OUTLET_DUP_SW1` | Duplex outlet + 1-section switch | Two chained outlet triangles (medium) with a 1-section switch circle at the tip |
| `EL_SWITCH_1` / `_2` / `_3` | Switch — 1 / 2 / 3 sections | Circle, offset from the wall with a tie; 0 / 1 / 3 dividers |
| `EL_SWITCH_3WAY` | Three-way (parallel) | As 1-section, fully solid-filled |
| `EL_SWITCH_4WAY` | Four-way (intermediate) | As 2-section, half solid-filled |
| `EL_SWITCH_BELL` | Bell push button | As 1-section, with a small filled centre dot |
| `EL_PANEL_SURFACE` | Load panel, aparente (surface) | 3:1 rectangle flush against the wall + **solid-filled** right triangle (hypotenuse on the secondary diagonal) |
| `EL_PANEL_FLUSH` | Load panel, embutido (flush) | Same rectangle + filled triangle, ~75% recessed past the wall line |

**Notations (editable block attributes).** Every symbol carries NBR 5444
notations as **block attributes** you can double-click to edit in BricsCAD:
`POTENCIA` (power), `COMANDO` (controlling-switch letter), `CIRCUITO` (circuit
number) on light/outlet points; `COMANDO1..N` (one per section) on switches;
`NOME` / `CIRCUITOS` on panels. The insertion dialogs collect these up-front and
they persist in the project data. (These tags were renamed from the earlier
`POT`/`LETRA`/`CIRC` in symbol version 6.)

**Wall rotation.** Wall-mounted symbols (outlets, switches, wall lights, panels)
prompt for a second point *toward the room interior* after the insertion point
and rotate so the symbol faces off the wall. Ceiling lighting points never
rotate. A symbol's redefinition auto-updates in older drawings via a version
stamp (`SymbolFactory::kSymbolVersion`), so a rebuilt plugin's new geometry
appears even in drawings that already contain the blocks.

Manual insertion (`EL-LIGHT`, `EL-OUTLET`, `EL-SWITCH`, `EL-PANEL`) opens a
**configuration dialog** first (lamp/watts, outlet type/purpose/height, switch
type/sections, panel name/main/embedded). Mounting **height** is picked from a
**Low / Medium / High combo that also accepts a typed exact value** (metres);
for outlets it additionally selects the fill (`≤0.6 m` low, `<1.5 m` medium,
else high).

To use your own office-standard symbols instead, define blocks with these names
in your template `.dwg`; the plugin uses yours and skips generation.
See `resources/symbols/README.md`.

---

## 6. Architecture notes

- **Portability:** the core (`models/`, `services/`) has **no** platform or wx
  dependency. All wxWidgets code is behind `ui/UiBridge.h`; the single real
  Win/Linux UI `#ifdef` lives in `ui/PaletteHost.cpp` (BcUiPanelMFC vs docked
  wxWindow). String/text seams live in `utilities/StringUtil` and `Platform.h`.
- **Persistence:** `services/DatabaseService` serializes each model into a
  `PropertyBag` (a typed key/value map), then into an `AcDbXrecord` resbuf chain
  stored in the `EL_ELECTRICAL_PROJECT` named-object dictionary tree.
- **Block attributes:** symbol builders create attribute *definitions*; values are
  per-insert and matched **by tag** (`AcDbAttributeDefinition::tagConst()`, which
  borrows the string rather than transferring ownership like `tag()` does). So
  reordering `addAttDef()` calls in `SymbolFactory` is safe. Any geometry or
  attribute change still requires bumping `SymbolFactory::kSymbolVersion`, since
  block definitions are cached inside each `.dwg`.
- **Undo:** `utilities/TransactionHelper` wraps document locks, transactions, and
  `UNDO Begin/End` groups so `EL-UNDO` peels back exactly one plugin action.
- **i18n:** `ui/Localization` loads flat-JSON packs from `resources/lang`; English
  is built into the binary as the fallback.

## 7. Refinement backlog (foundation → full)

These are functional first-pass implementations, flagged for engineering review:

- Coefficient-of-utilization uses a compact approximation of manufacturer CU
  tables — replace with a specific luminaire's photometric table if required.
- The circuit distributor applies conventional residential VA ceilings; extend
  with full demand-factor tables per NBR 5410 for commercial/industrial.
- Voltage drop is now computed per circuit (`CircuitDistributor::computeVoltageDrops`,
  run by `EL-WIRE-AUTO`) with a simplified copper/resistive model — shown in the load
  schedule and single-line diagram. Conductor **derating** (grouping/temperature) is
  still a stub to be filled from the NBR 5410 tables, and the drop model ignores power
  factor.
- The router uses nearest-neighbour ordering + orthogonal/vertical runs; a
  graph-based trace would improve wire-length accuracy and fill calculations.
