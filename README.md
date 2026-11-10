<p align="center"><img src="ico/branding/logo-128.png" alt="FaseCAD" width="96"></p>

# FaseCAD

**FaseCAD aims to be the best free and open source software for electrical
diagrams.**

FaseCAD is a free desktop application to draw electrical schematics and
produce the documentation of a control panel: diagrams, terminal strips,
cable lists and bills of material. It is a fork of
[QElectroTech](https://qelectrotech.org) that keeps everything QElectroTech
does well and adds the device-oriented workflow of professional electrical
CAD tools.

Website: [fasecad.com](https://fasecad.com/) ·
Downloads: [Releases](https://github.com/Brandix97/FaseCAD/releases)

> **Status:** 0.10 is the first preview. Keep a copy of important projects.

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
* Command tabs, light, dark and system themes, title block editor,
  PDF/PNG/SVG/DXF export.

### Goals

- **Parts management**: technical data and accessories, import from
  manufacturer data.
- **Terminals**: terminals defined by their part (pitch, cross-section,
  levels, jumpers).
- **Generated reports**: wiring lists and summaries as pages of the project
  that update on their own.
- **Device structure**: plant, location and device tags (IEC 81346) across
  the whole project, with navigators to find any device, terminal or cable.
- **Fast drawing**: reusable macros, automatic wire numbering and cross
  references.
- **Compatibility**: QElectroTech projects open in FaseCAD unchanged.

### Principles

- Free software under the GPL, forever, with no paid tier.
- Projects stay in an open XML format.
- Improvements that fit QElectroTech are offered back to it.

### Building

FaseCAD is written in C++17 with Qt 6 and built with CMake.

```sh
git clone --recursive https://github.com/Brandix97/FaseCAD.git
cd FaseCAD
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DPACKAGE_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On Windows the build uses MSYS2 UCRT64 (Qt 6, CMake, Ninja); see
`fasecad-dev/setup-windows.ps1`. Add `-DBUILD_WITH_KF=OFF` when the KDE
Frameworks are not available.

### License and credits

FaseCAD is free software, licensed under the
[GNU General Public License, version 2 or later](LICENSE). It is provided as
is, without any warranty.

FaseCAD is based on [QElectroTech](https://qelectrotech.org/), created in
2007 and maintained by the QElectroTech team and its community,
© 2006-2026 the developers of QElectroTech. All the original work, credits
and copyright notices are kept.

Maintained by Brandon ([@Brandix97](https://github.com/Brandix97)).
