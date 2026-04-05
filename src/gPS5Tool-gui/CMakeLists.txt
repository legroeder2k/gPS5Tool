pkg_check_modules(GTKMM_VARS REQUIRED IMPORTED_TARGET gtkmm-4.0)
pkg_check_modules(LIBSYSTEMD_VARS REQUIRED IMPORTED_TARGET libsystemd)
pkg_check_modules(LIBSQ3_VARS REQUIRED IMPORTED_TARGET sqlite3)
pkg_check_modules(LIBCURL_VARS REQUIRED IMPORTED_TARGET libcurl)

find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)
find_package(OpenSSL REQUIRED)


set(GRESOURCE_XML ${CMAKE_CURRENT_SOURCE_DIR}/resources.gresources.xml)
set(GRESOURCE_C   ${CMAKE_CURRENT_BINARY_DIR}/resources.c)

add_custom_command(
        OUTPUT ${GRESOURCE_C}
        COMMAND ${GLIB_COMPILE_RESOURCES} ${GRESOURCE_XML} --target=${GRESOURCE_C} --generate-source --sourcedir=${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${GRESOURCE_XML} ${CMAKE_CURRENT_SOURCE_DIR}/ui/mainWindow.ui ${CMAKE_CURRENT_SOURCE_DIR}/ui/portChooser.ui
        VERBATIM
)

set(SOURCE  main.cpp
            MainApplication.cpp
            PortChooser.cpp
            data/CodeDatabase.cpp
            flash/NorFlashFile.cpp
            serial/SerialPort.cpp
            serial/CommunicationLayer.cpp
)

# Enable -Wall -Wextra and all the good things
add_compile_options(-Wall -Wextra -Werror)

add_executable(gPS5Tool ${SOURCE} ${GRESOURCE_C})

target_link_libraries(gPS5Tool PRIVATE PkgConfig::GTKMM_VARS PkgConfig::LIBSYSTEMD_VARS PkgConfig::LIBSQ3_VARS PkgConfig::LIBCURL_VARS OpenSSL::Crypto)
target_include_directories(gPS5Tool PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
