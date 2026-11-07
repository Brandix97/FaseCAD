# Voltaria development notes

Working notes for whoever continues the work, on any machine (including an AI
assistant that has no memory of earlier sessions). Keep this file up to date.

## Repository

- Fork: https://github.com/Brandix97/voltaria (branch `voltaria`, the default)
- `upstream`: https://github.com/qelectrotech/qelectrotech-source-mirror
  (`master` stays identical to upstream; merge it into `voltaria` to update)
- Commit identity: `Brandon <59540263+Brandix97@users.noreply.github.com>`
- Upstream rules for code changes are in [AGENTS.md](../AGENTS.md): follow
  them so improvements can be offered back to QElectroTech.
- Never use "EPLAN" in names or branding (trademark).

## Windows setup

Run [setup-windows.ps1](setup-windows.ps1) once, then start the program with
[run-voltaria.bat](run-voltaria.bat).

- Toolchain: MSYS2 UCRT64, Qt 6, KDE Frameworks 6, GCC, CMake, Ninja.
- GCC uses about 3 GB of RAM per job: on a 16 GB machine build with `-j 4`,
  or the whole PC freezes.
- A debug build takes about 40 GB of disk.
- `gh.exe` sometimes keeps hanging after it has finished: wrap it in
  `timeout` and set `GH_NO_UPDATE_NOTIFIER=1`.

## Goal

The best free software for electrical diagrams, with a device-oriented
workflow: parts, terminals, cables and reports generated from the project.
Priorities chosen by Brandon:

1. Parts database (in progress, see below)
2. Terminals linked to parts
3. Automatic terminal strip summaries and reports

## Current task: parts database, phase 1

Status: plan written, waiting for two decisions before coding.

What exists today: `sources/materiallist/` reads a CSV "material file"
(one per user, path in Settings > General). Picking an article from
`ElementInfoWidget` copies its columns into the element informations: the main
block plus auxiliary blocks 1 to 4 (`MaterialList::columnsForBlock`,
`elementInfoKey`). The element keeps no link to the article. The bill of
materials and the nomenclature read those element informations.

Plan:

- SQLite parts database, new folder `sources/partsdb/`. Fields:
  manufacturer, order number, internal number, description, designation,
  supplier, category, model, voltage, current, width/height/depth, unit,
  price, datasheet URL, notes; free technical data as key/value; accessories
  (part -> part, quantity).
- The part is still copied into the element informations, so projects stay
  100 % compatible with QElectroTech. Link = manufacturer + order number
  (existing fields, no file format change).
- Parts manager window (Tools > Parts manager): tree by category or
  manufacturer, searchable table, edit form, new / duplicate / delete.
- Import of the existing CSV material file; CSV import and export.
- The existing "choose article" button uses the database; accessories fill
  the auxiliary blocks.
- Setting "Use the parts database" + path, off by default.
- Tests in `tests/qttest`.
- Phase 2: terminals linked to parts, more than 4 parts per element,
  manufacturer catalogue import.

Open questions for Brandon:

1. Database location: one per PC (user folder) or a shared network folder
   for the whole company (needs careful locking)?
2. Extra fields wanted (manufacturer article number, EAN, weight, power
   loss, colour...)?

Upstream check (2026-11-07): no open work on a parts database. Related:
PR #1680 (grouped parts list with summed quantities), PR #1400 (cabinet
layout, open).
