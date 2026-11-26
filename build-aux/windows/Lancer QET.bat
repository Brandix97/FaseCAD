@echo off

rem Se rend dans le dossier qui convient
set current_dir=%~dp0
cd /d %current_dir%

rem lance FaseCAD
rem Sans option --config-dir, la configuration de FaseCAD seront dans
rem "C:/Users/<USER>/AppData/Local/FaseCAD/FaseCAD"
rem Sans l'option --data-dir, les données utilisateur (elements, titleblocks,
rem log, ...) de FaseCAD sont stockées dans
rem "C:/Users/<USER>/AppData/Roaming/FaseCAD/FaseCAD"
set command=bin\fasecad.exe --common-elements-dir=elements/ --common-tbt-dir=titleblocks/ --lang-dir=lang/ %*
@start %command%
