# Ugly hack to make find_package/find_library find static libs first
if(APPLE)
  set(CMAKE_FIND_LIBRARY_SUFFIXES .a .dylib)
  set(CMAKE_FIND_FRAMEWORK LAST)
  set(SHARED_LIB_EXT .dylib)
elseif(UNIX)
  set(CMAKE_FIND_LIBRARY_SUFFIXES .a .so)
  set(SHARED_LIB_EXT .so)
else()
  set(CMAKE_FIND_LIBRARY_SUFFIXES .lib .dll)
  set(SHARED_LIB_EXT .dll)
endif()
  
# SDL2/SDL2_ttf
if(UNIX)
  find_package(SDL2)
  if(NOT SDL2_FOUND)
    find_path(SDL2_INCLUDE_DIRS SDL.h
	  REQUIRED
	  HINTS
	  PATH_SUFFIXES include include/SDL2
    )
    find_library(SDL2_LIBRARY
      REQUIRED
   	  NAMES SDL2
   	  HINTS
   	  PATH_SUFFIXES lib
    )
  set(SDL2_LIBRARIES ${SDL2_LIBRARY})
  endif()
  message(STATUS "Found SDL2 headers: ${SDL2_INCLUDE_DIRS}")
  message(STATUS "Found SDL2 library: ${SDL2_LIBRARIES}")
  include_directories(${SDL2_INCLUDE_DIRS})
  find_path(SDL2_TTF_INCLUDE_DIR SDL_ttf.h
	REQUIRED
	HINTS
	PATH_SUFFIXES include include/SDL2
  )
  find_library(SDL2_TTF_LIBRARY
    REQUIRED
   	NAMES SDL2_ttf
   	HINTS
   	PATH_SUFFIXES lib
  )
  message(STATUS "Found SDL2_TTF headers: ${SDL2_TTF_INCLUDE_DIR}")
  message(STATUS "Found SDL2_TTF library: ${SDL2_TTF_LIBRARY}")  
  include_directories(${SDL2_TTF_INCLUDE_DIR})
  find_library(FREETYPE_LIBRARY
    REQUIRED
   	NAMES libfreetype freetype
   	HINTS
   	PATH_SUFFIXES lib
  )
  set(FREETYPE_LIBRARIES "${FREETYPE_LIBRARY}")
  message(STATUS "Found FreeType library: ${FREETYPE_LIBRARIES}")
endif()

# zlib
find_package(ZLIB REQUIRED)
message(STATUS "Found zlib headers: ${ZLIB_INCLUDE_DIR}")
message(STATUS "Found zlib library: ${ZLIB_LIBRARY}")
include_directories(${ZLIB_INCLUDE_DIR})


# libpng
find_package(PNG REQUIRED)
message(STATUS "Found libpng headers: ${PNG_INCLUDE_DIR}")
message(STATUS "Found libpng library: ${PNG_LIBRARY}")
include_directories(${PNG_INCLUDE_DIR})

# mpg123
find_path(MPG123_INCLUDE_DIRS mpg123.h
  REQUIRED
  HINTS
  PATH_SUFFIXES include
)
find_library(MPG123_LIBRARY_RELEASE
  REQUIRED
  NAMES mpg123
  HINTS
  PATH_SUFFIXES lib
)
find_library(MPG123_LIBRARY_DEBUG
  NAMES "mpg123${CMAKE_DEBUG_POSTFIX}"
  HINTS
  PATH_SUFFIXES lib
)
select_library_configurations(MPG123)
message(STATUS "Found libmpg123 headers: ${MPG123_INCLUDE_DIRS}")
message(STATUS "Found libmpg123 library: ${MPG123_LIBRARIES}")
include_directories(${MPG123_INCLUDE_DIRS})

# libogg
find_package(Ogg REQUIRED)
message(STATUS "Found libogg headers: ${OGG_INCLUDE_DIRS}")
message(STATUS "Found libogg library: ${OGG_LIBRARIES}")
include_directories(${OGG_INCLUDE_DIRS})

