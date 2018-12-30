@setlocal enableextensions enabledelayedexpansion

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
: Appveyor Deployer
: See appveyor.yml for default variables
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
: Evaluate whether we should be deploying
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

if not [%DPL_ENABLED%] == [1] (
    echo "Deployer is not enabled..."
    exit /b
)

: Don't do DD installs because fmodex DLL handling is not implemented.
if [%CONFIGURATION%] == [DD] (
    echo "Deployer does not support DD build..."
    exit /b
)

if [%CONFIGURATION%] == [DD64] (
    echo "Deployer does not support DD build..."
    exit /b
)

: Substring match from https://stackoverflow.com/questions/7005951/batch-file-find-if-substring-is-in-string-not-in-a-file
: The below line says "if deployer is NOT in string"
: Note that APPVEYOR_REPO_BRANCH for pull request builds is the BASE branch that PR is merging INTO
if not [%APPVEYOR_REPO_TAG%] == [true] (
    if x%APPVEYOR_REPO_BRANCH:deployer=%==x%APPVEYOR_REPO_BRANCH% (
        echo "Deployer is enabled but we are not in a release tag or a 'deployer' branch..."
        exit /b
    )
) else (
    set "ASSET_FILES_OPTIONAL_GET=1"
)
: Release tags always get optional assets (music.dta)

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
: Get asset archives
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

if not exist "assets\deployer\archives" mkdir "assets\deployer\archives"

goto EXTRACT_ARCHIVES

::::::::::::::::::::::::::::::::
: ARCHIVE_NAME_PARTS
: Call this like a function. %archivepath% is the path to extract parts from.
::::::::::::::::::::::::::::::::

for %%a in (%archivepath%) do (
    set "file=%%~fa"
    set "filepath=%%~dpa"
    set "filename=%%~nxa"
)

set "localarchivepath=assets\deployer\archives\%filename%"

goto EOF

::::::::::::::::::::::::::::::::
: EXTRACT_ARCHIVES
::::::::::::::::::::::::::::::::

set "archivepath=%ASSET_ARCHIVE_PATH%"
call :ARCHIVE_NAME_PARTS
set "ASSET_ARCHIVE_PATH_LOCAL=%localarchivepath%"
if not exist "%localarchivepath%" appveyor DownloadFile "%ASSET_ARCHIVE_PATH%" -FileName "%localarchivepath%"

set "archivepath=%ASSET_ARCHIVE_PATCH_PATH%"
call :ARCHIVE_NAME_PARTS
set "ASSET_ARCHIVE_PATCH_PATH_LOCAL=%localarchivepath%"
if not exist "%localarchivepath%" appveyor DownloadFile "%ASSET_ARCHIVE_PATCH_PATH%" -FileName "%localarchivepath%"

echo "Testing for x86"
if not [%X86_64%] == [1] (
    echo "Attempting x86 asset download"
    set "archivepath=%ASSET_ARCHIVE_X86_PATH%"
    call :ARCHIVE_NAME_PARTS
    set "ASSET_ARCHIVE_X86_PATH_LOCAL=%localarchivepath%"
    if not exist "%localarchivepath%" appveyor DownloadFile "%ASSET_ARCHIVE_X86_PATH%" -FileName "%localarchivepath%"
)

echo "Testing for x64"
if [%X86_64%] == [1] (
    echo "Attempting x64 asset download"
    set "archivepath=%ASSET_ARCHIVE_X64_PATH%"
    call :ARCHIVE_NAME_PARTS
    set "ASSET_ARCHIVE_X64_PATH_LOCAL=%localarchivepath%"
    if not exist "%localarchivepath%" appveyor DownloadFile "%ASSET_ARCHIVE_X64_PATH%" -FileName "%localarchivepath%"
)

echo "Testing for optional"
if [%ASSET_FILES_OPTIONAL_GET%] == [1] (
    echo "Attempting optional asset download"
    set "archivepath=%ASSET_FILES_OPTIONAL_PATH%"
    call :ARCHIVE_NAME_PARTS
    set "ASSET_FILES_OPTIONAL_PATH_LOCAL=%localarchivepath%"
    if not exist "%localarchivepath%" appveyor DownloadFile "%ASSET_FILES_OPTIONAL_PATH%" -FileName "%localarchivepath%"
)

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
: Build the installers
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

