#include "mainwindow.h"

#include "channelmodel.h"
#include "channelscanner.h"
#include "m3uparser.h"
#include "settingsdialog.h"
#include "videowidget.h"

#include <DDialog>
#include <DIconButton>
#include <DLineEdit>
#include <DMessageManager>
#include <DSearchEdit>
#include <DTitlebar>

#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QLocale>
#include <QMenu>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QPushButton>
#include <QSaveFile>
#include <QShortcut>
#include <QSlider>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTreeView>
#include <QVBoxLayout>

#include <cmath>

DWIDGET_USE_NAMESPACE

namespace {

enum TreeRole { UrlRole = Qt::UserRole + 1, UserListRole };

const QString kFavoritesUrl = QStringLiteral("fav:");

QString catalogCachePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/PLAYLISTS.md");
}

QString groupTitle(const QString &heading)
{
    static const QHash<QString, QString> names{
        {QStringLiteral("Grouped by category"), QStringLiteral("Por categoría")},
        {QStringLiteral("Grouped by language"), QStringLiteral("Por idioma")},
        {QStringLiteral("Countries"), QStringLiteral("Países")},
        {QStringLiteral("Regions"), QStringLiteral("Regiones")},
    };
    return names.value(heading, heading);
}

// Flag emojis rarely render in desktop fonts; drop any leading symbols.
QString cleanTitle(const QString &title)
{
    const QList<uint> cps = title.toUcs4();
    int i = 0;
    while (i < cps.size() && !QChar::isLetterOrNumber(cps.at(i)))
        ++i;
    if (i == 0 || i == cps.size())
        return title;
    return QString::fromUcs4(reinterpret_cast<const char32_t *>(cps.constData()) + i, cps.size() - i);
}

