# FaseCAD development notes

Working notes for whoever continues the work, on any machine (including an AI
assistant that has no memory of earlier sessions). Keep this file up to date.

## Repository

- Fork: https://github.com/Brandix97/FaseCAD (branch `fasecad`, the default)
- `upstream`: https://github.com/qelectrotech/qelectrotech-source-mirror
  (`master` stays identical to upstream; merge it into `fasecad` to update)
- Commit identity: `Brandon <59540263+Brandix97@users.noreply.github.com>`
- Upstream rules for code changes are in [AGENTS.md](../AGENTS.md): follow
  them so improvements can be offered back to QElectroTech.
- Never use "EPLAN" in names or branding (trademark).

## Windows setup

Run [setup-windows.ps1](setup-windows.ps1) once, then start the program with
[run-fasecad.bat](run-fasecad.bat).

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

Decided (2026-11-07):

- Database location: local by default (user data folder). Settings get an
  option to choose another database file, e.g. one on a network share. With
  a shared file, open SQLite with a busy timeout and handle "database is
  locked" with a clear message instead of failing silently.

- Fields for phase 1, as asked by Brandon: technical data (key/value),
  supplier reference, internal reference, weight, dimensions (width,
  height, depth). Added on top of the plan above: supplier reference and
  weight. More fields later.
- Also asked: an image of the part and a DXF drawing (e.g. for cabinet
  layout). Decided: copied inside the database (BLOB) by default, with an
  option per file to only link it (path) instead. Buttons "Add image...",
  "Add DXF...", "Save as..." in the part form; Brandon never edits the
  database by hand.

### Design draft (2026-11-07, waiting for Brandon's OK)

Branch `parts-database` made from upstream `master` (AGENTS.md), merged into
`fasecad` when ready. SQLite through Qt (`QSQLITE`, already used in
`cli_export.cpp`), so no new dependency.

Tables:

- `part`: id, manufacturer, order_number (unique together with
  manufacturer), internal_reference, supplier, supplier_reference,
  description, designation, category, model, voltage, current, width,
  height, depth (mm), weight (kg), unit, price, datasheet_url, notes,
  created, modified.
- `part_data`: part_id, key, value (free technical data).
- `part_file`: part_id, kind (`image` / `dxf`), file_name, content (BLOB),
  or a path when the file is linked instead of embedded. Kept out of `part`
  so lists stay fast.
- `part_accessory`: part_id, accessory_id, quantity.
- `meta`: schema version, for later upgrades.

Code in `sources/partsdb/`:

- `partsdatabase.h/.cpp`: open, create, upgrade the schema; busy timeout;
  read/write parts. No widgets, so tests can use it.
- `partsmanagerdialog.h/.cpp/.ui`: Tools > Parts manager.
- `partsimport.h/.cpp`: import of the CSV material file, CSV import/export.
- Settings > General: "Use the parts database" (off by default) + database
  file (default: user data folder, `QStandardPaths::AppDataLocation`).
- Tests: `tests/qttest/partsdatabase`.

### Progress

- 2026-11-07 (desktop PC): branch `parts-database` (worktree
  `C:\Users\Brandon\voltaria-partsdb` on the desktop), not committed yet.
  Done: `sources/partsdb/partsdatabase.h/.cpp` (schema, parts, technical
  data, accessories, copied/linked files, settings, busy and newer-version
  errors) and `tests/qttest/tst_partsdatabase.cpp`: 11/11 pass (compiled
  standalone with MSYS2 GCC 16.2 / Qt 6.12, `-Wall -Wextra` clean).
  Next: parts manager window, Settings page entry, CSV import.
- 2026-11-07: parts manager window done, `sources/partsdb/
  partsmanagerdialog.h/.cpp` (code-built, no .ui), menu Lists > Parts
  manager... (`m_parts_manager` in QETDiagramEditor). Checked with a
  standalone demo and screenshots (tree, search, form, image preview).
  Not yet built inside the whole program.
