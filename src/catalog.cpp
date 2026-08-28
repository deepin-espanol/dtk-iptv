#include "catalog.h"

#include <QRegularExpression>

QList<CatalogNode> Catalog::parse(const QByteArray &markdown)
{
    static const QRegularExpression headingRe(QStringLiteral("^#{3,4}\\s+(.+)$"));
    static const QRegularExpression rowRe(QStringLiteral(
        "<tr><td[^>]*>(.*?)</td><td[^>]*>(\\d+)</td><td[^>]*><code>(https?://[^<]+)</code></td></tr>"));
    static const QRegularExpression bulletRe(QStringLiteral(
        "^(\\s*)- (.+?)\\s*<code>(https?://[^<]+)</code>"));

    QList<CatalogNode> groups;
    // Path of child indexes into the current group, one per bullet depth.
    QList<int> path;

    const auto lines = QString::fromUtf8(markdown).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        QRegularExpressionMatch m = headingRe.match(line);
        if (m.hasMatch()) {
            CatalogNode g;
            g.title = m.captured(1).trimmed();
            groups.append(g);
            path.clear();
            continue;
        }
        if (groups.isEmpty())
            continue;
        CatalogNode &group = groups.last();

        // Bare URL inside a code fence: the "everything in one file" playlist.
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1String("https://")) && !trimmed.contains(QLatin1Char('<'))
            && trimmed.endsWith(QLatin1String(".m3u"))) {
            CatalogNode all;
            all.title = QStringLiteral("*");
            all.url = trimmed;
            group.children.prepend(all);
            path.clear();
            continue;
        }

        m = rowRe.match(line);
        if (m.hasMatch()) {
            CatalogNode n;
            n.title = m.captured(1).trimmed();
            n.channels = m.captured(2).toInt();
            n.url = m.captured(3).trimmed();
            if (n.channels > 0)
                group.children.append(n);
            continue;
        }

        m = bulletRe.match(line);
        if (m.hasMatch()) {
            const int depth = m.captured(1).size() / 2;
            CatalogNode n;
            n.title = m.captured(2).trimmed();
            n.url = m.captured(3).trimmed();

            CatalogNode *parent = &group;
            const int usable = qMin(depth, path.size());
            for (int i = 0; i < usable; ++i)
                parent = &parent->children[path.at(i)];
            parent->children.append(n);
            path.resize(usable);
            path.append(parent->children.size() - 1);
        }
    }

    groups.removeIf([](const CatalogNode &g) { return g.children.isEmpty(); });
    return groups;
}
