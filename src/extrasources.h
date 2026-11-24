#pragma once

#include <QList>
#include <QString>

// Other free, legal playlists offered besides iptv-org. Like the iptv-org
// catalogue, none of them ship with the app: each is downloaded when opened.
struct ExtraSource
{
    const char *id;
    const char *title;
    const char *url;
    const char *description;
};

namespace ExtraSources {
inline const QList<ExtraSource> &all()
{
    static const QList<ExtraSource> sources{
        {"freetv", "Free-TV (internacional)", "https://raw.githubusercontent.com/Free-TV/IPTV/master/playlist.m3u8",
         "Canales gratuitos y legales de muchos países, agrupados por país."},
        {"tdt-tv", "TDTChannels · Televisión (España)", "https://www.tdtchannels.com/lists/tv.m3u8",
         "Canales en abierto de la TDT española, nacionales y autonómicos."},
        {"tdt-radio", "TDTChannels · Radio (España)", "https://www.tdtchannels.com/lists/radio.m3u8",
         "Emisoras de radio españolas."},
    };
    return sources;
}
}