- Dark mode: already exists upstream in two pieces, Display > Dark mode
  (sheet) and Settings > General > Application color (interface). Brandon
  asked for dark mode and chose one new option: Settings > General >
  Theme: "Colors below (as before)" (default) / Follow the system /
  Light / Dark. Done in the same `parts-database` worktree (to be split
  into its own branch before any pull request): `QetSettings::theme()`,
  `QETApp::applyTheme()` (Fusion + `QET::Palette::fusionLight/Dark`,
  follows Windows live), combo in generalconfigurationpage.ui,
  `tests/qttest/tst_themesetting.cpp` 6/6 pass standalone. A sheet on
  "System color" follows the dark palette by itself.
- 2026-11-08: command tabs (Brandon's request, ribbon-like; never call
  it after the trademark). Settings > General checkbox, off by default.
  `sources/ui/commandtabs.h/.cpp`: 7 tabs (Home, Insert, Connections,
  View, Project, Lists and reports, Tools), groups with captions, icon
  with text under it, menus kept, classic toolbars hidden while on.
  Buttons trigger the editor's own actions by ShortcutManager id
  (registered add_cable_list, export_cable_list, cable_numbering).
  `QETDiagramEditor::updateCommandTabs()` from rebuildToolBars().
  `tests/qttest/tst_commandtabs.cpp` 6/6 pass standalone.
