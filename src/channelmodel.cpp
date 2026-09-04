#include "channelmodel.h"

#include "playliststore.h"

#include <QGuiApplication>
#include <QPalette>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>

namespace {
const QSize kLogoSize(56, 40);
const int kScale = 2; // rendered at 2x so logos stay sharp on HiDPI
const int kMaxActive = 6;
const int kMaxQueued = 60;
const int kMaxCached = 4000;

// Bounding box of the visible pixels, so padded logos don't look tiny.
QRect visibleRect(const QImage &img)
{
    int left = img.width(), top = img.height(), right = -1, bottom = -1;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            if (qAlpha(line[x]) > 24) {
                left = qMin(left, x);
                right = qMax(right, x);
                top = qMin(top, y);
                bottom = qMax(bottom, y);
            }
        }
    }
    return right < 0 ? img.rect() : QRect(QPoint(left, top), QPoint(right, bottom));
}

QPixmap makeTile(const QColor &background)
{
    QPixmap tile(kLogoSize * kScale);
    tile.fill(Qt::transparent);
    QPainter p(&tile);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(background);
    p.drawRoundedRect(tile.rect(), 6 * kScale, 6 * kScale);
    return tile;
}

// Every logo ends up on a tile of the same size: trimmed, centred and scaled
// to fit, over a background that contrasts with the logo's own colours.
QIcon makeLogoIcon(QImage img)
{
    if (img.width() > 320 || img.height() > 320)
        img = img.scaled(320, 320, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    img = img.convertToFormat(QImage::Format_ARGB32);
    img = img.copy(visibleRect(img));

    qint64 luminance = 0, weight = 0;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const int a = qAlpha(line[x]);
            luminance += qint64(qGray(line[x])) * a;
            weight += a;
        }
    }
    const bool lightLogo = weight > 0 && luminance / weight > 185;

    QPixmap tile = makeTile(lightLogo ? QColor(0x30, 0x33, 0x3a) : QColor(0xf3, 0xf4, 0xf6));
    const int margin = 4 * kScale;
    const QSize box = tile.size() - QSize(2 * margin, 2 * margin);
    const QImage scaled = img.scaled(box, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPainter p(&tile);
    p.drawImage((tile.width() - scaled.width()) / 2, (tile.height() - scaled.height()) / 2, scaled);
    p.end();
    tile.setDevicePixelRatio(kScale);
    return QIcon(tile);
}

QIcon makePlaceholder()
{
    QPixmap tile = makeTile(QColor(128, 128, 128, 60));
    QPainter p(&tile);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(128, 128, 128, 170));
    const QPointF c(tile.width() / 2.0, tile.height() / 2.0);
    const qreal r = 9 * kScale;
    p.drawPolygon(QPolygonF{{c.x() - r * 0.7, c.y() - r}, {c.x() + r, c.y()}, {c.x() - r * 0.7, c.y() + r}});
    p.end();
    tile.setDevicePixelRatio(kScale);
    return QIcon(tile);
}

} // namespace

QSize ChannelModel::logoSize()
{
    return kLogoSize;
}

ChannelModel::ChannelModel(QNetworkAccessManager *nam, const PlaylistStore *store, QObject *parent)
    : QAbstractListModel(parent)
    , m_nam(nam)
    , m_store(store)
    , m_placeholder(makePlaceholder())
{
}

void ChannelModel::setChannels(const ChannelList &channels)
{
    beginResetModel();
    m_channels = channels;
    for (const QString &url : std::as_const(m_queue))
        m_pending.remove(url);
    m_queue.clear();
    if (m_logos.size() > kMaxCached)
        m_logos.clear();
    endResetModel();
}

QStringList ChannelModel::groups() const
{
    QSet<QString> set;
    for (const Channel &c : m_channels) {
        const auto parts = c.group.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString &g : parts)
            set.insert(g.trimmed());
    }
    QStringList list(set.cbegin(), set.cend());
    list.sort(Qt::CaseInsensitive);
    return list;
}

