#pragma once

#include <QString>

// Ce qu'est ce binaire, et quand il a été produit.
//
// La version est celle du fichier VERSION, à la racine des sources : trois
// nombres, incrémentés à la main quand un changement mérite une nouvelle
// version. L'horodatage du build est le moment de la compilation, au format
// aammjjhhmmss : deux builds n'ont jamais le même, ce qui permet de
// distinguer le 1.0.0 d'hier de celui qu'on a devant soi.
//
// Un binaire de version publiée ne porte aucun horodatage : son numéro de
// version le désigne entièrement, puisqu'il n'en existe qu'un. L'horodatage y
// est donc vide, et le code qui le lit doit le prévoir.
//
// Les deux sont définis dans un fichier écrit à chaque build ; voir
// cmake/GenerateVersion.cmake.
extern const char *const kAtelierVersion;
extern const char *const kAtelierBuildVersion;

inline QString appVersion() { return QString::fromLatin1(kAtelierVersion); }
inline QString buildVersion() { return QString::fromLatin1(kAtelierBuildVersion); }

// Les deux ensemble, pour un affichage qui a la place des deux. La version
// seule s'il s'agit d'une version publiée, qui n'a pas d'horodatage.
inline QString fullVersion() {
    const QString build = buildVersion();
    if (build.isEmpty()) {
        return appVersion();
    }
    return appVersion() + QStringLiteral(" (build ") + build + QLatin1Char(')');
}
