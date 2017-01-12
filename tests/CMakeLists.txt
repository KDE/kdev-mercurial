# Due to the use of system() and some unix-style paths this test will only run
# under Linux. (Maybe this can be fixed later)
#
# Moreover, I'm not sure if there is a cvs commandline client for windows
# (need to check this out ...)

if (UNIX)

# Running the test only makes sense if the mercurial command line client
# is present. So check for it before adding the test...
find_program(MERCURIAL NAMES hg)

if (MERCURIAL)
    set( EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR} )
    set(mercurialTest_SRCS
        test_mercurial.cpp
        ../ui/mercurialqueuesmanager.cpp
        ../ui/mercurialheadswidget.cpp
        ../models/mercurialqueueseriesmodel.cpp
        ../models/mercurialheadsmodel.cpp
        ../mercurialannotatejob.cpp
        ../mercurialpushjob.cpp
        ../mercurialvcslocationwidget.cpp
        ../mercurialplugin.cpp)
    ecm_add_test(${mercurialTest_SRCS}
        TEST_NAME test_kdevmercurial
        LINK_LIBRARIES Qt5::Test KDev::Vcs KDev::Util KDev::Tests
        GUI
    )
endif (MERCURIAL)

endif (UNIX)
