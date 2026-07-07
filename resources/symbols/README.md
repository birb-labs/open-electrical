# Electrical symbol blocks (NBR 5444)

The plugin **generates its symbol blocks natively** (see
`src/services/SymbolFactory.cpp`). The first time a symbol is needed, the block
record is created directly in the drawing, so **no external symbol library is
required** and there is no placeholder fallback for the standard elements.

Blocks created, by name:

| Block name   | Element                         | Glyph (NBR 5444 style)              |
|--------------|---------------------------------|-------------------------------------|
| `EL_LIGHT`   | Ceiling lighting point          | Circle with an inscribed cross      |
| `EL_OUTLET`  | Socket outlet                   | Half-disc on a base line + contacts |
| `EL_SWITCH`  | Wall switch                     | Small circle with an angled lever   |
| `EL_PANEL`   | Load panel / distribution board | Rectangle with a diagonal           |

Geometry is drawn around the block origin on layer `0` with BYBLOCK colour, so an
inserted reference takes its target layer's colour. The insertion scale is
applied from the project unit at insert time.

## Using your own symbols

To use office-standard symbols instead, define blocks with these exact names in
your template `.dwg`/`.dwt` (or import them from a `.dxf`) **before** inserting
elements. The plugin checks for an existing block first and only generates one
when it is missing, so your definitions win.
