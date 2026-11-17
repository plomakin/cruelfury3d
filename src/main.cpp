#include "GameItem.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QUrl>

int main(int argc, char* argv[]) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QString());
    app.setOrganizationDomain(QStringLiteral("cruelfury.plm"));
    app.setApplicationName(QStringLiteral("cruelfury.plm"));

    qmlRegisterType<GameItem>("CruelFury", 1, 0, "GameViewport");

    QQmlApplicationEngine engine;

    // Resolve qml/Main.qml relative to binary inside Click package or project tree
    const QString binDir = QCoreApplication::applicationDirPath();
    QStringList candidates = {
        binDir + QStringLiteral("/../../../qml/Main.qml"), // Click lib/aarch64-linux-gnu/bin/cruelfury -> qml/Main.qml
        binDir + QStringLiteral("/qml/Main.qml"),
        QDir::currentPath() + QStringLiteral("/qml/Main.qml"),
        QDir::currentPath() + QStringLiteral("/click/qml/Main.qml")
    };

    QUrl entryUrl;
    for (const QString& path : candidates) {
        if (QFileInfo::exists(path)) {
            entryUrl = QUrl::fromLocalFile(QFileInfo(path).canonicalFilePath());
            break;
        }
    }

    if (entryUrl.isEmpty()) {
        qFatal("Unable to locate qml/Main.qml");
        return 1;
    }

    const bool checkOnly = app.arguments().contains(QStringLiteral("--check-qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [entryUrl, checkOnly](QObject* obj, const QUrl& objUrl) {
                         if (!obj && entryUrl == objUrl) QCoreApplication::exit(2);
                         if (obj && checkOnly) {
                             QTimer::singleShot(120, &QCoreApplication::quit);
                         }
                     }, Qt::QueuedConnection);
    engine.load(entryUrl);
    return app.exec();
}