mkdir "assets\deployer\installer"
mkdir "assets\deployer\patch"

7z x -y "%ASSET_ARCHIVE_PATH_LOCAL%" -o"assets\deployer\installer" >null
7z x -y "%ASSET_ARCHIVE_PATCH_PATH_LOCAL%" -o"assets\deployer\patch" >null

: Copy optional files to full installer (music.dta)
if [%ASSET_FILES_OPTIONAL_GET%] == [1] (
    xcopy /I /Y "%ASSET_FILES_OPTIONAL_PATH_LOCAL%" "assets\deployer\installer"
)

: Copy EXE -- BUILD_PATH is from appveyor.yml
robocopy "%BUILD_PATH%" "assets\deployer\installer" /XF "*.debug" ".gitignore"
robocopy "%BUILD_PATH%" "assets\deployer\patch" /XF "*.debug" ".gitignore"

: Are we building DD? (we were supposed to exit earlier!)
if [%CONFIGURATION%] == [DD] ( set "DPL_INSTALLER_NAME=%DPL_INSTALLER_NAME%-DD" )
if [%CONFIGURATION%] == [DD64] ( set "DPL_INSTALLER_NAME=%DPL_INSTALLER_NAME%-DD" )

: If we are not a release tag, suffix the filename
if not [%APPVEYOR_REPO_TAG%] == [true] (
    set "INSTALLER_SUFFIX=-%APPVEYOR_REPO_BRANCH%-%GITSHORT%-%CONFIGURATION%"
) else (
    set "INSTALLER_SUFFIX="
)

if not [%X86_64%] == [1] ( goto X86_INSTALL )

::::::::::::::::::::::::::::::::
: X64_INSTALL
::::::::::::::::::::::::::::::::

: Extract DLL binaries
7z x -y "%ASSET_ARCHIVE_X64_PATH_LOCAL%" -o"assets\deployer\installer" >null
if [%ASSET_PATCH_GET_DLL%] == [1] (
    7z x -y "%ASSET_ARCHIVE_X64_PATH_LOCAL%" -o"assets\deployer\patch" >null
)

: Build the installer
7z a -sfx7z.sfx "%DPL_INSTALLER_NAME%-x64-Installer%INSTALLER_SUFFIX%.exe" .\assets\deployer\installer\*

: Build the patch
7z a "%DPL_INSTALLER_NAME%-x64-Patch%INSTALLER_SUFFIX%.zip" .\assets\deployer\patch\*

: Upload artifacts
appveyor PushArtifact "%DPL_INSTALLER_NAME%-x64-Installer%INSTALLER_SUFFIX%.exe"
appveyor PushArtifact "%DPL_INSTALLER_NAME%-x64-Patch%INSTALLER_SUFFIX%.zip"

: We only do x86 OR x64, one at a time, so exit now.
goto EOF

::::::::::::::::::::::::::::::::
: X86_INSTALL
::::::::::::::::::::::::::::::::

: Extract DLL binaries
7z x -y "%ASSET_ARCHIVE_X86_PATH_LOCAL%" -o"assets\deployer\installer" >null
if [%ASSET_PATCH_GET_DLL%] == [1] (
    7z x -y "%ASSET_ARCHIVE_X86_PATH_LOCAL%" -o"assets\deployer\patch" >null
)

: Build the installer
7z a -sfx7z.sfx "%DPL_INSTALLER_NAME%-Installer%INSTALLER_SUFFIX%.exe" .\assets\deployer\installer\*

: Build the patch
7z a "%DPL_INSTALLER_NAME%-Patch%INSTALLER_SUFFIX%.zip" .\assets\deployer\patch\*

: Upload artifacts
appveyor PushArtifact "%DPL_INSTALLER_NAME%-Installer%INSTALLER_SUFFIX%.exe"
appveyor PushArtifact "%DPL_INSTALLER_NAME%-Patch%INSTALLER_SUFFIX%.zip"

: We only do x86 OR x64, one at a time, so exit now.
goto EOF

::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
: EOF
::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::

endlocal
