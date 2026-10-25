# Lu par cpack pendant son exécution, c'est-à-dire après le build : à ce
# moment, le build a inscrit son horodatage, et c'est lui qui distingue deux
# paquets d'une même version. Fixer cette valeur dans CMakeLists la figerait au
# moment de la configuration du projet, qui peut précéder de plusieurs semaines
# de builds.
#
# L'horodatage va dans la version, derrière un `~`, et non dans le champ de
# release de Debian, pour deux raisons :
#
#  - `~` se classe avant tout le reste, y compris avant rien :
#    `1.0.0~261024225112` est *antérieur* à `1.0.0`, ce qui correspond bien à
#    un build de travail. Placé dans le champ de release, il serait postérieur,
#    et un build de travail installé empêcherait la version de le remplacer ;
#  - le champ de release désigne les mises en paquet successives d'une même
#    version — une dépendance corrigée, un résumé réécrit. L'employer ici
#    occuperait un champ qui sert à autre chose.
#
# La cible release, elle, n'inscrit aucun horodatage : le paquet s'appelle
# `1.0.0`, ce qui est la version. Rien de particulier à faire ici pour cela :
# le test ci-dessous trouve simplement un horodatage vide.

if(EXISTS "${CPACK_BUILD_VERSION_FILE}")
    file(READ "${CPACK_BUILD_VERSION_FILE}" _cartotheque_build)
    string(STRIP "${_cartotheque_build}" _cartotheque_build)
    if(_cartotheque_build)
        set(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION}~${_cartotheque_build}")
    endif()
endif()
