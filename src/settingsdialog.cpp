#include "settingsdialog.h"

#include "extrasources.h"

#include <DSwitchButton>

#include <QColorDialog>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

DWIDGET_USE_NAMESPACE

namespace {

QLabel *sectionTitle(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    QFont f = label->font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() * 1.1);
    label->setFont(f);
    label->setContentsMargins(0, 10, 0, 2);
    return label;
}

QLabel *hint(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setEnabled(false); // dimmed, secondary text
    return label;
}

// "Label ........ control" row.
QHBoxLayout *row(const QString &text, QWidget *control, QWidget *parent)
{
    auto *layout = new QHBoxLayout;
    layout->addWidget(new QLabel(text, parent), 1);
    layout->addWidget(control);
    return layout;
}

} // namespace

SettingsDialog::SettingsDialog(const Values &values, QWidget *parent)
    : DDialog(parent)
    , m_color(values.tuningColor)
    , m_scannerAvailable(values.scannerAvailable)
    , m_disabledSources(values.disabledSources)
{
    setTitle(QStringLiteral("Ajustes"));
    setIcon(windowIcon());

    auto *content = new QWidget(this);
    content->setFixedWidth(520);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);

    // --- Playback --------------------------------------------------------
    layout->addWidget(sectionTitle(QStringLiteral("Reproducción"), content));

    m_effectCombo = new QComboBox(content);
    m_effectCombo->addItem(QStringLiteral("Estática"), QStringLiteral("static"));
    m_effectCombo->addItem(QStringLiteral("Sin señal"), QStringLiteral("nosignal"));
    m_effectCombo->addItem(QStringLiteral("Color liso"), QStringLiteral("color"));
    m_effectCombo->setCurrentIndex(qMax(0, m_effectCombo->findData(values.tuningEffect)));
    m_effectCombo->setMinimumWidth(170);
    m_colorButton = new QPushButton(QStringLiteral("Color…"), content);

    auto *effectRow = new QHBoxLayout;
    effectRow->addWidget(new QLabel(QStringLiteral("Pantalla al cambiar de canal"), content), 1);
    effectRow->addWidget(m_colorButton);
    effectRow->addWidget(m_effectCombo);
    layout->addLayout(effectRow);

    auto *hwSwitch = new DSwitchButton(content);
    hwSwitch->setChecked(values.hardwareDecoding);
    layout->addLayout(row(QStringLiteral("Decodificación por hardware"), hwSwitch, content));
    layout->addWidget(hint(QStringLiteral("Desactivada se decodifica por software, que es más compatible. "
                                          "El cambio se aplica al reiniciar la aplicación."), content));

    // --- Playlists -------------------------------------------------------
    layout->addWidget(sectionTitle(QStringLiteral("Listas"), content));

    auto *catalogButton = new QPushButton(QStringLiteral("Actualizar ahora"), content);
    layout->addLayout(row(QStringLiteral("Catálogo de iptv-org"), catalogButton, content));
    m_catalogStatus = hint(QStringLiteral("Se actualiza solo cada vez que se abre la aplicación."), content);
    layout->addWidget(m_catalogStatus);

    layout->addWidget(new QLabel(QStringLiteral("Otras fuentes gratuitas"), content));
    for (const ExtraSource &source : ExtraSources::all()) {
        const QString id = QString::fromLatin1(source.id);
        auto *sourceSwitch = new DSwitchButton(content);
        sourceSwitch->setChecked(!m_disabledSources.contains(id));
        sourceSwitch->setToolTip(QString::fromUtf8(source.description));
        auto *sourceRow = row(QString::fromUtf8(source.title), sourceSwitch, content);
        sourceRow->setContentsMargins(12, 0, 0, 0);
        layout->addLayout(sourceRow);
        connect(sourceSwitch, &DSwitchButton::checkedChanged, this, [this, id](bool on) {
            if (on)
                m_disabledSources.removeAll(id);
            else if (!m_disabledSources.contains(id))
                m_disabledSources.append(id);
            emit disabledSourcesChanged(m_disabledSources);
        });
    }

    layout->addWidget(new QLabel(QStringLiteral("Mis listas"), content));
    m_lists = new QListWidget(content);
    m_lists->setFixedHeight(84);

    // Buttons sit beside the list so the dialog stays short on small screens.
    auto *addList = new QPushButton(QStringLiteral("Agregar lista…"), content);
    m_removeList = new QPushButton(QStringLiteral("Eliminar"), content);
    auto *listButtons = new QVBoxLayout;
    listButtons->addWidget(addList);
    listButtons->addWidget(m_removeList);
    listButtons->addStretch(1);
    auto *listRow = new QHBoxLayout;
    listRow->addWidget(m_lists, 1);
    listRow->addLayout(listButtons);
    layout->addLayout(listRow);

    // --- Channel scan ----------------------------------------------------
    layout->addWidget(sectionTitle(QStringLiteral("Canales"), content));

    m_scanButton = new QPushButton(content);
    m_scanSummary = new QLabel(content);
    m_scanSummary->setWordWrap(true);
    auto *scanRow = new QHBoxLayout;
    scanRow->addWidget(m_scanSummary, 1);
    scanRow->addWidget(m_scanButton);
    layout->addLayout(scanRow);

    m_scanProgress = new QProgressBar(content);
    m_scanProgress->setTextVisible(false);
    layout->addWidget(m_scanProgress);

    auto *hideSwitch = new DSwitchButton(content);
    hideSwitch->setChecked(values.hideBadChannels);
    layout->addLayout(row(QStringLiteral("Ocultar los canales que no funcionan"), hideSwitch, content));

    m_clearButton = new QPushButton(QStringLiteral("Olvidar resultados"), content);
    layout->addLayout(row(QStringLiteral("Resultados de escaneos anteriores"), m_clearButton, content));
    layout->addWidget(hint(QStringLiteral("El escaneo comprueba la lista que está abierta. Un canal puede fallar "
                                          "un día y volver otro: repite el escaneo de vez en cuando."), content));

    // Word-wrapped labels make the layout under-report its height, and DDialog
    // then squeezes the rows into each other. Pin the real height instead.
    content->setMinimumHeight(layout->totalHeightForWidth(content->width()));
    addContent(content);
    addButton(QStringLiteral("Cerrar"), true);

    // --- Wiring ----------------------------------------------------------
    const auto effectChanged = [this] {
        const QString effect = m_effectCombo->currentData().toString();
        m_colorButton->setVisible(effect == QLatin1String("color"));
        emit tuningEffectChanged(effect, m_color);
    };
    connect(m_effectCombo, &QComboBox::currentIndexChanged, this, effectChanged);
    connect(m_colorButton, &QPushButton::clicked, this, [this, effectChanged] {
        const QColor chosen = QColorDialog::getColor(m_color, this, QStringLiteral("Color de la pantalla"));
        if (!chosen.isValid())
            return;
        m_color = chosen;
        updateColorButton();
        effectChanged();
    });
    m_colorButton->setVisible(values.tuningEffect == QLatin1String("color"));
    updateColorButton();

    connect(hwSwitch, &DSwitchButton::checkedChanged, this, &SettingsDialog::hardwareDecodingChanged);
    connect(hideSwitch, &DSwitchButton::checkedChanged, this, &SettingsDialog::hideBadChannelsChanged);
    connect(catalogButton, &QPushButton::clicked, this, [this] {
        setCatalogStatus(QStringLiteral("Actualizando…"));
        emit catalogRefreshRequested();
    });
    connect(addList, &QPushButton::clicked, this, &SettingsDialog::addListRequested);
    connect(m_removeList, &QPushButton::clicked, this, [this] {
        if (QListWidgetItem *item = m_lists->currentItem())
            emit removeListRequested(item->data(Qt::UserRole).toString());
    });
    connect(m_lists, &QListWidget::currentRowChanged, this, [this](int current) { m_removeList->setEnabled(current >= 0); });
    connect(m_scanButton, &QPushButton::clicked, this, &SettingsDialog::scanToggled);
    connect(m_clearButton, &QPushButton::clicked, this, &SettingsDialog::clearResultsRequested);
}