QIcon starIcon(const QColor &color, bool filled)
{
    QPixmap pix(64, 64);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QPolygonF star;
    for (int i = 0; i < 10; ++i) {
        const qreal r = (i % 2 == 0) ? 26 : 11;
        const qreal a = -M_PI / 2 + i * M_PI / 5;
        star << QPointF(32 + r * std::cos(a), 34 + r * std::sin(a));
    }
    p.setPen(QPen(color, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(filled ? QBrush(color) : Qt::NoBrush);
    p.drawPolygon(star);
    return QIcon(pix);
}

// The icon theme's stop/fullscreen glyphs are dark and vanish on dark themes,
// so these are drawn with the palette colour instead.
QIcon stopIcon(const QColor &color)
{
    QPixmap pix(64, 64);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawRoundedRect(QRectF(15, 15, 34, 34), 5, 5);
    return QIcon(pix);
}

QIcon fullScreenIcon(const QColor &color)
{
    QPixmap pix(64, 64);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const qreal a = 10, b = 54, l = 14;
    p.drawPolyline(QPolygonF{{a, a + l}, {a, a}, {a + l, a}});
    p.drawPolyline(QPolygonF{{b - l, a}, {b, a}, {b, a + l}});
    p.drawPolyline(QPolygonF{{b, b - l}, {b, b}, {b - l, b}});
    p.drawPolyline(QPolygonF{{a + l, b}, {a, b}, {a, b - l}});
    return QIcon(pix);
}

QIcon skipIcon(const QColor &color, bool forward)
{
    QPixmap pix(64, 64);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    if (!forward) {
        p.translate(64, 0);
        p.scale(-1, 1);
    }
    p.setPen(QPen(color, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(color);
    p.drawPolygon(QPolygonF{{16, 16}, {40, 32}, {16, 48}});
    p.drawLine(QPointF(47, 16), QPointF(47, 48));
    return QIcon(pix);
}

QIcon sidebarIcon(const QColor &color)
{
    QPixmap pix(64, 64);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawRoundedRect(QRectF(8, 12, 48, 40), 7, 7);
    p.drawLine(QPointF(25, 12), QPointF(25, 52));
    return QIcon(pix);
}

QStandardItem *makeItem(const QString &text, const QString &url = QString())
{
    auto *item = new QStandardItem(text);
    item->setEditable(false);
    item->setData(url, UrlRole);
    return item;
}

void appendCatalogNode(QStandardItem *parent, const CatalogNode &node)
{
    QString text = node.title == QLatin1String("*") ? QStringLiteral("Todo en una lista") : cleanTitle(node.title);
    if (node.channels >= 0)
        text += QStringLiteral("  (%1)").arg(node.channels);
    QStandardItem *item = makeItem(text, node.url);
    parent->appendRow(item);
    for (const CatalogNode &child : node.children)
        appendCatalogNode(item, child);
}

DIconButton *iconButton(const QString &icon, const QString &tip, QWidget *parent)
{
    auto *b = new DIconButton(parent);
    b->setIcon(QIcon::fromTheme(icon));
    b->setIconSize(QSize(18, 18));
    b->setFixedSize(36, 36);
    b->setToolTip(tip);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    buildUi();
    loadCatalog();
    rebuildTree();

    const QString last = m_settings.value(QStringLiteral("lastList")).toString();
    if (!last.isEmpty() && findListItem(last))
        selectList(last);
    else
        selectDefaultList();

    // Nothing from iptv-org ships with the app: the catalogue is downloaded on
    // first start and refreshed on every later one.
    refreshCatalog(false);
}

void MainWindow::buildUi()
{
    setMinimumSize(900, 520);
    resize(1280, 720);

    // --- Title bar -------------------------------------------------------
    titlebar()->setIcon(windowIcon());
    m_search = new DSearchEdit(this);
    m_search->setPlaceHolder(QStringLiteral("Buscar canal"));
    m_search->setFixedWidth(320);
    titlebar()->addWidget(m_search, Qt::AlignCenter);

    auto *sidebarButton = iconButton(QString(), QStringLiteral("Mostrar u ocultar las listas (Ctrl+B)"), this);
    sidebarButton->setIcon(sidebarIcon(palette().color(QPalette::ButtonText)));
    sidebarButton->setFlat(true);
    titlebar()->addWidget(sidebarButton, Qt::AlignLeft);
    connect(sidebarButton, &DIconButton::clicked, this, [this] { setListsVisible(!m_listsVisible); });
    connect(new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_B), this), &QShortcut::activated, this,
            [this] { setListsVisible(!m_listsVisible); });

    auto *menu = new QMenu(this);
    menu->addAction(QStringLiteral("Ajustes"), this, &MainWindow::openSettings);
    titlebar()->setMenu(menu);

    // --- Playlists panel -------------------------------------------------
    m_treeModel = new QStandardItemModel(this);
    m_treeProxy = new QSortFilterProxyModel(this);
    m_treeProxy->setSourceModel(m_treeModel);
    m_treeProxy->setRecursiveFilteringEnabled(true);
    m_treeProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_listsPanel = new QWidget(this);
    auto *listsLayout = new QVBoxLayout(m_listsPanel);
    listsLayout->setContentsMargins(10, 10, 5, 10);

    auto *listSearch = new DSearchEdit(m_listsPanel);
    listSearch->setPlaceHolder(QStringLiteral("Filtrar listas"));

    m_tree = new QTreeView(m_listsPanel);
    m_tree->setModel(m_treeProxy);
    m_tree->setHeaderHidden(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto *addButton = new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")),
                                      QStringLiteral("Agregar lista"), m_listsPanel);

    listsLayout->addWidget(listSearch);
    listsLayout->addWidget(m_tree, 1);
    listsLayout->addWidget(addButton);

    connect(addButton, &QPushButton::clicked, this, [this] { addUserList(); });
    connect(m_tree, &QWidget::customContextMenuRequested, this, &MainWindow::showTreeMenu);
    connect(listSearch, &DSearchEdit::textChanged, this, [this](const QString &text) {
        m_treeProxy->setFilterFixedString(text.trimmed());
        if (text.trimmed().isEmpty()) {
            m_tree->collapseAll();
            if (QStandardItem *item = findListItem(m_currentList))
                m_tree->scrollTo(m_treeProxy->mapFromSource(item->index()));
        } else {
            m_tree->expandAll();
        }
    });
    connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current) {
                const QString url = current.data(UrlRole).toString();
                if (!url.isEmpty() && url != m_currentList)
                    loadPlaylist(url);
            });

    // --- Channels panel --------------------------------------------------
    m_channels = new ChannelModel(m_nam, &m_store, this);
    m_channelProxy = new ChannelFilterModel(this);
    m_channelProxy->setSourceModel(m_channels);
    m_channelProxy->setHideBad(m_settings.value(QStringLiteral("hideBadChannels"), false).toBool());

    m_scanner = new ChannelScanner(this);
    connect(m_scanner, &ChannelScanner::channelChecked, this, [this](const QString &url, bool ok) {
        m_store.setStatus(url, ok);
        ++(ok ? m_scanGood : m_scanBad);
    });
    connect(m_scanner, &ChannelScanner::progress, this, [this](int done, int) {
        // Refreshing the list on every result would make it jump constantly.
        if (done % 20 == 0)
            scanStatusChanged();
        else
            updateCount();
        emit scanStateChanged();
    });
    connect(m_scanner, &ChannelScanner::finished, this, [this](bool completed) {
        m_store.saveStatus();
        scanStatusChanged();
        emit scanStateChanged();
        if (completed)
            notify(QStringLiteral("Escaneo terminado: %1 canales funcionan y %2 no").arg(m_scanGood).arg(m_scanBad));
    });

    m_channelsPanel = new QWidget(this);
    auto *channelsLayout = new QVBoxLayout(m_channelsPanel);
    channelsLayout->setContentsMargins(5, 10, 5, 10);

    m_groupCombo = new QComboBox(m_channelsPanel);
    m_channelView = new QListView(m_channelsPanel);
    m_channelView->setModel(m_channelProxy);
    m_channelView->setIconSize(ChannelModel::logoSize());
    m_channelView->setUniformItemSizes(true);
    m_channelView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_channelView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_countLabel = new QLabel(m_channelsPanel);

    channelsLayout->addWidget(m_groupCombo);
    channelsLayout->addWidget(m_channelView, 1);
    channelsLayout->addWidget(m_countLabel);

    connect(m_search, &DSearchEdit::textChanged, this, [this](const QString &text) {
        m_channelProxy->setSearch(text);
        updateCount();
    });
    connect(m_groupCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        m_channelProxy->setGroup(index <= 0 ? QString() : m_groupCombo->itemText(index));
        updateCount();
    });
    connect(m_channelView, &QListView::activated, this, [this](const QModelIndex &index) {
        playChannel(m_channels->channelAt(m_channelProxy->mapToSource(index).row()));
    });
    connect(m_channelView, &QListView::clicked, this, [this](const QModelIndex &index) {
        playChannel(m_channels->channelAt(m_channelProxy->mapToSource(index).row()));
    });
    connect(m_channelView, &QWidget::customContextMenuRequested, this, &MainWindow::showChannelMenu);

    // --- Player panel ----------------------------------------------------
    auto *playerPanel = new QWidget(this);
    auto *playerLayout = new QVBoxLayout(playerPanel);
    playerLayout->setContentsMargins(0, 0, 0, 0);
    playerLayout->setSpacing(0);

    m_video = new VideoWidget(playerPanel);

    m_controls = new QWidget(playerPanel);
    auto *controlsLayout = new QHBoxLayout(m_controls);
    controlsLayout->setContentsMargins(10, 8, 10, 8);

    auto *prevButton = iconButton(QString(), QStringLiteral("Canal anterior (Re Pág)"), m_controls);
    auto *nextButton = iconButton(QString(), QStringLiteral("Canal siguiente (Av Pág)"), m_controls);
    prevButton->setIcon(skipIcon(palette().color(QPalette::ButtonText), false));
    nextButton->setIcon(skipIcon(palette().color(QPalette::ButtonText), true));
    connect(prevButton, &DIconButton::clicked, this, [this] { stepChannel(-1); });
    connect(nextButton, &DIconButton::clicked, this, [this] { stepChannel(1); });

    m_playButton = iconButton(QStringLiteral("media-playback-start"), QStringLiteral("Reproducir / Pausar (Espacio)"), m_controls);
    auto *stopButton = iconButton(QStringLiteral("media-playback-stop"), QStringLiteral("Detener"), m_controls);
    m_favButton = iconButton(QStringLiteral("non-starred"), QStringLiteral("Favorito"), m_controls);
    m_muteButton = iconButton(QStringLiteral("audio-volume-high"), QStringLiteral("Silenciar"), m_controls);
    auto *fullButton = iconButton(QStringLiteral("view-fullscreen"), QStringLiteral("Pantalla completa (F11)"), m_controls);

    m_titleLabel = new QLabel(m_controls);
    m_titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    m_volume = new QSlider(Qt::Horizontal, m_controls);
    m_volume->setRange(0, 100);
    m_volume->setFixedWidth(120);
    m_volume->setFocusPolicy(Qt::NoFocus);
    m_volume->setValue(m_settings.value(QStringLiteral("volume"), 80).toInt());
    m_video->setVolume(m_volume->value());

    controlsLayout->addWidget(prevButton);
    controlsLayout->addWidget(m_playButton);
    controlsLayout->addWidget(stopButton);
    controlsLayout->addWidget(nextButton);
    controlsLayout->addWidget(m_favButton);
    controlsLayout->addWidget(m_titleLabel, 1);
    controlsLayout->addWidget(m_muteButton);
    controlsLayout->addWidget(m_volume);
    controlsLayout->addWidget(fullButton);

    applyTuningEffect();
    updateFavoriteButton();
    stopButton->setIcon(stopIcon(palette().color(QPalette::ButtonText)));
    fullButton->setIcon(fullScreenIcon(palette().color(QPalette::ButtonText)));

    playerLayout->addWidget(m_video, 1);
    playerLayout->addWidget(m_controls);

    connect(m_playButton, &DIconButton::clicked, m_video, &VideoWidget::togglePause);
    connect(stopButton, &DIconButton::clicked, m_video, &VideoWidget::stop);
    connect(fullButton, &DIconButton::clicked, this, &MainWindow::toggleFullScreen);
    connect(m_favButton, &DIconButton::clicked, this, [this] {
        if (!m_current.url.isEmpty())
            toggleFavorite(m_current);
    });
    connect(m_muteButton, &DIconButton::clicked, this, [this] {
        m_video->setMuted(!m_video->isMuted());
        m_muteButton->setIcon(QIcon::fromTheme(m_video->isMuted() ? QStringLiteral("audio-volume-muted")
                                                                  : QStringLiteral("audio-volume-high")));
    });
    connect(m_volume, &QSlider::valueChanged, m_video, &VideoWidget::setVolume);
    connect(m_video, &VideoWidget::doubleClicked, this, &MainWindow::toggleFullScreen);
    connect(m_video, &VideoWidget::playingChanged, this, [this](bool playing) {
        m_playButton->setIcon(QIcon::fromTheme(playing ? QStringLiteral("media-playback-pause")
                                                       : QStringLiteral("media-playback-start")));
    });

    // --- Layout ----------------------------------------------------------
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_listsPanel);
    splitter->addWidget(m_channelsPanel);
    splitter->addWidget(playerPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 0);
    splitter->setStretchFactor(2, 1);
    splitter->setChildrenCollapsible(false);
    splitter->setSizes({250, 290, 740});
    setCentralWidget(splitter);

    // --- Shortcuts -------------------------------------------------------
    connect(new QShortcut(QKeySequence(Qt::Key_F11), this), &QShortcut::activated, this, &MainWindow::toggleFullScreen);
    connect(new QShortcut(QKeySequence(Qt::Key_Escape), this), &QShortcut::activated, this, [this] {
        if (m_videoFullScreen)
            toggleFullScreen();
    });
    connect(new QShortcut(QKeySequence(Qt::Key_Space), this), &QShortcut::activated, m_video, &VideoWidget::togglePause);
    // Channel up / down, like a TV remote.
    for (const auto key : {Qt::Key_PageDown, Qt::Key_MediaNext})
        connect(new QShortcut(QKeySequence(key), this), &QShortcut::activated, this, [this] { stepChannel(1); });
    for (const auto key : {Qt::Key_PageUp, Qt::Key_MediaPrevious})
        connect(new QShortcut(QKeySequence(key), this), &QShortcut::activated, this, [this] { stepChannel(-1); });
    connect(new QShortcut(QKeySequence::Find, this), &QShortcut::activated, this, [this] { m_search->setFocus(); });

    setListsVisible(m_settings.value(QStringLiteral("listsVisible"), true).toBool());

    const QByteArray geometry = m_settings.value(QStringLiteral("geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
}

// ---------------------------------------------------------------------------
// Playlists tree
// ---------------------------------------------------------------------------

void MainWindow::rebuildTree()
{
    m_treeModel->clear();
    QStandardItem *root = m_treeModel->invisibleRootItem();

    QStandardItem *fav = makeItem(QStringLiteral("Favoritos"), kFavoritesUrl);
    fav->setIcon(QIcon::fromTheme(QStringLiteral("starred")));
    root->appendRow(fav);

    QStandardItem *mine = makeItem(QStringLiteral("Mis listas"));
    mine->setIcon(QIcon::fromTheme(QStringLiteral("folder")));
    root->appendRow(mine);
    for (const UserList &l : m_store.userLists()) {
        QStandardItem *item = makeItem(l.name, l.url);
        item->setData(true, UserListRole);
        item->setToolTip(l.url);
        mine->appendRow(item);
    }

    if (m_catalog.isEmpty()) {
        QStandardItem *pending = makeItem(m_catalogFailed ? QStringLiteral("Catálogo de iptv-org no disponible")
                                                          : QStringLiteral("Descargando catálogo de iptv-org…"));
        pending->setEnabled(false);
        root->appendRow(pending);
    } else {
        root->appendRow(makeItem(QStringLiteral("Todos los canales (iptv-org)"), QString::fromLatin1(Catalog::kMainIndexUrl)));
    }
    for (const CatalogNode &group : std::as_const(m_catalog)) {
        QStandardItem *item = makeItem(groupTitle(group.title));
        item->setIcon(QIcon::fromTheme(QStringLiteral("folder")));
        root->appendRow(item);
        for (const CatalogNode &child : group.children)
            appendCatalogNode(item, child);
    }

    if (mine->rowCount() > 0)
        m_tree->expand(m_treeProxy->mapFromSource(mine->index()));
}

QStandardItem *MainWindow::findListItem(const QString &url, QStandardItem *parent) const
{
    if (url.isEmpty())
        return nullptr;
    if (!parent)
        parent = m_treeModel->invisibleRootItem();
    for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem *child = parent->child(i);
        if (child->data(UrlRole).toString() == url)
            return child;
        if (QStandardItem *found = findListItem(url, child))
            return found;
    }
    return nullptr;
}

