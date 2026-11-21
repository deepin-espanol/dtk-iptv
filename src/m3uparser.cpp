#include "m3uparser.h"

#include <QHash>
#include <QStringList>
#include <QUrl>

namespace {

// Parses the part of an #EXTINF line after the duration:
//   -1 tvg-id="x" tvg-logo="y" group-title="z",Channel name
void parseExtInf(const QString &line, Channel &ch)
{
    QHash<QString, QString> attrs;
    int i = line.indexOf(QLatin1Char(':')) + 1;
    const int n = line.size();
    int nameStart = -1;

    while (i < n) {
        const QChar c = line.at(i);
        if (c == QLatin1Char(',')) {
            nameStart = i + 1;
            break;
        }
        const int eq = line.indexOf(QLatin1Char('='), i);
        const int comma = line.indexOf(QLatin1Char(','), i);
        if (eq < 0 || (comma >= 0 && comma < eq)) {
            // No more attributes before the title.
            if (comma >= 0)
                nameStart = comma + 1;
            break;
        }
        // The key is the last whitespace-delimited token before '='.
        const int keyStart = line.lastIndexOf(QLatin1Char(' '), eq) + 1;
        const QString key = line.mid(qMax(keyStart, i), eq - qMax(keyStart, i)).trimmed().toLower();
        int valueEnd;
        QString value;
        if (eq + 1 < n && line.at(eq + 1) == QLatin1Char('"')) {
            valueEnd = line.indexOf(QLatin1Char('"'), eq + 2);
            if (valueEnd < 0)
                valueEnd = n;
            value = line.mid(eq + 2, valueEnd - eq - 2);
            i = valueEnd + 1;
        } else {
            valueEnd = eq + 1;
            while (valueEnd < n && !line.at(valueEnd).isSpace() && line.at(valueEnd) != QLatin1Char(','))
                ++valueEnd;
            value = line.mid(eq + 1, valueEnd - eq - 1);
            i = valueEnd;
        }
        attrs.insert(key, value);
    }

    if (nameStart >= 0)
        ch.name = line.mid(nameStart).trimmed();
    if (ch.name.isEmpty())
        ch.name = attrs.value(QStringLiteral("tvg-name"));
    ch.logo = attrs.value(QStringLiteral("tvg-logo"));
    ch.group = attrs.value(QStringLiteral("group-title"));
    ch.userAgent = attrs.value(QStringLiteral("http-user-agent"), attrs.value(QStringLiteral("user-agent")));
    ch.referrer = attrs.value(QStringLiteral("http-referrer"), attrs.value(QStringLiteral("referrer")));
}

// Kodi-style suffix: http://host/stream.m3u8|User-Agent=x&Referer=y
void splitHeaderSuffix(Channel &ch)
{
    const int bar = ch.url.indexOf(QLatin1Char('|'));
    if (bar < 0 || !ch.url.mid(bar).contains(QLatin1Char('=')))
        return;
    const QStringList pairs = ch.url.mid(bar + 1).split(QLatin1Char('&'), Qt::SkipEmptyParts);
    ch.url.truncate(bar);
    for (const QString &pair : pairs) {
        const QString key = pair.section(QLatin1Char('='), 0, 0).trimmed().toLower();
        const QString value = QUrl::fromPercentEncoding(pair.section(QLatin1Char('='), 1).toUtf8());
        if (key == QLatin1String("user-agent"))
            ch.userAgent = value;
        else if (key == QLatin1String("referer") || key == QLatin1String("referrer"))
            ch.referrer = value;
    }
}

} // namespace

ChannelList M3uParser::parse(const QByteArray &data, const std::function<bool(const QString &)> &skip)
{
    ChannelList result;
    Channel pending;
    bool hasInfo = false;

    const QList<QByteArray> lines = data.split('\n');
    for (const QByteArray &raw : lines) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.isEmpty())
            continue;

        if (line.startsWith(QLatin1String("#EXTINF"), Qt::CaseInsensitive)) {
            pending = Channel();
            parseExtInf(line, pending);
            hasInfo = true;
        } else if (line.startsWith(QLatin1String("#EXTGRP:"), Qt::CaseInsensitive)) {
            if (pending.group.isEmpty())
                pending.group = line.mid(8).trimmed();
        } else if (line.startsWith(QLatin1String("#EXTVLCOPT:"), Qt::CaseInsensitive)) {
            const QString key = line.mid(11).section(QLatin1Char('='), 0, 0).trimmed().toLower();
            const QString value = line.section(QLatin1Char('='), 1).trimmed();
            if (key == QLatin1String("http-user-agent"))
                pending.userAgent = value;
            else if (key == QLatin1String("http-referrer"))
                pending.referrer = value;
        } else if (line.startsWith(QLatin1Char('#'))) {
            continue;
        } else {
            pending.url = line;
            splitHeaderSuffix(pending);
            if (skip && skip(pending.url)) {
                pending = Channel();
                hasInfo = false;
                continue;
            }
            if (!hasInfo || pending.name.isEmpty())
                pending.name = line.section(QLatin1Char('/'), -1);
            result.append(pending);
            pending = Channel();
            hasInfo = false;
        }
    }
    return result;
}
