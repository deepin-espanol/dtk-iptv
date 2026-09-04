#pragma once

#include "channel.h"

#include <QAbstractListModel>
#include <QHash>
#include <QIcon>
#include <QPixmap>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStringList>

class PlaylistStore;
class QNetworkAccessManager;

class ChannelModel : public QAbstractListModel
{
    Q_OBJECT
public:
    ChannelModel(QNetworkAccessManager *nam, const PlaylistStore *store, QObject *parent = nullptr);

    void setChannels(const ChannelList &channels);
    const Channel &channelAt(int row) const { return m_channels.at(row); }
    const ChannelList &channels() const { return m_channels; }
    const PlaylistStore *store() const { return m_store; }
    void statusChanged(); // scan results changed: repaint the rows
    QStringList groups() const;
    static QSize logoSize();

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private:
    void requestLogo(const QString &url);
    void pumpQueue();

    QNetworkAccessManager *m_nam;
    const PlaylistStore *m_store;
    ChannelList m_channels;
    QIcon m_placeholder;

    // Logos are fetched lazily, only for rows the view actually asks for.
    QHash<QString, QIcon> m_logos; // null icon = failed, don't retry
    QSet<QString> m_pending;
    QStringList m_queue; // most recently requested last (served first)
    int m_active = 0;
};

class ChannelFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setSearch(const QString &text);
    void setGroup(const QString &group); // empty = all groups
    void setHideBad(bool hide);
    void statusChanged();

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override;

private:
    QString m_search;
    QString m_group;
    bool m_hideBad = false;
};