void MainWindow::selectList(const QString &url)
{
    QStandardItem *item = findListItem(url);
    if (!item)
        return;
    const QModelIndex index = m_treeProxy->mapFromSource(item->index());
    if (!index.isValid())
        return;
    // currentChanged triggers the load unless this list is already shown.
    m_tree->setCurrentIndex(index);
    m_tree->scrollTo(index);
}

void MainWindow::selectDefaultList()
{
    // Start with the channels of the user's country when iptv-org has them.
    const QString territory = QLocale::territoryToCode(QLocale::system().territory()).toLower();
    const QString countryUrl = QStringLiteral("https://iptv-org.github.io/iptv/countries/%1.m3u").arg(territory);
    if (!territory.isEmpty() && findListItem(countryUrl))
        selectList(countryUrl);
    else
        selectList(QString::fromLatin1(Catalog::kMainIndexUrl));
}

void MainWindow::showTreeMenu(const QPoint &pos)
{
    const QModelIndex index = m_tree->indexAt(pos);
    const QString url = index.data(UrlRole).toString();

    QMenu menu(this);
    if (!url.isEmpty()) {
        menu.addAction(QStringLiteral("Recargar"), this, [this, url] { loadPlaylist(url); });
    }
    if (index.data(UserListRole).toBool()) {
        menu.addAction(QStringLiteral("Eliminar lista"), this, [this, url] { removeUserList(url); });
    }
    menu.addSeparator();
    menu.addAction(QStringLiteral("Agregar lista…"), this, [this] { addUserList(); });
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

void MainWindow::removeUserList(const QString &url)
{
    m_store.removeUserList(url);
    const QString current = m_currentList;
    rebuildTree();
    if (current == url) {
        m_currentList.clear();
        selectDefaultList();
    } else {
        selectList(current);
    }
    emit userListsChanged();
}

void MainWindow::addUserList(QWidget *dialogParent)
{
    DDialog dialog(dialogParent ? dialogParent : this);
    dialog.setTitle(QStringLiteral("Agregar lista"));
    dialog.setMessage(QStringLiteral("Dirección web o archivo local de una lista M3U / M3U8"));
    dialog.setIcon(windowIcon());

    auto *content = new QWidget(&dialog);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *nameEdit = new DLineEdit(content);
    nameEdit->setPlaceholderText(QStringLiteral("Nombre (opcional)"));

    auto *urlRow = new QHBoxLayout;
    auto *urlEdit = new DLineEdit(content);
    urlEdit->setPlaceholderText(QStringLiteral("https://ejemplo.com/lista.m3u"));
    auto *browse = new QPushButton(QStringLiteral("Examinar…"), content);
    urlRow->addWidget(urlEdit, 1);
    urlRow->addWidget(browse);

    layout->addWidget(nameEdit);
    layout->addLayout(urlRow);
    dialog.addContent(content);
    dialog.addButton(QStringLiteral("Cancelar"));
    const int acceptIndex = dialog.addButton(QStringLiteral("Agregar"), true, DDialog::ButtonRecommend);

    connect(browse, &QPushButton::clicked, &dialog, [&dialog, urlEdit] {
        const QString file = QFileDialog::getOpenFileName(&dialog, QStringLiteral("Elegir lista"), QDir::homePath(),
                                                          QStringLiteral("Listas M3U (*.m3u *.m3u8);;Todos los archivos (*)"));
        if (!file.isEmpty())
            urlEdit->setText(file);
    });
    urlEdit->setFocus();

    if (dialog.exec() != acceptIndex)
        return;

    const QString url = urlEdit->text().trimmed();
    if (url.isEmpty())
        return;

    const QUrl parsed = QUrl::fromUserInput(url);
    if (!parsed.isValid() || (parsed.isLocalFile() && !QFileInfo::exists(parsed.toLocalFile()))) {
        notify(QStringLiteral("La dirección o el archivo no es válido: %1").arg(url), true);
        return;
    }

    QString name = nameEdit->text().trimmed();
    if (name.isEmpty())
        name = QFileInfo(parsed.path()).completeBaseName();
    if (name.isEmpty())
        name = parsed.host();

    if (!m_store.addUserList({name, url})) {
        notify(QStringLiteral("Esa lista ya está agregada"));
    } else {
        rebuildTree();
        emit userListsChanged();
    }
    m_currentList.clear();
    selectList(url);
}

// ---------------------------------------------------------------------------
// iptv-org catalogue
// ---------------------------------------------------------------------------

void MainWindow::loadCatalog()
{
    // The copy downloaded on a previous run, if there is one.
    QFile cached(catalogCachePath());
    if (cached.open(QIODevice::ReadOnly)) {
        m_catalogData = cached.readAll();
        m_catalog = Catalog::parse(m_catalogData);
    }
}

void MainWindow::refreshCatalog(bool notifyUser)
{
    QNetworkRequest req(QUrl(QString::fromLatin1(Catalog::kSourceUrl)));
    req.setTransferTimeout(20000);
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, notifyUser] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            if (notifyUser)
                emit catalogStatus(QStringLiteral("No se pudo actualizar: %1").arg(reply->errorString()));
            if (m_catalog.isEmpty()) {
                // First start without network: there is nothing to fall back on.
                m_catalogFailed = true;
                const QString current = m_currentList;
                rebuildTree();
                selectList(current);
                notify(QStringLiteral("No se pudo descargar el catálogo de iptv-org. Comprueba la conexión "
                                      "y vuelve a intentarlo desde Ajustes."), true);
            }
            return;
        }
        const QByteArray data = reply->readAll();
        const QList<CatalogNode> parsed = Catalog::parse(data);
        if (parsed.isEmpty()) {
            if (notifyUser)
                emit catalogStatus(QStringLiteral("El catálogo descargado no tiene un formato reconocido."));
            return;
        }
        const bool changed = data != m_catalogData;
        if (changed) {
            QSaveFile out(catalogCachePath());
            if (out.open(QIODevice::WriteOnly)) {
                out.write(data);
                out.commit();
            }
            m_catalogData = data;
            m_catalog = parsed;
            m_catalogFailed = false;
            const QString current = m_currentList;
            rebuildTree();
            if (!current.isEmpty()) {
                selectList(current);
            } else {
                // Nothing was showing yet: first start, or the cache was lost.
                const QString last = m_settings.value(QStringLiteral("lastList")).toString();
                if (!last.isEmpty() && findListItem(last))
                    selectList(last);
                else
                    selectDefaultList();
            }
        }
        if (notifyUser)
            emit catalogStatus(changed ? QStringLiteral("Catálogo actualizado con cambios nuevos.")
                                       : QStringLiteral("El catálogo ya estaba al día."));
    });
}

