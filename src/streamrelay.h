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
    static QString defaultUserAgent(); // browser-like; many servers reject ffmpeg's own

    void start(const QString &url, const QString &userAgent, const QString &referrer);
    void stop();

signals:
    void ready(const QUrl &localUrl); // first data arrived; safe to start playing
    void failed(const QString &reason);

private:
    void launch(bool lenientHls);
    void acceptClient();
    void flush();

    QTcpServer m_server;
    QPointer<QProcess> m_process;
    QPointer<QTcpSocket> m_client;
    QTimer m_timeout;
    QByteArray m_buffer;
    QString m_url;
    QString m_userAgent;
    QString m_referrer;
    int m_session = 0;
    bool m_ready = false;
    bool m_headerSent = false;
};
