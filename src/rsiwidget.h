/*
    SPDX-FileCopyrightText: 2005-2006 Tom Albers <toma@kde.org>
    SPDX-FileCopyrightText: 2006 Bram Schoenmakers <bramschoenmakers@kde.nl>

    SPDX-License-Identifier: GPL-2.0-or-later

*/

#ifndef RSIWIDGET_H
#define RSIWIDGET_H

#include "notificator.h"
#include "rsitimer.h"

class RSIDock;
class RSIRelaxPopup;
class BreakBase;

/**
 * @class RSIObject
 * This controls all RSIBreak components
 * @author Tom Albers <toma.org>
 */
class RSIObject : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.rsibreak.rsiwidget")

public:
    enum Effects { SimpleGray = 0, Plasma, SlideShow, Popup };

    /**
     * Constructor
     * @param parent Parent Widget
     * @param name Name
     */
    explicit RSIObject(QWidget *parent = nullptr);

    /**
     * Destructor
     */
    ~RSIObject();

    /**
     * Access to the timer
     */
    RSITimer *timer()
    {
        return m_timer;
    };

private slots:
    void slotWelcome();
    void slotLock();
    void minimize();
    void maximize();
    void setCounters(int);
    void updateIdleAvg(double);
    void readConfig();
    void tinyBreakSkipped();
    void bigBreakSkipped();

protected:
    /** Sets appropriate icon in tooltip and docker. */
    void setIcon(int);

private:
    void findImagesInFolder(const QString &folder);
    void loadImage();
    void configureTimer();

    RSIDock *m_tray;
    RSITimer *m_timer;
    BreakBase *m_effect;

    bool m_useImages;

    bool m_usePlasma;
    bool m_usePlasmaRO;

    RSIRelaxPopup *m_relaxpopup;

    QString m_currentIcon;

    Notificator m_notificator;

    /* Available through D-Bus */
public Q_SLOTS:
    void resume();
    void suspend();
    int idleTime()
    {
        return timer()->idleTime();
    }
    int tinyLeft()
    {
        return timer()->tinyLeft();
    }
    int bigLeft()
    {
        return timer()->bigLeft();
    }
    QString currentIcon()
    {
        return m_currentIcon;
    }
};

#endif
