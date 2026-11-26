#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

// A node of the playlist catalogue published by iptv-org in PLAYLISTS.md.
// The app ships none of it: it is downloaded at run time and cached.
struct CatalogNode
{
    QString title;
    QString url;      // empty for pure groups
    int channels = -1; // -1 when unknown
    QList<CatalogNode> children;
};

namespace Catalog {
// Id used to switch the catalogue off, like the other sources.
inline const char *const kSourceId = "iptv-org";
inline const char *const kSourceUrl = "https://raw.githubusercontent.com/iptv-org/iptv/master/PLAYLISTS.md";
inline const char *const kMainIndexUrl = "https://iptv-org.github.io/iptv/index.m3u";

// Returns the top-level groups (categories, languages, countries, regions...).
QList<CatalogNode> parse(const QByteArray &markdown);
}
