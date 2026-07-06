# Electrical symbol blocks (NBR 5444)

The plugin inserts block references by name for each element. If a block is not
defined in the drawing, a placeholder marker (a small circle) is drawn instead,
so the workflow never blocks on a missing symbol library.

Define these blocks in your template `.dwg`/`.dwt`, or import them from a `.dxf`
of standard NBR 5444 symbols, using these exact names:

| Block name   | Element                         | Suggested symbol (NBR 5444)         |
|--------------|---------------------------------|-------------------------------------|
| `EL_LIGHT`   | Ceiling/wall lighting point     | Circle with an inscribed "X"        |
| `EL_OUTLET`  | Socket outlet                   | Half-circle on a line (low/high)    |
| `EL_SWITCH`  | Wall switch                     | Small circle with a stem + key mark |
| `EL_PANEL`   | Load panel / distribution board | Filled rectangle labelled QD/QGBT   |

Insertion points are the block origin. Recommended block unit: meters (scale is
applied from the project unit at insert time).

A starter DXF of these symbols can be generated from the legacy
`../../../blocos/eletrica_blocos.dxf` in this repository, or authored directly
in BricsCAD and saved into the project template.