void ChannelModel::statusChanged()
{
    if (!m_channels.isEmpty())
        emit dataChanged(index(0), index(m_channels.size() - 1), {Qt::ForegroundRole, Qt::ToolTipRole});
}

int ChannelModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_channels.size();
}

QVariant ChannelModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_channels.size())
        return {};
    const Channel &c = m_channels.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        return c.name;
    case Qt::ToolTipRole: {
        QString tip = c.group.isEmpty() ? c.url : c.group + QLatin1Char('\n') + c.url;
        if (m_store->isBad(c.url))
            tip += QStringLiteral("\nNo funcionaba en el último escaneo");
        return tip;
    }
    case Qt::ForegroundRole:
        if (m_store->isBad(c.url))
            return QGuiApplication::palette().brush(QPalette::Disabled, QPalette::Text);
        return {};
    case Qt::DecorationRole: {
        if (c.logo.isEmpty())
            return m_placeholder;
        const auto it = m_logos.constFind(c.logo);
        if (it != m_logos.cend())
            return it->isNull() ? m_placeholder : *it;
        const_cast<ChannelModel *>(this)->requestLogo(c.logo);
        return m_placeholder;
    }
    default:
        return {};
    }
}

void ChannelModel::requestLogo(const QString &url)
{
    if (m_pending.contains(url))
        return;
    m_pending.insert(url);
    m_queue.append(url);
    // The user scrolled past these before we got to them; drop the oldest.
    while (m_queue.size() > kMaxQueued)
        m_pending.remove(m_queue.takeFirst());
    pumpQueue();
}

void ChannelModel::pumpQueue()
{
    while (m_active < kMaxActive && !m_queue.isEmpty()) {
        const QString url = m_queue.takeLast();
        const QUrl qurl(url);
        if (!qurl.isValid() || !qurl.scheme().startsWith(QLatin1String("http"))) {
            m_pending.remove(url);
            m_logos.insert(url, QIcon());
            continue;
        }

        QNetworkRequest req(qurl);
        req.setTransferTimeout(10000);
        QNetworkReply *reply = m_nam->get(req);
        ++m_active;
        connect(reply, &QNetworkReply::finished, this, [this, reply, url] {
            reply->deleteLater();
            --m_active;
            m_pending.remove(url);

            QImage image;
            QIcon icon;
            if (reply->error() == QNetworkReply::NoError && image.loadFromData(reply->readAll()) && !image.isNull())
                icon = makeLogoIcon(image);
            m_logos.insert(url, icon);

            if (!icon.isNull()) {
                for (int row = 0; row < m_channels.size(); ++row) {
                    if (m_channels.at(row).logo == url) {
                        const QModelIndex idx = index(row);
                        emit dataChanged(idx, idx, {Qt::DecorationRole});
                    }
                }
            }
            pumpQueue();
        });
    }
}

void ChannelFilterModel::setSearch(const QString &text)
{
    m_search = text.trimmed();
    invalidateFilter();
}

void ChannelFilterModel::setGroup(const QString &group)
{
    m_group = group;
    invalidateFilter();
}

void ChannelFilterModel::setHideBad(bool hide)
{
    m_hideBad = hide;
    invalidateFilter();
}

void ChannelFilterModel::statusChanged()
{
    if (m_hideBad)
        invalidateFilter();
}

bool ChannelFilterModel::filterAcceptsRow(int row, const QModelIndex &) const
{
    const auto *model = static_cast<ChannelModel *>(sourceModel());
    const Channel &c = model->channelAt(row);
    if (m_hideBad && model->store()->isBad(c.url))
        return false;
    if (!m_search.isEmpty() && !c.name.contains(m_search, Qt::CaseInsensitive))
        return false;
    if (!m_group.isEmpty()) {
        const auto parts = c.group.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        bool found = false;
        for (const QString &g : parts) {
            if (g.trimmed() == m_group) {
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
}
