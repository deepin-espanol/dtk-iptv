#pragma once

#include "channel.h"

#include <QObject>

// Checks which channels actually answer, by probing each one with ffprobe
// using the same headers the player would send.
class ChannelScanner : public QObject
{
    Q_OBJECT
public:
    explicit ChannelScanner(QObject *parent = nullptr);
    ~ChannelScanner() override;

    static bool available();

    void start(const ChannelList &channels);
    void stop();

    bool isRunning() const { return m_running; }
    int done() const { return m_done; }
    int total() const { return m_channels.size(); }

signals:
    void channelChecked(const QString &url, bool ok);
    void progress(int done, int total);
    void finished(bool completed); // false when stopped early

private:
    void launchNext();
    void probe(const Channel &channel, bool lenientHls);
    void completeOne();

    ChannelList m_channels;
    int m_next = 0;
    int m_done = 0;
    int m_active = 0;
    int m_generation = 0; // invalidates callbacks of a stopped scan
    bool m_running = false;
};
