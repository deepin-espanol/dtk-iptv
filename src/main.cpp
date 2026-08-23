#include "mainwindow.h"

#include <DApplication>

#include <QIcon>
#include <QSettings>

DWIDGET_USE_NAMESPACE

int main(int argc, char *argv[])
{
    // Qt reads this once, before the first player exists. Software decoding is
    // the default because broken VA-API setups garble the start of each stream.
    if (!qEnvironmentVariableIsSet("QT_FFMPEG_DECODING_HW_DEVICE_TYPES")) {
        const QSettings settings(QStringLiteral("dtk-iptv"), QStringLiteral("dtk-iptv"));
        if (!settings.value(QStringLiteral("hardwareDecoding"), false).toBool())
            qputenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", ",");
    }

    DApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("dtk-iptv"));
    app.setApplicationName(QStringLiteral("dtk-iptv"));
    app.setApplicationDisplayName(QStringLiteral("IPTV"));
    app.setApplicationVersion(QStringLiteral(APP_VERSION));
    app.setProductName(QStringLiteral("IPTV"));
    const QIcon icon(QStringLiteral(":/dtk-iptv.svg"));
    app.setProductIcon(icon);
    app.setWindowIcon(icon);
    app.setApplicationDescription(QStringLiteral(
        "Reproductor de IPTV. Incluye por defecto las listas públicas de iptv-org "
        "y permite agregar listas M3U propias."));
    app.loadTranslator();

    MainWindow w;
    w.show();
    return app.exec();
}
