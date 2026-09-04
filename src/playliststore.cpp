#include "playliststore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

PlaylistStore::PlaylistStore()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    m_path = dir + QStringLiteral("/playlists.json");
    m_statusPath = dir + QStringLiteral("/channel-status.json");
    load();

    QFile statusFile(m_statusPath);
    if (statusFile.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(statusFile.readAll()).object();
        for (auto it = root.begin(); it != root.end(); ++it)
            m_status.insert(it.key(), it.value().toBool());
    }
}

bool PlaylistStore::addUserList(const UserList &list)
{
    for (const UserList &l : std::as_const(m_lists)) {
        if (l.url == list.url)
            return false;
    }
    m_lists.append(list);
    save();
    return true;
}

void PlaylistStore::removeUserList(const QString &url)
{
    m_lists.removeIf([&](const UserList &l) { return l.url == url; });
    save();
}

bool PlaylistStore::isFavorite(const QString &url) const
{
    for (const Channel &c : m_favorites) {
        if (c.url == url)
            return true;
    }
    return false;
}

bool PlaylistStore::toggleFavorite(const Channel &ch)
{
    const bool was = isFavorite(ch.url);
    if (was)
        m_favorites.removeIf([&](const Channel &c) { return c.url == ch.url; });
    else
        m_favorites.append(ch);
    save();
    return !was;
}

PlaylistStore::Status PlaylistStore::status(const QString &url) const
{
    const auto it = m_status.constFind(url);
    if (it == m_status.cend())
        return Status::Unknown;
    return *it ? Status::Good : Status::Bad;
}

void PlaylistStore::setStatus(const QString &url, bool ok)
{
    m_status.insert(url, ok);
}

void PlaylistStore::saveStatus() const
{
    QJsonObject root;
    for (auto it = m_status.cbegin(); it != m_status.cend(); ++it)
        root.insert(it.key(), it.value());
    QSaveFile f(m_statusPath);
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    f.commit();
}

void PlaylistStore::clearStatus()
{
    m_status.clear();
    saveStatus();
}

void PlaylistStore::load()
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();

    for (const QJsonValue &v : root.value(QLatin1String("lists")).toArray()) {
        const QJsonObject o = v.toObject();
        const UserList l{o.value(QLatin1String("name")).toString(), o.value(QLatin1String("url")).toString()};
        if (!l.url.isEmpty())
            m_lists.append(l);
    }
    for (const QJsonValue &v : root.value(QLatin1String("favorites")).toArray()) {
        const QJsonObject o = v.toObject();
        Channel c;
        c.name = o.value(QLatin1String("name")).toString();
        c.url = o.value(QLatin1String("url")).toString();
        c.logo = o.value(QLatin1String("logo")).toString();
        c.group = o.value(QLatin1String("group")).toString();
        c.userAgent = o.value(QLatin1String("userAgent")).toString();
        c.referrer = o.value(QLatin1String("referrer")).toString();
        if (!c.url.isEmpty())
            m_favorites.append(c);
    }
}

void PlaylistStore::save() const
{
    QJsonArray lists;
    for (const UserList &l : m_lists)
        lists.append(QJsonObject{{QStringLiteral("name"), l.name}, {QStringLiteral("url"), l.url}});

    QJsonArray favs;
    for (const Channel &c : m_favorites) {
        favs.append(QJsonObject{{QStringLiteral("name"), c.name},
                                {QStringLiteral("url"), c.url},
                                {QStringLiteral("logo"), c.logo},
                                {QStringLiteral("group"), c.group},
                                {QStringLiteral("userAgent"), c.userAgent},
                                {QStringLiteral("referrer"), c.referrer}});
    }

    QSaveFile f(m_path);
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(QJsonObject{{QStringLiteral("lists"), lists}, {QStringLiteral("favorites"), favs}}).toJson());
    f.commit();
}
