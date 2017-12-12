cmake_minimum_required(VERSION 3.0)

project(mercurial)

find_package (ECM "5.14.0" REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${mercurial_SOURCE_DIR}/cmake/modules ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDECMakeSettings)
include(ECMAddTests)
include(ECMInstallIcons)
include(ECMQtDeclareLoggingCategory)
include(FeatureSummary)

set(KF5_DEP_VERSION "5.15.0")
find_package(KF5 ${KF5_DEP_VERSION} REQUIRED COMPONENTS
    I18n
    ItemModels
)

set(KDEVPLATFORM_VERSION "5.2.40")
find_package(KDevPlatform ${KDEVPLATFORM_VERSION} CONFIG REQUIRED)

add_definitions(
    -DQT_DEPRECATED_WARNINGS
    -DQT_DISABLE_DEPRECATED_BEFORE=0x050500
    -DQT_NO_URL_CAST_FROM_STRING
    -DQT_STRICT_ITERATORS
    -DQT_USE_FAST_CONCATENATION
    -DQT_USE_FAST_OPERATOR_PLUS
)

check_cxx_compiler_flag(-Wno-missing-field-initializers HAVE_MFI_FLAG)
check_cxx_compiler_flag(-Werror=undefined-bool-conversion HAVE_UBC_FLAG)
check_cxx_compiler_flag(-Werror=tautological-undefined-compare HAVE_TUC_FLAG)
if (HAVE_MFI_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers")
endif()
if (HAVE_UBC_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=undefined-bool-conversion")
endif()
if (HAVE_TUC_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror=tautological-undefined-compare")
endif()

########### next target ###############

ecm_qt_declare_logging_category(kdevmercurial_LOG_SRCS
    HEADER debug.h
    IDENTIFIER PLUGIN_MERCURIAL
    CATEGORY_NAME "kdevplatform.plugins.mercurial"
)

set(kdevmercurial_PART_SRCS
    ui/mercurialqueuesmanager.cpp
    ui/mercurialheadswidget.cpp
    models/mercurialqueueseriesmodel.cpp
    models/mercurialheadsmodel.cpp
    mercurialpushjob.cpp
    mercurialannotatejob.cpp
    mercurialvcslocationwidget.cpp
    mercurialplugin.cpp
    mercurialpluginmetadata.cpp
    mercurialjob.cpp
    ${kdevmercurial_LOG_SRCS}
)

set(kdevmercurial_UIS
    ui/mercurialheadswidget.ui
    ui/mercurialqueuesmanager.ui
)

ki18n_wrap_ui(kdevmercurial_PART_SRCS ${kdevmercurial_UIS})
kdevplatform_add_plugin(kdevmercurial JSON kdevmercurial.json SOURCES ${kdevmercurial_PART_SRCS})

target_link_libraries(kdevmercurial
    KDev::Util
    KDev::Interfaces
    KDev::Vcs
    KDev::Project
)

# process tests subfolder only after ui files have been registered to cmake above
# so that proper dependencies are created for the included generated headers
add_subdirectory(tests)
add_subdirectory(icons)

# kdebugsettings file
install(FILES kdevmercurial.categories DESTINATION ${KDE_INSTALL_CONFDIR})

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
