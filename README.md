<p align="center"><img src="ico/branding/logo-128.png" alt="FaseCAD" width="96"></p>

# FaseCAD

FaseCAD is a free desktop application to draw electrical schematics and
produce the documentation of a control panel: diagrams, terminal strips,
cable lists and bills of material.

Website: [fasecad.com](https://fasecad.com/)

### What it does

* Electrical schematics over several folios, with cross references between
  coils and contacts, and a symbol library of more than 8,000 elements
  (electrical, pneumatic, hydraulic…), plus an editor for your own symbols.
* Terminal strips: a terminal device in the toolbar (strip, number, colour,
  type, cross-section, levels), bridges with up to two bars per terminal,
  and strip drawings in schematic, realistic and terminal-diagram styles.
* Cables and their cores, cable lists and terminal summaries as tables on
  the folios.
* A parts database and a bill of material grouped by manufacturer and
  order number.
* Ribbon-style command tabs, light and dark themes, title block editor,
  PDF/PNG/SVG/DXF export.

### Building

FaseCAD is written in C++17 with Qt 6 and built with CMake.

```sh
git clone --recursive https://github.com/Brandix97/FaseCAD.git
cd FaseCAD
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DPACKAGE_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On Windows the build uses MSYS2 UCRT64 (Qt 6, CMake, Ninja). Add
`-DBUILD_WITH_KF=OFF` when the KDE Frameworks are not available.

### License and origin

FaseCAD is free software, licensed under the
[GNU General Public License, version 2 or later](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html).
You may use, copy, modify and redistribute it under the terms of that
license. It is provided as is, without any warranty.

FaseCAD is based on [QElectroTech](https://qelectrotech.org/), © 2006-2026
the developers of QElectroTech. Their copyright notices are kept in the
source files, and their names in the credits of the About box.
