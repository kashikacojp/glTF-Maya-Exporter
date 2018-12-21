# glTF-Maya-Exporter

## Maya glTF 2.0 Exporter

The prebuild binary is here: https://kashika.co.jp/product/gltfexporter/

## Version
1.5.3

## Introduction
This is the glTF 2.0 exporter for AUTODESK MAYA (
https://www.autodesk.co.jp/products/maya/). 

This repositry contains mel scripts and C++ source codes.


## Support Maya version

We support MAYA2017 and MAYA2018 on Windows and macOS, now.


## Support features

- [x] Export mesh

- [x] Material Lambert, phong, phongE

- [x] Material Parameters: baseColor, Roughness

- [x] Bump mapping support (with Bump2d node)

- [x] Material aiStandardSurface

- [ ] Material StingrayPBS

- [x] Transform/Skeleton

- [x] Mac support

- [x] Linux support

- [x] VRM format (https://dwango.github.io/vrm/)

- [x] Substance Painter Texture Node (NEED option => Automatic connections : true)

- [x] SkinMesh animation

- [x] Blend shape animation

## How to Build

### Generate project file by CMake

- 1: Create build directory. `$mkdir build`

- 2: Move Currenct directory `$cd build`

- 3: Create project `$cmake ..`

- 4: Make it..  `$cmake --build .`

### Use Visual Studio (deprecated)

- 1: Requirements: Visual studio 2017.

- 2: Generate Draco solution: RUN externals/build_draco2017.bat

- 3: Build draco project: Open externals/draco/build/draco.sln and build it.

- 4: Install target maya version on your system. (ex. C:\Program Files\Autodesk\Maya2018 )

- 5: Open solution file: /windows/glTFExporter/glTFExporter.sln

- 6: Select target version from configuration and build it.


## Externals modules

- draco: https://github.com/google/draco/

- glm: https://github.com/g-truc/glm

- picojson: https://github.com/kazuho/picojson/


## License

This software is MIT License.
Copyright (c) 2018 Kashika, Inc.

