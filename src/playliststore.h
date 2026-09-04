#pragma once

#include "channel.h"

#include <QHash>

struct UserList
{
    QString name;
    QString url; // http(s) URL or local file path
};

// Persists the playlists added by the user and the favourite channels.
class PlaylistStore
{
public:
    PlaylistStore();

    const QList<UserList> &userLists() const { return m_lists; }
    bool addUserList(const UserList &list); // false if the URL already exists
    void removeUserList(const QString &url);

    const ChannelList &favorites() const { return m_favorites; }
    bool isFavorite(const QString &url) const;
    bool toggleFavorite(const Channel &ch); // returns the new state

    // Results of the channel scanner, keyed by stream URL.
    enum class Status { Unknown, Good, Bad };
    Status status(const QString &url) const;
    bool isBad(const QString &url) const { return status(url) == Status::Bad; }
    void setStatus(const QString &url, bool ok); // call saveStatus() when done
    void saveStatus() const;
    void clearStatus();

private:
    void load();
    void save() const;

    QString m_path;
    QList<UserList> m_lists;
    ChannelList m_favorites;
    QString m_statusPath;
    QHash<QString, bool> m_status;
};
