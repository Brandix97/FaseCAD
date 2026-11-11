# FaseCAD development notes

Working notes for whoever continues the work, on any machine (including an AI
assistant that has no memory of earlier sessions). Keep this file up to date.

## Folders on Brandon's PC (since 2026-11-10)

- Everything FaseCAD lives in `C:\Users\Brandon\FaseCAD\`: `marca/` and
  `web/` (brand kit and website) and `software/`, this repository (branch
  `fasecad`). The former folders `C:\Users\Brandon\voltaria` and
  `voltaria-partsdb` are gone: older notes below that name them mean
  `software/` now.
- Builds: `software/build` (Debug) and `software/build-release`
  (RelWithDebInfo, the one to run and package), both with
  `-G Ninja -DPACKAGE_TESTS=ON -DBUILD_WITH_KF=ON -DBUILD_KF=OFF
  -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64` and the SQLite3 paths of ucrt64.
- Launchers (local, not committed): `fasecad-dev/run-release.bat` and
  `run-dev.bat`; both use `QET_SETTINGS_DIR=fasecad-dev/qet-settings` (the
  new-design settings, local too). Local files are listed in
  `.git/info/exclude`.

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
  the FaseCAD brand kit (C:\Users\Brandon\FaseCAD\marca, made from the
  brandon-engineer.com logo: an A whose right leg is a bolt, square
  frame, red #FA130C) copied to ico/branding/ (logo.svg = svg/
  fasecad-app.svg, logo-16..256.png = software/fasecad-*.png,
  splash.png = software/splash.png), ico/windows_icon/branding.ico =
  ico/fasecad.ico. Exe icon through
  build-aux/windows/branding-icon.rc.in. QETLogo uses it. Texts:
  sources/branding/brandingtranslator.* rewrites "QElectroTech" as
  "FaseCAD" in every translated or source text, except the About
  window and "QElectroTech team" (GPL credit); About header says
  "FaseCAD / based on QElectroTech x". Settings and file names keep
  QElectroTech (shared settings with the installed 0.90).
- Templates (= macros, upstream already has them: right-click > Create a
  template, .qetmak files in the user's macros dir, Collections >
  Templates; the root was renamed "Macros" -> "Templates"). FaseCAD
  ships 4 examples in templates/examples/ (qrc): Direct-on-line start,
  Star-delta start (KM2 fed from Q1, KM3 star bridge, coils linked to
  the power contacts), Digital IO card (S1, B1 PNP, H1, K1 on the
  right channels), Analog IO card (B2 4-20 mA, B3 probe, AO card).
  Made by build-aux/templates/make_example_templates.py (project ->
  `qelectrotech --resave` -> check with --export-png -> templates).
  sources/branding/exampletemplates.* copies them once into
  <macros dir>/Examples at start (setting templates/installed_examples;
  a deleted one never comes back, a user file is never replaced).
  tst_exampletemplates. Names in English (translation pending).
- 2026-11-08 (FaseCAD branch only): examples folder renamed "Default";
  template tree icons 16 px; autonumbering dock hidden the first time
  (setting diagrameditor/autonumbering_dock_hidden_once), still in the
  panels menu. New "Devices" dock (sources/ui/devicespanel.*), tabbed
  with Undo and Projects: every symbol by label from the project
  DATABASE (no folio built), reports/slaves/thumbnails/wire definitions
  left out, natural sort, same labels grouped, double-click shows it;
  View > Panels > Devices. tst_devicespanel. (DB element.type values are
  lower case: simple, master, slave, terminal, next_report...; diagram.pos
  counts from 1.)
- Devices panel: multi-select (Shift/Ctrl), Delete key + context menu
  (Show on the sheet / Delete): one undo macro, one
  DeleteQGraphicsItemCommand per sheet built from that sheet's selection
  (wires go too); bridged/multi-level terminals refused like the sheet.
- Parts database in element properties: the "..." article button of
  ElementInfoWidget opens PartPickerDialog (sources/partsdb/
  partpickerdialog.*: search, category, table, "Parts manager..." as its
  child) when the parts database is used, else the CSV as before. The
  part goes through PartsMaterial::record() -> applyMaterialRecord(),
  accessories fill auxiliary blocks 1-4 with their quantity, one undo
  step. This was phase 1 "choose article uses the database".
- "Parts" tab in element properties (Simple, Master, Terminal):
  sources/partsdb/elementpartswidget.* + partslinks.* (list kept in the
  element information "parts": one line per part, manufacturer TAB
  order number TAB units). The tab edits through ElementInfoWidget
  (partsValue/setPartsValue, writeMaterialRecord), which hides the key
  from its custom rows and saves it in its own undo command. Adding a
  part asks whether to overwrite the information: part 1 -> main
  article, parts 2-5 -> auxiliary 1-4 (with units as quantity).
  tst_elementparts (app test).
- Bill of materials of the parts (2026-11-08): sources/partsdb/partsbom.*.
  Each device counts its Parts list (units), or else its articles (main
  + auxiliary 1-4, quantity, "2,5" accepted); exclude_from_bom honoured;
  reports/thumbnails/wire definitions skipped (slaves count, like the
  nomenclature). Lines grouped by manufacturer + order number, units
  added, device labels listed; parts database fills description,
  supplier, unit... Read from the project database cache, no folio
  built. Table on the sheet: CableListModel identifier "parts_bom"
  (CableQueryWidget(PartsBomTag)), saved with the project, refreshes.
  Actions diagrameditor.add_parts_bom / export_parts_bom; menu Lists and
  Project; ribbon Devices > "Parts list", Tools > Reports / Export.
  tst_partsbom (app test). Spare terminals of strips are not counted.
- Terminal diagram style (2026-11-09): DrawStyle::TerminalDiagram=3
  ("terminal_diagram"), TerminalStrip/GraphicsItem/terminaldiagramdrawer.*.
  Table per terminal: function text ("=" repeated), PLC address of inner
  device, inner pin, bridges, colour, type symbol, section, number; under
  it the outer wire with its cable core colour (Cable::colorOfCore, else
  wire colour, else number) and one box per device (label, pins, symbol).
  Data via AbstractRealTerminalInterface::diagramColumn(). Outer side =
  side with a cable core, else lower. tst_terminaldiagram.
- Terminal device (2026-11-09): sources/terminals/ (faseterminal,
  terminalsettingswidget, terminalpropertieswidget), symbol
  terminals/fasecad_terminal.elmt in qrc, embedded as
  import/FaseCAD/terminal.elmt. Toolbar "Add terminals..." (data
  "terminal", id diagrameditor.add_terminal; ribbon Insert > Devices and
  Devices > Terminals): form strip/number/colour (RD BU BG GNYE BK)/type
  (normal fuse sectional PE diode LED)/section (0.5-95), then click-place
  with number +1 (DiagramEventAddElement::setPrepare). "Terminal" tab in
  properties. FaseTerminal::ChangeCommand: label "-STRIP:N" (formula
  removed), terminal_color/terminal_section infos, type via ElementData,
  moves/adds to strip (created if missing), one undo step.
  stripNameForLabel ignores a leading "-". Beige added to TerminalColor.
  tst_faseterminal. Fixes 2026-11-09: Terminal tab is applied after the
  Information tab (shown first via moveTab) so Info's command can't write
  the label back; pasted terminals get the next free number of their strip
  (FaseTerminal::pastedTerminals/numberPasted in PasteDiagramCommand, same
  step, only when pasted elements are auto-numbered).
  Live panel fix: TerminalPropertiesWidget overrides setLiveEdit (base
  never set m_live_edit); strip/number apply on Enter/focus out, combos at
  once; form refreshes on elementInfoChange. Level field 1-4: same strip +
  number = one physical terminal (Group/UnGroup/ChangeTerminalLevel);
  ChangeCommand now runs its steps one by one (step()), owns them.
  Schematic/realistic styles: under the cable each wire goes to its device
  box (pins, label, symbol) when devices are known (drawDeviceFan in
  terminalstripdrawer.cpp), else the old single target box.
  Strip name once per row: labels "-S:n" whose left neighbour (folio,
  x then y) is the same strip show the number only
  (Element::showsTerminalNumberOnly / labelForDisplay, set in
  TerminalStripLabels::refresh, rescheduled on elementInfoChange). Empty
  strip in the form = FaseTerminal::inheritedStrip (left neighbour).
  Levels stepped: realistic (geometry width x levels, drawSteppedLevels,
  levelInset), table (bars in number row), sheet symbol
  (TerminalElement::setLevelMark).
  Level marks / number-only did not follow paste + level change:
  TerminalStripLabels::schedule returned early when strip labels and
  bridges were both off; removed. ChangeCommand::scheduleLabels also
  refreshes the folios of every terminal of the strip (redo and undo).
  tst_autonumincrementwidth fails (spin box not visible): untouched area,
  not checked whether it failed before.
  Example project generator:
  scratchpad terminals/make_terminal_example.py (not in repo).
- Project extensions (FaseCAD branch only): saved as .edrw (XML),
  .edrwz (zipped, the default), .edrwdb (database); .qet/.qetz/.qetdb
  still open. One place: sources/branding/projectextensions.h, used by
  QETProject::isContainerPath, QetDbFile::isDatabasePath, the open
  filter, handledFileExtensions, the save dialog, delivery export,
  previous versions. "Save a copy for an older QElectroTech" stays .qet.
  Checked: --resave to .edrw (XML) and .edrwz (zip) both reopen.
- Backup copies `examples/2026-11-08-*_industrial.qet` (made by the
  program's "Create a backup copy?" dialog during GUI tests) were MOVED
  to the session scratchpad (examples-backups/): they made
  tst_multielementinfo fail, since it opens every examples/*.qet by name.
  Never open examples/*.qet directly in GUI tests: use a copy.
- Devices panel reads the database only while visible (refresh on show).

Previously open question (answered above):

1. Extra fields wanted (manufacturer article number, EAN, weight, power
   loss, colour...)?

Upstream check (2026-11-07): no open work on a parts database. Related:
PR #1680 (grouped parts list with summed quantities), PR #1400 (cabinet
layout, open).

## Rename to FaseCAD everywhere (2026-11-09)
- Kept on purpose: GPL licence headers ("Copyright ... The QElectroTech
  Team") in every file, author/translator credits in About, the .qet
  extension and qelectrotech_version XML attribute (compatibility), the
  CMake target name `qelectrotech` (tests use $<TARGET_FILE:qelectrotech>),
  .ts translations (BrandingTranslator rebrands tr() texts at run time),
  ChangeLog, examples, elements collection, Linux hicolor icon names and
  MIME ids, the "Lancer QET.bat" file name.
- Changed: exe OUTPUT_NAME fasecad (build/fasecad.exe; run-partsdb.bat
  updated; old build/qelectrotech.exe left, stale). Org/app name FaseCAD,
  domain fasecad.com (main.cpp via utils/brandmigration). First start
  copies (never moves) QElectroTech settings + AppConfig/AppData folders +
  stalefiles to the FaseCAD ones; marker key brand/copied_from_former_name.
  With QET_SETTINGS_DIR: <dir>/FaseCAD/FaseCAD.ini.
- About: FaseCAD text, fasecad.com, line "Based on QElectroTech, (c)
  2006-YYYY ... GPL v2 or later". Help links -> fasecad.com,
  donate -> fasecad.com/donate.html, upstream YouTube entries removed.
- Non-tr literals (CLI usage, MCP live-server errors, PDF creator, project
  lock "app", diagnostics, .qetz errors) say FaseCAD. Tests updated.
- Installers: WiX/CPack new UpgradeCode 7D02C145-5F03-4EE2-AB9B-31E7C8399DE4
  and component GUIDs (would otherwise upgrade/replace an installed QET),
  FaseCAD names/registry keys, nsis_base/files/ico/fasecad.ico (kit icon).
- qet-mcp: finds fasecad(.exe) first, writes settings for both names,
  scripts dir FaseCAD/FaseCAD. README rewritten for FaseCAD.

## Release 0.10 (2026-11-10)
- Visible version QetVersion::productVersion() = "0.10" (About,
  --version, PDF creator, diagnostics); currentVersion() stays 0.200.1
  (file format, compared when projects open).
- First start on a computer without settings under either name
  (BrandMigration::applyFirstStartDefaults, GUI only): command tabs,
  parts database, theme System.
- Portable zip FaseCAD-0.10-windows-x64.zip: cmake --install of
  build-release (RelWithDebInfo) + strip --strip-debug + windeployqt6
  --compiler-runtime + every /ucrt64 DLL found by ldd; FaseCAD.bat at the
  root. Checked with PATH=C:\Windows only (--version, export, first start).
  No installer: NSIS/WiX not installed on this PC.
- Full ctest before release: 221/229; fixed tests that still used the
  QElectroTech settings folder and two that expected the autonumbering
  dock shown (FaseCAD hides it on first start). Known on Windows:
  tst_dockminwidth (upstream), tst_lazyfolios (probe hangs; end
  lazy_folios_probe.exe by hand), resave/lazysave timeouts at -j4.
- Launchers fasecad-dev/run-partsdb.bat and run-release.bat use
  QET_SETTINGS_DIR=fasecad-dev/qet-settings (new design settings).

## Cabinet construction 2D (started 2026-11-11)
Brandon's request. Plan agreed in phases; 1-4 written on 2026-11-11
(not committed yet), 5-6 to do.
1. DONE Folio kind: `Diagram::FolioKind {Schematic, Cabinet}`, XML
   attributes `folioKind="cabinet"` + `cabinetScale` on <diagram>,
   written for a cabinet only (old files and schematic folios unchanged).
   Scale 1:N, default 1:5; `Diagram::pixelsPerMm()` = 2/N (2 px = 1 mm of
   sheet, as the dimensions). `QETProject::addNewCabinetDiagram(pos)`.
   Project panel context menu on a folio: "Insert a sheet number above"
   removed, "below" renamed "Insert a schematic sheet", new "Insert a
   cabinet construction sheet" (inserted below). Cabinet folios have
   their own icon (tree + tab). Dimensions on a cabinet folio show the
   real measure (x scale).
2-4. DONE `sources/cabinet/`: `CabinetItem` (QGraphicsItem type 1016;
   kinds MountingPlate, DinRail, WireDuct; measures kept in mm, drawn at
   the folio's scale; <cabinet_items><cabinet_item .../>), `CabinetSnap`
   (5 mm grid + edges of the other parts; Ctrl = free),
   `cabinetcommands` (ChangeCabinetItemCommand, ChangeCabinetScaleCommand),
   `cabinetdialogs` (duct width/height, plate size, scale, edit a part on
   double click; icons painted in code). Tool
   `diagramevent/diagrameventaddcabinetitem`: duct/rail drawn click-click
   (or drag), horizontal/vertical by the mouse, yellow box under the mouse
   with the length in mm, a length can be typed + Enter; plate follows the
   mouse and one click places it. Editor: actions add_cabinet_duct/rail/
   plate + cabinet_scale, in a menu (`menu.cabinet`, Edit menu and the
   Insert tab button "Cabinet construction 2D", enabled on a cabinet
   folio only). DIN rail has no dialog (TH35-7.5; profile editable by
   double click), as Brandon asked. Tests: `tst_cabinetfolio` (17) and
   the app test `tst_cabinetcommands` (4; QET_CABINET_SHOT=<file.png>
   saves a picture of a painted cabinet). Spanish texts added by hand to
   lang/qet_es.ts (unfinished). Not checked by hand in the real window
   yet: Brandon tries it in the debug build (run-dev.bat). In the test
   picture the hyphen and the point of "TH35-7.5" were missing (font of
   the offscreen platform?): look at it in the real program.
5. TODO Cabinet device navigator (the "Navigator" placeholder on the
   Devices tab): devices of the schematic with their reference, placed /
   not placed, linked to the schematic element; a device without DXF is a
   rectangle of the parts database's width x height.
6. DONE (2026-11-11) DXF import: own reader `sources/cabinet/dxfreader`
   (LINE, LWPOLYLINE/POLYLINE with bulges, ARC, CIRCLE, ELLIPSE, SPLINE as
   NURBS, SOLID/TRACE, INSERT of blocks; $INSUNITS to mm; texts and
   dimensions left out and counted). A drawing is a CabinetItem of kind
   Drawing ("dxf_drawing", path kept in the project as <path d="M.. L..
   C.."/>, no link to the file), placed like a plate with one click.
   Command "Insert a DXF…" (diagrameditor.add_dxf, Insert tab next to
   Cabinet construction 2D), on any folio: real size on a cabinet folio,
   1:1 on a schematic. $EXTMAX of manufacturer files can be stale: do
   not trust it. Tests: tst_dxfreader (FASECAD_DXF_SAMPLES=<folder>,
   FASECAD_DXF_SHOTS=<folder> for pictures). His test files are in
   C:\Users\Brandon\FaseCAD\dxf-pruebas (3 manufacturer front views,
   AC1024, mm, entities LINE/ARC/CIRCLE/LWPOLYLINE/SPLINE/ELLIPSE).
Snapping on a cabinet folio (2026-11-11, Brandon's request):
`sources/cabinet/cabinetsnap`. The magnet (Diagram::gridSnap) on a cabinet
folio catches corners and side middles of the other CabinetItems (10
screen px), else lines up per axis with them, else whole mm (no grid);
orange marker: square = corner, triangle = middle. Used when moving a
part, placing a plate/DXF, both ends of a duct/rail, and the points of a
dimension on a cabinet folio. Ctrl or magnet off = free. Rails are not
painted under a DXF drawing (clipped), as Brandon asked.
New files of FaseCAD carry "Copyright 2026 The FaseCAD Team / This file
is part of FaseCAD" (Brandon asked, 2026-11-11); files from QElectroTech
keep their header (GPL).
Ideas offered, not done: handles to stretch a rail/duct with the mouse,
rotate with space, plate carrying what is on it, cutting list (metres of
duct/rail per type) into the material list, overlap warning.
