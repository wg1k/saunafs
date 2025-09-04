add_definitions(-DMETARESTORE)
add_definitions(-DMETADUMPER_SERVICE)
add_definitions(-DAPPNAME=sfsmetadumper -DAPP_EXAMPLES_SUBDIR="${METADUMPER_EXAMPLES_SUBDIR}")

aux_source_directory(. METADUMPER_SERVICE_SOURCES)

add_library(metadumper_service_lib "")
target_sources(metadumper_service_lib PRIVATE ${METADUMPER_SERVICE_SOURCES})
target_include_directories(metadumper_service_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(metadumper_service_lib metadumper_lib)

add_executable(sfsmetadumper ${MAIN_SRC})
target_include_directories(sfsmetadumper PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(sfsmetadumper metadumper_service_lib sfscommon ${PAM_LIBRARIES})
if(SYSTEMD_FOUND)
    target_link_libraries(sfsmetadumper ${SYSTEMD_LIBRARIES})
endif()

configure_file(saunafs-metadumper.service.in saunafs-metadumper.service @ONLY)

install(TARGETS sfsmetadumper
    DESTINATION ${SBIN_SUBDIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/saunafs-metadumper.service
    DESTINATION /usr/lib/systemd/system
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

install(DIRECTORY
    DESTINATION ${RUN_SUBDIR}/saunafs
    DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