// ---------------------------------------------------------------------------
// Channels
// ---------------------------------------------------------------------------

void MainWindow::loadPlaylist(const QString &url)
{
    if (m_listReply) {
        m_listReply->disconnect(this);
        m_listReply->abort();
        m_listReply->deleteLater();
        m_listReply = nullptr;
    }

    m_currentList = url;
    m_settings.setValue(QStringLiteral("lastList"), url);

    if (url == kFavoritesUrl) {
        setChannels(m_store.favorites());
        return;
    }

    const QUrl parsed = QUrl::fromUserInput(url);
    if (parsed.isLocalFile()) {
        QFile file(parsed.toLocalFile());
        if (!file.open(QIODevice::ReadOnly)) {
            setChannels({});
            notify(QStringLiteral("No se pudo abrir %1: %2").arg(file.fileName(), file.errorString()), true);
            return;
        }
        setChannels(M3uParser::parse(file.readAll()));
        return;
    }

    setChannels({});
    m_countLabel->setText(QStringLiteral("Cargando lista…"));

    QNetworkRequest req(parsed);
    req.setTransferTimeout(30000);
    QNetworkReply *reply = m_nam->get(req);
    m_listReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_countLabel->setText(QStringLiteral("No se pudo cargar la lista"));
            notify(QStringLiteral("No se pudo cargar la lista: %1").arg(reply->errorString()), true);
            return;
        }
        setChannels(M3uParser::parse(reply->readAll()));
    });
}

