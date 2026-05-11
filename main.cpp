#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QDir>
#include <QCoreApplication>
#include <QIcon>

#include "src/LaMaBridge.h"
#include "src/SAMBridge.h"
#include "src/ResultImageProvider.h"
#include "src/ImageUtils.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <cstdio>
#endif

int main(int argc, char *argv[])
{
#if defined(Q_OS_WIN) && defined(QT_DEBUG)
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
#endif

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QGuiApplication app(argc, argv);
    app.setApplicationName("LaMaQt");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("LaMaQt");
    app.setWindowIcon(QIcon(":/resources/app.ico"));

    QQuickStyle::setStyle("Basic");

    // ── VLC plugin path ────────────────────────────────────────────────────────
    QString exeDir    = QCoreApplication::applicationDirPath();
    QString pluginPath = exeDir + "/plugins";
    if (!QDir(pluginPath).exists())
        pluginPath = "C:/libVLC/bin/X64/plugins";
    qputenv("VLC_PLUGIN_PATH", pluginPath.toLocal8Bit());

    // ── QML engine ──────────────────────────────────────────────────────────────
    QQmlApplicationEngine engine;

    // Result image provider: supplies image://result/frame?v=N to QML
    // Ownership is transferred to engine via addImageProvider — no manual delete.
    auto *resultProvider = new ResultImageProvider();
    engine.addImageProvider("result", resultProvider);

    // lamaBridge: context property exposed to all QML files
    LaMaBridge lamaBridge;
    lamaBridge.setResultProvider(resultProvider);
    engine.rootContext()->setContextProperty("lamaBridge", &lamaBridge);

    // samBridge: SAM 2 segmentation bridge
    SAMBridge samBridge;
    engine.rootContext()->setContextProperty("samBridge", &samBridge);

    // imageUtils: C++ image file loading utility (used by SourcePanel)
    ImageUtils imageUtils;
    engine.rootContext()->setContextProperty("imageUtils", &imageUtils);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.loadFromModule("CompactLaMaQt", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
