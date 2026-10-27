cmake_minimum_required(VERSION 3.16)

# La version : trois nombres dans un fichier, incrémentés par les cibles de
# bump.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" ATELIER_VERSION)
string(STRIP "${ATELIER_VERSION}" ATELIER_VERSION)

project(pdcartes-barquette VERSION ${ATELIER_VERSION} LANGUAGES CXX)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)

# La bibliothèque partagée : le format, la liste des jeux installés et le
# style commun.
add_subdirectory(lib)

# L'identification du build. Elle est écrite à chaque build, et non à chaque
# configuration, afin que la version affichée par le programme et celle du
# paquet soient bien celles du binaire courant ; voir
# cmake/GenerateVersion.cmake.
set(BUILD_VERSION_CPP ${CMAKE_CURRENT_BINARY_DIR}/BuildVersion.cpp)
set(BUILD_VERSION_TXT ${CMAKE_CURRENT_BINARY_DIR}/BuildVersion.txt)
set(GENERATE_VERSION ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateVersion.cmake)

execute_process(COMMAND ${CMAKE_COMMAND} -DVERSION=${ATELIER_VERSION}
    -DOUT_CPP=${BUILD_VERSION_CPP} -DOUT_TXT=${BUILD_VERSION_TXT} -P ${GENERATE_VERSION})

add_custom_target(build-version ALL
    BYPRODUCTS ${BUILD_VERSION_CPP} ${BUILD_VERSION_TXT}
    COMMAND ${CMAKE_COMMAND} -DVERSION=${ATELIER_VERSION}
        -DOUT_CPP=${BUILD_VERSION_CPP} -DOUT_TXT=${BUILD_VERSION_TXT} -P ${GENERATE_VERSION}
    COMMENT "Horodatage du build")
set_source_files_properties(${BUILD_VERSION_CPP} PROPERTIES GENERATED TRUE)

add_executable(pdcartes-barquette
    ${BUILD_VERSION_CPP}
    src/Version.h
    src/main.cpp
    src/AtelierWindow.cpp
    src/AtelierWindow.h
    src/CardPreview.cpp
    src/CardPreview.h
    src/CardStyleEditor.cpp
    src/CardStyleEditor.h
    src/PlaceArtDialog.cpp
    src/PlaceArtDialog.h
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

add_dependencies(pdcartes-barquette build-version)
target_link_libraries(pdcartes-barquette PRIVATE cartotheque Qt6::Widgets)

install(TARGETS pdcartes-barquette RUNTIME DESTINATION bin)
install(FILES data/pdcartes-barquette.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "pdcartes-barquette")
set(CPACK_PACKAGE_VENDOR "pdcartes-barquette")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "L'atelier des jeux de cartes")
set(CPACK_PACKAGE_DESCRIPTION
    "Un programme pour fabriquer des jeux de cartes : on lui donne des images, ou
une DLL Windows dont il extrait les images, on place les faces et les dos dans
une grille, et il produit un seul fichier à copier dans le dossier des cartes,
où les programmes de jeu qui lisent ce format le trouvent.")
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

# Le paquet d'un build de travail : `cmake --build build --target deb`. Son nom
# porte l'horodatage derrière un `~` ; voir cmake/CPackOptions.cmake.
add_custom_target(deb
    COMMAND ${CMAKE_COMMAND} -E env CARTOTHEQUE_KEEP_VERSION=1
        ${CMAKE_CPACK_COMMAND} -G DEB --config ${CMAKE_CURRENT_BINARY_DIR}/CPackConfig.cmake
    DEPENDS pdcartes-barquette
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Construction du paquet Debian")

# ---------------------------------------------------------------------------
# Le paquet d'une version :
#
#     cmake --build build --target release
#
# Le paquet est nommé par la version seule -- pdcartes-barquette_1.0.0_amd64.deb
# -- et le binaire qu'il contient n'annonce que cette version. Un build
# ordinaire conserve au contraire son horodatage derrière un `~`, ce qui le
# classe avant la version qu'il prépare ; voir cmake/CPackOptions.cmake.
#
# L'absence d'horodatage est attachée à la cible et non à la configuration : il
# n'y a donc pas d'option qu'on pourrait laisser active sans le remarquer, et
# qui produirait un jour deux paquets 1.0.0 que dpkg ne saurait pas
# départager.
add_custom_target(release
    # 1. La version, sans horodatage. CARTOTHEQUE_KEEP_VERSION empêche
    #    ensuite la cible build-version d'intervenir : sans cela, le build
    #    qui suit réécrirait la version avec son horodatage.
    COMMAND ${CMAKE_COMMAND} -DVERSION=${ATELIER_VERSION} -DRELEASE=ON
        -DOUT_CPP=${BUILD_VERSION_CPP} -DOUT_TXT=${BUILD_VERSION_TXT}
        -P ${GENERATE_VERSION}

    # 2. Le binaire avant le paquet : une erreur de compilation arrête tout
    #    ici, et ne laisse pas une version incomplète sur le disque.
    COMMAND ${CMAKE_COMMAND} -E env CARTOTHEQUE_KEEP_VERSION=1
        ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target pdcartes-barquette

    # 3. Le paquet, par la cible qui sait déjà le construire.
    COMMAND ${CMAKE_COMMAND} -E env CARTOTHEQUE_KEEP_VERSION=1
        ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR} --target deb

    COMMENT "Le paquet de la version ${ATELIER_VERSION}")
