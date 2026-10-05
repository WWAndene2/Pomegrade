# Pokemon remake tooling (Pomegrade): reads DS and 3DS game data so a DS game
# (Pokemon Platinum) can be rebuilt in a 3DS one (Omega Ruby). Plain C++17,
# no dependencies; builds on the desktop.
#   cmake -S tools/remake -B build-remake -G Ninja && ninja -C build-remake
#   ./build-remake/remake_tool inventory platinum.nds > inventory.json
cmake_minimum_required(VERSION 3.16)
project(remake_tools C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MELONDS_SRC ${CMAKE_CURRENT_LIST_DIR}/../../melonDS-android/melonDS-android-lib/src)

add_library(remake STATIC
    src/Bytes.cpp
    src/NdsRom.cpp
    src/Narc.cpp
    src/NitroCompression.cpp
    src/FormatSniffer.cpp
    src/Inventory.cpp
    src/Garc.cpp
    src/Png.cpp
    src/NitroTexture.cpp
    src/GxDisplayList.cpp
    src/Gltf.cpp
    src/NitroDictionary.cpp
    src/Nsbtx.cpp
    src/Nsbmd.cpp
    src/EmulatorTextureName.cpp
    src/TextureIndex.cpp
    src/MapMatrix.cpp
    src/LandData.cpp
    src/WorldMap.cpp
    src/N3dsWorld.cpp
    src/PlatinumWorld.cpp
    src/TerrainScan.cpp
    src/TownLayout.cpp
    src/TownShapes.cpp
    src/TopView.cpp
    src/OrasMeasure.cpp
    src/TownBuilder.cpp
    src/OrasTown.cpp
    src/OrasZone.cpp
    src/OrasInspect.cpp
    src/TownCheck.cpp
    src/Amx.cpp
    src/MapHeaders.cpp
    src/ZoneEvents.cpp
    src/BinLinker.cpp
    src/N3dsRom.cpp
    src/PicaCommands.cpp
    src/PicaTexture.cpp
    src/Bch.cpp
    src/BchWriter.cpp
    src/BchTextureFile.cpp
    src/Bps.cpp
    # the DS core's hash, so texture names match the emulator's by construction
    ${MELONDS_SRC}/xxhash/xxhash.c
)
target_include_directories(remake PUBLIC src PRIVATE ${MELONDS_SRC})

add_executable(remake_tool src/RemakeTool.cpp)
target_link_libraries(remake_tool PRIVATE remake)

# The Sinnoh test mod's prototypes (Twinleaf over Littleroot): off by default, see prototype/README.md
option(REMAKE_PROTOTYPES "Build the remake prototypes in prototype/" OFF)
if(REMAKE_PROTOTYPES)
    foreach(prototype TwinleafBuild ZoneWarps SliceGarc BpsMake PiecePreview)
        add_executable(${prototype} prototype/${prototype}.cpp)
        target_link_libraries(${prototype} PRIVATE remake)
    endforeach()
endif()
