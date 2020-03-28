@echo off
set CMAKE="c:\Program Files\CMake\bin\cmake.exe"

echo Cloning libzmq...

if NOT EXIST libzmq (
	git clone https://github.com/altera2015/libzmq.git
)

cd libzmq

echo Building libzmq for Windows 32 bit

if NOT EXIST build (
mkdir build
cd build
%CMAKE% -G "Visual Studio 15 2017" -A Win32 ..
msbuild libzmq-static.vcxproj /property:Configuration=Release
cd ..
)

echo Building libzmq for Windows 64 bit

if NOT EXIST build64 (
mkdir build64
cd build64
%CMAKE% -G "Visual Studio 15 2017" -A x64 ..
msbuild libzmq-static.vcxproj /property:Configuration=Release
cd..
)

cd ..

msbuild /property:Configuration=Release /property:Platform=x64
msbuild /property:Configuration=Release /property:Platform=Win32

"C:\Program Files (x86)\Inno Setup 5\compil32" /cc "setup.iss"