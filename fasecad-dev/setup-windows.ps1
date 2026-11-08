# FaseCAD: set up a Windows development machine.
#
# Installs Git, GitHub CLI and MSYS2 (with Qt 6, KDE Frameworks 6, CMake,
# Ninja, GCC and SQLite), clones the FaseCAD fork, configures git and builds
# a debug version.
#
# Run it from a normal PowerShell window (not as administrator; winget asks
# for elevation itself when an installer needs it):
#
#   powershell -ExecutionPolicy Bypass -File setup-windows.ps1
#
# Every step can be run again safely: what is already there is skipped.

param(
	[string]$Dir = "$HOME\dev\fasecad",
	[string]$UserName = "Brandon",
	[string]$UserEmail = "59540263+Brandix97@users.noreply.github.com",
	[int]$Jobs = 0
)

$ErrorActionPreference = "Stop"

function Step($text) { Write-Host "`n==> $text" -ForegroundColor Cyan }

function Refresh-Path {
	$env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" +
		[Environment]::GetEnvironmentVariable("Path", "User")
}

# 1. Tools ------------------------------------------------------------------
Step "Installing Git, GitHub CLI and MSYS2 with winget"
foreach ($id in "Git.Git", "GitHub.cli", "MSYS2.MSYS2") {
	winget list --id $id -e --accept-source-agreements | Out-Null
	if ($LASTEXITCODE -eq 0) {
		Write-Host "$id already installed"
	} else {
		winget install --id $id -e --silent --accept-source-agreements --accept-package-agreements
	}
}
Refresh-Path

$bash = "C:\msys64\usr\bin\bash.exe"
if (-not (Test-Path $bash)) { throw "MSYS2 not found in C:\msys64" }

# 2. MSYS2 packages ---------------------------------------------------------
Step "Updating MSYS2 (run twice: the first pass may only update the core)"
& $bash -lc "pacman -Syu --noconfirm"
& $bash -lc "pacman -Syu --noconfirm"

Step "Installing Qt 6 and the build dependencies"
$packages = @(
	"toolchain", "qt6-base", "qt6-svg", "qt6-tools", "qt6-translations",
	"qt6-declarative", "qt6-pdf", "cmake", "ninja", "pkg-config", "sqlite3",
	"kcoreaddons", "kwidgetsaddons", "extra-cmake-modules"
) | ForEach-Object { "mingw-w64-ucrt-x86_64-$_" }
& $bash -lc "pacman -S --needed --noconfirm $($packages -join ' ')"
if ($LASTEXITCODE -ne 0) { throw "pacman failed" }

# 3. GitHub login -----------------------------------------------------------
Step "Checking the GitHub login"
$gh = "C:\Program Files\GitHub CLI\gh.exe"
& $gh auth status 2>$null | Out-Null
if ($LASTEXITCODE -ne 0) {
	Write-Host "Log in: GitHub.com, HTTPS, Yes, Login with a web browser"
	& $gh auth login
}
& $gh auth setup-git

# 4. Clone ------------------------------------------------------------------
Step "Cloning FaseCAD into $Dir"
if (Test-Path "$Dir\.git") {
	Write-Host "Already cloned, updating"
	git -C $Dir pull --ff-only
	git -C $Dir submodule update --init --recursive
} else {
	New-Item -ItemType Directory -Force (Split-Path $Dir) | Out-Null
	git clone --recursive --branch fasecad https://github.com/Brandix97/FaseCAD.git $Dir
	git -C $Dir remote add upstream https://github.com/qelectrotech/qelectrotech-source-mirror.git
	git -C $Dir fetch upstream
}
git -C $Dir config user.name $UserName
git -C $Dir config user.email $UserEmail
Add-Content "$Dir\.git\info\exclude" "build/`nbuild.log`nbuild-configure.log"

# 5. Build ------------------------------------------------------------------
if ($Jobs -le 0) {
	# GCC needs about 3 GB of RAM per job on this code base: more jobs than
	# that freezes the machine.
	$ramGb = [math]::Floor((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB)
	$cores = [Environment]::ProcessorCount
	$Jobs = [math]::Max(1, [math]::Min($cores, [math]::Floor($ramGb / 3)))
}
Step "Building with $Jobs parallel jobs (this takes a while)"
$env:MSYSTEM = "UCRT64"
$env:CHERE_INVOKING = "1"
Push-Location $Dir
try {
	& $bash -lc ("cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug " +
		"-DPACKAGE_TESTS=ON -DCMAKE_PREFIX_PATH=/ucrt64 " +
		"-DBUILD_WITH_KF=ON -DBUILD_KF=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5 " +
		"-DSQLite3_INCLUDE_DIR=/ucrt64/include " +
		"-DSQLite3_LIBRARY=/ucrt64/lib/libsqlite3.dll.a")
	if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
	& $bash -lc "cmake --build build -j $Jobs 2>&1 | tee build.log"
	if ($LASTEXITCODE -ne 0) { throw "build failed, see $Dir\build.log" }
} finally {
	Pop-Location
}

Step "Done. Start it with $Dir\fasecad-dev\run-fasecad.bat"
