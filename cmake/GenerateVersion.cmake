# Ce que le programme annonce de lui-même : la version qui lui a été donnée,
# et le moment où ce build précis a été produit.
#
# La version vient du fichier VERSION, incrémenté à la main ou par les cibles
# de bump. L'horodatage est pris ici à chaque build, de sorte que deux builds
# d'une même version restent distinguables : la version seule ne suffit pas
# pour un programme qu'on recompile plutôt qu'on publie.
#
# Deux formes sont écrites : un fichier source compilé dans le programme, et
# une ligne seule que la mise en paquet relit, cpack s'exécutant bien après le
# compilateur.
#
# Avec RELEASE, aucun horodatage : une version est désignée par son numéro
# seul, et deux paquets d'une même version n'ont pas à se distinguer puisqu'il
# n'y en a qu'un. Seule la cible release le demande ; voir CMakeLists.txt.

# La mise en paquet recompile le programme depuis cpack, après que cpack a lu
# la version dont il nomme le paquet. Réhorodater à ce moment mettrait une
# version sur le paquet et une version postérieure dans le binaire qu'il
# contient : pendant la mise en paquet, l'horodatage est donc laissé tel
# quel.
if(DEFINED ENV{CARTOTHEQUE_KEEP_VERSION} AND EXISTS "${OUT_TXT}")
    return()
endif()

if(RELEASE)
    set(BUILD_VERSION "")
else()
    string(TIMESTAMP BUILD_VERSION "%y%m%d%H%M%S")
endif()

file(WRITE "${OUT_CPP}"
"// Écrit à chaque build par cmake/GenerateVersion.cmake. Ne pas le modifier
// ni le versionner : il décrit ce build, et non ce code source.
//
// extern, car une constante déclarée dans la portée de nommage serait propre
// à ce fichier, alors que le programme doit la lire depuis ailleurs.
extern const char *const kAtelierVersion;
const char *const kAtelierVersion = \"${VERSION}\";
extern const char *const kAtelierBuildVersion;
const char *const kAtelierBuildVersion = \"${BUILD_VERSION}\";
")

file(WRITE "${OUT_TXT}" "${BUILD_VERSION}")