# libvorbis
find_path(VORBIS_INCLUDE_DIR vorbisfile.h
  REQUIRED
  HINTS
  PATH_SUFFIXES include include/vorbis
)
find_library(VORBIS_LIBRARY_RELEASE
  REQUIRED
  NAMES vorbis
  HINTS
  PATH_SUFFIXES lib
)
find_library(VORBIS_LIBRARY_DEBUG
  NAMES "vorbis${CMAKE_DEBUG_POSTFIX}"
  HINTS
  PATH_SUFFIXES lib
)
select_library_configurations(VORBIS)

find_library(VORBISFILE_LIBRARY_RELEASE
  REQUIRED
  NAMES vorbisfile
  HINTS
  PATH_SUFFIXES lib
)
find_library(VORBISFILE_LIBRARY_DEBUG
  NAMES "vorbisfile${CMAKE_DEBUG_POSTFIX}"
  HINTS
  PATH_SUFFIXES lib
)
select_library_configurations(VORBISFILE)

find_library(VORBISENC_LIBRARY_RELEASE
  REQUIRED
  NAMES vorbisenc
  HINTS
  PATH_SUFFIXES lib
)
find_library(VORBISENC_LIBRARY_DEBUG
  NAMES "vorbisenc${CMAKE_DEBUG_POSTFIX}"
  HINTS
  PATH_SUFFIXES lib
)
select_library_configurations(VORBISENC)
message(STATUS "Found libvorbis headers: ${VORBIS_INCLUDE_DIR}")
message(STATUS "Found libvorbis libraries: ${VORBIS_LIBRARIES} ${VORBISFILE_LIBRARIES} ${VORBISENC_LIBRARIES}")
include_directories(${VORBIS_INCLUDE_DIR})

# flac - we don't use this directly but alure links to it
find_library(FLAC_LIBRARY_RELEASE
  REQUIRED
  NAMES FLAC
  HINTS
  PATH_SUFFIXES lib
)
find_library(FLAC_LIBRARY_DEBUG
  NAMES "FLAC${CMAKE_DEBUG_POSTFIX}"
  HINTS
  PATH_SUFFIXES lib
)
select_library_configurations(FLAC)
message(STATUS "Found flac library: ${FLAC_LIBRARY}")

# openal-soft
find_path(OPENAL_INCLUDE_DIRS al.h
  REQUIRED
  HINTS
  PATH_SUFFIXES include include/AL
)
find_library(OPENAL_LIBRARY_RELEASE
  REQUIRED
  NAMES OpenAL32 openal
  HINTS
  PATH_SUFFIXES lib
)
find_library(OPENAL_LIBRARY_DEBUG
  NAMES "OpenAL32${CMAKE_DEBUG_POSTFIX}" "openal${CMAKE_DEBUG_POSTFIX}"
  HINTS
  PATH_SUFFIXES lib
)
select_library_configurations(OPENAL)
message(STATUS "Found openal headers: ${OPENAL_INCLUDE_DIRS}")
message(STATUS "Found openal library: ${OPENAL_LIBRARIES}")
include_directories(${OPENAL_INCLUDE_DIRS})

# galaxy
if(WINDOWS)
  find_path(AKELEDIT_INCLUDE_DIR AkelEdit.h
	REQUIRED
	HINTS
	PATH_SUFFIXES include
  )
  find_file(AKELEDIT_LIBRARY AkelEdit.dll
	REQUIRED
	NAMES AkelEdit
	HINTS
	PATH_SUFFIXES bin
  )
  message(STATUS "Found AkelEdit header: ${AKELEDIT_INCLUDE_DIR}")
  message(STATUS "Found AkelEdit library: ${AKELEDIT_LIBRARY}")
  include_directories(${AKELEDIT_INCLUDE_DIR})
endif()

