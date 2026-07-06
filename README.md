# BricsCAD.Electrical

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

> **Status: foundation build.** All 18 commands are registered and functional,
> the data model + drawing persistence are complete, `EL-CONFIG` is wired
> end-to-end through the wxWidgets dialog, and the calculation/report services
> are implemented (the lumen method is fully implemented; the circuit
> distributor/router are functional first-pass algorithms meant to be refined).

---

## 1. Requirements

| | Windows | Linux |
|---|---|---|
| BricsCAD | V26 Ultimate (Pro+ required for compiled plugins) | V26 Ultimate |
| Compiler | Visual Studio 2022, toolset **v143**, Windows SDK **10.0.19041.0** | GCC/Clang matching the host libstdc++ |
| BRX SDK | **BRX SDK V26** (must match the BricsCAD major version) | **BRX SDK V26** (Linux) |
| wxWidgets | 3.2.x (Unicode) | 3.2.x (Unicode) — the toolkit BricsCAD itself uses |
| CMake | 3.20+ | 3.20+ |

> The **BRX SDK is not redistributable**. Register at bricsys.com as an
> *ARX/BRX/TX* developer; Bricsys grants the SDK download and a free developer
> license after manual approval. The toolset/SDK **must** match the target
> BricsCAD version or the module will fail to load.

---

## 2. Project layout

```
BricsCAD.Electrical/
├── CMakeLists.txt              # cross-platform build
├── cmake/FindBRX.cmake         # locates the BRX SDK (BRX::BRX target)
├── src/
│   ├── main.cpp                # BRX entry point, command registration
│   ├── Platform.h              # the Win/Linux #ifdef seam
│   ├── commands/               # EL- command handlers (+ Commands table)
│   ├── models/                 # ProjectSettings, Room, elements, Circuit, Conduit
│   ├── services/               # DatabaseService, LightingCalculator, distributor,
│   │                           #   Router, ReportGenerator, ProjectContext
│   ├── ui/                     # wxWidgets dialogs/palette + Localization + dcl/
│   └── utilities/              # geometry, transactions, string conversion
└── resources/
    ├── lang/                   # en.json, pt-BR.json, es.json
    └── symbols/                # symbol-block naming guide (NBR 5444)
```

---

## 3. Building

### 3.1 Common

```bash
git clone <this repo>
cd "BricsCAD.Electrical"
```

Point CMake at the BRX SDK with `-DBRX_SDK_ROOT=...`.

### 3.2 Windows (MSVC)

```bat
cmake -S . -B build ^
  -G "Visual Studio 17 2022" -A x64 -T v143 ^
  -DBRX_SDK_ROOT="C:/BRX_SDK_V26" ^
  -DwxWidgets_ROOT_DIR="C:/wxWidgets-3.2"
cmake --build build --config Release
```

Output: `build/Release/BricsCAD.Electrical.brx`.

### 3.3 Linux (GCC/Clang)

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBRX_SDK_ROOT=/opt/brx_sdk_v26 \
  -DwxWidgets_CONFIG_EXECUTABLE=$(which wx-config)
cmake --build build -j
```

Output: `build/BricsCAD.Electrical.brx` (a shared object with the `.brx`
extension). The `resources/` folder is copied next to the `.brx` automatically.

> **Toolchain matching matters.** On both platforms the plugin must share the
> host's CRT/toolset. Do not mix Clang objects into a GCC-built BricsCAD host.

---

## 4. Installing / loading

### 4.1 Manual (APPLOAD)

1. Start BricsCAD V26.
2. Run `APPLOAD`.
3. Browse to `BricsCAD.Electrical.brx` and **Load**.
4. Keep `resources/` in the same folder as the `.brx` (localization + symbols).
5. You should see: `BricsCAD.Electrical loaded (Windows|Linux). Type EL for the palette.`

### 4.2 Auto-load on startup (optional)

Add the module to the demand-load registry (Windows) under
`HKCU\Software\Bricsys\BricsCAD\V26x64\en_US\Applications\BricsCAD.Electrical`
with `Loader` = full path to the `.brx` and `LoadCtrls` = `2` (load at startup),
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
2. **`EL-ROOM`** — select a closed polyline to demarcate a room; set name, usage,
   function and ceiling height. Area is computed automatically.
3. Place elements:
   - **`EL-LIGHT`** / **`EL-LIGHT-AUTO`** — lighting points (auto uses the lumen
     method with NBR 5413/ISO 8995 lux targets and lays them on a grid).
   - **`EL-OUTLET`** / **`EL-OUTLET-AUTO`** — outlets (single/duplex/triplex, each
     module with its own height; auto spaces them along the perimeter per
     NBR 5410).
   - **`EL-SWITCH`** / **`EL-SWITCH-AUTO`** — switches (auto places one by each
     room's door, linked to that room's lights).
   - **`EL-PANEL`** — load panels (first one is the main board, QGBT).
4. **`EL-CIRCUIT`** / **`EL-CIRCUIT-AUTO`** — distribute circuits (lighting and
   outlets never mixed; specific outlets get dedicated circuits; conductor/breaker
   sized from the NBR 5410 ladder; phases balanced).
5. **`EL-CONDUIT`** / **`EL-CONDUIT-AUTO`** — route conduits. Auto asks whether to
   **omit horizontal conduits** (vertical ceiling→floor drops only).
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
| `EL-LIGHT` / `EL-LIGHT-AUTO` | Lighting point — manual / automatic |
| `EL-OUTLET` / `EL-OUTLET-AUTO` | Outlet — manual / automatic |
| `EL-SWITCH` / `EL-SWITCH-AUTO` | Switch — manual / automatic |
| `EL-PANEL` | Load panel |
| `EL-CIRCUIT` / `EL-CIRCUIT-AUTO` | Circuit distribution — manual / automatic |
| `EL-CONDUIT` / `EL-CONDUIT-AUTO` | Conduit routing — manual / automatic |
| `EL-WIRE` / `EL-WIRE-AUTO` | Wiring — manual / automatic |
| `EL-REPORT` | Generate reports |
| `EL-UNDO` | Undo the last plugin action |

### 5.3 Symbols

The plugin inserts block references named `EL_LIGHT`, `EL_OUTLET`, `EL_SWITCH`,
`EL_PANEL`. If they are not defined in the drawing, a placeholder marker is drawn
instead. See `resources/symbols/README.md` to define NBR 5444 symbols.

---

## 6. Architecture notes

- **Portability:** the core (`models/`, `services/`) has **no** platform or wx
  dependency. All wxWidgets code is behind `ui/UiBridge.h`; the single real
  Win/Linux UI `#ifdef` lives in `ui/PaletteHost.cpp` (BcUiPanelMFC vs docked
  wxWindow). String/text seams live in `utilities/StringUtil` and `Platform.h`.
- **Persistence:** `services/DatabaseService` serializes each model into a
  `PropertyBag` (a typed key/value map), then into an `AcDbXrecord` resbuf chain
  stored in the `EL_ELECTRICAL_PROJECT` named-object dictionary tree.
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
- Voltage-drop and conductor-derating (grouping/temperature) are stubs to be
  filled from the NBR 5410 tables.
- The router uses nearest-neighbour ordering + orthogonal/vertical runs; a
  graph-based trace would improve wire-length accuracy and fill calculations.
```
