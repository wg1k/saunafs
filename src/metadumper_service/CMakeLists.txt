add_definitions(-DMETARESTORE)
add_definitions(-DMETADUMPER_SERVICE)
add_definitions(-DAPPNAME=saunafs-metadumper -DAPP_EXAMPLES_SUBDIR="${USR_SHARE_DOC}")

aux_source_directory(. METADUMPER_SERVICE_SOURCES)

add_library(metadumper_service_lib "")
target_sources(metadumper_service_lib PRIVATE ${METADUMPER_SERVICE_SOURCES})
target_include_directories(metadumper_service_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(metadumper_service_lib metadumper_lib)

add_executable(saunafs-metadumper ${MAIN_SRC})
target_include_directories(saunafs-metadumper PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(saunafs-metadumper metadumper_service_lib sfscommon ${PAM_LIBRARIES})
if(SYSTEMD_FOUND)
    target_link_libraries(saunafs-metadumper ${SYSTEMD_LIBRARIES})
endif()
install(TARGETS saunafs-metadumper RUNTIME DESTINATION ${SBIN_SUBDIR})