void MainWindow::setChannels(const ChannelList &channels)
{
    m_channels->setChannels(channels);

    const QString previousGroup = m_groupCombo->currentIndex() > 0 ? m_groupCombo->currentText() : QString();
    {
        const QSignalBlocker blocker(m_groupCombo);
        m_groupCombo->clear();
        m_groupCombo->addItem(QStringLiteral("Todos los grupos"));
        m_groupCombo->addItems(m_channels->groups());
        const int index = previousGroup.isEmpty() ? 0 : m_groupCombo->findText(previousGroup);
        m_groupCombo->setCurrentIndex(qMax(0, index));
    }
    m_groupCombo->setVisible(m_groupCombo->count() > 2);
    m_channelProxy->setGroup(m_groupCombo->currentIndex() > 0 ? m_groupCombo->currentText() : QString());
    updateCount();
}

void MainWindow::updateCount()
{
    const int shown = m_channelProxy->rowCount();
    const int total = m_channels->rowCount();
    if (total == 0)
        m_countLabel->setText(m_currentList == kFavoritesUrl ? QStringLiteral("Aún no tienes favoritos")
                                                             : QStringLiteral("Sin canales"));
    else if (shown == total)
        m_countLabel->setText(QStringLiteral("%1 canales").arg(total));
    else
        m_countLabel->setText(QStringLiteral("%1 de %2 canales").arg(shown).arg(total));

    if (m_scanner && m_scanner->isRunning())
        m_countLabel->setText(m_countLabel->text()
                              + QStringLiteral(" · escaneando %1/%2").arg(m_scanner->done()).arg(m_scanner->total()));
}

