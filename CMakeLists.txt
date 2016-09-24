project(mercurial)

cmake_minimum_required(VERSION 2.8.12)

find_package (ECM 0.0.9 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${mercurial_SOURCE_DIR}/cmake/modules ${ECM_MODULE_PATH})

include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMAddTests)
include(ECMOptionalAddSubdirectory)
include(ECMInstallIcons)
include(ECMSetupVersion)
include(ECMMarkAsTest)
include(ECMMarkNonGuiExecutable)
include(ECMGenerateHeaders)
include(ECMPackageConfigHelpers)
include(GenerateExportHeader)
include(FeatureSummary)
include(WriteBasicConfigVersionFile)
include(CheckFunctionExists)
include(KDEInstallDirs)
include(KDECMakeSettings)

find_package(KF5 REQUIRED COMPONENTS
  Config
  GuiAddons
  WidgetsAddons
  IconThemes
  I18n
  ItemModels
  ItemViews
  JobWidgets
  KCMUtils
  KIO
  Parts
  WindowSystem
  XmlGui)

find_package(KDevPlatform 5.0.0 REQUIRED)

include_directories(
    ${mercurial_BINARY_DIR}
    ${mercurial_SOURCE_DIR}
)

#add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

add_subdirectory(tests)
add_subdirectory(icons)

########### next target ###############

set(kdevmercurial_PART_SRCS
    ui/mercurialqueuesmanager.cpp
    ui/mercurialheadswidget.cpp
    models/mercurialqueueseriesmodel.cpp
    models/mercurialheadsmodel.cpp
    mercurialpushjob.cpp
    mercurialvcslocationwidget.cpp
    mercurialplugin.cpp
    mercurialpluginmetadata.cpp
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

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
