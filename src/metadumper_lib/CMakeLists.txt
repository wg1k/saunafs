include_directories(${CMAKE_CURRENT_SOURCE_DIR})
aux_source_directory(. METADUMPLIB_SOURCES)
add_library(metadumper_lib ${METADUMPLIB_SOURCES})
target_link_libraries(metadumper_lib sfscommon)