void MainWindow::showChannelMenu(const QPoint &pos)
{
    const QModelIndex index = m_channelView->indexAt(pos);
    if (!index.isValid())
        return;
    const Channel ch = m_channels->channelAt(m_channelProxy->mapToSource(index).row());

    QMenu menu(this);
    menu.addAction(QStringLiteral("Reproducir"), this, [this, ch] { playChannel(ch); });
    menu.addAction(m_store.isFavorite(ch.url) ? QStringLiteral("Quitar de favoritos")
                                              : QStringLiteral("Añadir a favoritos"),
                   this, [this, ch] { toggleFavorite(ch); });
    menu.exec(m_channelView->viewport()->mapToGlobal(pos));
}

void MainWindow::playChannel(const Channel &ch)
{
    if (ch.url == m_current.url && m_video->isPlaying())
        return;
    m_current = ch;
    m_titleLabel->setText(ch.name);
    m_titleLabel->setToolTip(ch.url);
    updateFavoriteButton();
    m_video->play(ch);
}

void MainWindow::stepChannel(int delta)
{
    // Follows whatever the channel list is showing, filters included.
    const int count = m_channelProxy->rowCount();
    if (count == 0)
        return;
    const QModelIndex current = m_channelView->currentIndex();
    int row;
    if (current.isValid())
        row = ((current.row() + delta) % count + count) % count;
    else
        row = delta > 0 ? 0 : count - 1;

    const QModelIndex index = m_channelProxy->index(row, 0);
    m_channelView->setCurrentIndex(index);
    m_channelView->scrollTo(index);
    playChannel(m_channels->channelAt(m_channelProxy->mapToSource(index).row()));
}

