# Electrical symbol blocks (NBR 5444)

The plugin **generates its symbol blocks natively** (see
`src/services/SymbolFactory.cpp`). The first time a symbol is needed, the block
record is created directly in the drawing, so **no external symbol library is
required** and there is no placeholder fallback for the standard elements.

Every symbol shares one local convention: **+Y points away from the wall**
(the wall itself is the line y=0); the block reference is rotated at insertion
time to match the real wall orientation.

Blocks created, by name (NBR 5444):

| Block name                | Element                                | Glyph                                                          |
|----------------------------|-----------------------------------------|-----------------------------------------------------------------|
| `EL_LIGHT`                | Ceiling lighting point                  | Circle (bigger than the switch circle), horizontal chord + stem to the south pole (3 reserved sectors: wattage / switch letter / circuit no.) |
| `EL_LIGHT_WALL`           | Wall luminaire (arandela)               | Half-disc, flat side on the wall, split by a segment from the wall-centre to the apex |
| `EL_OUTLET_L`             | Outlet, low                             | Triangle + wall stem, unfilled                                  |
| `EL_OUTLET_M`             | Outlet, medium                          | Triangle + wall stem, right half filled                        |
| `EL_OUTLET_H`             | Outlet, high                            | Triangle + wall stem, fully filled                              |
| `EL_OUTLET_<h1>[_h2][_h3]`| Multi-gang outlet (1-3 modules)         | Triangles chained apex-to-base, one per module (`h` = `L`/`M`/`H`); only the first has the wall stem |
| `EL_OUTLET_SW2`           | Outlet + 2-section switch (medium only) | Wall stem + 1 medium-fill triangle, with a 2-section switch circle at the triangle's tip (no wall tie of its own) |
| `EL_OUTLET_DUP_SW1`       | Duplex outlet + 1-section switch (medium only) | Wall stem + 2 chained medium-fill triangles, with a 1-section switch circle at the last triangle's tip |
| `EL_SWITCH_1`             | Switch, 1 section                       | Circle offset from the wall + tie to the wall                  |
| `EL_SWITCH_2`             | Switch, 2 sections                      | As above, split by the vertical diameter through the wall pole |
| `EL_SWITCH_3`             | Switch, 3 sections                      | As above, split into 3 equal slices, one boundary at the wall pole |
| `EL_SWITCH_3WAY`          | Three-way (parallel)                    | As 1-section, fully solid-filled                                |
| `EL_SWITCH_4WAY`          | Four-way (intermediate)                 | As 2-section, half solid-filled                                 |
| `EL_SWITCH_BELL`          | Bell push button                        | As 1-section, with a small filled centre dot                    |
| `EL_PANEL_SURFACE`        | Load panel, aparente (surface)          | 2:1 rectangle flush against the wall, right triangle on the secondary diagonal |
| `EL_PANEL_FLUSH`          | Load panel, embutido (flush-mounted)    | Same rectangle, ~75% recessed past the wall line                |

`SymbolFactory::outletChainBlockName()` builds the multi-gang outlet block name
from each module's mounting height in metres, using the same low/medium/high
buckets as everywhere else in the plugin (`<=0.6 m`, `<1.5 m`, else high).

## Notations (block attributes)

Every symbol carries editable, non-constant `AcDbAttributeDefinition`s. Values are
stamped per insert and matched **by tag**, never by definition order - reordering
the `addAttDef()` calls is safe.

| Block                | Tags                              | Placement |
|----------------------|-----------------------------------|-----------|
| `EL_LIGHT`           | `POTENCIA`, `COMANDO`, `CIRCUITO` | Inside the circle: power in the upper sector, command lower-left, circuit lower-right (drawn between hyphens, e.g. `-3-`) |
| `EL_LIGHT_WALL`      | `COMANDO`, `CIRCUITO`             | Inside the half-disc; circuit without hyphens |
| outlets (all chains + combos) | `POTENCIA`, `CIRCUITO`   | **Outside**, beside the triangle chain |
| `EL_SWITCH_*`        | `COMANDO1`..`COMANDO<n>`          | **Outside** the circle, just past the rim, on each sector's bisector (1 section -> right; 2 -> left/right; 3 -> 210/90/330 degrees) |
| `EL_PANEL_*`         | `NOME`, `CIRCUITOS`               | Outside: name above, circuit count below |

Per NBR 5444 the switch command letters sit *beside* the symbol, not inside it -
the same convention the outlets already followed for power and circuit. `COMANDO<n>`
is emitted once per **section**, while three-way / four-way switches draw sectors
but carry a single command (`COMANDO1`).

`POTENCIA` on a lighting point is the `"<count>x<watts>W"` notation (e.g. `2x32W`),
derived from the lamp count and the individual lamp power - the total is never an
input. On outlets it is the total VA.

Geometry is drawn around the block origin on layer `0` with BYBLOCK colour, so an
inserted reference takes its target layer's colour. The insertion scale is
applied from the project unit at insert time.

## Dimensions (base unit)

Every symbol is derived from one base unit, `kSymUnit = 0.09` drawing units (the
switch circle radius), so the symbols stay in proportion:

| Symbol | Size (drawing units) |
|---|---|
| Switch circle | radius `kSymUnit` = 0.09 → **diameter 0.18** |
| Outlet triangle (single) | equilateral, **side = switch diameter = 0.18** (half-base `kSymUnit`) |
| Outlet duplex / triplex | same triangle chained 2x/3x (no extra scale) |
| Outlet + switch combo | outlet triangle 0.18 + a switch circle of the same 0.18 diameter |
| Ceiling light circle | radius `1.6*kSymUnit` ~ 0.144 (kept larger than the switch) |
| Panel (QGBT) | rectangle **width = 2*diameter = 0.36, height = diameter = 0.18** (2:1) |

A single outlet is therefore the same size as a switch, as requested. All symbols
are inserted at scale **1:1** (`scaleFor` returns 1.0 for every type; the panel is
no longer scaled 1.5x, which had made the QGBT look oversized).

## Symbol versioning (important)

Block definitions live inside each `.dwg`, not in the `.lrx`. `SymbolFactory`
stamps `kSymbolVersion` as XData (RegApp `EL_ELECTRICAL`) on every block table
record it creates, and `ensure()` redefines any block whose stamp is stale.

> **Bump `SymbolFactory::kSymbolVersion` on ANY geometry or attribute-definition
> change**, otherwise drawings that already contain the block keep rendering the
> old symbol forever. Current version: **7** (symbol sizes normalised to the base
> unit: single outlet == switch, panel 2×diameter wide; v6 = switch commands moved
> outside the circle; combo outlet+switch tags renamed `POT`/`CIRC` ->
> `POTENCIA`/`CIRCUITO`).

## Using your own symbols

To use office-standard symbols instead, define blocks with these exact names in
your template `.dwg`/`.dwt` (or import them from a `.dxf`) **before** inserting
elements. The plugin checks for an existing block first and only generates one
when it is missing, so your definitions win.
