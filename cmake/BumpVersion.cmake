# Incrémente la version, dans celle de ses trois parties qui est demandée :
#
#   cmake -DFILE=VERSION -DPART=major|minor|patch -P cmake/BumpVersion.cmake
#
# Incrémenter une partie remet à zéro celles qui la suivent : après 1.1.9, une
# incrémentation mineure donne 1.2.0 et non 1.2.9. L'horodatage du build n'est
# pas touché : il dit quand le binaire a été produit, la version dit ce qu'il
# contient.

if(NOT DEFINED FILE OR NOT DEFINED PART)
    message(FATAL_ERROR "BumpVersion.cmake attend -DFILE=<chemin> et -DPART=major|minor|patch")
endif()

file(READ "${FILE}" CURRENT)
string(STRIP "${CURRENT}" CURRENT)

if(NOT CURRENT MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    message(FATAL_ERROR "'${CURRENT}' dans ${FILE} n'est pas une version de trois nombres")
endif()

set(MAJOR ${CMAKE_MATCH_1})
set(MINOR ${CMAKE_MATCH_2})
set(PATCH ${CMAKE_MATCH_3})

if(PART STREQUAL "major")
    math(EXPR MAJOR "${MAJOR} + 1")
    set(MINOR 0)
    set(PATCH 0)
elseif(PART STREQUAL "minor")
    math(EXPR MINOR "${MINOR} + 1")
    set(PATCH 0)
elseif(PART STREQUAL "patch" OR PART STREQUAL "micro")
    math(EXPR PATCH "${PATCH} + 1")
else()
    message(FATAL_ERROR "PART vaut major, minor ou patch, et non '${PART}'")
endif()

set(NEXT "${MAJOR}.${MINOR}.${PATCH}")
file(WRITE "${FILE}" "${NEXT}\n")
message(STATUS "${CURRENT} -> ${NEXT}")
