#pragma once

#include "playliststore.h"

#include <DDialog>

#include <QColor>

class QComboBox;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;

DWIDGET_BEGIN_NAMESPACE
class DSwitchButton;
DWIDGET_END_NAMESPACE

// The settings view. It owns no state: it shows what the main window gives
// it and reports every change through signals.
class SettingsDialog : public Dtk::Widget::DDialog
{
    Q_OBJECT
public:
    struct Values
    {
        QString tuningEffect; // "static", "nosignal" or "color"
        QColor tuningColor;
        bool hardwareDecoding = false;
        bool hideBadChannels = false;
        bool scannerAvailable = true;
    };

    explicit SettingsDialog(const Values &values, QWidget *parent = nullptr);

    void setUserLists(const QList<UserList> &lists);
    void setCatalogStatus(const QString &text);
    // good/bad are the known results for the current list.
    void setScanState(bool running, int done, int total, int channels, int good, int bad);

signals:
    void tuningEffectChanged(const QString &effect, const QColor &color);
    void hardwareDecodingChanged(bool enabled);
    void catalogRefreshRequested();
    void addListRequested();
    void removeListRequested(const QString &url);
    void scanToggled(); // start, or stop when one is running
    void hideBadChannelsChanged(bool hide);
    void clearResultsRequested();

private:
    void updateColorButton();

    QColor m_color;
    QComboBox *m_effectCombo;
    QPushButton *m_colorButton;
    QLabel *m_catalogStatus;
    QListWidget *m_lists;
    QPushButton *m_removeList;
    QLabel *m_scanSummary;
    QProgressBar *m_scanProgress;
    QPushButton *m_scanButton;
    QPushButton *m_clearButton;
    bool m_scannerAvailable;
};
