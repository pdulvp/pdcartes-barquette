cmake_minimum_required(VERSION 3.16)

# La version : trois nombres dans un fichier, avancés par les tâches de bump.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" ATELIER_VERSION)
string(STRIP "${ATELIER_VERSION}" ATELIER_VERSION)

project(cardset-creator VERSION ${ATELIER_VERSION} LANGUAGES CXX)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

# Le format, la liste des jeux installés et le style partagé.
add_subdirectory(lib)

# Quel build c'est. Écrit à chaque build plutôt qu'à chaque configure, pour que
# la version sur la ligne de commande et sur le paquet soit le build qui est
# devant vous ; voir cmake/GenerateVersion.cmake.
set(BUILD_VERSION_CPP ${CMAKE_CURRENT_BINARY_DIR}/BuildVersion.cpp)
set(BUILD_VERSION_TXT ${CMAKE_CURRENT_BINARY_DIR}/BuildVersion.txt)
set(GENERATE_VERSION ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateVersion.cmake)

execute_process(COMMAND ${CMAKE_COMMAND} -DVERSION=${ATELIER_VERSION}
    -DOUT_CPP=${BUILD_VERSION_CPP} -DOUT_TXT=${BUILD_VERSION_TXT} -P ${GENERATE_VERSION})

add_custom_target(build-version ALL
    BYPRODUCTS ${BUILD_VERSION_CPP} ${BUILD_VERSION_TXT}
    COMMAND ${CMAKE_COMMAND} -DVERSION=${ATELIER_VERSION}
        -DOUT_CPP=${BUILD_VERSION_CPP} -DOUT_TXT=${BUILD_VERSION_TXT} -P ${GENERATE_VERSION}
    COMMENT "Stamping the build with the time it was made")
set_source_files_properties(${BUILD_VERSION_CPP} PROPERTIES GENERATED TRUE)

add_executable(cardset-creator
    ${BUILD_VERSION_CPP}
    src/Version.h
    src/main.cpp
    src/AtelierWindow.cpp
    src/AtelierWindow.h
    src/FaceGrid.cpp
    src/FaceGrid.h
    src/ImageJoin.cpp
    src/ImageJoin.h
    src/SourceTray.cpp
    src/SourceTray.h
    src/SplitDialog.cpp
    src/SplitDialog.h
    src/HelpDialog.cpp
    src/HelpDialog.h
)

add_dependencies(cardset-creator build-version)
target_link_libraries(cardset-creator PRIVATE cartotheque Qt6::Widgets)

install(TARGETS cardset-creator RUNTIME DESTINATION bin)
install(FILES data/cardset-creator.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "cardset-creator")
set(CPACK_PACKAGE_VENDOR "cardset-creator")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "L'atelier des jeux de cartes")
set(CPACK_PACKAGE_DESCRIPTION
    "De quoi fabriquer un jeu de cartes pour le rami et la belote de la maison :
on y dépose des images, ou on y ouvre un DLL pour Windows, on range les
faces et les dos, et il en sort un fichier unique à déposer dans le dossier de
configuration.")
set(CPACK_PACKAGE_CONTACT "pdulvp <pdulvp@laposte.net>")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_STRIP_FILES TRUE)
# Le greffon qui lit les SVG : sans lui, un jeu vectoriel ne s'affiche pas.
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-svg-plugins")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_BUILD_VERSION_FILE "${BUILD_VERSION_TXT}")
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPackOptions.cmake")
set(CPACK_PACKAGE_VERSION "${ATELIER_VERSION}")
include(CPack)

add_custom_target(deb
    COMMAND ${CMAKE_COMMAND} -E env CARTOTHEQUE_KEEP_VERSION=1
        ${CMAKE_CPACK_COMMAND} -G DEB --config ${CMAKE_CURRENT_BINARY_DIR}/CPackConfig.cmake
    DEPENDS cardset-creator
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Building the Debian package")
