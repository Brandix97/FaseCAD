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

## Version 0.11 (2026-11-11)
- Visible version 0.11 (commit 627fcf5d5): cabinet construction 2D.
- Portable zip FaseCAD-0.11-windows-x64.zip (86 MB) made from
  build-release the same way as 0.10.1, checked with PATH=C:\Windows
  only (--version), copied to FaseCAD\web\httpdocs for Brandon to upload.
  The download page of the website lists 0.11 (construir.py VERSION /
  DESCARGA, site regenerated). Not pushed, no GitHub release yet.
- The zip starts from FaseCAD.exe at its root, no FaseCAD.bat (Brandon's
  rule from 2026-11-11: what he is given to start is always an exe).
  Launcher source build-aux/windows/launcher/fasecad-launcher.c + .rc
  (icon fasecad.ico, no console); it starts bin\fasecad.exe with
  --common-elements-dir=elements/ --common-tbt-dir=titleblocks/
  --lang-dir=lang/ in the folder, and hands on its arguments. README
  says FaseCAD.exe.

## Device properties like the manufacturer packages (started 2026-11-12)
Brandon's request: give the properties the same structure as the part
packages (.edz) of the manufacturer portals his CAE software uses (never
name it, in code or texts). His example, in Downloads\ (folder name is
his): a compact PLC as .edz, a DXF for the
cabinet and a STEP "for the cabinets in 3D one day".
Status: analysed, plan below waiting for his OK (AGENTS.md).

What the .edz holds (a 7z archive, LZMA; Windows tar fails on it; read
with scratchpad py/sevenzip.py, every CRC fine):
- manifest.xml: one package of type part; items part xml, manufacturer
  xml, connectionpoints xml, picture, two macros (schematic, 2D/3D).
- part.xml: one <part>, ~70 article properties: order number, type
  number, GTIN, descriptions in several languages ("en_US@...;de_DE@...;"),
  product group (top / group / sub), disciplines (electrical, fluid...),
  code letter, width x height x depth, clearances (above, below, front),
  mounting space, weight, quantity unit, external document URL, picture
  and macro paths. <variant>: technical data (voltage, power loss, wire
  section, PLC: CPU, station type, PLC type) and 37 <functiontemplate>: position, function
  definition (category / group / id), plug (X10 X11 X12 P1), pin
  designation (1..20), pin description (L+ M FE, DI a.0, AI 0, DQ a.0...),
  PLC address index, bus interface.
- connectionpoints.xml: 35 pins: plug, name, 3D position and direction,
  min/max section (mm2 and AWG), max wires, torque, stripping length,
  screwdriver, terminal type.
- manufacturer.xml: the manufacturer's address.
- .ema macros: XML with the proprietary numeric property ids, 3D as
  binary mesh streams. 14 variants of the one device -K1: multi-line parts
  (power supply, DI, AI, DQ, rack), single-line (Profinet port), overview
  (whole PLC, 37 pins), panel layout. Not to import: FaseCAD can make its
  own from the function templates.
- The DXF (AC1015, mm; LINE ARC ELLIPSE SPLINE POINT): DxfReader draws it
  as is, at the device's real size (tst_dxfreader manufacturerFiles,
  picture checked).
- The STEP (AP214, from NX): exact B-rep, 319 solids (planes, cylinders,
  B-splines). Showing it needs a B-rep kernel: OpenCASCADE (LGPL 2.1 with
  exception, MSYS2 package) - later, ask before installing.

Levels, theirs -> FaseCAD:
- Project -> QETProject properties (exist).
- Page: type, description, = and + that its functions inherit -> folio:
  FolioKind (Schematic, Cabinet), title block plant / locmach (exist).
- Function: device tag, function definition, pin designations and
  descriptions, plug, function text, technical data, PLC data, extra
  fields -> element informations: label, plant, location, function, plc_*
  exist; missing: ++ installation site, function kind, per-pin
  description and plug, technical data, numbered extra fields.
- Device (functions with the same tag + their parts) -> elements with the
  same label (Devices panel) + information "parts".
- Part -> partsdb `part` (+ part_data, part_file); function templates and
  connection points -> new tables.
- Macro with variants -> templates (.qetmak); variants made from the
  function templates.

Plan offered (phases):
1. Parts database v3 + .edz import: the article fields above, tables
   part_function (templates) and part_connection (pins), files image /
   DXF / STEP, texts in several languages (shown in FaseCAD's language).
   Parts manager tabs Functions, Connection points, Files. Import of
   .edz (part, pins, manufacturer, picture; macros left out).
2. Element properties grouped like theirs: Identification (= ++ + -,
   inherited from the folio), Function (kind, text, technical data, extra
   fields), Connection points (pin, description, plug, kind, PLC
   address), Parts (exists), Display. Stored as element informations, old
   files unchanged.
3. Make the PLC boxes of a part from its function templates (power
   supply, DI, AI, DQ, bus port), one device tag, like the variants.
4. Cabinet device navigator (cabinet phase 5): devices of the schematic
   with their part, drag one onto a cabinet folio = its DXF at real size
   (or its width x height), clearances shown.
5. Later: 3D (STEP through OpenCASCADE, pins' 3D positions for wire
   lengths).
Questions asked: which phase first; how to read the 7z of an .edz (the
public-domain LZMA SDK decoder in the tree, MSYS2 libarchive, or none:
the user unzips with 7-Zip).
Brandon (2026-11-12): start; he is not sure FaseCAD should open .edz at
all (rights of the other vendor), so phase 1 is done WITHOUT any .edz
reading; the importer waits for his decision. Answer given: reading a
file format is lawful in the EU (CJEU C-406/10 SAS Institute: data file
formats are not protected by copyright), commercial tools import .edz
openly (CADMATIC Electrical, AUCOTEC Engineering Base); the limits are
the trademark (never name it), the data's own rights (never ship or
publish manufacturers' files, e.g. in tests or the website), the terms
of the portal the user downloaded from (bind the user), no circumvention
of protections, no automated download from their portal. Not legal
advice: an IP lawyer for certainty.
Phase 1 DONE (not committed):
- Parts database schema v3 (`PartsDatabase::schemaVersion()` 3; v1 and v2
  upgraded in one transaction by `upgradeSchema`): part columns gtin,
  code_letter, power_loss, clearance_top/bottom/left/right/front/rear;
  tables part_function (kind id, plug, pin, description, address) and
  part_connection (plug, pin, section min/max, max wires, stripping
  length, torque, screwdriver, x y z, direction "x y z"); file kind
  "step". `PartFunctionKind` is FaseCAD's own list (ids never change:
  supply_plus, digital_input, bus_port, coil, normally_open...), names
  translated in context PartFunction; unknown id = Other.
- Parts manager: tabs General, Mounting (size, clearances, power loss),
  Technical data, Functions (kind list per row; Add copies the selected
  row with the next pin/description/address: "DI a.0" -> "DI a.1"; Move
  up/down), Connection points (Add copies the wire data with the next
  pin; "From the functions" adds the missing plug+pin), Accessories,
  Cable, Files (image, DXF, 3D model STEP, link). Width/height/depth moved
  from General to Mounting.
- Tests: tst_partsdatabase 18 (+4: functionsAndConnectionsReadBack,
  functionKindsKeepTheirIds, stepModelIsAFileOfItsOwn,
  versionTwoIsUpgraded), new tst_partsmanager (widget test, 4;
  FASECAD_PARTS_SHOTS=<folder> saves a picture of each tab and
  FASECAD_PARTS_QM=<file.qm> loads a language; on this PC the offscreen
  platform garbles letters whatever the font: good for the layout only).
- Spanish: the whole parts module (contexts ElementPartsWidget,
  PartFunction, PartPickerDialog, PartsBom, PartsCableTypes,
  PartsDatabase, PartsManagerDialog: 168 texts, unfinished) with
  scratchpad py/step16.py from an lupdate of sources/partsdb only, plus
  the menu entry "Parts manager..." and its tip (step17.py).