void MainWindow::openSettings()
{
    SettingsDialog::Values values;
    values.tuningEffect = m_settings.value(QStringLiteral("tuningEffect"), QStringLiteral("static")).toString();
    values.tuningColor = QColor(m_settings.value(QStringLiteral("tuningColor"), QStringLiteral("#103a8c")).toString());
    values.hardwareDecoding = m_settings.value(QStringLiteral("hardwareDecoding"), false).toBool();
    values.hideBadChannels = m_settings.value(QStringLiteral("hideBadChannels"), false).toBool();
    values.scannerAvailable = ChannelScanner::available();

    SettingsDialog dialog(values, this);
    dialog.setUserLists(m_store.userLists());

    const auto showScanState = [this, &dialog] {
        int good = m_scanGood, bad = m_scanBad;
        if (!m_scanner->isRunning())
            countScanResults(good, bad);
        dialog.setScanState(m_scanner->isRunning(), m_scanner->done(), m_scanner->total(), m_channels->rowCount(), good, bad);
    };
    showScanState();

    connect(&dialog, &SettingsDialog::tuningEffectChanged, this, [this](const QString &effect, const QColor &color) {
        m_settings.setValue(QStringLiteral("tuningEffect"), effect);
        m_settings.setValue(QStringLiteral("tuningColor"), color.name());
        applyTuningEffect();
    });
    connect(&dialog, &SettingsDialog::hardwareDecodingChanged, this,
            [this](bool on) { m_settings.setValue(QStringLiteral("hardwareDecoding"), on); });
    connect(&dialog, &SettingsDialog::catalogRefreshRequested, this, [this] { refreshCatalog(true); });
    connect(this, &MainWindow::catalogStatus, &dialog, &SettingsDialog::setCatalogStatus);
    connect(&dialog, &SettingsDialog::addListRequested, this, [this, &dialog] { addUserList(&dialog); });
    connect(&dialog, &SettingsDialog::removeListRequested, this, &MainWindow::removeUserList);
    connect(this, &MainWindow::userListsChanged, &dialog, [this, &dialog] { dialog.setUserLists(m_store.userLists()); });

    connect(&dialog, &SettingsDialog::scanToggled, this, [this] {
        if (m_scanner->isRunning()) {
            m_scanner->stop();
        } else {
            m_scanGood = m_scanBad = 0;
            m_scanner->start(m_channels->channels());
        }
    });
    connect(&dialog, &SettingsDialog::hideBadChannelsChanged, this, [this](bool hide) {
        m_settings.setValue(QStringLiteral("hideBadChannels"), hide);
        m_channelProxy->setHideBad(hide);
        updateCount();
    });
    connect(&dialog, &SettingsDialog::clearResultsRequested, this, [this] {
        m_store.clearStatus();
        scanStatusChanged();
        emit scanStateChanged();
    });
    connect(this, &MainWindow::scanStateChanged, &dialog, showScanState);

    dialog.exec();
}

