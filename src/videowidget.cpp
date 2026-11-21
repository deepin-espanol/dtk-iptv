#include "videowidget.h"

#include "streamrelay.h"

#include <QAudioOutput>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QVideoSink>

VideoWidget::VideoWidget(QWidget *parent)
    : QWidget(parent)
    , m_player(new QMediaPlayer(this))
    , m_audio(new QAudioOutput(this))
    , m_sink(new QVideoSink(this))
    , m_relay(new StreamRelay(this))
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMinimumSize(320, 180);
    setOverlay(QStringLiteral("Elige un canal para empezar"));

    m_player->setAudioOutput(m_audio);
    m_player->setVideoSink(m_sink);

    m_noise = QImage(192, 108, QImage::Format_Grayscale8);
    m_noiseTimer.setInterval(50);
    connect(&m_noiseTimer, &QTimer::timeout, this, qOverload<>(&QWidget::update));

    connect(m_sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        m_frame = frame;
        if (frame.isValid())
            setTuning(false);
        update();
    });

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        switch (status) {
        case QMediaPlayer::LoadingMedia:
        case QMediaPlayer::BufferingMedia:
        case QMediaPlayer::StalledMedia:
            setOverlay(QStringLiteral("Cargando %1…").arg(m_title));
            break;
        case QMediaPlayer::BufferedMedia:
        case QMediaPlayer::LoadedMedia:
            setOverlay(QString());
            // Radio and other audio-only streams never deliver a frame.
            if (!m_player->hasVideo())
                setTuning(false);
            break;
        case QMediaPlayer::EndOfMedia:
            m_frame = QVideoFrame();
            setTuning(true);
            setOverlay(QStringLiteral("La emisión ha terminado"));
            break;
        default:
            break;
        }
    });

    connect(m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &text) {
        if (error == QMediaPlayer::NoError)
            return;
        fail(text);
    });

    connect(m_relay, &StreamRelay::ready, this, [this](const QUrl &localUrl) {
        m_player->setSource(localUrl);
        m_player->play();
    });
    connect(m_relay, &StreamRelay::failed, this, &VideoWidget::fail);

    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        emit playingChanged(state == QMediaPlayer::PlayingState);
    });
}

void VideoWidget::play(const Channel &channel)
{
    m_title = channel.name;
    m_frame = QVideoFrame();
    m_relay->stop();
    m_player->stop();
    setTuning(true);

    const QUrl url = QUrl::fromUserInput(channel.url);
    if (!url.isLocalFile() && StreamRelay::available()) {
        setOverlay(QStringLiteral("Conectando con %1…").arg(m_title));
        m_player->setSource(QUrl());
        m_relay->start(channel.url, channel.userAgent, channel.referrer);
    } else {
        setOverlay(QStringLiteral("Cargando %1…").arg(m_title));
        m_player->setSource(url);
        m_player->play();
    }
}

void VideoWidget::fail(const QString &reason)
{
    m_relay->stop();
    m_player->stop();
    m_frame = QVideoFrame();
    setTuning(true);
    setOverlay(QStringLiteral("No se puede reproducir %1\n%2").arg(m_title, reason));
}

void VideoWidget::stop()
{
    m_relay->stop();
    m_player->stop();
    m_player->setSource(QUrl());
    m_frame = QVideoFrame();
    setTuning(false);
    setOverlay(QStringLiteral("Detenido"));
}

void VideoWidget::togglePause()
{
    if (m_player->source().isEmpty())
        return;
    if (isPlaying())
        m_player->pause();
    else
        m_player->play();
}

bool VideoWidget::isPlaying() const
{
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}

void VideoWidget::setVolume(int percent)
{
    m_audio->setVolume(qBound(0, percent, 100) / 100.0f);
}

void VideoWidget::setMuted(bool muted)
{
    m_audio->setMuted(muted);
}

bool VideoWidget::isMuted() const
{
    return m_audio->isMuted();
}

void VideoWidget::setTuningEffect(TuningEffect effect, const QColor &color)
{
    m_effect = effect;
    if (color.isValid())
        m_effectColor = color;
    setTuning(m_tuning);
    update();
}

void VideoWidget::setTuning(bool tuning)
{
    m_tuning = tuning;
    // Only the static needs animating.
    if (tuning && m_effect == TuningEffect::Static)
        m_noiseTimer.start();
    else
        m_noiseTimer.stop();
    update();
}

void VideoWidget::paintTuningEffect(QPainter &p)
{
    switch (m_effect) {
    case TuningEffect::Static: {
        QRandomGenerator *rng = QRandomGenerator::global();
        for (int y = 0; y < m_noise.height(); ++y) {
            uchar *line = m_noise.scanLine(y);
            for (int x = 0; x < m_noise.width(); x += 4) {
                const quint32 r = rng->generate();
                line[x] = r & 0xff;
                line[x + 1] = (r >> 8) & 0xff;
                line[x + 2] = (r >> 16) & 0xff;
                line[x + 3] = (r >> 24) & 0xff;
            }
        }
        p.drawImage(rect(), m_noise);
        break;
    }
    case TuningEffect::NoSignal: {
        static const QColor bars[] = {QColor(192, 192, 192), QColor(192, 192, 0), QColor(0, 192, 192),
                                      QColor(0, 192, 0),     QColor(192, 0, 192), QColor(192, 0, 0),
                                      QColor(0, 0, 192)};
        const int count = int(std::size(bars));
        for (int i = 0; i < count; ++i) {
            const int x0 = width() * i / count;
            const int x1 = width() * (i + 1) / count;
            p.fillRect(x0, 0, x1 - x0, height(), bars[i]);
        }
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(qBound(18, height() / 11, 64));
        p.setFont(f);
        const QString label = QStringLiteral("SIN SEÑAL");
        QRect box = p.fontMetrics().boundingRect(label).adjusted(-24, -10, 24, 10);
        box.moveCenter(QPoint(width() / 2, height() / 4));
        p.fillRect(box, Qt::black);
        p.setPen(Qt::white);
        p.drawText(box, Qt::AlignCenter, label);
        p.setFont(font());
        break;
    }
    case TuningEffect::SolidColor:
        p.fillRect(rect(), m_effectColor);
        break;
    }
}

void VideoWidget::setOverlay(const QString &text)
{
    if (m_overlay == text)
        return;
    m_overlay = text;
    update();
}

void VideoWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    if (m_frame.isValid()) {
        QVideoFrame::PaintOptions options;
        options.backgroundColor = Qt::black;
        options.aspectRatioMode = Qt::KeepAspectRatio;
        m_frame.paint(&p, rect(), options);
    } else if (m_tuning) {
        paintTuningEffect(p);
    }

    // Audio-only streams never deliver a frame: show the title instead.
    const QString text = !m_overlay.isEmpty() ? m_overlay : (m_frame.isValid() ? QString() : m_title);
    if (text.isEmpty())
        return;

    QFont f = font();
    f.setPointSizeF(f.pointSizeF() * 1.3);
    p.setFont(f);
    const QRect box = p.fontMetrics()
                          .boundingRect(rect().adjusted(40, 40, -40, -40), Qt::AlignCenter | Qt::TextWordWrap, text)
                          .adjusted(-16, -10, 16, 10);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 150));
    p.drawRoundedRect(box, 8, 8);
    p.setPen(Qt::white);
    p.drawText(box, Qt::AlignCenter | Qt::TextWordWrap, text);
}

void VideoWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit doubleClicked();
}
