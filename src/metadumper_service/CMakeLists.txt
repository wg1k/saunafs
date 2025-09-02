include_directories(${CMAKE_CURRENT_SOURCE_DIR} ../master)
add_definitions(-DMETARESTORE)
add_definitions(-DAPPNAME=saunafs-metadumper -DAPP_EXAMPLES_SUBDIR="${USR_SHARE_DOC}")

collect_sources(METADUMPER_SERVICE)

# Add file_transfer.cc to the sources
list(APPEND METADUMPER_SERVICE_SOURCES file_transfer.cc)

# Include merger.cc from metarestore to resolve undefined references
list(APPEND METADUMPER_SERVICE_SOURCES ../metarestore/merger.cc)

file(GLOB METADUMPER_SERVICE_MASTER_SOURCES ../master/filesystem*.cc)

if(DB_FOUND)
    file(GLOB METADUMPER_SERVICE_HSTRING_SOURCES ../master/hstring_*storage.cc)
else()
    file(GLOB METADUMPER_SERVICE_HSTRING_SOURCES ../master/hstring_memstorage.cc)
endif()

file(GLOB METADUMPER_SERVICE_BACKEND_SOURCES ../master/metadata_backend*.cc)

add_library(metadumper_service_lib ${METADUMPER_SERVICE_SOURCES} ${METADUMPER_SERVICE_MASTER_SOURCES} ${METADUMPER_SERVICE_HSTRING_SOURCES}
    ${METADUMPER_SERVICE_BACKEND_SOURCES}
    ../master/acl_storage.cc ../master/chunks.cc ../common/quota_database.cc ../master/chunk_goal_counters.cc
    ../master/restore.cc ../master/locks.cc ../master/task_manager.cc ../master/snapshot_task.cc
    ../master/metadata_loader.cc ../master/setgoal_task.cc ../master/settrashtime_task.cc
    ../master/changelog.cc)

target_link_libraries(metadumper_service_lib sfscommon metadumper_lib)
if(JUDY_LIBRARY)
    target_link_libraries(metadumper_service_lib ${JUDY_LIBRARY})
endif()

add_executable(saunafs-metadumper ${METADUMPER_SERVICE_MAIN})
target_link_libraries(saunafs-metadumper metadumper_service_lib sfscommon ${PAM_LIBRARIES})
install(TARGETS saunafs-metadumper RUNTIME DESTINATION ${SBIN_SUBDIR})
