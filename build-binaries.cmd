set PATH=%PATH%;%~dp0scripts
set WSLENV=NDK/p

wsl bash ./build-linux.sh
call build-windows
call build-install
call build-vsix

call make-version.cmd VERSION version.h

mkdir "%~dp0_setup/"
set OUTPUT="%~dp0_setup\micro-profiler_symbols.v%VERSION%.zip"

pushd "%~dp0_build.windows.x86\_bin\RelWithDebInfo"
	call mkzip micro-profiler_frontend.pdb "%OUTPUT%"
	call mkzip micro-profiler_Win32.pdb "%OUTPUT%"
popd
pushd "%~dp0_build.windows.x64\_bin\RelWithDebInfo"
	call mkzip micro-profiler_standalone.pdb "%OUTPUT%"
	call mkzip micro-profiler_x64.pdb "%OUTPUT%"
popd
