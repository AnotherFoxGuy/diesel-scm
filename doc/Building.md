Building from Source
===============================================================================

Prerequisites
-------------------------------------------------------------------------------
Building Fuel from source requires Qt version 5. Qt is available at:
	http://www.qt.io/download-open-source/

To run Fuel a compiled binary of Fossil must be available either in the system
path or in the same folder as the Fuel executable. You can find the latest
Fossil binaries from the Fossil homepage at:
	https://www.fossil-scm.org/download.html

Retrieving the source
-------------------------------------------------------------------------------
The source is available as a tar.gz archive at the following location

	https://fuel-scm.org/fossil/wiki?name=Downloads

Additionally you can clone the source code directly from our site using fossil

	mkdir fuel
	cd fuel
	fossil clone https://fuel-scm.org/fossil fuel.fossil
	fossil open fuel.fossil


Windows (Qt5 / MinGW)
-------------------------------------------------------------------------------
1. Open a Command Prompt and cd into the folder containing the Fuel source code

		cd fuel

2. Make a build folder and cd into it

		md build
		cd build

3. Generate the makefile with cmake

		cmake -DCMAKE_BUILD_TYPE=Release ..

4. Build the project

		make -j6

5. Copy the Qt DLLs

		make copy_dll


Windows (Qt5 / MSVC)
-------------------------------------------------------------------------------
1. Open a Command Prompt and cd into the folder containing the Fuel source code

		cd fuel

2. Make a build folder and cd into it

		md build
		cd build

3. Generate the Visual Studio project makefile with cmake

		cmake ..

4. Open the generated project

		start fuel.vcxproj

5. Build the project
	Use the IDE to build the project or alternatively you can use via MSBuild

		call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
		msbuild /p:Configuration=Release /m Fuel.vcxproj

6. Copy the Qt DLLs

		msbuild /p:Configuration=Release copy_dll.vcxproj

4. Enjoy

		bin\Fuel.exe


Mac OS X
-------------------------------------------------------------------------------
Build Steps:

1. Open a Terminal and add your Qt bin folder to the path

		export PATH=$PATH:/path/to/qt/version/clang_64/bin

2. Go into the folder containing the Fuel source code

		cd fuel

3. Generate the makefile with cmake

		cmake -DCMAKE_BUILD_TYPE=Release ..

5. Build the project

		make -j6

6. (Optional) Include the Fossil executable within the Fuel application bundle

		cp /location/to/fossil Fuel.app/Contents/MacOS

7. Package Qt dependencies into Fuel to make a standalone application bundle

		macdeployqt Fuel.app

8. Enjoy

		open Fuel.app


Unix-based OS
-------------------------------------------------------------------------------
Build Steps:

1. cd into the folder containing the Fuel source code

		cd fuel

2. Make a build folder and cd into it

		md build
		cd build

3. Generate the makefile with cmake

		cmake -DCMAKE_BUILD_TYPE=Release ..

4. Build the project

		make -j6

5. Enjoy

		bin/Fuel

