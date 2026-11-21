#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QTcpServer>
#include <QTimer>
#include <QUrl>

class QProcess;
class QTcpSocket;

// Fetches a stream with an ffmpeg child process and re-serves it on localhost.
//
// QMediaPlayer cannot send a custom User-Agent or Referer, retry dropped
// connections or say why a stream failed. ffmpeg can, so it handles the
// network side and the player only ever reads a local MPEG-TS stream.
class StreamRelay : public QObject
{
    Q_OBJECT
public:
    explicit StreamRelay(QObject *parent = nullptr);
    ~StreamRelay() override;

    static bool available();
    // YouTube pages are turned into a stream URL with yt-dlp first.
    static bool isYouTube(const QString &url);
    static bool youtubeAvailable();
    static QString ytdlpPath();
    // Arguments that make yt-dlp print the stream URL(s): one combined, or video then audio.
    static QStringList ytdlpArgs(const QString &url);
    static QString defaultUserAgent(); // browser-like; many servers reject ffmpeg's own

    void start(const QString &url, const QString &userAgent, const QString &referrer);
    void stop();

signals:
    void ready(const QUrl &localUrl); // first data arrived; safe to start playing
    void failed(const QString &reason);

private:
    void resolveYouTube();
    void launch(bool lenientHls);
    static void killProcess(QPointer<QProcess> &process);
    void acceptClient();
    void flush();

    QTcpServer m_server;
    QPointer<QProcess> m_process;
    QPointer<QProcess> m_resolver; // yt-dlp, for YouTube links
    QPointer<QTcpSocket> m_client;
    QTimer m_timeout;
    QByteArray m_buffer;
    QString m_url;
    QString m_audioUrl; // separate audio stream, when the source splits them
    QString m_userAgent;
    QString m_referrer;
    int m_session = 0;
    bool m_ready = false;
    bool m_headerSent = false;
};
