project(mercurial)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${php_SOURCE_DIR}/cmake/)

find_package(KDE4 REQUIRED)
find_package(KDevPlatform 0.9.96 REQUIRED)

include_directories(
    ${KDEVPLATFORM_INCLUDE_DIR}
    ${KDE4_INCLUDES}
    ${mercurial_BINARY_DIR}
    ${mercurial_SOURCE_DIR}
)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=9526)
add_subdirectory(tests)
add_subdirectory(icons)

########### next target ###############

set(kdevmercurial_PART_SRCS
    ui/mercurialheadswidget.cpp
    models/mercurialheadsmodel.cpp
    mercurialvcslocationwidget.cpp
    mercurialplugin.cpp
)

set(kdevmercurial_UIS
    ui/mercurialheadswidget.ui
)

kde4_add_ui_files(kdevmercurial_PART_SRCS ${kdevmercurial_UIS})
kde4_add_plugin(kdevmercurial ${kdevmercurial_PART_SRCS})

target_link_libraries(kdevmercurial
    ${KDE4_KDEUI_LIBS}
    ${KDEVPLATFORM_UTIL_LIBRARIES}
    ${KDEVPLATFORM_INTERFACES_LIBRARIES}
    ${KDEVPLATFORM_VCS_LIBRARIES}
    ${KDEVPLATFORM_PROJECT_LIBRARIES}
)

install(TARGETS kdevmercurial DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############
install( FILES kdevmercurial.desktop DESTINATION ${SERVICES_INSTALL_DIR} )
install( FILES kdevmercurial.rc DESTINATION ${DATA_INSTALL_DIR}/kdevmercurial )
install( FILES kdevmercurial-log.style DESTINATION ${DATA_INSTALL_DIR}/kdevmercurial )
add_definitions(-DLOG_STYLE_FILE="\\"${DATA_INSTALL_DIR}/kdevmercurial/kdevmercurial-log.style\\"")