- build-release rebuilt (target qelectrotech, then qelectrotech_lrelease:
  building qelectrotech alone does NOT remake lang/*.qm); fasecad.exe
  --version checked with PATH=C:\Windows only. The Debug build dir was
  left half rebuilt (stopped at 6/517 steps): rebuild before app tests.
Next (phase 2): element properties grouped like the reference
(Identification = ++ + -, Function, Connection points, Parts, Display).

Example project for Brandon (2026-11-12, his request "un proyecto ejemplo
con el 1200"): C:\Users\Brandon\FaseCAD\esquemas\ejemplo-s7-1200.edrwz,
5 folios: power (-X0, -Q1, -G1 PM1207, -K1 part 1, 24 V rails with dots,
folio reports), DI a (S1 S2 S3, B1 PNP), DI b + AI 0 (S4, B5 0-10 V
transmitter), DQ (K11 K12 coils, H1 H2 lamps), cabinet folio 1:1 (plate
400x300, ducts 40x60, TH35 rail, Q1/G1 outlines, the CPU front from his
DXF). Symbols: the collection's 6es7_214-1ag40-0xb0 parts 2-4 (part 4's
"XBA0" typo fixed per instance) + an own "part 1 of 4" (24 V X10:1-3 and
PROFINET X1 P1; the collection's p1 is the 230 V AC 1BG40), all embedded in
the project. Only -K1 part 1 and -G1 carry the article (BOM counts 1 CPU).
Made by scratchpad scripts, NOT in the repo (manufacturer data): py/edz_parts.py
(his part files -> parts.json: 36 functions, 35 connection points with y
from the top and the wire direction flipped), partsimport/ (C++ with
PartsDatabase: writes the parts), dxf2path/ (C++ with DxfReader: the CPU
path), py/make_s7_example.py (project, then fasecad --resave + --export-png
to check). His parts database %APPDATA%\FaseCAD\FaseCAD\parts.sqlite now
v3 with the CPU (image, DXF, STEP) and the PM1207 (typed from the
catalogue); copy of the empty v2 beside it:
parts.sqlite.copia-antes-del-ejemplo-2026-11-12. Run the C++ tools from
their own folder: from build-release they load its Qt DLLs and miss the
SQLite driver.

## Terminal potentials keep the voltage apart (2026-11-12)
Brandon asked what the "Potential" field of a terminal (element editor)
is for: with the block's "Function separation" on, terminals of one value
stay joined and the others are apart; with it off the field does nothing.
Reading the code for the answer showed that the voltage / protocol, which
crosses a separation, also went from one potential to another (a terminal
of two levels: a voltage edit with "Apply properties to all conductors of
this potential" reached the other level) and to a terminal without one.
Fixed on his "Corrigelo" (not committed):
- relatedPotentialTerminal (terminal.cpp): with ignore_function_separation,
  a block whose terminals have a potential (hasTerminalPotentials) keeps
  them apart all the same; a block without any behaves as before (no
  symbol of the collection, the tests or the examples has the attribute).
  Every voltage path goes through Conductor::functionSeparatedConductors:
  properties dialog, search and replace, scripting, multi-edit, bridges.
- New tst_terminalpotential (5 cases): 3 failed before the fix, all pass
  after it; tst_bridgetool, tst_duplicatelabels, tst_potentialtypes and
  tst_conductormultiedit pass too (build-release, ctest -R).
- Left as is: a bridge reaches every pin of the terminal symbol it lands
  on (BridgeManager informationOf, informationSets, writeInformation,
  conductorsOverBridges), so a bridge on a two-level symbol still reaches
  both levels. Changing that is a decision about bridges (offered).
- fasecad.exe relinked on 2026-11-13 together with the terminal texts.

## Terminal texts of a placed symbol (2026-11-13)
Brandon's request (screenshot of a 4-pole switch, terminals 1..8): a tab
of the element properties to change the text of each terminal, and the
terminal strip summaries showing that text. Plan shown, his answer
"prueba y vemos". Not committed.
- Element::setTerminalTexts / terminalTexts / terminalText: texts by
  Terminal::stableUuid() string, empty = the symbol's name. Saved by
  appendDataXml as <terminalTexts><terminalText terminal= text=/>, only
  when set (a symbol without texts saves as before); read in fromXml as
  the prefix is (no store writes while loading). element_value rows get
  it through elementValueRowsOf (tag list + "terminalTexts"), so .qet and
  .edrwz both keep it (generic xmlRows / rowsToXml).
- Terminal::name() = the element's text, else symbolName() (the old
  name(): master contact label or definition name). Everything listing a
  terminal name follows: drawing (boundingRect, prepareNameChange()),
  TrueTerminalStrip device_pin / inner_connection (strip plans), cable
  list, CLI export, projectDataBase terminal table.
- projectDataBase::renameTerminals(Element*): UPDATE terminal.name, emits
  dataBaseUpdated; populateFromDocument applies the texts to the
  terminals of folios not built (master_label cleared for those).
- ChangeTerminalTextsCommand (undocommand/): one undo step, redraws the
  loaded folios so strip plans show the new names. Brandon's first try
  (screenshot, changeover contacto_conmutado 11/12/14): the folio did not
  change, because the collection's symbols draw their numbers as UserText
  dynamic texts, not as terminal names (show_name). The command now also
  rewrites, for each terminal whose name changes, the nearest UserText
  saying its current name (<= 40 px from its dock point, one text per
  terminal; skipped when the terminal has no name or goes back to none).
  Tested on that very symbol (theCollectionContactShowsTheTexts).
- His FaseCAD showed English: HKCU\Software\FaseCAD\FaseCAD lang=en (only
  the settings dialog writes it); left as is, told him.
- "los contactos no me muestra para agregar los componentes": he chose
  parts on contacts (an auxiliary contact block...). Slaves now get
  add_info_and_parts() in ElementPropertiesWidget::updateUi; the
  ElementInfoWidget tab is removed from the tab bar right after (kept in
  m_list_editor, so Apply / live edit save the "parts" information), a
  contact still shows its master's information. PartsBom::devices
  already counted 'slave'; a slave without a label of its own now goes by
  the label of the first symbol it is linked to (its master). Tests:
  tst_elementparts aContactHasAPartsTab, tst_partsbom
  contactPartsGoByTheMaster.
- ElementTerminalTextsWidget (ui/), tab "Terminal texts" ("Textos de
  bornes") for Simple, Master, Slave and terminal symbols with terminals:
  a row per terminal (symbol name, text); a text equal to the symbol's
  name or empty is no text; live edit in the properties panel (queued).
- Spanish: 6 texts, unfinished (scratchpad py/step18.py).
- Test tst_terminaltexts (6 cases, every_feature.qet: contact fc9cc920
  wired to strip X1).

## IEC 60617 panel symbols (2026-11-12/13)
Brandon asked for the symbols "according to IEC 61082-1". That standard
only sets the document rules; the symbols are IEC 60617, a paid database
(subscription, ~CHF 280/year). He chose ~80 panel symbols as .elmt.
- New collection elements/10_electric/05_iec_60617_cuadros (75 symbols in
  12 folders: contacts, coils, contactor main contacts, protection,
  isolation, push-buttons/selectors, position switches and sensors,
  machines, transformers/supplies, signalling/measuring, terminals/earth/
  connectors, components). Names in en/es/fr/de/it/pt/tr/zh.
- Drawn by fasecad-dev/simbolos-iec/generar.py (run it again after any
  change; uuids are stable). --preview FILE.html draws them all.
  Rules there: rest state, 10 grid, poles 20 apart, contacts move right
  when actuated (link and actuator on the left), label on the left
  (right-aligned, grows leftwards), terminal numbers right of each lead.
- Checked against DIN EN 60617 drawings on Wikimedia Commons where unsure
  (switch-disconnector = bar + circle, thermal relay, delay "parachute"
  direction, change-over, position switch, surge arrester). Not verified
  against the official database: detent, positive opening, mushroom head,
  proximity rhombus, integrating-meter band, RCD drawn as contacts + IΔ
  relay + summation CT.
- Seen in FaseCAD (release build, test projects in a copy of the exe):
  all 75 load and draw as in the preview.
- `elements` is the upstream submodule: the new folder is untracked there,
  so git does not carry it to the other machine (regenerate it with the
  script) and it is not in a commit. cmake --install copies it into the
  packages. Where it should live for good is still to decide (fork of
  qelectrotech-elements, or a FaseCAD folder installed next to it).
- The website guide (guia-simbolos) draws the contactor semicircle on the
  right and the change-over mirrored (NC right); IEC drawings have the
  semicircle toward the moving contact and NC on the left. Not changed.

## Collection clean-up, folio index, parts collection, numbering (2026-11-13)
Brandon's list (answers to the questions in brackets):
1. remove QElectroTech's original automatic numbering [instead: the
   symbol of the collection carries its default name, and a placed
   symbol takes the next free number of that name, checked against the
   project's devices];
2. Lists: an automatic folio index [columns: folio and title];
3. delete every manufacturer-specific symbol of the collection [all
   three groups];
4. remove the company collections;
5. a "Parts collection" in the collection panel, ordered as the parts
   manager [dragging a part places its symbol with the part].
Order agreed: 4, 3, 2, 5, 1. Not committed.

- 4 DONE: no company collection of symbols (loadCollections(common,
  false, custom) in ElementsCollectionWidget and ElementDialog) nor of
  title blocks (ElementsPanel::reload, QETApp::
  availableTitleBlockTemplatesCollections); the two folder rows of the
  settings are hidden. company:// locations still resolve, so old
  projects that name one keep working (they embed their copies anyway).
- 3 DONE: deleted elements/10_electric/20_manufacturers_articles (135
  brands, 4236 symbols), elements/60_energy/41_manufacturers_articles
  (20) and the brand folders of 98_graphics/99_assembly_plan/
  01_thumbnails_mounting_plate (23 brands, 559; 99_divers kept): 4272
  symbols left of 9087. NOTE: elements/ is the git SUBMODULE of
  qelectrotech/qelectrotech-elements (no FaseCAD fork); FaseCAD's own
  05_iec_60617_cuadros is untracked there. Recover with
  `git -C elements checkout -- .`. Committing the collection needs a
  decision: a FaseCAD fork of the elements repository, or the folder
  taken into the FaseCAD repository.
- 2 DONE: Lists > "Automatic folio index" (FolioIndex in
  sources/factory/folioindex.*): an "Index" folio first with a summary
  table "SELECT folio, title FROM project_summary_view ORDER BY pos"; a
  second click shows it; a summary table with that query is the index,
  and QetGraphicsTableItem::modelReseted grows it onto more index folios
  (FolioIndex::scheduleGrow) instead of the "Table limitation" warning.
  Not an undo step (like Add a summary). Test tst_folioindex (3 cases).
  Spanish: step19.py (5 texts, unfinished).
- 5 DONE: Parts collection. Parts database schema v4: part.symbol (an
  ElementsLocation string; upgradeSchema adds the column). Parts manager:
  General > Symbol (typed, or "Choose..." through
  PartsManagerDialog::setSymbolChooser, set by QETDiagramEditor with
  ElementDialog::getOpenElementLocation; the standalone test has none);
  every write emits PartsDatabaseChanges::changed. Panel:
  PartsCollectionItem (ElementsCollection/), root "Parts collection" ->
  categories (PartsDatabase::categories() order, "(No category)" last)
  -> parts (parts() order: manufacturer, order number), icon = part
  image or symbol icon; ElementsCollectionModel::addPartsCollection /
  reloadPartsCollection (widget reload + PartsDatabaseChanges); mimeData:
  element uri + text = symbol, "application/x-fasecad-part" = JSON
  manufacturer/order_number (PartsPlacement); no edit/delete menu, only
  "Parts manager..." and reload; flat search skips parts. Folio:
  DiagramView accepts the part mime; dropped onto a placed symbol ->
  PartsPlacement::addPart (one undo step, units +1 if there); otherwise
  startPartPlacement = startElementPlacement + setPrepare(preparer):
  each copy gets the part and the article (PartsMaterial record, block 0)
  when it had no part. Double-click: insertPartRequested ->
  QETDiagramEditor -> startPartPlacement. Tests: tst_partscollection,
  tst_partsdatabase symbolIsKeptAndVersionThreeUpgraded, tst_partsmanager
  symbolIsTyped. Spanish: step20.py (11 texts).
- 1 DONE (Brandon insisted 2026-11-13: "te dije de eliminar la parte de
  enumeración automática"): QElectroTech's numbering formulas are gone
  from the interface, the engine stays (old projects keep their labels;
  cable numbering uses it, CableNumberingDialog). Removed: project
  properties page "Automatic numbering" (ProjectPropertiesDialog: enum
  TerminalStrip = 2, Autonum = -1 shows nothing, changeTo* empty; the
  cable tool opens CableNumberingDialog), the numbering dock (never
  created), Information tab "Automatic numbering"/"Number" rows (not
  built) and "Label formula" row (hidden; typing a label empties the
  formula), conductor "Text formula" and numbering combo (hidden; typing
  the text empties the formula), folio numbering choice of the title
  block, Settings > New sheet "Auto Numbering" tab (only "Cable
  numbering" left). New: DeviceNaming (sources/devicenaming.*), called
  by DiagramEventAddElement instead of Element::setUpFormula: default
  name = the symbol's own label without digits ("_" = none), else "-" +
  folder prefix (qet_labels.xml); label = first free number among
  ElementAutoNumSchemeCommand::labelsHeldBesides; only Simple and Master
  (contacts take the master's label, terminals their strip numbering).
  New wires: only their potential's number (ConductorAutoNumerotation::
  numerate no longer calls numerateNewConductor). qet_labels.xml got
  prefixes for 05_iec_60617_cuadros (K K Q F Q S B M T P X, none for
  12_componentes). Test tst_devicenaming; tst_dockminwidth no longer
  expects the dock. Spanish: step21.py.
- .edz importer REMOVED (Brandon: "no quiero que pongas un importador de
  edz"; it came from upstream QElectroTech, June 2026): action and
  functions of ElementsCollectionWidget, sources/import/ (48 files,
  bundled LZMA SDK) and its lines in cmake/qet_compilation_vars.cmake.
  The brand name is gone from comments too (5 places reworded).

## Several symbols per part, .faspart files (2026-11-13)
Brandon: a "Symbols" tab with several symbols per part, Tab switching
between them while a part from the parts collection is placed; and an
exporter/importer of parts as ".faspart". His answers: one file per part,
symbols inside the file, ask for each part already in the database.
Not committed.
- Parts database schema v5: table part_symbol (part_id, position,
  location); Part::symbols (QStringList) replaces Part::symbol; upgrade
  copies the v4 column into the table (the column stays, unread). New
  databases have no "symbol" column.
- Parts manager: General "Symbol" row gone; tab "Symbols" (list with the
  symbol's name and icon via setSymbolDescriber, Add... through the
  symbol chooser or a typed location, Remove, Move up/down).
- Placement: DiagramEventAddElement::setAlternatives / switchSymbol: Tab
  and Shift+Tab rebuild the pending symbol from the next location, same
  position and rotation, status bar "Symbol i of n". Diagram::event now
  gives Tab to a running tool first (it used to cycle the items at once).
  PartsPlacement::Key carries the symbols; DiagramView::startPartPlacement
  sets them.
- .faspart (PartsArchive, sources/partsdb/partsarchive.*): a QetZip with
  faspart.json {format "faspart", version 1, part (PartsDatabase::toJson:
  every column, technical data, functions, connection points), accessories
  by manufacturer + order number, files, symbols} + files/ + symbols/.
  contentOf / write / read / store (replace keeps the id, its files are
  replaced). PartsSymbolFiles (app side): reads a symbol's definition;
  writes imported symbols to the user collection imported_parts/<maker>/
  (qet_directory "Imported parts"/"Piezas importadas"), reusing an
  identical file, renaming a different one. Parts manager: "Export" menu
  (the selected part / all the parts of the list, one file each, ask
  before replacing) and "Import..." (several files; Replace / Keep mine /
  Replace all / Keep all mine). The table stays single-selection.
- Tests: tst_partsarchive (new, standalone), tst_partsdatabase
  symbolsAreKeptAndVersionFourUpgraded, tst_partsmanager
  symbolsAreChosenAndOrdered, tst_partscollection tabGoesThroughTheSymbols.
  Spanish: step27.py (33 texts).
- Found by the tests: PartsDatabase::parts() lists the columns only, so
  the parts collection now reads each part whole (part(id)) to get its
  symbols. A switched-back symbol was brought into the project again and,
  once (Shift+Tab back to the pilot light), QETProject::importElement
  showed its "already in the project" dialog although the uuids matched
  (cause not found; importing the same symbols again by hand asks
  nothing). DiagramEventAddElement now keeps where each alternative went
  (m_alternative_paths) and builds it from there. The Tab test places
  the repository's symbols (QET_APP_TEST_MAIN_PREPARED sets the common
  collection before the app reads it) in a project registered with
  QETApp.

## Version 0.12 (2026-11-14)
- Brandon: "súbelo a github y prepárame el zip". Pushed to origin fasecad
  (45b96700e..00f258c38): launcher, collection taken into the repository
  (his choice: no submodule, no fork; .gitmodules without elements; the
  old .git/modules/elements stays unused), the code of 12-13 November,
  version 0.12. Before pushing, every test carrying data copied from the
  manufacturer's files (tst_partsdatabase plc(): GTIN, sizes, power loss,
  clearances, terminal data; tst_partsarchive) was given made-up parts
  (ACME...), and the part values were taken out of these notes.
- Tests run before the push: the parts, terminal, naming, index and
  bridge tests, and the four that start fasecad.exe (tst_qetdbfile,
  tst_savevaluerows, tst_xrefdisplay, tst_elementautonumids) with
  QT_QPA_PLATFORM_PLUGIN_PATH=C:/msys64/ucrt64/share/qt6/plugins/platforms
  so the child finds the offscreen plugin: all pass. Full ctest not run.
- FaseCAD was open from build-release: its fasecad.exe was renamed to
  fasecad-en-uso-0.12pre.exe (Windows allows it for a running program)
  and 0.12 linked in its place; delete that copy once FaseCAD is closed.
- Zip FaseCAD-0.12-windows-x64.zip (74.8 MB, 4955 entries) by
  scratchpad package.sh 0.12 + Python zipfile, checked unzipped elsewhere
  with PATH=C:\Windows only (--version 0.12); copied to
  FaseCAD\web\httpdocs.
- GitHub release v0.12 "FaseCAD 0.12 (preview)", pre-release, tag on
  76dd1b99a, the zip attached; notes in English and Spanish since 0.10.1
  (0.11 had no release): scratchpad release-notes-0.12.md.
- Website: construir.py VERSION 0.12, download page with a 0.12 entry
  (three paragraphs + button, translated in textos.py into en de it zh
  tr), 0.11 kept below; site and fasecad-web.zip rebuilt, for Brandon to
  upload with the 0.12 zip.
- fasecad-en-uso-0.12pre.exe deleted once FaseCAD was closed.

## Editing the common collection (2026-11-15, after 0.12)
- Brandon: opening a symbol of the common collection in the element
  editor said "read only" ("¿puedes desbloqueármelos?"). QET's
  ElementsLocation::isWritable() refused every file of the common
  collection, and the panel left out new / edit / delete for it and
  refused drops into it.
- First done as an "Allow editing" setting, off by default (the rule of
  AGENTS.md); Brandon: "estaba pensando en deshabilitar la protección",
  so the protection is gone instead, with no setting: a symbol of the
  common collection is writable when its file (or, for a new one, its
  folder) is, so only a read-only installation stays read only. The
  panel allows new symbol, new folder, edit and delete folder, delete
  symbol and drops there on the same condition. The collection root is
  never deleted.
- From build-release the common collection is the repository's
  elements/ (resolveConfiguredDataPath tries next to the binary, then
  its parent), so edits there can be committed. In the zip it is the
  elements/ beside FaseCAD.exe, which a new version replaces.
- Test: tst_commoncollectionedit (a symbol and a new one are written, a
  read-only file stays read only, drop into a folder).
- Not released: goes out with the next version (0.12.1 or 0.13).

## Cables -W1, -W2..., texts left aligned, wire texts above / left (2026-11-15)
- Brandon: picking the cable tool asked to define a numbering rule ("ya
  se supone que no usamos la enumeración automática original"); the
  cable name grew from the line to the left; wire texts should stand
  left of vertical wires and above horizontal ones. Explained first, as
  he asked; his answers: with a dash (-W1), left aligned by default.
- Why: on 13 November the cables kept their QET numbering rule (the
  formulas of SelectAutonumW / NumerotationContext), so
  QETDiagramEditor::cableNumberingGate asked for one and pointed to the
  removed "Auto Numbering" page; without a rule every cable was "W".
  The cable texts were right aligned by default (QETApp::
  cableTextAlignment, CablePart::textLeft): their end at the start of
  the line.
- Now:
  - CableManager::baseName() "-W", nextDesignation() = DeviceNaming::
    nextLabel over the designations of the project's cables (a pasted
    copy is held by the project already, so each takes its own).
  - The gate is gone; AddCableCommand no longer steps a rule; CableCopy
    does not read one.
  - CableRenumber::plan(project, x_axis_first, include_by_hand): -W1...
    in folio / axis order, skipping the names kept by hand.
    RenumberCablesCommand only writes names.
  - CableNumberingDialog: note, axis (saved when chosen), "Renumber all
    cables", Close. No cable numbering tab in Settings > New project.
  - The rules projects hold stay in the file, unused (saved back as
    read). ProjectAutoNumConfigPage still has a cable part but is not
    shown anywhere since the 13th.
  - QETApp::cableTextAlignment(): left unless "right" or "center" is
    saved.
  - ConductorProperties: AlignTop / AlignLeft by default (constructor
    and fromSettings); fromXml without the attributes keeps AlignBottom
    / AlignRight, so old files look as they did. Existing projects keep
    the defaults saved in them (project properties > Conductor).
- His registry had the old values saved by the settings dialog
  (cable-management/cable-align right, diagrameditor/
  defaultconductorhorizontal-alignment AlignBottom, vertical
  AlignRight): set to left / AlignTop / AlignLeft for him.
- Test: tst_cablenaming (first free number, no window when the tool is
  picked, renumber order and names by hand, default alignments, an old
  file without the attributes). Spanish texts unfinished.
- Not released: next version.

## Cable cores numbered from the left (2026-11-16)
- Brandon (screenshot of -W2 4x1.5): cores 3, 2, 1 from the left, he
  wanted 1, 2, 3. CableManager::crossedConductors() ordered the
  crossings from the point the line was drawn from, and that order
  gives each core its number (CableCreateDialog, qet.addCable): a line
  drawn from the right numbered from the right.
- Now from the left end, or the top end for a line more vertical than
  horizontal, whichever end it was drawn from. The other callers
  (CablePart wireFor / settleCores, CableCopy) pick the nearest wire to
  each mark, so the order does not matter to them. Cables already drawn
  keep their numbers.
- Test: tst_cablenaming coresAreNumberedFromTheLeftWhicheverEndIsDrawnFirst
  (every line across the folios of every_feature.qet, both ways).
- Then (he said yes): the label above the line is lifted clear of the
  core numbers it would cover (CablePart::standClearOfCores, called by
  labelLines() when the cable's text offset is null): 2 units of room,
  repeated while a taller number further along is reached. A label he
  moved by hand ("Position du texte") stays where he put it. A core
  being dragged counts where it was picked up, so the name does not
  jump. Test: theNameStandsClearOfTheFirstCore (through boundingRect).

## Terminals: strip left, number right; inside / outside of the panel (2026-11-16)
- Brandon (screenshot of a row XPERS 23, 24, 26...): the strip name on
  the left of the circle, the terminal number on the right. Then: "una
  opción en los terminales para invertir la lógica ... dentro de cuadro
  y fuera de cuadro para que sea intuitivo". His answers: shown in the
  properties and on the folio while selected, per terminal, terminals
  already placed updated too; the wire texts (his screenshot has them on
  the right) left for later.
- Labels:
  - 11_conexiones/borne.elmt and borne_tierra_pe.elmt: a terminal_number
    field, left aligned, caps centred on the circle (generar.py:
    Sym.terminal_number_at, written after the terminals so their uuids
    stay; the PE one behind its "PE"). The label field was already left
    of the circle, centred.
  - Split labelling ("Split the label and the terminal number") is the
    default of new projects (diagrameditor/defaultterminallabelling true
    when unset; his registry set to true). Projects saved without it
    still read off.
  - Element::labelForDisplay: a terminal showing its number only (the
    next of a row, TerminalStripLabels) leaves the label field empty when
    split with a number field (it drew "24 o 24").
- Inside / outside (TerminalSides, sources/TerminalStrip/terminalsides):
  the outside is the side wired to a cable core, else the lowest
  connection point (the rule TrueRealTerminal::diagramColumn had, which
  now reads it from here); the hidden information "outside_swapped"
  turns it round (the highest other one goes out).
  - Information tab of a terminal: "Swap inside and outside of the
    panel" under "Show name", and a line "Inside the panel: top.
    Outside the panel: bottom[, the side of the cable]." Written only
    when ticked. Same box in the multi-element editor.
  - TerminalElement::updateSideMarks: "Inside" / "Outside" (6 pt, blue)
    beside the ends while selected; a ViewOnlyText paints nothing
    without a widget, so prints, PDF and images never have them.
    Refreshed on selection, move/turn, information and
    TerminalStripLabels::refresh (cables).
- Update elements from the collections: the old definitions are kept
  before the replacement and TerminalFieldsUpdate::adopt() gives each
  placed terminal the label / number fields of the new symbol: a field
  missing in the old one is added (own uuid), one still at the old
  symbol's anchor and alignment moves to the new one, one moved or
  deleted by hand is left alone. The report says how many.
- Test: tst_terminalsides. Spanish texts unfinished.
- Test run of the terminal batch: tst_terminallabeldisplay asserted the
  old default (no split without a setting): changed to the new one, and
  the settings page offers split checked. tst_terminalsides crashed:
  a DynamicElementTextItem without an element reads the element's
  rotation in its constructor (setKeepVisualRotation); the anchor of a
  symbol's field is now measured on an empty UserText copy made on the
  element itself and deleted at once. The FaseCAD he had open had that
  crash in "Update elements from the collections": told him not to use
  it until restarted.

## Contact mirror: position absolute / relative, contact symbols turned (2026-11-16)
- Brandon (screenshot of the coil with its contacts listed under it,
  14/12 left, 11 right, "29.2"): "puedes hacer que se vea abajo como"
  that: it exists, "Mostrar contactos" in Project properties > Cross
  references (Bobina), with terminal numbers; the position format of his
  screenshot is %f.%c. Told him; not made the default yet (waiting for
  his answer: coils only or all, %f.%c).
- Then: the mirror's position chosen in the properties, absolute or
  relative, and an angle for the contact icon. His answers: only the
  symbols turn (numbers upright), 0/90/180/270.
- ElementData::XRefDisplay: absolute (manual is a folio point; xml
  anchor="folio") and angle (xml angle="90|180|270"), written only when
  used. CrossRefItem::autoPos / the Shift-drag measure the manual point
  from the folio when absolute.
- CrossRefItem::drawContact: the symbol lines are drawn through a turn
  around their middle (turned upright a symbol is 24 tall, three rows of
  10, footprint at the left of its rows); terminal numbers drawn upright,
  centred where their spot went; position text right of the footprint.
  Angle 0 draws exactly as before.
- XRefDisplayForm (sources/ui): the fields, shared by the "Relay
  contact display" dialog (rewritten on it, gets the symbol position)
  and the new "Contact mirror" tab (ElementXRefDisplayWidget) of a
  master's properties (not PLC): "Use the project setting" box, live,
  ChangeElementDataCommand; refreshed on XRefPropertiesChanged.
  Switching relative/absolute converts X/Y so the mirror does not move.
- Test: tst_contactmirror. Spanish texts unfinished.
- Then: "Por defecto quiero que esté Mostrar contactos". XRefProperties::
  fromSettings reads "contacts" when the settings say nothing (new
  projects, the settings page); fromXml still reads "cross" for a file
  which says nothing. His registry had cross saved for every type:
  coil, protection, commutator set to contacts (plc and cable left,
  they draw their own way). %f.%c not asked for: not changed. Test:
  tst_contactmirror aNewProjectShowsContacts.

## Deleted terminals leave their strips (2026-11-16)
- Brandon: a terminal deleted from the folio stayed in the strip editor
  and in the strips drawn on the folios. Cause: DeleteQGraphicsItemCommand
  took the symbol off the folio (kept alive for undo) and nobody told its
  strip; only a reload dropped it (TerminalStrip::fromXml warning).
- TerminalStrip::takeOut / putBack, with TerminalStrip::Hole: a physical
  terminal whose every level goes is taken out whole (position kept), one
  keeping levels loses those (levels kept), the bridges they lay in are
  kept as they were; putBack restores all of it in place.
- TakeOutOfStripsCommand (TerminalStrip/UndoCommand), a child of
  DeleteQGraphicsItemCommand (every deletion path, cut too) and of
  RemoveDiagramCommand (a sheet takes its terminals along; its redo/undo
  now run the children). Added only when a terminal of a strip is deleted.
- Bridged and multi-level terminals: the editor still refuses to delete
  them (hasNonDeletableTerminal, unchanged); the command handles them
  anyway, for the scripting paths. No new text. Test: tst_deletedterminals.

## Only the first auxiliary block (2026-11-16)
- Brandon: the properties "Auxiliary block", "Manufacturer of auxiliary..."
  repeated four times; keep one. His choice: the first block only.
- QETInformation::shownAuxiliaryBlocks() (1) and isExtraAuxiliaryInfo():
  the blocks 2 to 4 stay in elementInfoKeys() (project database,
  nomenclatures, old projects) but are offered only where an element has
  a value in them: Information tab (row hidden, still keeps its value),
  several elements selected, the info list of a placed text, the composite
  text dialog, the element editor (info combo, default informations).
  The nomenclature column list is unchanged.
- Parts tab: block_count 2 (main + aux 1), the message says one auxiliary;
  "choose article" from the parts database brings one accessory only.

## Text source "Part data" (2026-11-16)
- Brandon: a source of text enabled only when the element has a part,
  showing data of the article from the parts database, chosen in a large
  search window instead of a list. His choice: every part of the element
  is listed, the text remembers which.
- DynamicElementTextItem::PartInfo, partField (PartTexts::reference():
  manufacturer TAB order number TAB id; id = a field such as "voltage" or
  "data:<technical data name>") and partValue (the value last read).
  Saved as text_from="UserText" with <part_info reference=".."/> and the
  value in <text>: QElectroTech shows the same text.
- sources/partsdb/parttexts.*: names, values, fieldsOf(part/element),
  refresh(elements) (opens the database only when a text needs it);
  called at the end of Diagram::fromXml (load, lazy build, paste) and on
  PartsDatabaseChanges::changed (parts manager) for built folios.
- sources/partsdb/partfielddialog.*: search bar (every word, any column),
  table Data / Value / Part, double click or Enter.
- Text properties: row "Part data" (rows after it shifted by one), source
  "Part data" disabled without parts, the window run from the delegate
  like the font one, undo with partField / partValue / textFrom.
- Test tst_parttexts. Spanish texts unfinished ("Datos del artículo").

## Cross references: contacts at the folio foot by default (2026-11-16)
- XRefProperties::fromSettings: snapto defaults to bottom (displayhas was
  contacts already); fromXml still reads label for a file without it. His
  registry: coil, protection, commutator snapto = bottom. Test:
  tst_contactmirror aNewProjectShowsContacts.

## Side panels which hide by themselves (2026-11-16)
- Brandon: an option to hide the side panels; his choice: automatic hide
  (setting + a pin on each panel).
- sources/ui/dockautohide.*: Settings > General "Hide the side panels
  automatically" (diagrameditor/autohide_docks, off). A left/right panel
  not pinned folds into a vertical tab (a QToolBar at the window edge);
  resting on the tab (350 ms) or clicking it slides the panel out over the
  drawing (its widget moved into an overlay frame, given back on fold);
  it folds when the mouse leaves for 600 ms (not while a button is held,
  a popup or dialog is open, or the keyboard is in it) or when something
  else of the window gets the focus. Pin in the overlay keeps it open;
  while the setting is on, the docked panels have a title bar with a pin
  (folds it again) and a close button. A folded panel is a hidden dock:
  saveState keeps it, the setting off shows it again, tabbed as before.
  Pinned and folded panels in diagrameditor/autohide_pinned / _folded.
  The overlay can be widened by its inner edge.
- The older DockFold bars (fold one side away) are left as they are.
- Test tst_dockautohide.

## Version 0.13 (2026-11-17)
- Brandon: "Prepárame el software para subirlo a la web y lánzalo también a
  github". New features since 0.12, so 0.13 (not 0.12.1).
- Commits: e86983b7b (the code of 15 to 17 November: collection editable,
  cables -W1..., terminal sides and numbers, deleted terminals leave their
  strip, contact mirror tab and folio foot, first auxiliary block only,
  part data texts, side panels which hide by themselves), 4f4b8b2e7
  (version 0.13), and this one. The two symbols Brandon edited himself in
  the element editor (contacto_conmutado, contactor_3p) went in as he left
  them: generar.py would draw them again as before.
- Checked before pushing: no EPLAN, no manufacturer data (a real order
  number in a comment of parttexts.h replaced by K-24). Tests: the new
  ones (tst_deletedterminals, tst_parttexts, tst_contactmirror,
  tst_dockautohide), the related ones (duplicatelabels, multielementinfo,
  terminallabeldisplay, bridgetool, terminalstriporder, elementparts,
  terminaldiagram, faseterminal, terminaltexts, terminalsides,
  partsdatabase, projectsettingrows, xrefdisplay, terminalstriprows,
  xrefpos, textfixedsizeintegration) and the four starting fasecad.exe
  (tst_qetdbfile 609 s, tst_savevaluerows, tst_xrefdisplay,
  tst_elementautonumids 268 s) with QT_QPA_PLATFORM_PLUGIN_PATH: all pass.
  tst_textfixedsizeintegration times out under ctest (300 s): its runner
  links a probe from the app objects first; run by hand, and its probe
  with the plugin path, it passes. Full ctest not run.
- Zip FaseCAD-0.13-windows-x64.zip (78,492,311 bytes, 4955 entries, the
  same files as 0.12) by scratchpad package.sh 0.13 + Python zipfile;
  unzipped elsewhere, PATH=C:\Windows only: "FaseCAD 0.13". Copied to
  web/httpdocs/downloads.
- Website: construir.py VERSION 0.13, download page with a 0.13 entry
  (three paragraphs + button, in textos.py for en de it zh tr), 0.12 kept
  below; llms.txt download line; site and fasecad-web.zip rebuilt.
- GitHub release v0.13 "FaseCAD 0.13 (preview)", pre-release, tag on
  48058a874, the zip attached; notes in English and Spanish: scratchpad
  release-notes-0.13.md. For Brandon to upload: web/fasecad-web.zip and
  httpdocs/downloads/FaseCAD-0.13-windows-x64.zip.

## Bridged and multi-level terminals can be deleted (2026-11-17, after 0.13)
- Brandon: in his project the terminals could not be deleted; he wants
  to delete them without taking them out of the strip editor first.
  Cause: DeleteQGraphicsItemCommand::hasNonDeletableTerminal() refused a
  bridged terminal (a drawn bridge too) or a level of a multi-level one.
- The check is gone (editor delete, devices panel, scripting). The
  deletion already takes them out of the strip with their level and
  bridge (TakeOutOfStripsCommand); now it also takes the deleted symbols
  off the landings of the drawn bridges (letGoOfBridges():
  ChangeBridgeTerminalsCommand children, tension kept). The drawn line
  stays where it is. Test: tst_deletedterminals
  aDrawnBridgeLetsGoOfADeletedTerminal.

## A device's DXF on a cabinet sheet from the Devices panel (2026-11-18)
Brandon's request (cabinet phase 5, first part): right click on a device
of the Devices panel -> "Insert the DXF on a cabinet sheet"; the drawing
shows the device label in its middle; a placed device gets a green tick.
His answers: no DXF in the parts -> ask for a DXF file; no cabinet sheet ->
offer to add one at the end; one commit with the 2026-11-17 change.
- CabinetItem::Properties `device` (uuid of the symbol) + `device_label`,
  XML attributes `device` / `device_label` on a dxf_drawing, written only
  when set (old files unchanged). The label is painted in the middle on a
  white box, bold blue outlines, ~1/5 of the drawing's height, shrunk to
  90 % of its width. followDevice(): on dataBaseUpdated the drawing reads
  its symbol's label from element_info (newQuery runs the statement at
  once: the uuid is written in it) and takes it, no undo (derived, like
  cross references); symbol gone = label kept. description() starts with
  the label (readout while placing).
- New sources/cabinet/cabinetdevices.*: cabinetFolios(), placed() (built
  folios: their CabinetItems, the tool's preview left out; folios not
  built: cabinet_item of their deferredXml(), nothing built),
  drawingFromParts() (element info "parts" -> first part with a DXF in
  the parts database, linked files read from disk).
- CabinetDialogs: askDxfDrawing() / setDrawing() / skippedText() moved
  out of QETDiagramEditor::askDxfDrawing() (now a wrapper), placedIcon()
  (green tick; blank variant keeps the labels aligned when the project
  has a cabinet sheet).
- DevicesPanel: context menu entry (sub-menu per cabinet sheet when there
  are several), insertDrawing(): builds the folios of the line's symbols,
  the drawing stands for the first symbol with parts, then
  DiagramEventAddCabinetItem on the cabinet folio (follows the mouse, one
  click, one undo step). Tick + tooltip "Drawn on the cabinet sheet(s) N"
  on the line (by symbol uuid, or by label for a drawing whose symbol is
  gone) and on the child symbol. Refresh also on the undo stack's
  indexChanged. Whole panel translated to Spanish (it had no texts in
  qet_es.ts), unfinished.
- Tests: tst_cabinetfolio deviceDrawingIsSaved; tst_devicespanel
  drawingsOnCabinetSheetsAreFound (saved, reopened, folio not built
  stays so), drawingComesFromTheParts. Passed (build-release, offscreen
  with QT_PLUGIN_PATH=MSYS2 plugins): tst_cabinetfolio 18,
  tst_devicespanel 6, tst_cabinetcommands, tst_dxfreader,
  tst_deletedterminals.
- Tried in the real program on a copy of ejemplo-s7-1200 (scratchpad
  settings, copy of his parts database): -K1 -> its PLC DXF placed on
  folio 5 with "-K1" in the middle, tick on -K1 and on its part-1 symbol.
- Then Brandon (same day, his option "A"): one line per device. -K1 drawn
  as 4 symbols (folios 1-4) is one line, no arrow, no children; new
  column "Article" (order numbers of all its symbols' parts, each once;
  tooltip "manufacturer number × units"); Sheet "1-E11" or "1, 2, 3, 4";
  description tooltip (cut in a narrow panel). The line stands for the
  first symbol made of parts (double click, DXF link), else the first;
  delete and insert take all its symbols (symbols_role). Symbols without
  a label are no device: they keep one "(No label) (n)" line opening on
  them (the only case with arrows). Search also finds the article and
  any symbol's description. Parts read with
  projectDataBase::elementInformation() (cache, no folio built).
  DevicesPanel::lines() + Device::parts; test oneLinePerDevice.
- build-release rebuilt everything (~25 min) at each build: .ninja_deps
  had a bad record ("ninja: warning: premature end of file;
  recovering"), and every load dropped the records written after it.
  Fixed with `ninja -t recompact` + one full build; now incremental
  (devicespanel.cpp + link: 53 s). If the warning comes back, recompact.
- Then (his "OK a todo" to 10 points): several devices at once and
  "Number...".
  - Insert DXF on several selected lines: a queue (DevicesPanel::m_queue,
    list order, lines with the tick left out, placed_role). Each one is
    a DiagramEventAddCabinetItem on the cabinet folio; its finish is
    connected BEFORE Diagram::setEventInterface (whose own lambda
    deletes the tool), reads tool->ending() and starts the next with
    QTimer::singleShot(0). New in the tool: Ending {Placed, Skipped
    (right click), Cancelled (Esc)} and setHint() for the status bar
    ("-K1 (1 of 3): click where it goes | Right click to skip it | Esc to
    stop"). No DXF in the parts -> file asked when its turn comes; no
    file = skipped.
  - "Number..." (tool button beside the search field + context menu):
    first label + step, preview old -> new, devices ordered by where
    their main symbol stands (positionLessThan: sheet, column, row),
    numberedLabels() keeps zeros, the last number only (-X0:1 -> -X0:2),
    letters after it stay, adds 1 if none. A new label used by a device
    left out -> warning, nothing renamed. Every symbol of a device gets
    label = new, formula = "" (ChangeElementInformationCommand map, one
    undo step, "Number n device(s)"); contacts and the cabinet DXF label
    follow (updateProjectDB). Selection: selectedLines() (the "(No
    label)" line stands for each of its symbols).
  - Tests: tst_devicespanel labelsAreNumbered,
    devicesAreOrderedByPosition (9 passed); tst_cabinetfolio 18,
    tst_cabinetcommands 6 passed. Not tried by hand in the real window:
    Brandon was at the PC (the demo was closed under my input, which
    went to his WhatsApp; nothing sent). He tries it himself.

## Version 0.14 (2026-11-18)
- Brandon (after trying the queue and "Number..." in the demo): "Va de
  lujo, lánzalo ya en GitHub y prepárame el Zip para subirlo a la web".
- Commits: 2222441f2 (the work of 17 and 18 November: deletable bridged
  and multi-level terminals, device DXFs on cabinet sheets one by one or
  in a queue, label in the middle, green tick, one line per device with
  its article, Number...), 32c52590d (version 0.14), and this one.
- Checked before pushing: no other CAE software named, no manufacturer
  data (a real order number in tst_devicespanel and in a comment of
  devicespanel.cpp replaced by ACME PLC-24DC; the product name out of these
  notes). Tests: tst_devicespanel 9, tst_cabinetfolio 18,
  tst_cabinetcommands 6, tst_dxfreader, tst_deletedterminals,
  tst_partsbom, tst_elementparts, and the four starting fasecad.exe
  (tst_qetdbfile, tst_savevaluerows, tst_xrefdisplay,
  tst_elementautonumids 313 s, tst_qetdbfile 626 s) with
  QT_QPA_PLATFORM_PLUGIN_PATH: all pass. Full ctest
  not run.
- FaseCAD (the demo, a hard link fasecad-demo.exe) was running: the old
  build-release\fasecad.exe was renamed fasecad-en-uso-0.14pre.exe and
  0.14 linked in its place; delete both once the demo is closed.
- Zip FaseCAD-0.14-windows-x64.zip (78,540,830 bytes, 4955 entries, the
  same files as 0.13) by scratchpad package.sh 0.14 + Python zipfile;
  unzipped elsewhere, PATH=C:\Windows only: "FaseCAD 0.14". Copied to
  web/httpdocs/downloads.
- Website: construir.py VERSION 0.14, download page with a 0.14 entry
  (three paragraphs + button, in textos.py for en de it zh tr), 0.13 kept
  below; llms.txt download line; site and fasecad-web.zip rebuilt.
- GitHub release v0.14 "FaseCAD 0.14 (preview)", pre-release, notes in
  English and Spanish (scratchpad release-notes-0.14.md: the panel is
  named *Devices* there, its title is not translated). For Brandon to
  upload: web/fasecad-web.zip and
  httpdocs/downloads/FaseCAD-0.14-windows-x64.zip.

## Next part: routing, wire numbering, sections, title blocks, 3D (2026-11-19)
- Brandon asked for: a cabinet routing sheet (shortest way through the
  ducts between the DXF centres of a mounting plate, "Calculate routing"
  from the schematic, editable, length shown on click, export), wire
  numbering with examples + own, wire sections, better title blocks, 3D
  with STEP. Plan agreed: order routing -> numbering -> sections ->
  title blocks -> 3D; all six numbering examples (potential, sheet +
  number, sheet + column, from/to IEC 62491, potential type, own
  formula); sections: only "section from the protection" (IEC 60204-1
  table 6); export as CSV, not xlsx; title blocks: a new way of making
  them, "be creative"; a terminal goes to the centre of its strip DXF.
  3D: QOpenGLWidget boxes first, STEP through OpenCASCADE only after
  asking him to install the MSYS2 package.
- Also asked (point 7): Tab changes the anchor point while placing a
  plate, rail, duct or DXF. DONE: DiagramEventAddCabinetItem::Anchor
  (middle left by default, then top left, top middle, top right,
  middle right, bottom right, bottom middle, bottom left, centre;
  Shift+Tab back). Plate/DXF hang from the mouse by it (the snap
  catches whatever corner or middle). Duct/rail: left/right are its
  ends and top/bottom its sides whichever way it runs (so the default
  is the old behaviour); in the middle of its length it grows both
  ways. The yellow box shows a 3x3 glyph with the anchor in red; the
  status bar says "Tab: anchor point (...)". The DXF queue of the
  Devices panel keeps the anchor from one drawing to the next.
  Tests: tst_cabinetfolio tabChangesTheAnchor, ductRunsFromItsAnchor
  (toolPutsAPlateDown updated to the new default). Spanish unfinished.

## Routing sheet of a mounting plate (2026-11-19)
- Diagram::FolioKind::Routing ("routing"), attributes routingSheet
  (cabinet folio uuid) + routingPlate (plate CabinetItem uuid) +
  cabinetScale; written for a routing sheet only. drawingScale() counts
  it like a cabinet. QETProject::addNewRoutingDiagram(pos, cabinet,
  plate): title "Cabinet routing", the cabinet's scale.
- sources/cabinet/cabinetrouter.*: the way through the ducts, in mm.
  Ducts = their middle lines; two ducts meet where their rects (+2 mm)
  touch, at their nearest points (T, corner, crossing, in line, side by
  side). A device enters each duct square from its centre (Manhattan).
  Dijkstra; out of the ducts a mm costs double (so the way inside the
  ducts wins a tie), the length given is the real one. No way: a
  straight line, through_ducts false. Test tst_cabinetrouter (plain).
- sources/cabinet/cabinetrouting.*: Route (key = sorted "uuid/terminal"
  ends, number, from/to + terminals, section, colour, cable, points in
  mm from the plate's top left, manual, through_ducts). calculate():
  WiringListExport::wires() (now public, with the wire number "num";
  folio reports merged) -> each end label to the drawing with that
  device_label on the plate, or the part before ":" (a terminal ->
  its strip). Ways changed by hand kept unless the user says no.
  RoutingBackdrop (not saved, z -1000): paints the plate and the parts
  on it by calling the cabinet sheet's CabinetItem::paint() at the
  routing scale, refreshed on the cabinet scene's changed(); builds the
  cabinet folio lazily. CableRouteItem (type UserType+1022): blue,
  orange by hand, red straight; selected: corners to drag, an info box
  (wire, from -> to, length, section, state); double click adds a
  corner on a segment or takes one away. SetCableRoutesCommand (a
  calculation), ChangeCableRouteCommand (one way). Saved as
  <cable_routes><cable_route key number from from_terminal to
  to_terminal section color cable points="x,y ..." manual straight/>.
  CSV: UTF-8 with BOM, ";": Wire, From, Terminal, To, Terminal,
  Section, Colour, Cable, Length (mm), Way.
- Editor: in the "Cabinet construction 2D" menu: Routing sheet of a
  mounting plate... (cabinet sheet), Calculate the routing, Export the
  routing to CSV... (routing sheet); the scale works on a routing sheet
  too. Projects panel: "Insert a routing sheet for this cabinet" on a
  cabinet sheet. Icon CabinetDialogs::routingIcon() (panel, tabs).
- Test tst_cabinetrouting (app): -K1/-K2 wire 12 -> 125 + 300 + 125 =
  550 mm, undo, info text, CSV, kept by hand, saved and reopened.
  Spanish (step49.py) unfinished.
- Not done yet: wires through junction symbols (an end without a
  drawing: left out and counted), duct fill, a reserve per end.
- Then Brandon (screenshot: devices on a rail between a top and a
  bottom duct, ways in the bottom one): drag the wire up into the top
  duct to choose its duct. DONE: a press on a stretch (selected or not)
  drags it across (horizontal up/down, vertical sideways), its
  neighbours stay square; near the middle of a duct running the same
  way (12 px on screen) it catches on it; a stretch from a device gets
  a corner so the device end stays. Cursor SizeVer/SizeHor on a
  stretch, SizeAll on a corner of a selected way. Simplified on
  release, manual, one undo step. CabinetRouting::ductsOf() (mm).
  Router: entries now square into horizontal ducts above/below first,
  vertical beside only without one, nearest end only without either;
  never across another device; simplified() keeps a turn back. Tests:
  tst_cabinetrouter aDeviceGoesUpOrDownFirst, tst_cabinetrouting
  aStretchIsDraggedOntoAnotherDuct (real scene mouse events).
- Then: right click on a way -> "Reset the position" (every selected way
  with it; CabinetRouting::reset(): calculated again between the two
  drawings, manual false; one undo step "Reset the position of n
  wire(s)"). A way moved by hand keeps its colour (blue; red only for a
  calculated way no duct joins); "manual" still kept on recalculation
  and in the CSV. Test: aStretchIsDraggedOntoAnotherDuct resets too.

## Wire numbering at once (2026-11-19)
- sources/wirenumbering.*: WireNumbering::Scheme {name, formula,
  per_wire}; examples(): %N (by potential 1, 2, 3), %F.%n (3.1),
  %F%2n (301), %F-%C (sheet-column), %S:%T/%D:%E per wire (from/to, IEC
  62491), %P (potential type: L1, N, PE, P1, M1; 24 V -> P, 0 V -> M, N
  and PE alone without a number). Own formulas in the settings
  (wirenumbering/own). Variables %N %n %F %C %R %P %S %T %D %E %%, a
  width between % and the letter (%3N -> 001).
- plan(Options{formula, per_wire, scope project/sheet/selection, order
  rows/columns first, start, keep_fixed}) -> Change list; one name per
  potential (relatedPotentialConductors) unless per wire; a name never
  shared by two potentials (".2" added); labels frozen by hand kept.
  command(): one undo step "Number n wire(s)" through the conductors'
  "properties".
- WireNumberingDialog (Numbering menu "Number the wires...", ribbon
  Connections > Wires > Number): scheme, formula, per wire, scope,
  order, start, keep fixed, preview table, Save as mine / Delete.
- Test tst_wirenumbering (app). Spanish (step50.py) unfinished.

## Sections from the protections (2026-11-19)
- Brandon chose only "section from the protection". sources/
  wiresections.*: IEC 60204-1 table 6 (copper, PVC, 40 C), methods B1,
  B2, C, E; D.1 temperature factors 30-60 C, D.2 grouping factors (1, 2,
  4, 6, 9 circuits; the first row at or over the number); the same data
  as web/fuente/datos_cables.py. minimumSection(In, conditions): the
  smallest section with Iz >= In, 0 when over 120 mm2.
- A protection = master symbol of kind "protection". Its In: the
  information "Rated current" (16 A, C16, In = 10 A), else the current
  of its first part in the parts database. Its wires: those on its even
  terminals (2, 4, 6, 2/T1), else those at the bottom of the symbol,
  and every wire on the same potential. check() -> a Row per protection
  (In, required, wires, without a section, smaller); apply() -> one undo
  step "Give n wire(s) the section of their protection" (only wires
  without one or with a smaller one; a bigger section is left).
- WireSectionsDialog (Project menu "Sections from the protections...",
  ribbon Connections > Wires > Sections): installation method, ambient
  and grouped circuits (kept in the settings wiresections/*), table,
  "Give them their section".
- Test tst_wiresections (app): table values and factors, reading
  "1,5 mm2"/"C16", a breaker -Q1 C16 giving 2.5 mm2 to its outgoing wire
  only, undo. Spanish (step51.py) unfinished.
- Ideas not done: the section from the load (motor kW/cos phi), voltage
  drop over the routed length, PE section from the phase (table 1 of
  IEC 60364-5-54), a warning when a wire is smaller than its protection.

## Title blocks made of blocks (2026-11-19)
- Brandon: "the original way of making title blocks I do not like at
  all, be creative". He chose "by blocks" and, besides logo, company,
  project and sheet, the blocks revisions, signatures, client and site,
  format and scale.
- Cells (titleblockcell.*, titleblocktemplate.cpp): fill (attribute
  fill, #AARRGGBB), borders (attribute borders, letters l t r b or
  "none", absent = all, drawn as drawRect() did when all), stacked
  (attribute stacked="true": the label small at the top left, ink at
  alpha 150 unless label-color, the value below, shrunk to fit, never
  cut). QElectroTech ignores the attributes: a classic grid,
  "label: value". DXF export still draws "label: value" and every side.
- sources/titleblock/titleblockdesign.*: Design {name, style
  modern/classic, height px, accent, accent_bar, revision_rows 2/3/5,
  blocks {kind, width %, texts}, logo bytes + type}. templateXml():
  twelve rows, the columns of the blocks in t% (the last one r100% for
  the 4 px accent bar and the rounding), a field per datum. The design
  rides along as <fasecad-design> in the template (kept by the
  project's collection, which stores the XML as it is); the logo is the
  template's logo "logo" (base64, svg or png). completeDesign() (A3:
  logo 7, company 13, project 18, client 12, revisions 20, signatures
  13, format 8, sheet 9), compactDesign() (A4).
- Data: folio %{projecttitle} %{title} %{folio} %{author} %{date}
  %{plant} %{locmach}; project properties company, company-address,
  company-contact, client, site, site-address, checked-by,
  checked-date, approved-by, approved-date, standard and
  revision<n>-index/-date/-description/-author (1 = the latest, the
  table shows the latest at the top); worked out for each folio:
  %{paper} (BorderTitleBlock: the smallest A size holding the folio, 4
  px slack, else "w x h mm"; also on a border change) and %{scale}
  (QETProject::updateDiagramsFolioData: "1:10" on a cabinet or routing
  folio, "Not to scale" otherwise, unless the project sets "scale";
  Diagram::setCabinetScale/setFolioKind emit needFolioData). paper and
  scale stay out of the folio's Custom tab.
- TitleBlockDesigner (Project menu "Title block designer...", ribbon
  File > Properties > Title block): add-block buttons (one block of
  each kind), the preview with the folio's data (click picks, drag the
  line between two blocks shares their width, drag a block moves it,
  Delete / Ctrl+arrows), the picked block's width and texts with a
  "Data" menu of variables, revisions shown, logo image; name, style,
  height in mm, accent colour and bar, start again from complete/
  compact. OK: the template into the project (setTemplateXmlDescription,
  not undoable, as the old editor) and the folio or every folio given
  it in one undo step "Use the title block %1" (UseTitleBlockCommand,
  without ChangeTitleBlockCommand's showMe()). Opened on a folio whose
  template carries a design, it opens that design.
- TitleBlockDataDialog (Project menu "Title block data...", ribbon
  "Title block data"): company, client and site, signatures, standard,
  revisions table (New revision: next index A->B, 3->4, Z->AA, today).
  One undo step "Change the title block data" (the project properties
  had none before).
- Test tst_titleblockdesign (app). Spanish (step52.py) unfinished.
- Not done: the old template editor does not show fill/borders/
  stacked (it keeps them); a vertical title block (folio's title block
  on the right) is the same strip turned.

## Cabinet in 3D with STEP (2026-11-20)
- Brandon allowed installing OpenCASCADE: pacman -S
  mingw-w64-ucrt-x86_64-opencascade (7.9.3, 98 new packages, none
  upgraded). Its headers need _USE_MATH_DEFINES.
- CMake: Qt6 OpenGL + OpenGLWidgets and the OpenCASCADE toolkits
  (TKDESTEP TKXCAF TKLCAF TKCAF TKCDF TKDE TKXSBase TKMesh TKPrim
  TKTopAlgo TKShHealing TKGeomAlgo TKBRep TKGeomBase TKG3d TKG2d TKMath
  TKernel TKVCAF, as import library files) linked INTERFACE to
  qelectrotech_objects, the OpenCASCADE include folder and
  QET_HAS_OPENCASCADE given to cabinetstep.cpp only: no full rebuild,
  only the exe and the tests relink. Without OpenCASCADE: boxes, no STEP.
- sources/cabinet/cabinet3d.*: Solid {kind plate/rail/duct/device, name,
  item uuid, min/max mm (x right, y up, z out of the plate), colour,
  part, step bytes, step_name}. solidsOf(cabinet folio): plate 2 mm
  behind z=0, rail its profile's depth, duct its depth or 60 mm, device
  on the rails under it, as deep as its part (parts database depth) or
  60 mm; the part found from the symbol's "parts" information (or a
  symbol with the same label), its first STEP file.
- sources/cabinet/cabinetstep.*: read() (STEPCAFControl_Reader from
  memory, XCAFPrs_DocumentExplorer leaf nodes with their colours and
  their faces' own colours, BRepMesh, triangles; kept by SHA-1),
  orientation() (the 24 turns of a box, the best fit of width and
  height, then depth at 0.3), placement() (centred on the drawing, its
  back on what it stands on), write() (XCAF assembly "Cabinet": boxes
  coloured, devices' models placed, named "-K1 (manufacturer number)").
  OpenCASCADE's console printers removed.
- sources/cabinet/cabinet3dview.*: Cabinet3dView (QOpenGLWidget, one
  shader, opaque / see-through ducts / edges / selection buffers,
  labels of the devices painted over with QPainter; left turn, right or
  middle move, wheel nearer, click picks, double click fits) and
  Cabinet3dWindow (Show everything, See-through ducts, Export as STEP,
  what the picked part is). Cabinet menu "Cabinet in 3D..." and ribbon
  "Cabinet in 3D" (diagrameditor.cabinet_3d), on a cabinet folio.
- Test tst_cabinet3d (app): solids on the plate, a model turned to its
  drawing, the cabinet written as STEP and read back (also a device's
  own STEP), the window. QET_CABINET3D_PNG=<png> (QT_QPA_PLATFORM not
  offscreen, run from tests/qttest) draws a cabinet without showing it.
  Spanish (step53.py) unfinished.
- Release: TKDESTEP imports TKXCAF -> TKVCAF -> TKV3d -> TKService,
  which imports ffmpeg (avcodec, x265, aom...), freeimage and openvr:
  with ldd the zip takes about 45 MB of OpenCASCADE and 80 MB of codecs
  more. To slim it, OpenCASCADE built without ffmpeg/freeimage
  (USE_FFMPEG=OFF). Start-up: about 30 ms more.
- Not done: turning a model by hand when the automatic turn is wrong
  (front/back of a model whose sizes match both ways), the inside of a
  duct, the doors and walls of the enclosure, wires in 3D.

## 3D: colours in the STEP export, enclosure and navigator (2026-11-20)
- Brandon: the STEP exported was all grey in his viewer. It was the
  export: a device's model was read without its colours
  (STEPControl_Reader). Now addModel(): the model read with
  STEPCAFControl_Reader into its own XCAF document and copied with
  XCAFDoc_Editor::Extract (parts, names, colours) under an assembly
  placed as a component; components named too. Test: the plate's colour
  and the one of -K1's own model read back, COLOUR_RGB >= 4.
- Enclosure (his choice: parts database, STEP from the disk, or by hand:
  height, width, depth -> hollow box of 5 mm sheet, open at the front;
  plate height, width -> 3 mm plate against the back). Kept as JSON in
  the folio's plate (CabinetItem::Properties::enclosure, attribute
  "enclosure" of a plate; one undo step each change, through
  ChangeCabinetItemCommand): source, part or path or measures, plate
  (the node id used as the plate), quarter_turns, hidden ("node:<id>",
  "item:<uuid>"). sources/cabinet/cabinetenclosure.*.
- "Use as the mounting plate" (right click in the navigator or the
  view): the thin axis of that node, its front the side towards the
  middle of the enclosure, its top y (z when the plate faces along y),
  quarter turns by the menu; its front centre on the front centre of
  the folio's plate, the folio's plate not drawn. Offers to give the
  folio's plate the size of the one taken (same undo step). Not taken
  yet: the enclosure stands beside the plate.
- His enclosure from a manufacturer (17 MB, 35 nodes, y up): its plate
  499 x 1696, its door at the front. Read 20 s;
  meshing now parallel, deflection diagonal/300 in 0.05..2 mm: 45 s,
  378k triangles (was 86 s, 604k). Kept on disk
  (CabinetStep::cacheFolder(), CacheLocation/step/<sha1>.mesh,
  QDataStream "FSTP" v1), read in the background with a progress bar;
  the view draws boxes until a model is read (CabinetStep::cached()).
- CabinetStep::Model has nodes (name, id = XCAFPrs id, parent, depth,
  assembly, vertex range, box). Navigator (QTreeWidget beside the view):
  plate, ducts, rails, devices, enclosure tree; eye column hides/shows a
  row or a group (kept in the plate, undo "Hide %1"); a row picks in 3D
  and a pick in 3D picks the row. Picking the enclosure by its
  triangles (Moller-Trumbore), not its boxes.
- Export: what is shown, the enclosure placed (its STEP copied with its
  colours; a box by measures as boxes).
- Tests tst_cabinet3d (9 + 2 to look at): QET_ENCLOSURE_STEP,
  QET_ENCLOSURE_PLATE, QET_ENCLOSURE_HIDE, QET_CABINET3D_PNG draw a real
  enclosure without showing it (run from tests/qttest, not offscreen).
  Manufacturer files are read from where they are, never copied.

## Rails and ducts as they are, in 3D (2026-11-20)
- Brandon: the DIN rail and the duct more real. Cabinet3d::piecesOf():
  a rail is its TH35 profile of 1 mm sheet (bottom 27 mm wide, sides,
  lips at the top, scaled to its width and depth), zinc plated, with
  long holes 5.2 x 15 mm every 25 mm in its bottom; a duct (RAL 7030)
  its bottom, a band at the foot of each wall, fingers of 6 mm and slots
  of 6 mm, and its cover. The view draws the pieces; the STEP export
  writes each rail and duct as a compound of their boxes (with covers).
- His choice: a "Duct covers" check box (on by default) instead of the
  see-through ducts. Test tst_cabinet3d railsAndDuctsAreMadeOfTheirPieces.

## 3D: the colours of a manufacturer's model (2026-11-20)
- Brandon: the model of a PLC from its manufacturer drawn orange. Its
  STEP gives the whole box RAL 1037 sun yellow and each of its 996
  faces its own colour (titanium grey, anthracite, white, petrol...).
  Two faults: the face colours were bound in the order of the style
  map, so the solid's colour could come after and win: now solids,
  shells... first, faces last. And OpenCASCADE keeps linear RGB while
  the file and the screen are sRGB: read with
  Values(..., Quantity_TOC_sRGB), boxes written with Quantity_TOC_sRGB
  (the STEP writer then writes the sRGB values: checked, 0.5 stays 0.5;
  before, the plate and ducts came out too light in other viewers).
  Disk cache version 2, so models read before are read again.
- tst_cabinet3d aModelKeepsTheColoursOfItsFaces (QET_DEVICE_STEP=<stp>,
  local only): the colours of a model, most used first.

## Devices off the plate, placed in 3D (2026-11-20)
- Brandon: beacons, push buttons, switch disconnectors are put in the
  cabinet off the plate; a navigator group "Unplaced 3D" for them. His
  choice: the devices of the schematic whose part has a STEP model and
  that have no drawing on any cabinet folio (Cabinet3d::unplacedOf():
  DevicesPanel::lines(devices()) by label, CabinetDevices::placed(),
  the first part of its symbols with a STEP file), and placing them as
  proposed.
- Right click (or double click) "Place in 3D", then a click on a face
  (Cabinet3dView::faceAt(): the enclosure by its triangles, the folio's
  parts by their boxes, the face looking towards the one looking): the
  device stands there, its back on the face (mountedPlacement(): the
  model's side out (+z by default, "Another side out" cycles the six),
  its top the face's top (faceUp(): up, or towards the back on a roof),
  quarter turns about the face, "Sink" mm into it). Dragging it moves it
  on its face (plane of the face, one undo step on release).
- Kept in the plate's enclosure JSON, "mounted": symbol, label,
  manufacturer, order_number, node (the enclosure part it stands on, in
  the axes of the enclosure's model: it goes with it, is hidden with
  it), at, normal, up, quarter_turns, side, sink. Each a Solid with
  mounted = its index (its STEP from the parts database). Navigator: under
  the part it stands on, else under Devices; "Take it out of 3D" puts it
  back in Unplaced. Exported as STEP where it stands.
- Tests: aDeviceStandsOnItsFace (door, sink, turn, side, a side wall, the
  roof, JSON), aDeviceOffThePlateIsPlacedIn3D (parts database with a
  STEP, -S1 unplaced, placed on the plate, sunk, undo, exported, taken
  out). Spanish (step56.py) unfinished.

## Version 0.15 (2026-11-20)
- Brandon: "Vale preparame el commit y el zip para subirlo a la web".
  New features since 0.14, so 0.15.
- Commits: 0498d6584 (the work of 19 and 20 November: routing sheet,
  anchor point by Tab, wire numbering, sections from the protections,
  title blocks of blocks, the cabinet in 3D with STEP, its enclosure,
  navigator and devices off the plate), 14104d45e (version 0.15), and
  this one.
- Checked before: no other CAE software named, no manufacturer data (the
  product names and order numbers of his enclosure and PLC replaced in
  these notes, a comment of cabinetstep.cpp and tst_cabinet3d). Tests:
  tst_cabinet3d, tst_titleblockdesign, tst_wiresections,
  tst_wirenumbering, tst_cabinetrouting, tst_cabinetrouter,
  tst_cabinetfolio, tst_cabinetcommands, tst_devicespanel,
  tst_titleblockbulkformat, tst_exampletemplates, tst_titleblockcache,
  tst_borderpropertiespaper, tst_reportfoliotitle, tst_diagramcontext,
  tst_folioindex, tst_wiringlistexport, tst_dockautohide, tst_parttexts,
  tst_deletedterminals and the four starting fasecad.exe
  (tst_qetdbfile 566 s, tst_elementautonumids 274 s, tst_savevaluerows,
  tst_xrefdisplay): 24/24 pass. Full ctest not run; tst_lazyfolios is the
  known Windows failure.
- Zip FaseCAD-0.15-windows-x64.zip (146,802,037 bytes, 5068 entries):
  68 MB more than 0.14, the 23 OpenCASCADE toolkits and what they import
  (ffmpeg and its codecs, freeimage, openvr, tbb). Unzipped elsewhere,
  PATH=C:\Windows only: "FaseCAD 0.15 (QElectroTech 0.200.1-dev)".
  Copied to web/httpdocs/downloads.
- Website: construir.py VERSION and DESCARGA 0.15, download page with a
  0.15 entry (three paragraphs + button, in textos.py for en de it zh
  tr), 0.14 kept below with its own link; llms.txt download line; site
  and fasecad-web.zip rebuilt.
- Not pushed and no GitHub release: he asked for the commit and the zip.
  For him to upload: web/fasecad-web.zip and
  httpdocs/downloads/FaseCAD-0.15-windows-x64.zip.
- Next (asked the same evening, plan agreed): a part's image as a source
  of the Texts tab (width 20, height auto, never stretched, a copy kept
  in the project), and symbols imported from a .elmt file into the parts
  database.

## A part's image in the Texts tab, symbols kept in the parts database (2026-11-20, after 0.15)
- Image: an image of a part is a piece of data of "Part data" like the
  others (PartTexts: id "image:" + file name, "Image: photo.png" in the
  search window, value "(image)"; imageOf() reads it as a PNG no larger
  than 512 px). The text keeps it (DynamicElementTextItem::partImage, a
  Q_PROPERTY, undo through QPropertyUndoCommand; saved as
  <part_info reference><image>base64</image></part_info>, the <text>
  empty, so QElectroTech reads an empty user text) and shows it instead
  of letters (showsImage(), imageSize(): its width the text width, 20
  when "auto", its height the text height or as the image is, the
  smaller scale of the two, never stretched; boundingRect/shape/contains
  the image). Chosen in the search window, the width row goes to 20 and
  the height to auto, in the same undo step. PartTexts::refresh() reads
  the image again from the database. Test tst_parttexts
  aPartImageIsShownAsItIs.
- Symbols: a .elmt imported in the Symbols tab of the parts manager
  ("Import a file...", the part saved first) is kept as a file of the
  part (PartFileKind::Symbol, "symbol"), named among the part's symbols
  "partsdb:<part id>/<file name>" (sources/partsdb/partsdbsymbols.*; the
  same file again is the one kept, another with the same name takes
  -2). Where a symbol of the collection is needed (parts collection:
  icon, tooltip, drag and placement; the describer of the manager) it is
  put in the collection under Imported parts (PartsDbSymbols::resolve(),
  in partssymbolfiles.cpp with the collection, through
  PartsSymbolFiles::write(), the same one used again). The .faspart of
  the part carries it as its other symbols. Tests tst_partsdbsymbols,
  tst_partsmanager aSymbolFileIsKeptInTheDatabase (tst_partsarchive and
  tst_partsmanager build partsdbsymbols.cpp and link Qt::Xml now).
  Spanish (step57.py) unfinished.

## Backup on open, project symbols of a part, the manager opened on a part (2026-11-20, after 0.15)
- Backup: "Create a backup copy?" was asked on every project opened
  (BackupDialog, upstream). Now a setting, projects/backup_on_open, in
  Settings > General > Projects ("When a project is opened:" combo,
  m_backup_on_open, added in code to gridLayout_2 row 14): "always"
  (default, he asked for it: the copy is made without asking), "ask"
  (the old window), "never". The copy is beside the project,
  yyyy-MM-dd-hh-mm_<file>. Never when nobody is there (live mode).
  Spanish step58.py unfinished.
- A part whose symbol is embedded in a project
  ("project0+embed://import/10_electric/.../bobine3.elmt"): his
  PLC dragged from the collection placed nothing once
  that project was closed. PartsSymbolFiles::collectionTwin() gives the
  symbol of the common or user collection at the same path;
  PartsDbSymbols::resolve() uses it (else the project symbol while its
  project is open, else nothing), so the collection item and the
  describer work with parts already in the database, nothing rewritten.
  The symbol chooser of the manager no longer stores project symbols:
  the twin, else a copy under Imported parts. Test tst_partsdbsymbols
  aProjectSymbolTakesItsTwinOfTheCollection.
- Parts manager from the context menu of a part in the collection: it
  opens on that part (showManager(parent, part_id), goToPart(): search
  cleared, the "all parts" group, the part selected; asks first when the
  shown part has unsaved changes; PartsCollectionItem::partId()). Test
  tst_partsmanager openedOnThePartRightClicked.
- Copy format (ElementTextFormatPattern / ApplyElementTextFormatCommand):
  he copied -K1, which shows the photo of its relay, onto -K2 and got no
  photo. The painter matched texts only by source and created only
  element information texts. Now it keeps the data of a part a text
  shows (partField, partValue, partImage) and the text height (all
  texts, as the width). Pass 1b: a PartInfo text of the target showing
  the same data (else an image for an image) takes the format; pass 3
  leaves PartInfo out. Pass 4b: none there, one is created showing the
  same data of the target's own part: the same part, the same text;
  another part, the same field, or its image of the same name, else its
  first image (read from the parts database once per command). An
  element made of no part gets nothing. Test tst_parttexts
  copyFormatBringsThePartData.
- Copy format moved a label: it gave the target text the top-left
  corner of the copied one, so a longer label aligned right (L233333
  copied from -XIN:L1, a terminal) ran over its wire and its "Position
  X" (anchorPos, what the Texts tab shows) changed. The pattern keeps
  anchorPos too; redo places the text by it once font, width and height
  are set (not a text of a group, its group places it); terminal texts
  keep their own. Test tst_parttexts copyFormatKeepsWhereATextIsAligned.

## Terminal names always printed (2026-11-20, after 0.15)
- His fuse (common fusible.elmt: terminals with show_name="true", the 1
  and 2 drawn by Terminal::paint, not texts) printed without its
  numbers: the print window and the export dialog had "Draw the names
  of the terminals" (ExportProperties::draw_terminal_names, off by
  default, Diagram::drawTerminalNames()). He: "always printed, remove
  the option". Gone: the box of both windows (export options moved up a
  row), the ExportProperties member and its "drawterminalnames" setting
  (old value left unread in the settings), Diagram's flag and its
  setter; Terminal::paint draws a shown name always; cli_export and the
  live server print no longer hide names (markers still hidden). Test
  tst_terminalnamesprinted (renders a symbol with and without its name
  shown, as the print window applies its properties: they differ).

## Panel collection: terminal names instead of number texts (2026-11-21)
- He set terminal 1 of fusible_seccionador_3p in the element editor:
  name shown, Liberation Sans 8, middle-left (halign 1, valign 128),
  label X 1, Y 4; "the same on all the elements, and remove the text it
  replaces". Done by script on 10_electric/05_iec_60617_cuadros (69 of
  74 symbols; the other 5 have unnamed terminals): every named terminal
  shows its name, label (1, 4) facing north, (1, -4) south, (-5, -7)
  east (the PE of the machines, S1/S2 of the current transformer: above
  the wire); the nearest user text writing that name removed (192
  texts, the "N" of N.1/N.2 too). Symbols whose terminals had no text
  (meters, R, C, L, varistor, PE earth) now show 1/2/PE. Each symbol a
  new uuid (projects tell symbols apart by it). Placed elements change
  with "Update elements from the collections". Copy of the folder before
  the change in the session scratchpad (backup_cuadros_antes_nombres_bornes).

## Parts manager: technical rows copied and pasted; categories with an icon (2026-11-21)
- Technical data: rows selected whole, several at once; Copy the rows /
  Paste rows (buttons, context menu, Ctrl+C / Ctrl+V on the table only:
  a cell being edited keeps its own). Clipboard text is one row a line,
  name TAB value (a spreadsheet reads and writes it). Paste: a name
  already there takes the value, the others go under the selection (or
  at the end), and what was pasted is selected. Remove takes every
  selected row. Icons from the theme (QIcon::fromTheme) so the
  standalone test needs no qeticons. Test tst_partsmanager
  technicalRowsAreCopiedAndPasted.
- Categories: the Category field is a list (category_cb): "(No
  category)", the categories with their icon, "Manage categories..."
  last. PartCategoriesDialog (sources/partsdb/partcategories.*): add,
  rename (its parts and icon follow), remove (its parts left without
  category, asked first), icon: FaseCAD icons (PartCategoryIconPicker),
  an image file (SVG as is, others a PNG of 64 px at most, 512 KB max),
  or none. Table part_category (name COLLATE NOCASE, icon BLOB) made
  with CREATE TABLE IF NOT EXISTS on opening, schema still 5: 0.15
  opens the database, ignoring it; a read-only database goes on without.
  categoryList() = the table and the categories parts have. Icons in
  the list, the tree of the manager and the folders of the parts
  collection. 32 icons drawn for it in ico/categories (white drawing on
  a tile of the colour of its family; in qelectrotech.qrc). His 30
  categories got theirs by script (only rows of part_category added).
  Tests tst_partsdatabase categoriesHaveIconsAndTakeTheirParts,
  tst_partsmanager aCategoryIsPickedFromTheList. Spanish step59.py.
- Several parts at once: the parts table selects with Ctrl/Shift
  (ExtendedSelection); the form shows the row clicked last
  (selectedPartId: the current row when selected). Delete takes every
  selected part ("Delete the %n selected parts...?"), Export "The
  selected parts...". A save on switching parts reselects what was
  selected (selectParts). Test severalPartsAreDeletedAtOnce.
- Picture first: column 6 of the parts table (moved to the front with
  moveSection, so the other columns keep their numbers) shows the first
  image of the part (firstImages(): one query), "No Picture" without.
  PartThumbnails (sources/partsdb/partthumbnails.*): read only when the
  row is drawn (PartPictureDelegate asks), in a thread with its own
  connection, JPEG decoded small (QImageReader::setScaledSize), kept as a
  96 px PNG in CacheLocation/part-pictures (key: database, file id,
  size, name): his database has ~1300 images, 214 MB, up to 14 MB each.
  Test thePictureOfEachPartComesFirst.

## 3D: room in the cabinet (2026-11-21)
- The mounting clearances of the parts database (clearance_top...rear,
  171 of his parts have some) were read nowhere. Now Solid::clearance
  (Cabinet3d::Clearance) from Parts::fill. CabinetSpace
  (sources/cabinet/cabinetspace.*), boxes only: zone() the free space a
  side asks (rear never: the plate is there; devices placed in 3D ask
  none); check(): parts into each other (one at least a device; rails
  and ducts meeting left alone), a part (or the enclosure) in the free
  space asked, a device through a wall or keeping the door from closing;
  worst first; describe() the line read. 0.5 mm is touching.
- Cabinet3dView: bodies() (a device with its model read takes the
  model's placed box; one placed in 3D only once its model is read),
  inside(): rays from the middle of the plate against every triangle of
  the enclosure but its plate (CabinetSpace::hit, Moller-Trumbore),
  hidden parts too (a hidden door still closes); no hit forward: the
  front of the box (open manual box). setMarks(): see-through boxes.
- Cabinet3dWindow: "Mounting clearances" (amber boxes in 3D, dashed on
  the sheet: CabinetItem::setClearance, static clearancesShown, not
  saved) and "Check the room": list under the navigator, red/orange
  dots; a line picked selects the part in 3D, the free space and the
  problem drawn, the parts selected on the sheet. Cleared on reload.
  Tests tst_cabinet3d theRoomIsChecked, theWindowChecksTheRoom.
- Same day: the part picker (PartPickerDialog) has the picture column
  and category icons; with purpose "enclosure" (3D, From the parts
  database) its own remembered category, first time the one named like
  enclosures (envolvente|enclosure|...). Collection: category icons 22 px
  (PartCategoryIcons::smallIcon, sharp at any scale); parts with no
  symbol to place are not listed (he asked; tst_partscollection follows).
- Collection icons still big: a QIcon of one pixmap counts its pixels as
  points (a 22 pt icon of 33 px at 150 % showed 33 pt; checked with a
  small Qt program). PartCategoryIcons::sized(image, side): one pixmap
  per scale (1 first, its size the icon's), so the view never enlarges
  it. Categories 16 pt like the folders, a part's picture or symbol 24
  pt (was 48); the pictures read small at once (PartThumbnails::scaled)
  instead of whole, which also loads the collection faster.

## Device names from the letter of the part (2026-11-21)
- Dragging from the parts collection gave no name: DeviceNaming ran
  before the preparer gave the element its part, and his panel symbols
  have no label. Now baseName(element, letter): 1. "-" + the device
  letter (code_letter) of the part placed (DiagramEventAddElement::
  setNamingLetter, set by DiagramView::startPartPlacement through
  DeviceNaming::partLetter) or of the first of the element's parts with
  one; 2. the symbol's own label without digits, else the folder
  prefix; 3. "-?" (an element with no terminal still gets no name).
  Test tst_devicenaming theLetterOfThePartComesFirst; defaultNameOfThe
  Symbol now expects "-?1".

## Subcategories of parts (2026-11-21)
- He chose: categories of parts, and a category removed sends its
  subcategories up one level. A subcategory is a path in the category of
  the part, "Relays › Interface" (PartCategoryPath in partsdatabase.h:
  separator " › " U+203A, split/join/normalized/parent/leaf/isWithin/
  less), so the schema stays 5 and 0.15 reads a category of its own.
- categoryList(): the categories they are in too, in tree order.
  partsInCategory(name, with_subcategories). addCategory(path) makes its
  categories. renameCategory moves its subcategories, their parts and
  icons (a path moves it under another; never inside itself).
  removeCategory: its parts to the category it is in (none at the top),
  its subcategories up one level (merged into one of the same name).
  Both give the paths changed (moved) for the manager to follow. An
  empty category is bound as "" (a null QString is NULL: NOT NULL failed).
- Categories manager: a tree, "Add a subcategory...", rename keeps the
  category it is in, remove says where the parts and subcategories go.
  Parts manager: the tree nested, a category lists the parts of its
  subcategories; the list of the form shows paths. Picker filter and
  parts collection (folders in folders, subfolders before the parts)
  the same. Tests tst_partsdatabase subcategoriesAreKeptAsPaths,
  tst_partscollection aSubcategoryIsAFolderInItsCategory.

## Information tab: nothing from the article number on (2026-11-21)
- He asked to remove the fields of the Information tab from "Article
  number" on, that one too: the article, its block fields, the PLC
  fields, xref (QETInformation::isEditedInInformationTab: before
  ELMT_DESIGNATION in elementInfoKeys()). The single element tab hides
  those rows but keeps them: currentInfo() writes the element back from
  its rows, so dropping them would have erased manufacturer, quantity...
  that the bill of materials and the texts read. The several-elements
  editor has no row for them (its apply starts from each element's own
  information, so they stay). tst_parttexts onlyTheFirstAuxiliaryBlock
  IsOffered checks the hidden rows keep their values; tst_multielement
  info now edits comment, location and function.

## One backup on opening, written over (2026-11-21)
- "A backup file each time" annoyed him: the copy made on opening was
  yyyy-MM-dd-hh-mm_<file>, one more per opening. Now QETProject::
  openingCopyOf(): "<name>_backup.<ext>" beside it, and makeOpeningCopy()
  writes it to "<copy>.new" then over the copy before (the old one kept
  until the new one is there). A file named *_backup is not copied
  again. The setting (always/ask/never) stays. The dated copies already
  made were left where they are. Test tst_openingcopy.
- Same day: "Download the symbol..." in the context menu of a symbol of
  the collection panel (ElementsCollectionWidget::saveSymbol: a symbol
  of a folder copied as its file is, one of a project written from its
  XML); a part of the parts collection saves its symbol, or its symbols
  to a folder. The folder chosen is remembered (collection/download-
  folder). Test tst_openingcopy aSymbolIsDownloaded (the test sets the
  common collection to the repository's).

## A deleted table kept slowing its folio (2026-11-22)
- His folio 3/6 stuttered after he put a table (a terminal summary) and
  deleted it. The saved file draws in 7 ms (timed with a throwaway probe
  rendering each folio), so it was what memory kept: a deleted table
  stays alive in the undo command, and its ProjectDBModel stayed
  connected to dataBaseUpdated, querying and laying the table out again
  at every change of the project. ProjectDBModel::setFollowing(): off
  while none of the tables showing it is on a folio (QetGraphicsTable
  Item::itemChange, ItemSceneHasChanged, the chain from the head table),
  marked stale; back on a folio (undo) it reads the database at once.
  Test tst_folioindex aTableOffItsFolioStopsFollowing.
- Also seen: the cabinet folios draw in ~80 ms, the DXF of -K1 (his PLC)
  having 27 000 lines and each circuit breaker 6 000, stroked anew at each
  repaint. Not changed yet (a cache of the drawing would do).
- Still slow after that: page 3 (12 terminals, 6 cables) repainted in
  251 ms and took 79 ms per move of the mouse (other pages 4 ms / 0.03
  ms), measured with a throwaway probe painting each item alone: the 6
  CableParts took 76 ms, each asking QETApp for its fonts some thirty
  times, each a QSettings read of the registry. QETApp now keeps the
  fonts the settings give (diagramTexts*, dynamicTextsItemFont,
  indiTextsItemFont, cableTextsFont, cableCoreFont): read once, forgotten
  when the general settings are applied (forgetSettingsFonts) and at
  most two seconds later anyway. Page 3: 29 ms a repaint, 8 ms a move.

## Wires: hops and kept apart (2026-11-22)
- He asked for the hop at a crossing (it existed, issue #436, off by
  default) and for two automatic wires not to share a stretch. A project
  made from the menu (QETDiagramEditor::newProject, newAutoConnectProject,
  useNewProjectWiring) now hops the horizontal wire and keeps wires
  apart; a project opened keeps what it was saved with (Proyecto >
  Propiedades > General: "Cruces de conductores", "Evitar que un cable
  nuevo quede encima de otro").
- QETProject::wiresApart(), saved as apart="true" on <wire_crossings>
  (written when hops or apart are on; 0.15 reads hop and ignores apart).
- Conductor::keepApart(): only an automatic path with a middle stretch
  (cas 3/4 of the router, now Conductor::automaticPath with the place of
  the stretch as a parameter). Each grid place strictly between the two
  ends is tried; the least length on another wire of the folio wins, then
  the fewest crossings, then the nearest to the midpoint. The path is kept
  as one moved by hand (modified_path, profile), so it is saved and the
  reload draws the same; a reshaped wire is never touched. Called when a
  wire is drawn (Terminal mouse release, ConductorCreator) and at the end
  of an element move for the wires it redrew (ElementsMover::endMovement,
  a ChangeConductorCommand child of the move: one undo).
- Test tst_wiresapart.
- Full ctest after it: all pass but tst_autonumincrementwidth (no autonumbering
  panel: setUpAutonumberingWidget() is never called, already so in 0.15),
  tst_selectionpanelheight (asks for a window 1500 px high) and
  tst_lazyfolios (the known Windows failure: with step prints, the paste
  of its edits, Ctrl+C then Ctrl+Shift+V, builds every folio).
  tst_terminaltexts brought to the contact without number texts (the
  names of its terminals follow). Timeouts at -j6 pass alone (-j2).

## Version 0.16 (2026-11-22)
- Brandon: "Preparame el zip para subirlo a la web". New features since
  0.15 (the notes above, 20 to 22 November), so 0.16: qetversion 0.16.
  Not committed: he asked for the zip only.
- Checked before: no other CAE software named; manufacturer data out of
  the changes (an order number in tst_partsdatabase replaced by
  ACME-C12; a PLC order number and two product names out of these notes,
  an old one from 0.12 too, which GitHub's history still has).
- The collection had lost interruptor_automatico_magnetico_3p.elmt
  (deleted 2026-11-20 about 23:11, before the terminal names script) and
  had a copy of 05_seccionamiento/interruptor_seccionador_4p.elmt loose
  at its root (2026-11-21 00:05, same uuid). He chose: the breaker back
  from 0.15 with the terminal names of the others (termnames_apply.py on
  that file only; its I> text kept), the copy removed.
- Zip FaseCAD-0.16-windows-x64.zip (146,931,895 bytes, 5068 entries, the
  same files as 0.15) by scratchpad package.sh 0.16 + zip016.py. The
  DLL pass of package.sh copied nothing the first time (the exe just
  stripped, likely held by the antivirus): run again by hand, then the
  files matched 0.15. Unzipped elsewhere, PATH=C:\Windows only:
  "FaseCAD 0.16 (QElectroTech 0.200.1-dev)". Copied to
  web/httpdocs/downloads.
- Website: construir.py VERSION and DESCARGA 0.16, download page with a
  0.16 entry (parts database, room in the 3D cabinet, wires; textos.py
  for en de it zh tr), 0.15 kept below with its own link; llms.txt
  download line; site and fasecad-web.zip rebuilt, no warning.
- For him to upload: web/fasecad-web.zip and
  httpdocs/downloads/FaseCAD-0.16-windows-x64.zip.

## His list of 2026-11-25
- Brandon: a list of eight (bridges, automatic bridge, red zones, parts
  database on a server, the AI panel, an official parts catalog, the heat
  of the cabinet, the plate from an enclosure of the database). Plan
  agreed ("si, continua"); his choices: drawn bridges converted on
  opening, PostgreSQL, the catalog online from fasecad.com, the AI through
  fasecad.com, the plate clicked in 3D.

### Bridges
- "Draw a bridge" taken out (action, both command tabs, the default
  toolbar list, its event in addItemGroupTriggered; DiagramEventAddBridge
  kept, unused, tst_tooldoubleclick still builds it). A paste brings no
  drawn bridge (Diagram::fromXml no longer reads BridgeCopy).
- QETProject::convertDrawnBridges(), right after readBridgeXml: with
  drawn bridges, every folio is built, each strip takes what sorting gave
  (setAutomaticBridges(BridgeManager::bridgesForStrip)), then
  TerminalStrip::keepAutomaticBridges() makes them bridges set by hand
  (same terminals, groove, colour); the lines and the Bridge objects go
  (removeBridge: the project is modified, saving keeps it).
- A wire between two terminals of one strip side by side bridges them
  (sources/TerminalStrip/wirebridges.*: WireBridges::bridge, a
  BridgeTerminalsCommand child of the wire's step) from Terminal mouse
  release, ConductorCreator, AutoConnect::makeWire, the auto conductors
  of DiagramEventAddElement and ElementsMover. Deleting the wire
  (DeleteQGraphicsItemCommand, both its elements staying): WireBridges::
  letGo takes out of the bar a terminal no other wire joins to it (an end
  of the bar; the bar whole when one terminal would be left). Not
  between terminals apart (isBridgeable). Test tst_wirebridges.

### Red zones while placing a DXF
- CabinetClearances (sources/cabinet/cabinetclearances.*): while
  DiagramEventAddCabinetItem places a Drawing, every device drawing of
  the folio and the preview get the clearance of their part
  (left/top/right/bottom) and are drawn in red (CabinetItem::
  setPlacingShown, refreshClearance; the zone only, not over the device).
  Back as before when the tool ends. The parts are read through
  CabinetParts (sources/cabinet/cabinetparts.*: by symbol, else label),
  shared with the heat. Test tst_cabinet3d aDxfIsPlacedOutOfTheRedZones.

### Parts database on a PostgreSQL server
- mingw-w64-ucrt-x86_64-postgresql installed (he said yes): libpq for
  the QPSQL driver, and a test server (initdb in the scratchpad, port
  55432, trust).
- PartsDatabase: open(path) of a postgresql://user@host:port/db goes to
  openServer(Server); path() is the server when useServer()
  (parts-database/use-server; server URL and password, base64, in the
  settings). Every statement through sql() -> serverSql(): INSERT OR
  IGNORE -> ON CONFLICT DO NOTHING, id INTEGER PRIMARY KEY -> identity,
  "PRIMARY KEY COLLATE NOCASE" reordered, REAL -> DOUBLE PRECISION, BLOB
  -> BYTEA, LIKE -> ILIKE, RETURNING id on inserts into part/part_file
  (insertedId()). A collation nocase (ICU, und-u-ks-level2, not
  deterministic) is made on the server. SELECT DISTINCT ... ORDER BY
  COLLATE became GROUP BY (both engines). copyFrom(source): parts,
  files, symbols, accessories (ids mapped), categories and icons; parts
  already there kept.
- Settings > General: "In a database file / On a PostgreSQL server" and
  "Server..." (PartsServerDialog: test, copy my parts to the server).
- Test tst_partsdatabase aServerHoldsThePartsToo, run when
  FASECAD_TEST_PGSERVER names a server (it makes and drops a database);
  skipped otherwise. The test's manufacturer data replaced by made-up
  names (Acme, Bolt, Wireco...).

### The heat of the cabinet
- CabinetThermal (sources/cabinet/cabinetthermal.*): the devices of a
  cabinet folio (drawings, and the devices placed in 3D only, one per
  device) with the power loss of their parts x units; P = k A (Ti - Ta),
  A the surface the placement leaves free (the seven ways of the climate
  control of enclosures), k of the material (5.5, 4.5, 12, 3.5). Too hot:
  the watts to take out, a fan's m3/h (3.1 Q / dT) when the outside is
  cooler than the maximum, else a cooling unit. Devices without power
  loss count 0 W and are listed in a warning. The enclosure's measures
  from the plate (manual, or its part's width/height/depth).
- "Thermal analysis of the cabinet..." in the cabinet menu and the tab
  next to the 3D (icon: a thermometer drawn). Test tst_cabinet3d
  theHeatOfTheCabinetIsWorkedOut.

### A plate from an enclosure of the database
- "Add a mounting plate": by measures, or the plate of an enclosure of
  the parts database (its STEP needed). Once the plate is on the sheet
  the 3D window opens asking for a click on its plate
  (Cabinet3dWindow::askForPlate; usePlate(id, true) gives the sheet's
  plate its size). Test tst_cabinet3d thePlateIsClickedInTheEnclosure.

### Official parts catalog
- web/fuente/catalogo.php (copied by construir.py): the .faspart files
  he uploads to fasecad-datos/catalogo/, an index rebuilt when they
  change (catalogo-indice.json), ?lista (q, categoria, pagina), ?foto,
  ?pieza; a licence prepared and off (catalogo-config.json: licencia,
  claves; X-FaseCAD-Clave). Not run here: no PHP on this PC.
- PartsCatalogDialog (sources/partsdb/partscatalogdialog.*): search,
  category, photos, pages, "Add to my parts database" (PartsArchive::
  store with PartsSymbolFiles::write), replace or keep. Its icon a
  placeholder mark (dashed square, FC): his icon to come. Address and
  licence key under "Server..."; the key is FaseCadLicence
  (fasecad/licence-key), shared with the AI. Menu Lists and the Parts tab.
  Test tst_partscatalog (a small HTTP server in the test answers as
  catalogo.php).

### The AI panel
- "IA" at the bottom right of the window (status bar, checkable) shows
  the AssistantPanel dock: the talk, the instruction (Enter sends), Stop,
  New conversation, Service... (address, licence key).
- AssistantSession: the Messages API loop, streamed, through
  web/fuente/ia.php (his key in fasecad-datos/ia-config.json, models and
  max_tokens allowed, requests per hour per client, the licence prepared
  and off, use per day in ia-uso.json, nothing of the talk kept). Model
  claude-opus-5-5, thinking adaptive, effort high, max_tokens 64000,
  fallbacks "default" with server-side-fallback-2026-07-01 (a declined
  request goes on with the model the category goes to), cache on the
  system prompt and the conversation. Tools: run_script (JavaScript with
  qet, QetScripting::runSource, one undo step named "AI: <title>"),
  list_symbols (a folder of the collection). The state of FaseCAD
  (project, folios, folio shown, selection) opens each instruction. The
  answers are kept as they came (thinking blocks, fallback blocks);
  every tool result of a turn in one message; at most 30 rounds.
- Scripts of the AI write no file: QetScriptApi::setNoFiles (LiveRun::
  no_files) makes the 15 calls saving or exporting refuse.
- Test tst_assistant (a server streaming as the API does: a tool call,
  then a text). ia.php not run here (no PHP).

### 2026-11-26, after trying it
- Bridges with gaps: 2-4-6-8 wired made no bridge (only terminals side
  by side were). TerminalStrip::canJoinWithBar / joinWithBar (any gaps,
  one level, the bar one of them sits in lengthened), leaveBar,
  restoreBar; WireBridges uses its own steps (JoinCommand, LeaveCommand:
  the bar exactly as it was on undo; BridgeTerminalsCommand did not put a
  lengthened bar back right). Test tst_wirebridges everyOtherTerminalIsBridged.
- He looked at the 0.16 copy: no IA button, no server choice there. The
  IA button now floats at the bottom right of the drawing (round, red,
  over the MDI area) and its panel opens over the right of it
  (AssistantPanel is a QFrame on m_workspace, placed on resize).
- Cabinet in 3D: a "Loading the cabinet in 3D..." window while it comes
  up (it looked closed). "Cabinet in 3D" and "Thermal analysis" out of
  the Cabinet construction 2D menu (kept as tab buttons): he took the 3D
  entry for the plate choice, which is in "Add a mounting plate...".
- Heat map in the thermal analysis (CabinetHeatMap): the plate coloured
  from the outside temperature (blue) to the highest allowed and above
  (red): the walls' rise, 0.7 of it at the bottom to 1.3 at the top, plus
  around each drawing W / (8 (4 a + 0.05)) K fading with the distance
  (sigma half its size + 30 mm). Said on the window to be an estimate.
- fasecad.com account (FaseCadAccount, utils/fasecadaccount.*): user and
  password instead of the address and the licence key, for the AI panel
  (gear in its head) and the official catalog ("Sign in..." in place of
  "Server..."). The password kept with Windows DPAPI (crypt32), sent as
  X-FaseCAD-Acceso, base64 "user:password". FaseCadLicence is unused now.
  Server side: web/fuente/cuentas.php (shared by ia.php and catalogo.php,
  copied by construir.py), accounts in fasecad-datos/usuarios.json as
  PBKDF2-SHA256 hashes, made with web/usuarios.py. The AI always needs an
  account; the catalog lists freely and asks for one to download unless
  catalogo-config.json says {"cuenta": false}. 401 for a wrong account.
  PHP not run here (no PHP on this PC).
- Plate from an enclosure: the part chooser opened by "Choose the
  enclosure..." has "Only parts with a STEP model" (on by default,
  remembered); only that window (PartPickerDialog::offerOnlyWithStep),
  not the enclosure chooser of the 3D window. PartsDatabase::partsWithFile.
- A device without DXF or STEP: on the plate a box of the width and
  height of its part (CabinetDevices::drawingFromParts, after every DXF
  was tried; "Box W × H mm"); in 3D the drawn box already takes the part
  depth, and a device not drawn is offered to place in 3D as a STEP box of
  width × height × depth (Cabinet3d::boxStep, cached; needs OpenCASCADE).
  Test aDeviceWithoutModelsIsABoxOfItsMeasures.
- Account buttons at the right of the command tabs, before the command
  search (ui/accountbuttons.*, CommandTabs::addCornerWidget): "Report a
  bug", "Sign in", "Request support". FaseCadServices (utils/) asks
  cuenta.php?estado at start: "Sign in" grey until the server says
  {"servicio": true} (fasecad-datos/cuenta-config.json); then ?yo with the
  account gives the subscription; bug and support only with one running.
  SupportDialog sends ?soporte (subject, text, version, system, locale);
  the server keeps each in fasecad-datos/soporte/*.json and mails
  "correo" of the config when set; 10 a day per user. usuarios.json
  entries are now {"hash", "correo", "suscripcion_hasta"} (the bare hash
  still reads); usuarios.py "suscripcion NOMBRE FECHA|no". Test
  tst_accountbuttons. PHP not run here.
- The IA button sends out rings (AssistantRipple: two red rings growing
  and fading, 2.6 s, under the button, transparent for the mouse), quiet
  while the panel is open. The button stands reach + 2 px from the corner
  so the rings are not cut.
- One computer at a time for each account. cuenta.php?entrar (account +
  {equipo: computer id kept in the settings, nombre: host name, forzar})
  opens the session: the server keeps sha256 of a random key per user in
  fasecad-datos/sesiones.json; 409 {ocupada, equipo, desde} when another
  computer has it alive (seen in the last 15 min) and it is not forced.
  Every request after sends X-FaseCAD-Sesion (FaseCadAccount::applyTo:
  AI, catalog, account); cuentas_exigir() answers 409 when it is not the
  open one. ?salir frees it. FaseCAD keeps it alive every 5 min
  (?yo), notices it taken (sessionLost: a non-blocking message), and at
  start opens one quietly when none is kept. Signing in: the window, then
  "open on PC-X since ...: close it there and go on here?"
  (AccountButtons::signIn, used by the tab button, the AI gear and the
  catalog). The session key is kept with DPAPI like the password.
- Three addresses (Brandon, 2026-11-26): fasecad.com/pro (sales page, made
  by another session, with a notify list), my.fasecad.com (customer area)
  and api.fasecad.com (what the program talks to: ia.php, catalogo.php,
  cuenta.php; the defaults in AssistantSession, PartsCatalogDialog and
  FaseCadServices). Code in web/api.fasecad.com, web/my.fasecad.com and
  web/fasecad-php (fasecad.php: SQLite fasecad-datos/fasecad.sqlite with
  usuarios, sesiones, uso_ia, soporte, stripe_eventos, intentos;
  usuarios.json imported once; web.php: browser session, CSRF, layout).
  my: registro (mail confirmation), entrar/olvido/restablecer, resumen,
  suscripcion (Stripe Checkout 20 EUR/month + VAT by Stripe Tax or a
  txr_, billing portal), stripe.php webhook (signature checked, each event
  once, period end to suscripcion_hasta; past_due keeps what was paid),
  gasto (AI tokens and cost by month from precios_ia), soporte (program
  and web messages, answers), cuenta (password, email, close the program
  session), admin (accounts, manual subscription, answers, AI totals).
  api/ia.php: Pro only (402) and limite_mes of AI cost (429).
  Sign-in window links to registro, olvido and my. construir.py makes
  fasecad-servicios.zip. Setup in web/LEEME.md. PHP NOT run (no PHP here,
  he said not to install it).

## Version 1.0 (2026-11-26)
- Brandon: "Lanza la version FaseCAD" and reset the history. He chose 1.0,
  the same GitHub repository with one commit, and a GitHub release with
  the zip. The whole history before is kept in
  C:\Users\Brandon\FaseCAD\respaldo-historial-FaseCAD-2026-11-26.bundle
  (git bundle --all, 242 MB): `git clone <bundle>` gives it back.
- qetversion productVersion 1.0. Full ctest: all pass but the three that
  failed before (tst_autonumincrementwidth, tst_selectionpanelheight,
  tst_lazyfolios); six slow ones timed out at 300 s with -j4 and passed
  one by one.
- Zip FaseCAD-1.0-windows-x64.zip (147,638,855 bytes, 5068 files): the
  0.16 files plus libpq and libssl (PostgreSQL). package.sh's DLL pass
  found nothing when run from build-release (ldd resolved the DLLs copied
  there): run from C:\ it copies them. The loose
  elements/contacto_conmutado.elmt (a copy of
  05_iec_60617_cuadros/01_contactos/contacto_conmutado.elmt, same uuid)
  is left out of the zip and of the commit. Unzipped elsewhere,
  PATH=C:\Windows only: "FaseCAD 1.0 (QElectroTech 0.200.1-dev)".
- 2026-11-27: "Connection symbols" (Insert and Connections tabs) keeps
  only Outgoing report and Incoming report (his screenshot of the Insert
  group): corner, tees, branches, interruption and the Symbols menu out of
  the tabs; the actions stay in the program.
- 2026-11-27: the parts manager no longer shows the Functions and
  Connection points tabs (Brandon: keep it simple; nothing in the program
  used them yet). Their tables are built and kept hidden, so a part saved
  again keeps its functions and connection points, and .faspart and the
  database keep them. tst_partsmanager checks the tabs are gone and the
  data kept.
- 2026-11-27, his list (he chose: like a protection; the language of the
  project; accessories become parts of the symbol):
  - Part::max_current (A), parts manager General after Rated current,
    with a "?" (max_current_help) whose tooltip says the wires downstream
    depend on what is connected and points to Calculate sections.
    WireSections::check: any device whose parts say a maximum current
    (the highest) counts as a protection of it (maxCurrent, the database
    opened once). The action is now "Calculate sections…" (tab button
    "Calculate sections"), window titled the same, columns Device/Current.
  - Part::translations, JSON {"description": {"en": ...}, "model": {...}}
    (PartTranslations in partsdatabase.*: fields, languages es en de fr it
    pt tr zh, translation/setTranslation/text). Parts manager tab
    "Translations" (a row per language). Folios already show the project
    languages (ProjectLanguages, active language, phrases by source
    text); Collect texts of the project now also fills, for each phrase
    that is the description or model of a part, the languages with no
    words yet from its translations (ProjectLanguagesDialog::fillFromParts;
    "en-GB" takes "en"); translatableTexts collects "model" too.
  - Schema 6: max_current REAL, translations TEXT (both upgrades and
    PostgreSQL through serverSql).
  - Accessories tab hidden (kept like functions); picking a part for the
    main article (ElementInfoWidget) or adding it (ElementPartsWidget)
    adds it and its accessories to the parts of the symbol, each with its
    quantity, instead of the auxiliary blocks.
  - Category "Cables" (PartsDatabase::cablesCategory) always in a
    database (addDefaultCategories after the schema, cable icon read from
    the resources), never removed nor renamed (database and dialog); the
    Cable tab only for parts within it (showCableTab).
  - Tests: tst_parttranslations (new), tst_wiresections maximum current,
    tst_partsdatabase (Cables apart in the counts, version 6 upgrades).
- 2026-11-27, Spanish: the command tabs (CommandTabs, 156 texts) in es,
  and in de fr it pt tr zh as well (his "resto de idiomas": those six
  for now; the other 29 languages of lang/ still English there). The
  main window strings with no Spanish (QETDiagramEditor, 112: the Devices
  dock, history of saves, cable plan...) filled. lupdate on a copy
  counts 687 empty Spanish translations in the whole program before
  this, now about 450 (QObject 160, GeneralConfigurationPage 49,
  dimensions, bridges, cable manager...). They follow the language of
  Settings > General, as every text does.
- Set version (Tools > Versions, Project menu; ProjectVersions in
  sources/projectversions.*): version 1 keeps a picture of the devices
  (symbols by label) and their wiring (for each terminal, the terminals
  of other labelled devices on the same potential, over folio reports;
  so moving or redrawing is no change); each next version adds at the end
  "Changes of version N" sheet(s) (46 lines each, one free text) with the
  devices added, removed and rewired (− / + lines), or "No device was
  added, removed or rewired." Kept as <project_versions> (versions with
  date, the last picture) in the project and in its settings for .qetdb;
  project property project_version for title blocks
  (%{project_version}). Asked first; no undo (adding a folio has none).
  Test tst_projectversions.
- 2026-11-27: more than 2 wires on a terminal: the limit was his own
  setting (Settings > General, wiring rules on, 2 per terminal); set to 0
  (no limit) in HKCU\Software\FaseCAD\FaseCAD\diagrameditor
  wiring_rules_max_wires. A project with its own rule keeps it.
- A core of a cable shows no text of its own (Conductor::isCableCore, in
  updateTextVisibility and the one-text-per-folio branch): the cable
  names its cores. Test tst_cabletarget aCoreShowsNoTextOfItsOwn.

## Version 1.1 (2026-11-28)
- Brandon: "Generame la version 1.1". qetversion productVersion 1.1.
- Set version now takes the number from the title block «Indice Rev»
  (indexrev): 1 when empty or not a number, else +1, and keeps the changes.
- Translations of the FaseCAD strings (base_todo: shared list + the
  CommandTabs strings, 1471) done for es de fr it pt pt_BR (copy of pt)
  tr zh zh_TW (converted from zh) ru uk pl cs, all type="unfinished".
  Still to do: sk ja ko ca sv nl nl_BE nl_NL hu el ro lv da nb no fi hr
  sl sr rs ar mn. Duplicate messages (sources with an apostrophe written
  as &apos;) merged in de es it pt pt_BR tr zh zh_TW.
- First start: the language is already "system" (QLocale), English when
  there is no translation; no change. His registry still says lang=en.
- Zip FaseCAD-1.1-windows-x64.zip (148,368,508 bytes, 5068 files) in
  web/httpdocs/downloads, from scratchpad package.sh 1.1 run from C:\.
  Unzipped elsewhere, PATH=C:\Windows only: "FaseCAD 1.1 (QElectroTech
  0.200.1-dev)". Not committed, not pushed, no GitHub release, website
  (construir.py VERSION) not changed.
