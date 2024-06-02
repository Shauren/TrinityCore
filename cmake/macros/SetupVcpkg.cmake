# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

macro(SetupVcpkg version)
  set(VCPKG_CRT_LINKAGE dynamic)
  set(VCPKG_INSTALL_OPTIONS "--no-print-usage")
  set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)
  set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

  if(NOT EXISTS "${CMAKE_BINARY_DIR}/dep/vcpkg-${version}/scripts/buildsystems/vcpkg.cmake")
    file(DOWNLOAD https://github.com/microsoft/vcpkg/archive/${version}.tar.gz "${CMAKE_BINARY_DIR}/dep/vcpkg.tar.gz")
    file(ARCHIVE_EXTRACT
      INPUT "${CMAKE_BINARY_DIR}/dep/vcpkg.tar.gz"
      DESTINATION "${CMAKE_BINARY_DIR}/dep/"
    )
  endif()

  set(NEW_CMAKE_TOOLCHAIN_FILE "${CMAKE_BINARY_DIR}/dep/vcpkg-${version}/scripts/buildsystems/vcpkg.cmake")
  if (CMAKE_TOOLCHAIN_FILE AND NOT CMAKE_TOOLCHAIN_FILE STREQUAL NEW_CMAKE_TOOLCHAIN_FILE)
    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE ${CMAKE_TOOLCHAIN_FILE} CACHE INTERNAL "")
  endif()
  set(CMAKE_TOOLCHAIN_FILE ${NEW_CMAKE_TOOLCHAIN_FILE} CACHE STRING "")
  set(WITH_VCPKG "1" CACHE INTERNAL "")
endmacro()
