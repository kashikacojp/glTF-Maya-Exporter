cmake_minimum_required(VERSION 3.2.3)

#Use solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

project("glTF-Maya-Exporter")

# -- options ----------------------------------------------------
option(GLTF_MAYA_EXPORTER_BUILD_WITH_DRACO          "Build with Draco"             ON)
option(GLTF_MAYA_EXPORTER_BUILD_MAYA_GLTF_EXPORTER  "Build Maya glTF Exporter"     ON)
option(GLTF_MAYA_EXPORTER_BUILD_MAYA_VRM_EXPORTER   "Build Maya VRM Exporter"      ON)
option(GLTF_MAYA_EXPORTER_BUILD_CPP_INTERFACE       "Build cpp interface and test" OFF)

if (WIN32)
    SET(GLTF_MAYA_EXPORTER_MAYA_PATH "C:/Program Files/Autodesk/Maya2018" CACHE PATH "Installed Maya Directory")
elseif(APPLE)
    SET(GLTF_MAYA_EXPORTER_MAYA_PATH "/Applications/Autodesk/maya2018" CACHE PATH "Installed Maya Directory")
endif()
# ===============================================================

set(CMAKE_POSITION_INDEPENDENT_CODE On)

# -- draco setting ---------------------------------------------- 
if(GLTF_MAYA_EXPORTER_BUILD_WITH_DRACO)
    # DRACO setting
    add_subdirectory(externals/draco)

    ## beautify ME!
    set_target_properties(draco PROPERTIES FOLDER Draco)
    set_target_properties(draco_attributes PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_attributes_dec PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_attributes_enc PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_attributes_pred_schemes_dec PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_attributes_pred_schemes_enc PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_decode PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_encode PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_mesh_dec PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_mesh_enc PROPERTIES FOLDER Draco)
    set_target_properties(draco_core PROPERTIES FOLDER Draco)
    set_target_properties(draco_core_bit_coders PROPERTIES FOLDER Draco)
    set_target_properties(draco_dec_config PROPERTIES FOLDER Draco)
    set_target_properties(draco_decoder PROPERTIES FOLDER Draco)
    set_target_properties(draco_enc_config PROPERTIES FOLDER Draco)
    set_target_properties(draco_encoder PROPERTIES FOLDER Draco)
    set_target_properties(draco_io PROPERTIES FOLDER Draco)
    set_target_properties(draco_mesh PROPERTIES FOLDER Draco)
    set_target_properties(draco_metadata PROPERTIES FOLDER Draco)
    set_target_properties(draco_metadata_dec PROPERTIES FOLDER Draco)
    set_target_properties(draco_metadata_enc PROPERTIES FOLDER Draco)
    set_target_properties(draco_point_cloud PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_point_cloud_dec PROPERTIES FOLDER Draco)
    set_target_properties(draco_compression_point_cloud_enc PROPERTIES FOLDER Draco)
    set_target_properties(draco_points_dec PROPERTIES FOLDER Draco)
    set_target_properties(draco_points_enc PROPERTIES FOLDER Draco)
    set_target_properties(dracodec PROPERTIES FOLDER Draco)
    set_target_properties(dracoenc PROPERTIES FOLDER Draco)

    set(DRACO_LIB draco)
    add_definitions(-DENABLE_BUILD_WITH_DRACO)

    set(DRACO_SRC_PATH externals/draco/src/)
endif()



if(GLTF_MAYA_EXPORTER_BUILD_MAYA_GLTF_EXPORTER OR GLTF_MAYA_EXPORTER_BUILD_MAYA_VRM_EXPORTER)
    if (WIN32)
        SET(MAYA_INCLUDE_PATH ${GLTF_MAYA_EXPORTER_MAYA_PATH}/include/)
        SET(MAYA_LIBRARY_PATH ${GLTF_MAYA_EXPORTER_MAYA_PATH}/lib/)
    elseif(APPLE)
        SET(MAYA_INCLUDE_PATH ${GLTF_MAYA_EXPORTER_MAYA_PATH}/include/)
        SET(MAYA_LIBRARY_PATH ${GLTF_MAYA_EXPORTER_MAYA_PATH}/Maya.app/Contents/MacOS)
    elseif(UNIX)
        SET(MAYA_INCLUDE_PATH ${GLTF_MAYA_EXPORTER_MAYA_PATH}/include/)
        SET(MAYA_LIBRARY_PATH ${GLTF_MAYA_EXPORTER_MAYA_PATH}/lib/)
    endif()

    set(BUILD_DIR ${CMAKE_BINARY_DIR})
    message(BUILD_DIR: "${BUILD_DIR}")
    message(MAYA_INCLUDE_PATH: "${MAYA_INCLUDE_PATH}")
    message(MAYA_LIBRARY_PATH: "${MAYA_LIBRARY_PATH}")

endif()

# -- compile options, include and libraries -----------------

set (CMAKE_CXX_STANDARD 11)

include_directories(
    ${CMAKE_BINARY_DIR}
    externals/glm/
    externals/
    src/
    ${BUILD_DIR}/
    ${DRACO_SRC_PATH}
    ${MAYA_INCLUDE_PATH}
)

link_directories(${MAYA_LIBRARY_PATH})

# -----------------------------------------------------------

# -- KIL lib ------------------------------------
add_library( kil STATIC
    ./src/kil/CopyTextureFile.cpp
    ./src/kil/CopyTextureFile_GdiPlus.cpp
    ./src/kil/CopyTextureFile_STB.cpp
    ./src/kil/ResizeTextureFile.cpp
)

