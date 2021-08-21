# This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

CollectSourceFiles(
  ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE_SOURCES)

if (WIN32)
  list(APPEND PRIVATE_SOURCES ${sources_windows})
endif()

GroupSources(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(client-data-extractor
  ${PRIVATE_SOURCES}
)

CollectIncludeDirectories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  PUBLIC_INCLUDES
  # Exclude
  ${CMAKE_CURRENT_SOURCE_DIR}/PrecompiledHeaders)

target_include_directories(client-data-extractor
  PUBLIC
    ${PUBLIC_INCLUDES}
  PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(client-data-extractor
  PRIVATE
    trinity-core-interface
  PUBLIC
    common
    casc)

set_target_properties(client-data-extractor
    PROPERTIES
      FOLDER
        "tools")

if (UNIX)
  install(TARGETS client-data-extractor DESTINATION bin)
elseif(WIN32)
  install(TARGETS client-data-extractor DESTINATION "${CMAKE_INSTALL_PREFIX}")
endif()