- Full test run of `voltaria` (2026-11-07): 204/216 at -j 8; of the 12,
  the timeouts pass when run alone (slow debug build, -j 8 too heavy for
  them). Real failures, upstream code, Windows only so far:
  tst_dockminwidth (wire editor needs 359 px > 300),
  tst_propertiesdockscroll, tst_lazyfolios (lazy_folios_probe hung,
  ctest's timeout does not kill the grandchild on Windows).
- 2026-11-08: `parts-database` worktree built (1474/1474, same 4
  upstream warnings). 13/13 targeted tests pass (the 3 new ones plus
  palette, toolbar, shortcut, menu and settings tests). Checked in the
  real program with QET_SETTINGS_DIR (theme=3, command_tabs=true): dark
  interface and sheet, tabs shown. Fixed: ToolbarSettings::applyTo made
  the tabs movable again; toolbars with property `qet_fixed_toolbar` are
  now skipped. Launcher: `fasecad-dev/run-partsdb.bat` (untracked).
- Next: Brandon tries it; then split into branches (parts database,
  theme, command tabs, cable management), commit, push.
- 2026-11-08: "gestion de mangueras" (cable management), asked by
  Brandon; he chose all four parts, done in phases:
  1. DONE Cable manager + core occupation: `sources/cablelist/
     cablemanagerdialog.h/.cpp` (Lists > Cable manager..., tab
     Connections), `cablechecks.h/.cpp` (no name, duplicate name, no
     type, no core wired, fewer spare cores than wanted; setting
     cablemanager/minimum_spare, default 1), `tst_cablechecks` 5/5.
     CableList rows now also carry "uuid" (not a column). Edits go
     through ChangeCablePropertiesCommand on the project undo stack.
     Checked in the real program on a copy of industrial.qet with three
     cables added by hand (scratchpad, made with `--resave` for uuids):
     list, problems, cores used/spare with wire/from/to, edit + Ctrl+Z.
  2. DONE Cable plan: CableListModel with identifier "cable_plan" shows
     CableList::coreRows() (one row per core; keys plan_*; optional
     <only_cable> uuid saved in the model XML). Lists > Add a cable plan
     (AddTableDialog + CableQueryWidget(project, ...) with a cable combo,
     all columns preselected), Project > Export the cable plan to CSV
     (CableList::coresToCsv), PDF via the sheets. The manager's cores
     table now reuses coreRows. `tst_cableplan` (app test). Bugs found
     and fixed on the way: crash opening a project with a cable table
     (model connected to the project's undo stack before it existed;
     `QETProject::m_undo_stack` now starts as nullptr), and a plan read
     from file showing every core spare (now refreshed on
     projectDataBase::dataBaseUpdated). Checked with --export-png.
     Upstream issue seen, not ours: industrial.qet copy with 3 cables
     takes 13 s on "cables" and prints 618 "disconnect: Unexpected
     nullptr" warnings in the original build too.
  3. DONE Cable types from the parts database. Parts schema v2 (v1 is
     upgraded with ALTER TABLE): core_count, core_colors (|), section,
     sheath_color; parts manager tab "Cable". `partscabletypes.h/.cpp`
     turns parts with cores into CableTypeRecord lines (name = article
     number, else model, else order number; column "source").
     CableCreateDialog::addPartsDatabaseTypes() appends them after the
     CSV lines when the database is used; they are never written to the
     CSV (m_file_columns; editing one says to use the parts manager).
     Settings > General now has "Use the parts database" + its file +
     Browse (this was missing from phase 1 of the parts database).
     tst_partsdatabase 14/14. NOT yet checked by drawing a cable in the
     real program.
- Slowness analysis (2026-11-08): Release is only 15-25% faster than
  Debug. Real cause: building a folio costs ~0.3 s; a project with
  cables builds ALL folios at open (QETProject::cableLabelFolios()
  returns diagrams() when there are cables, to follow cores through
  folio reports): industrial.qet 2.6 s -> 18.7 s with 3 cables. Ideas:
  build only folios holding cable cores + report chains; profile
  Diagram::buildDeferred. Also upstream bug: Element::~Element calls
  disconnect(nullptr, nullptr, this, nullptr), which Qt refuses (618
  warnings at close, and the protection it intends does nothing).
  Release build dir: `build-release` (excluded from git).
- Command tabs v2 (ribbon like the CAE software Brandon uses; never
  name it): large/small/icon-only buttons in columns of 3, menus (▾),
  split buttons for connection symbols and "Place texts", placeholders
  "Cabinet construction 2D" (Insert, Devices) disabled with "coming
  soon", search box + fold arrow in the tab corner, 9 tabs (File, Home,
  Insert, Edit, View, Devices, Connections, Tools, Master data). Extra
  ids resolved by QETDiagramEditor::commandTabsAction(): panel.projects,
  panel.collections, panel.undo, menu.all (every menu bar menu). Option
  "Hide the menu bar too" (QetSettings::commandTabsHideMenuBar, off).
  Symbol icons recoloured on dark themes. English only for now: Spanish
  translation to be prepared later (Brandon's choice).
- 2026-11-08: RENAMED Voltaria -> FaseCAD ("Voltaria" was taken).
  GitHub repo Brandix97/voltaria -> Brandix97/FaseCAD (GitHub redirects
  the old URL), default branch voltaria -> fasecad, notes folder
  voltaria-dev -> fasecad-dev, run-voltaria.bat -> run-fasecad.bat.
  The folders on the desktop PC keep their old names for now:
  C:\Users\Brandon\voltaria (main clone) and
  C:\Users\Brandon\voltaria-partsdb (worktree, branch parts-database).
  On the laptop: `git remote set-url origin
  https://github.com/Brandix97/FaseCAD.git`, `git fetch origin --prune`,
  `git branch -m voltaria fasecad`, `git branch -u origin/fasecad`.
  Branding files use neutral names, so a future rename only touches
  BrandingTranslator::productName(), the About header and the images.
- Branding (belongs to the `fasecad` branch, NOT to upstream PRs):
  logo F1 (F whose stem ends in a bolt, white on red tile, style of the
  earlier "2a") in ico/branding/ (logo.svg, logo-16..256.png,
  splash.png), ico/windows_icon/branding.ico, exe icon through
  build-aux/windows/branding-icon.rc.in. QETLogo uses it. Texts:
  sources/branding/brandingtranslator.* rewrites "QElectroTech" as
  "FaseCAD" in every translated or source text, except the About
  window and "QElectroTech team" (GPL credit); About header says
  "FaseCAD / based on QElectroTech x". Settings and file names keep
  QElectroTech (shared settings with the installed 0.90).
- Backup copies `examples/2026-11-08-*_industrial.qet` in the
  partsdb worktree were made by the program's "Create a backup copy?"
  dialog during tests: never commit them; ask Brandon before deleting.

Previously open question (answered above):

1. Extra fields wanted (manufacturer article number, EAN, weight, power
   loss, colour...)?

Upstream check (2026-11-07): no open work on a parts database. Related:
PR #1680 (grouped parts list with summed quantities), PR #1400 (cabinet
layout, open).
