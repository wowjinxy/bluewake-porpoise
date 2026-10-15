@echo off
setlocal
set "game_dir=%~dp0build\native-win"
if not exist "%game_dir%\switchwaker.exe" if exist "%game_dir%\Release\switchwaker.exe" set "game_dir=%game_dir%\Release"
if not exist "%game_dir%\switchwaker.exe" (
    echo WindowWaker has not been built in build\native-win.
    echo Build the switchwaker target first; see README.md.
    exit /b 2
)
pushd "%game_dir%" || exit /b 2
"%game_dir%\switchwaker.exe" %*
set "game_exit=%errorlevel%"
popd
exit /b %game_exit%
