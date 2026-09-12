#pragma once

#include "channel.h"

#include <QColor>
#include <QImage>
#include <QMediaPlayer>
#include <QTimer>
#include <QVideoFrame>
#include <QWidget>

class QAudioOutput;
class QVideoSink;
class StreamRelay;

// Video surface + player. Frames are painted by the widget itself so that
// mouse events and the status overlay stay under our control.
class VideoWidget : public QWidget
{
    Q_OBJECT
public:
    // What fills the screen while a channel is being tuned or has no signal.
    enum class TuningEffect { Static, NoSignal, SolidColor };

    explicit VideoWidget(QWidget *parent = nullptr);

    void setTuningEffect(TuningEffect effect, const QColor &color);

    void play(const Channel &channel);
    void stop();
    void togglePause();
    bool isPlaying() const;

    void setVolume(int percent);
    void setMuted(bool muted);
    bool isMuted() const;

signals:
    void playingChanged(bool playing);
    void doubleClicked();

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;

private:
    void setOverlay(const QString &text);
    void fail(const QString &reason);
    void setTuning(bool tuning);
    void paintTuningEffect(QPainter &p);

    QMediaPlayer *m_player;
    QAudioOutput *m_audio;
    QVideoSink *m_sink;
    StreamRelay *m_relay;
    QVideoFrame m_frame;
    QString m_title;
    QString m_overlay;

    TuningEffect m_effect = TuningEffect::Static;
    QColor m_effectColor = QColor(0x10, 0x3a, 0x8c);
    bool m_tuning = false; // no picture yet (or lost): show the effect
    QTimer m_noiseTimer;
    QImage m_noise;
};
