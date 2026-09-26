if(NOT DEFINED CSX_AIO_EXPECTED_PREFIX OR CSX_AIO_EXPECTED_PREFIX STREQUAL "")
    message(FATAL_ERROR "CSX AIO install reset requires an expected staging prefix")
endif()

# Component installs update one payload and must preserve the rest of staging.
if(DEFINED CMAKE_INSTALL_COMPONENT AND NOT CMAKE_INSTALL_COMPONENT STREQUAL "")
    return()
endif()

if(NOT CMAKE_INSTALL_PREFIX STREQUAL CSX_AIO_EXPECTED_PREFIX)
    message(
        FATAL_ERROR
        "Refusing to install: --prefix must be \"${CSX_AIO_EXPECTED_PREFIX}\" "
        "(got \"${CMAKE_INSTALL_PREFIX}\"). This full install recursively "
        "deletes its entire --prefix directory before staging the AIO package, "
        "so it must never be pointed at a live game install. Use "
        "BuildRelease.bat <PRESET>-WITH-AUTO-DEPLOYMENT or the "
        "DEPLOY_ALL/COPY_SHADERS targets to deploy to a game folder."
    )
endif()

file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}")
