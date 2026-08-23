#pragma once

#include <QList>
#include <QString>

struct Channel
{
    QString name;
    QString url;
    QString logo;
    QString group; // may hold several groups separated by ';'
    QString userAgent; // required by some servers, empty otherwise
    QString referrer;
};

using ChannelList = QList<Channel>;
