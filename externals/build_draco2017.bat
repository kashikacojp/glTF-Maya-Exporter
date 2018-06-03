
call "C:\\Program Files (x86)\\Microsoft Visual Studio\2017\\Community\\VC\\Auxiliary\\Build\\vcvars64"

cd %~dp0
cd draco
rmdir /S /Q build
mkdir build
cd build

cmake -G "Visual Studio 15 Win64" -DCMAKE_C_COMPILER="C:/Program Files (x86)/Microsoft Visual Studio/2017/Community/VC/Tools/MSVC/14.12.25827/bin/Hostx64/x64/cl.exe" -DCMAKE_CXX_COMPILER="C:/Program Files (x86)/Microsoft Visual Studio/2017/Community/VC/Tools/MSVC/14.12.25827/bin/Hostx64/x64/cl.exe" ..

cd %~dp0
pause
