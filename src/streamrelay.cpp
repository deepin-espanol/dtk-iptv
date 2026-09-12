#include "streamrelay.h"

#include <QProcess>
#include <QStandardPaths>
#include <QTcpSocket>

namespace {

const qint64 kMaxBuffered = 32 * 1024 * 1024;
const int kStartTimeoutMs = 25000;
const char kBrowserUserAgent[] =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36";

QString ffmpegPath()
{
    static const QString path = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    return path;
}

// Turns ffmpeg's last words into something a viewer can act on.
QString explain(const QString &log)
{
    const auto has = [&](const char *s) { return log.contains(QLatin1String(s), Qt::CaseInsensitive); };

    if (has("403") || has("401") || has("451") || has("access denied"))
        return QStringLiteral("El servidor deniega el acceso. Suele ser un bloqueo geográfico o un canal protegido.");
    if (has("404") || has("410"))
        return QStringLiteral("El canal ya no existe en esa dirección.");
    if (has("5XX") || has("Server Error"))
        return QStringLiteral("El servidor del canal está dando errores.");
    if (has("timed out") || has("refused") || has("No route") || has("reset by peer")
        || has("Failed to resolve") || has("Name or service") || has("Network is unreachable"))
        return QStringLiteral("El servidor del canal no responde.");
    if (has("Invalid data") || has("does not contain any stream") || has("Input/output error")
        || has("End of file"))
        return QStringLiteral("El canal no está emitiendo ahora o su formato no es compatible.");

    const QStringList lines = log.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    return lines.isEmpty() ? QStringLiteral("El canal no envía datos.") : lines.last().trimmed();
}

} // namespace

StreamRelay::StreamRelay(QObject *parent)
    : QObject(parent)
{
    m_server.listen(QHostAddress::LocalHost, 0);
    connect(&m_server, &QTcpServer::newConnection, this, &StreamRelay::acceptClient);

    m_timeout.setSingleShot(true);
    m_timeout.setInterval(kStartTimeoutMs);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        stop();
        emit failed(QStringLiteral("El servidor del canal no responde."));
    });
}

StreamRelay::~StreamRelay()
{
    stop();
}

QString StreamRelay::defaultUserAgent()
{
    return QString::fromLatin1(kBrowserUserAgent);
}

bool StreamRelay::available()
{
    return !ffmpegPath().isEmpty();
}

void StreamRelay::start(const QString &url, const QString &userAgent, const QString &referrer)
{
    stop();
    if (!m_server.isListening()) {
        emit failed(QStringLiteral("No se pudo abrir el puerto local de reproducción."));
        return;
    }
    m_url = url;
    m_userAgent = userAgent;
    m_referrer = referrer;
    ++m_session;
    m_timeout.start();
    launch(true);
}

void StreamRelay::stop()
{
    m_timeout.stop();
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        // Reap the child once it exits instead of blocking the UI on it.
        connect(m_process, &QProcess::finished, m_process, &QObject::deleteLater);
        if (m_process->state() == QProcess::NotRunning)
            m_process->deleteLater();
        m_process = nullptr;
    }
    if (m_client) {
        m_client->disconnect(this);
        m_client->abort();
        m_client->deleteLater();
        m_client = nullptr;
    }
    m_buffer.clear();
    m_ready = false;
    m_headerSent = false;
}

void StreamRelay::launch(bool lenientHls)
{
    QStringList args{QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"),
                     QStringLiteral("-loglevel"), QStringLiteral("error")};

    if (m_url.startsWith(QLatin1String("http"), Qt::CaseInsensitive)) {
        // Many servers reject ffmpeg's default "Lavf" agent outright.
        args << QStringLiteral("-user_agent")
             << (m_userAgent.isEmpty() ? defaultUserAgent() : m_userAgent);
        if (!m_referrer.isEmpty())
            args << QStringLiteral("-referer") << m_referrer;
        args << QStringLiteral("-reconnect") << QStringLiteral("1")
             << QStringLiteral("-reconnect_streamed") << QStringLiteral("1")
             << QStringLiteral("-reconnect_delay_max") << QStringLiteral("4");
    }
    args << QStringLiteral("-rw_timeout") << QStringLiteral("15000000");
    // Accept HLS segments without a known file extension. The option only
    // exists for HLS input, so other formats are retried without it.
    if (lenientHls)
        args << QStringLiteral("-extension_picky") << QStringLiteral("0");
    args << QStringLiteral("-i") << m_url
         << QStringLiteral("-sn") << QStringLiteral("-dn")
         << QStringLiteral("-c") << QStringLiteral("copy")
         << QStringLiteral("-f") << QStringLiteral("mpegts") << QStringLiteral("pipe:1");

    auto *process = new QProcess(this);
    m_process = process;

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
        m_buffer += process->readAllStandardOutput();
        if (!m_ready) {
            m_ready = true;
            m_timeout.stop();
            emit ready(QUrl(QStringLiteral("http://127.0.0.1:%1/live-%2.ts").arg(m_server.serverPort()).arg(m_session)));
        }
        flush();
    });

    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        process->deleteLater();
        m_process = nullptr;
        m_timeout.stop();
        emit failed(QStringLiteral("No se pudo iniciar ffmpeg."));
    });

    connect(process, &QProcess::finished, this, [this, process, lenientHls] {
        const QString log = QString::fromUtf8(process->readAllStandardError());
        process->deleteLater();
        m_process = nullptr;

        if (m_ready) {
            // The stream ended: let the player drain what is left, then see EOF.
            flush();
            if (m_client)
                m_client->disconnectFromHost();
            return;
        }
        if (lenientHls && log.contains(QLatin1String("Option not found"))) {
            launch(false);
            return;
        }
        m_timeout.stop();
        emit failed(explain(log));
    });

    process->start(ffmpegPath(), args);
}

void StreamRelay::acceptClient()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        if (m_client) {
            m_client->disconnect(this);
            m_client->abort();
            m_client->deleteLater();
        }
        m_client = socket;
        m_headerSent = false;
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            socket->readAll(); // the request itself is irrelevant
            if (socket != m_client || m_headerSent)
                return;
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: video/mp2t\r\nConnection: close\r\n\r\n");
            m_headerSent = true;
            flush();
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void StreamRelay::flush()
{
    if (m_client && m_headerSent && m_client->state() == QAbstractSocket::ConnectedState) {
        // A paused player stops reading; drop data rather than grow forever.
        if (m_client->bytesToWrite() < kMaxBuffered)
            m_client->write(m_buffer);
        m_buffer.clear();
    } else if (m_buffer.size() > kMaxBuffered) {
        m_buffer.clear();
    }
}
