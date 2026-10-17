#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QIcon>
#include <QLocale>

#include "AtelierWindow.h"
#include "Version.h"

namespace {

// Qt se rabat sur « hicolor » quand le bureau n'a pas dit quel thème
// employer, et hicolor est presque vide : on pointe vers un thème installé.
void ensureIconTheme() {
    const QString configured = QIcon::themeName();
    if (!configured.isEmpty() && configured != QLatin1String("hicolor")) return;

    const QStringList candidates = {QStringLiteral("Adwaita"), QStringLiteral("breeze"),
                                    QStringLiteral("oxygen"), QStringLiteral("Papirus"),
                                    QStringLiteral("gnome")};
    for (const QString &name : candidates) {
        for (const QString &dir : QIcon::themeSearchPaths()) {
            if (!QFileInfo::exists(dir + QLatin1Char('/') + name +
                                   QLatin1String("/index.theme"))) {
                continue;
            }
            QIcon::setFallbackThemeName(name);
            return;
        }
    }
}

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("cardset-creator");
    QApplication::setApplicationName("cardset-creator");
    QApplication::setApplicationVersion(fullVersion());
    QApplication::setDesktopFileName(QStringLiteral("cardset-creator"));

    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));
    ensureIconTheme();

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate(
        "main", "L'atelier des jeux de cartes : des images d'un côté, un jeu de l'autre."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        QCoreApplication::translate("main", "jeu"),
        QCoreApplication::translate("main", "Un fichier .cartes à ouvrir au démarrage."));
    parser.process(app);

    AtelierWindow window;
    window.show();
    const QStringList asked = parser.positionalArguments();
    if (!asked.isEmpty()) window.openPath(asked.first());
    return app.exec();
}
