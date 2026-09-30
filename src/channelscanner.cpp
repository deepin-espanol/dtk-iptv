#include "channelscanner.h"

#include "streamrelay.h"

#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

namespace {

const int kParallel = 12;
const int kProbeTimeoutMs = 20000;

QString ffprobePath()
{
    static const QString path = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
    return path;
}

} // namespace

ChannelScanner::ChannelScanner(QObject *parent)
    : QObject(parent)
{
}

ChannelScanner::~ChannelScanner()
{
    stop();
}

bool ChannelScanner::available()
{
    return !ffprobePath().isEmpty();
}

void ChannelScanner::start(const ChannelList &channels)
{
    stop();
    m_channels = channels;
    m_next = 0;
    m_done = 0;
    m_active = 0;
    if (m_channels.isEmpty() || !available()) {
        emit finished(false);
        return;
    }
    m_running = true;
    emit progress(0, total());
    launchNext();
}

void ChannelScanner::stop()
{
    if (!m_running)
        return;
    m_running = false;
    ++m_generation;
    const auto processes = findChildren<QProcess *>();
    for (QProcess *process : processes) {
        process->disconnect(this);
        process->kill();
        connect(process, &QProcess::finished, process, &QObject::deleteLater);
        if (process->state() == QProcess::NotRunning)
            process->deleteLater();
    }
    m_active = 0;
    emit finished(false);
}

void ChannelScanner::launchNext()
{
    while (m_running && m_active < kParallel && m_next < m_channels.size()) {
        ++m_active;
        probe(m_channels.at(m_next++), true);
    }
}

void ChannelScanner::probe(const Channel &channel, bool lenientHls)
{
    QStringList args{QStringLiteral("-v"), QStringLiteral("error")};
    if (channel.url.startsWith(QLatin1String("http"), Qt::CaseInsensitive)) {
        args << QStringLiteral("-user_agent")
             << (channel.userAgent.isEmpty() ? StreamRelay::defaultUserAgent() : channel.userAgent);
        if (!channel.referrer.isEmpty())
            args << QStringLiteral("-referer") << channel.referrer;
    }
    args << QStringLiteral("-rw_timeout") << QStringLiteral("10000000");
    if (lenientHls)
        args << QStringLiteral("-extension_picky") << QStringLiteral("0");
    args << QStringLiteral("-show_entries") << QStringLiteral("stream=codec_type")
         << QStringLiteral("-of") << QStringLiteral("csv=p=0") << channel.url;

    auto *process = new QProcess(this);
    auto *killer = new QTimer(process);
    killer->setSingleShot(true);
    connect(killer, &QTimer::timeout, process, &QProcess::kill);
    killer->start(kProbeTimeoutMs);

    const int generation = m_generation;

    connect(process, &QProcess::finished, this,
            [this, process, channel, lenientHls, generation](int exitCode, QProcess::ExitStatus status) {
                const QByteArray out = process->readAllStandardOutput();
                const QByteArray err = process->readAllStandardError();
                process->deleteLater();
                if (generation != m_generation)
                    return;
                // The HLS-only option makes ffprobe reject other formats.
                if (lenientHls && err.contains("Option not found")) {
                    probe(channel, false);
                    return;
                }
                const bool ok = status == QProcess::NormalExit && exitCode == 0 && !out.trimmed().isEmpty();
                emit channelChecked(channel.url, ok);
                completeOne();
            });

    connect(process, &QProcess::errorOccurred, this, [this, process, generation](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        if (generation == m_generation)
            completeOne(); // could not test it: leave its status untouched
    });

    process->start(ffprobePath(), args);
}

void ChannelScanner::completeOne()
{
    --m_active;
    ++m_done;
    emit progress(m_done, total());
    if (m_done >= total()) {
        m_running = false;
        emit finished(true);
    } else {
        launchNext();
    }
}
