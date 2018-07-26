rmdir /q /s build
mkdir build

cmake.exe -G "Visual Studio 15 2017 Win64" -Bbuild -H.