# alure
find_path(ALURE_INCLUDE_DIR alure.h
  REQUIRED
  HINTS
  PATH_SUFFIXES include include/AL include/OpenAL
)
find_library(ALURE_LIBRARY_RELEASE
  REQUIRED
  NAMES alure-static ALURE32-STATIC
  HINTS
  PATH_SUFFIXES lib
)
find_library(ALURE_LIBRARY_DEBUG
  NAMES "alure-static${CMAKE_DEBUG_POSTFIX}" "ALURE32-STATIC${CMAKE_DEBUG_POSTFIX}"
  HINTS
  PATH_SUFFIXES lib
)
select_library_configurations(ALURE)
message(STATUS "Found alure headers: ${ALURE_INCLUDE_DIR}")
message(STATUS "Found alure library: ${ALURE_LIBRARIES}")
include_directories(${ALURE_INCLUDE_DIR})

# libxmp
find_path(XMP_INCLUDE_DIR xmp.h
  REQUIRED
  HINTS
  PATH_SUFFIXES include
)
find_library(XMP_LIBRARY_RELEASE
  REQUIRED
  NAMES xmp libxmp
  HINTS
  PATH_SUFFIXES lib
)
find_library(XMP_LIBRARY_DEBUG
  NAMES "xmp${CMAKE_DEBUG_POSTFIX}" "libxmp${CMAKE_DEBUG_POSTFIX}"
  HINTS
  PATH_SUFFIXES lib
)
select_library_configurations(XMP)
message(STATUS "Found xmp headers: ${XMP_INCLUDE_DIR}")
message(STATUS "Found xmp library: ${XMP_LIBRARIES}")
include_directories(${XMP_INCLUDE_DIR})

# ktexcomp
find_path(KTEXCOMP_INCLUDE_DIR BC.h
  HINTS
  PATH_SUFFIXES include
)
find_library(KTEXCOMP_LIBRARY_RELEASE
  NAMES KTexComp
  HINTS
  CONFIG
  PATH_SUFFIXES lib
)
find_library(KTEXCOMP_LIBRARY_DEBUG
  NAMES "KTexComp${CMAKE_DEBUG_POSTFIX}"
  HINTS
  CONFIG
  PATH_SUFFIXES lib
)
if(KTEXCOMP_LIBRARY_RELEASE)
  select_library_configurations(KTEXCOMP)
  message(STATUS "Found KTexComp headers: ${KTEXCOMP_INCLUDE_DIR}")
  message(STATUS "Found KTexComp library: ${KTEXCOMP_LIBRARIES}")
  include_directories(${KTEXCOMP_INCLUDE_DIR})
endif()

if(WINDOWS)
  set(FMOD_INSTALL_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/External/fmod/lib/${OLDUNREAL_CPU}/fmod.dll)
  set(FMOD_LINK_LIBRARY
    ${CMAKE_CURRENT_SOURCE_DIR}/External/fmod/lib/${OLDUNREAL_CPU}/fmod_vc.lib)
else()
  set(FMOD_INSTALL_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/External/fmod/lib/${OLDUNREAL_CPU}/libfmod${SHARED_LIB_EXT})
  set(FMOD_LINK_LIBRARY ${FMOD_INSTALL_LIBRARY})
endif()

# Header-only or precompiled stuff
include_directories(External/glm)
include_directories(External/fmod/inc)
include_directories(External/curl/include)
if(WINDOWS)
  include_directories(External/dxsdk/Include)
elseif(APPLE)
  include_directories(External/metal-cpp)
endif()

# Platform/distro packages and libs
if(LINUX)
  find_package(Threads REQUIRED)
  if (OLDUNREAL_BUILD_WX_LAUNCHER)
    set(wxWidgets_CONFIGURATION mswu)
    find_package(wxWidgets COMPONENTS core base REQUIRED)
    include(${wxWidgets_USE_FILE})
  endif()
elseif(WINDOWS)
  find_package(Threads REQUIRED)
elseif(APPLE)
  find_package(Threads REQUIRED)
  find_library(COCOA_FRAMEWORK Cocoa)
  find_library(METAL_FRAMEWORK Metal)
  find_library(FOUNDATION_FRAMEWORK Foundation)
  find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
  find_library(METALKIT_FRAMEWORK MetalKit)
endif()