void SettingsDialog::updateColorButton()
{
    QPixmap swatch(32, 32);
    swatch.fill(Qt::transparent);
    QPainter p(&swatch);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(128, 128, 128), 2));
    p.setBrush(m_color);
    p.drawRoundedRect(QRectF(2, 2, 28, 28), 6, 6);
    m_colorButton->setIcon(QIcon(swatch));
}

void SettingsDialog::setUserLists(const QList<UserList> &lists)
{
    m_lists->clear();
    for (const UserList &list : lists) {
        auto *item = new QListWidgetItem(QStringLiteral("%1  —  %2").arg(list.name, list.url), m_lists);
        item->setData(Qt::UserRole, list.url);
        item->setToolTip(list.url);
    }
    m_removeList->setEnabled(false);
}

void SettingsDialog::setCatalogStatus(const QString &text)
{
    m_catalogStatus->setText(text);
}

void SettingsDialog::setScanState(bool running, int done, int total, int channels, int good, int bad)
{
    m_scanButton->setText(running ? QStringLiteral("Detener") : QStringLiteral("Escanear canales"));
    m_scanButton->setEnabled(m_scannerAvailable && (running || channels > 0));
    m_scanProgress->setVisible(running);
    m_scanProgress->setRange(0, qMax(1, total));
    m_scanProgress->setValue(done);
    m_clearButton->setEnabled(!running);

    if (!m_scannerAvailable)
        m_scanSummary->setText(QStringLiteral("Para escanear hace falta tener instalado ffprobe (paquete ffmpeg)."));
    else if (running)
        m_scanSummary->setText(QStringLiteral("Escaneando %1 de %2 · %3 funcionan, %4 no").arg(done).arg(total).arg(good).arg(bad));
    else if (good + bad == 0)
        m_scanSummary->setText(QStringLiteral("Lista actual: %1 canales, sin escanear").arg(channels));
    else
        m_scanSummary->setText(QStringLiteral("Lista actual: %1 canales · %2 funcionan, %3 no, %4 sin comprobar")
                                   .arg(channels).arg(good).arg(bad).arg(qMax(0, channels - good - bad)));
}