# -- KML lib ------------------------------------
if(WIN32)
    set(Compatibility ./src/kml/Compatibility.cpp)
endif()

add_library( kml STATIC
    ./src/kml/Bound.cpp
    ./src/kml/CalculateBound.cpp
    ./src/kml/CalculateNormalsMesh.cpp
    ${Compatibility}
    ./src/kml/FlatIndicesMesh.cpp
    ./src/kml/GLTF2GLB.cpp
    ./src/kml/glTFExporter.cpp
    ./src/kml/Material.cpp
    ./src/kml/Mesh.cpp
    ./src/kml/Node.cpp
    ./src/kml/NodeExporter.cpp
    ./src/kml/Options.cpp
    ./src/kml/SaveToDraco.cpp
    ./src/kml/SplitNodeByMaterialID.cpp
    ./src/kml/Transform.cpp
    ./src/kml/TriangulateMesh.cpp
)

target_link_libraries( kml 
                       kil 
                       ${DRACO_LIB})

# force define for maya header ifdef condition (building for makefile project)
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_WIN32 -DNT_PLUGIN")
elseif(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOSMac_")
elseif(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Dlinux -m64 -g -pthread -D_BOOL -DLINUX -DLINUX_64 -DFUNCPROTO -DREQUIRE_IOSTREAM -fPIC -Wno-deprecated -fno-gnu-keywords")
endif()

# -- glTF Exporter for maya --------------------
if (GLTF_MAYA_EXPORTER_BUILD_MAYA_GLTF_EXPORTER)

    add_library( glTFExporter SHARED
        ./src/glTFExporter/glTFExporterRegister.cpp
        ./src/glTFExporter/glTFExporter.cpp
        ./src/glTFExporter/glTFTranslator.cpp
        ./src/glTFExporter/murmur3.c
        ./src/glTFExporter/ProgressWindow.cpp
    )

    target_link_libraries( glTFExporter
                        kml
                        OpenMaya
                        OpenMayaAnim
                        OpenMayaUI
                        Foundation
    )
    SET_TARGET_PROPERTIES(glTFExporter PROPERTIES PREFIX "")
    if(WIN32)
      SET_TARGET_PROPERTIES(glTFExporter PROPERTIES SUFFIX ".mll")
    elseif(APPLE)
      SET_TARGET_PROPERTIES(glTFExporter PROPERTIES SUFFIX ".bundle")
    else()
      SET_TARGET_PROPERTIES(glTFExporter PROPERTIES SUFFIX ".so")
      SET_TARGET_PROPERTIES(glTFExporter PROPERTIES LINK_FLAGS "-shared -m64 -g -pthread -pipe -D_BOOL -DLINUX -DLINUX_64 -DFUNCPROTO -DREQUIRE_IOSTREAM -fPIC -Wno-deprecated -fno-gnu-keywords -Wl,-Bsymbolic")
    endif()


    # [VisualStudio] default startup
    if (WIN32)
    if (CMAKE_VERSION VERSION_GREATER 3.6.0)
        set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT glTFExporter)
    endif()
    endif()
endif()

# -- VRM Exporter for maya --------------------
if (GLTF_MAYA_EXPORTER_BUILD_MAYA_VRM_EXPORTER)
    
    add_library( vrmExporter SHARED
        ./src/glTFExporter/vrmExporterRegister.cpp
        ./src/glTFExporter/glTFExporter.cpp
        ./src/glTFExporter/glTFTranslator.cpp
        ./src/glTFExporter/murmur3.c
        ./src/glTFExporter/ProgressWindow.cpp
    )

	target_compile_definitions(vrmExporter PUBLIC ENABLE_VRM=1)

    target_link_libraries( vrmExporter
                        kml
                        OpenMaya
                        OpenMayaAnim
                        OpenMayaUI
                        Foundation
    )
    SET_TARGET_PROPERTIES(vrmExporter PROPERTIES PREFIX "")
    if(WIN32)
      SET_TARGET_PROPERTIES(vrmExporter PROPERTIES SUFFIX ".mll")
    elseif(APPLE)
      SET_TARGET_PROPERTIES(vrmExporter PROPERTIES SUFFIX ".bundle")
    else()
      SET_TARGET_PROPERTIES(vrmExporter PROPERTIES SUFFIX ".so")
      SET_TARGET_PROPERTIES(vrmExporter PROPERTIES LINK_FLAGS "-shared -m64 -g -pthread -pipe -D_BOOL -DLINUX -DLINUX_64 -DFUNCPROTO -DREQUIRE_IOSTREAM -fPIC -Wno-deprecated -fno-gnu-keywords -Wl,-Bsymbolic")
    endif()

endif()

# -- cpp interface and test -----------------------------

if (GLTF_MAYA_EXPORTER_BUILD_CPP_INTERFACE)

    # -- dll interface --
    add_library( glTFCppExporterLib SHARED
        ./src/cppexporter/main.cpp
    )
    if (ANDROID)
        set(androidlibs
            log
            m)
    endif()
    target_link_libraries( glTFCppExporterLib
                           kml
                           ${androidlibs}
    )

    # -- interface test ---
    add_executable( glTFCppExporterLibTest
        ./src/cppexporter/test/test.cpp
    )
    target_link_libraries( glTFCppExporterLibTest
                           glTFCppExporterLib
    )

    # [VisualStudio] default startup
    if (WIN32)
    if (CMAKE_VERSION VERSION_GREATER 3.6.0)
        set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT glTFCppExporterLibTest)
    endif()
    endif()
endif()
