# La bibliothèque partagée : le format des jeux de cartes, la liste des jeux
# installés, le dessin d'une carte, et le style commun (palette, habillage des
# boîtes de dialogue).
#
# Bibliothèque statique : elle est liée dans le binaire du programme qui
# l'emploie.
#
#   add_subdirectory(../pdcartes-barquette/lib cartotheque)
#   target_link_libraries(monjeu PRIVATE cartotheque)

add_library(cartotheque STATIC
    CardSet.cpp
    CardSet.h
    CardArt.cpp
    CardArt.h
    CardDeck.cpp
    CardDeck.h
    DrawnDeck.cpp
    DrawnDeck.h
    DllResources.cpp
    DllResources.h
    Palette.cpp
    Palette.h
    DialogChrome.cpp
    DialogChrome.h
)

target_include_directories(cartotheque PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cartotheque PUBLIC Qt6::Widgets)
set_target_properties(cartotheque PROPERTIES POSITION_INDEPENDENT_CODE ON AUTOMOC ON)
