#include "mainwindow.h"

#include <DAboutDialog>
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

    DAboutDialog about;
    app.setAboutDialog(&about);
    about.setWindowTitle(QStringLiteral("IPTV"));
    about.setProductName(QStringLiteral("<span>IPTV</span>"));
    about.setProductIcon(icon);
    about.setVersion(DApplication::buildVersion(QStringLiteral(APP_VERSION)));
    about.setDescription(QStringLiteral(
        "<div style='text-align:center;'><img src=':/icons/Logo-Racoon.png' width='128'/></div>"
        "<span style='font-size:8pt; font-weight:600;'>Deepin en Español </span>"
        "<a href='https://deepinenespanol.org'>https://deepinenespanol.org</a><br/>"
        "<span style='font-size:8pt; font-weight:600;'>Deepin Latin Code - developers</span>"
        "<br/><br/>"
        "<span style='font-size:7pt;'>Esta aplicación es solo un reproductor: no aloja ni distribuye "
        "canales. Las listas por defecto provienen de iptv-org y obedecen a su licencia.</span>"));
    about.setWebsiteName(QStringLiteral("deepinenespanol.org"));
    about.setWebsiteLink(QStringLiteral("https://deepinenespanol.org"));
    app.loadTranslator();

    MainWindow w;
    w.show();
    return app.exec();
}