void MainWindow::countScanResults(int &good, int &bad) const
{
    good = bad = 0;
    for (const Channel &c : m_channels->channels()) {
        switch (m_store.status(c.url)) {
        case PlaylistStore::Status::Good: ++good; break;
        case PlaylistStore::Status::Bad: ++bad; break;
        case PlaylistStore::Status::Unknown: break;
        }
    }
}

void MainWindow::scanStatusChanged()
{
    m_channels->statusChanged();
    m_channelProxy->statusChanged();
    updateCount();
}

void MainWindow::applyTuningEffect()
{
    const QString effect = m_settings.value(QStringLiteral("tuningEffect"), QStringLiteral("static")).toString();
    const QColor color(m_settings.value(QStringLiteral("tuningColor"), QStringLiteral("#103a8c")).toString());
    m_video->setTuningEffect(effect == QLatin1String("nosignal") ? VideoWidget::TuningEffect::NoSignal
                             : effect == QLatin1String("color")  ? VideoWidget::TuningEffect::SolidColor
                                                                 : VideoWidget::TuningEffect::Static,
                             color);
}

void MainWindow::toggleFavorite(const Channel &ch)
{
    m_store.toggleFavorite(ch);
    updateFavoriteButton();
    if (m_currentList == kFavoritesUrl)
        setChannels(m_store.favorites());
}

void MainWindow::updateFavoriteButton()
{
    const bool fav = !m_current.url.isEmpty() && m_store.isFavorite(m_current.url);
    m_favButton->setIcon(starIcon(fav ? QColor(0xf5, 0xa6, 0x23) : palette().color(QPalette::ButtonText), fav));
    m_favButton->setToolTip(fav ? QStringLiteral("Quitar de favoritos") : QStringLiteral("Añadir a favoritos"));
}

// ---------------------------------------------------------------------------
// Window
// ---------------------------------------------------------------------------

void MainWindow::toggleFullScreen()
{
    m_videoFullScreen = !m_videoFullScreen;
    m_listsPanel->setVisible(!m_videoFullScreen && m_listsVisible);
    m_channelsPanel->setVisible(!m_videoFullScreen);
    m_controls->setVisible(!m_videoFullScreen);
    titlebar()->setVisible(!m_videoFullScreen);

    if (m_videoFullScreen) {
        m_wasMaximized = isMaximized();
        showFullScreen();
    } else if (m_wasMaximized) {
        showMaximized();
    } else {
        showNormal();
    }
}

void MainWindow::setListsVisible(bool visible)
{
    m_listsVisible = visible;
    m_settings.setValue(QStringLiteral("listsVisible"), visible);
    if (!m_videoFullScreen)
        m_listsPanel->setVisible(visible);
}

void MainWindow::notify(const QString &text, bool warning)
{
    DMessageManager::instance()->sendMessage(
        this, QIcon::fromTheme(warning ? QStringLiteral("dialog-warning") : QStringLiteral("dialog-information")), text);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_videoFullScreen)
        m_settings.setValue(QStringLiteral("geometry"), saveGeometry());
    m_settings.setValue(QStringLiteral("volume"), m_volume->value());
    m_video->stop();
    DMainWindow::closeEvent(event);
}
