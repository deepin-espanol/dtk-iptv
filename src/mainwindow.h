#pragma once

#include "catalog.h"
#include "channel.h"
#include "playliststore.h"

#include <DMainWindow>

#include <QPointer>
#include <QSettings>

class ChannelFilterModel;
class ChannelModel;
class ChannelScanner;
class QComboBox;
class QLabel;
class QListView;
class QNetworkAccessManager;
class QNetworkReply;
class QSlider;
class QSortFilterProxyModel;
class QStandardItem;
class QStandardItemModel;
class QTreeView;
class VideoWidget;

DWIDGET_BEGIN_NAMESPACE
class DIconButton;
class DSearchEdit;
DWIDGET_END_NAMESPACE

class MainWindow : public Dtk::Widget::DMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

signals:
    void catalogStatus(const QString &text);
    void userListsChanged();
    void scanStateChanged();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void rebuildTree();
    void selectList(const QString &url);
    void selectDefaultList();
    bool sourceEnabled(const char *id) const;
    QStandardItem *findListItem(const QString &url, QStandardItem *parent = nullptr) const;

    void loadCatalog();
    void refreshCatalog(bool notify);

    void loadPlaylist(const QString &url);
    void setChannels(const ChannelList &channels);
    void updateCount();

    void addUserList(QWidget *dialogParent = nullptr);
    void removeUserList(const QString &url);
    void showTreeMenu(const QPoint &pos);
    void showChannelMenu(const QPoint &pos);

    void playChannel(const Channel &ch);
    void stepChannel(int delta);
    void openSettings();
    void scanStatusChanged();
    void countScanResults(int &good, int &bad) const;
    void applyTuningEffect();
    void toggleFavorite(const Channel &ch);
    void updateFavoriteButton();
    void toggleFullScreen();
    void setListsVisible(bool visible);
    void notify(const QString &text, bool warning = false);

    QSettings m_settings;
    PlaylistStore m_store;
    QList<CatalogNode> m_catalog;
    QByteArray m_catalogData;
    bool m_catalogFailed = false; // last download attempt failed

    QNetworkAccessManager *m_nam = nullptr;
    QPointer<QNetworkReply> m_listReply;
    QString m_currentList;
    Channel m_current;

    QStandardItemModel *m_treeModel = nullptr;
    QSortFilterProxyModel *m_treeProxy = nullptr;
    ChannelModel *m_channels = nullptr;
    ChannelFilterModel *m_channelProxy = nullptr;
    ChannelScanner *m_scanner = nullptr;
    int m_scanGood = 0;
    int m_scanBad = 0;

    QWidget *m_listsPanel = nullptr;
    QWidget *m_channelsPanel = nullptr;
    QWidget *m_controls = nullptr;
    QTreeView *m_tree = nullptr;
    QListView *m_channelView = nullptr;
    QComboBox *m_groupCombo = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QSlider *m_volume = nullptr;
    VideoWidget *m_video = nullptr;
    Dtk::Widget::DSearchEdit *m_search = nullptr;
    Dtk::Widget::DIconButton *m_playButton = nullptr;
    Dtk::Widget::DIconButton *m_favButton = nullptr;
    Dtk::Widget::DIconButton *m_muteButton = nullptr;

    bool m_listsVisible = true;
    bool m_videoFullScreen = false;
    bool m_wasMaximized = false;
};
