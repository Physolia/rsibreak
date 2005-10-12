/* This file is part of the KDE project
   Copyright (C) 2005 Tom Albers <tomalbers@kde.nl>

   The parts for idle detection is based on
   kdepim's karm idletimedetector.cpp/.h

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <qpushbutton.h>
#include <qlayout.h>
#include <qtimer.h>
#include <qtooltip.h>
#include <qevent.h>
#include <qlabel.h>
#include <qdatetime.h>
#include <qlineedit.h>
#include <qimage.h>
#include <qpixmap.h>
#include <qwidget.h>
#include <qdir.h>
#include <qstringlist.h>
#include <qfileinfo.h>

#include "config.h"     // HAVE_LIBXSS
#ifdef HAVE_LIBXSS      // Idle detection.
 #include <X11/Xlib.h>
 #include <X11/Xutil.h>
 #include <X11/extensions/scrnsaver.h>
 #include <fixx11h.h>
#endif // HAVE_LIBXSS

#include <kwin.h>
#include <klocale.h>
#include <kapplication.h>
#include <kaccel.h>
#include <kmessagebox.h>
#include <kdebug.h>
#include <kconfig.h>

#include <stdlib.h>

#include "rsiwidget.h"
#include "rsidock.h"

RSIWidget::RSIWidget( QWidget *parent, const char *name )
    : QWidget( parent, name )
{
    kdDebug() << "Entering RSIWidget" << endl;
    m_tray = new RSIDock(this,"Tray Item");
    m_tray->show();
    connect( m_tray, SIGNAL( quitSelected() ), kapp, SLOT( quit() ) );
    connect( m_tray, SIGNAL( configChanged() ), SLOT( slotReadConfig() ) );
    connect( m_tray, SIGNAL( dialogEntered() ), SLOT( slotStop() ) );
    connect( m_tray, SIGNAL( dialogLeft() ), SLOT( slotStart() ) );

    m_idleLong = false;
    bool idleDetection = false;

#ifdef HAVE_LIBXSS      // Idle detection.
    int event_base, error_base;
    if(XScreenSaverQueryExtension(qt_xdisplay(), &event_base, &error_base))
        idleDetection = true;
#endif

    if (idleDetection)
        kdDebug() << "IDLE Detection is possible" << endl;
    else
        kdDebug() << "IDLE Detection is not possible" << endl;


    srand ( time(NULL) );

    KMessageBox::information(parent,
                             i18n("Welcome to RSIBreak\n\n"
                                  "In your tray you can now see a clock. "
                                  "When you right-click on that you will see "
                                  "a menu, from which you can go to the "
                                  "configuration for example.\nWhen you want to "
                                  "know when the next break is, hover "
                                  "over the icon.\n\nUse RSIBreak wisely."),
                             i18n("Welcome"),
                             "dont_show_welcome_again_for_001");
    QBoxLayout *topLayout = new QVBoxLayout( this, 5);

    m_countDown = new QLabel(this);
    m_countDown->setAlignment( AlignCenter );
    m_countDown->setBackgroundOrigin(QWidget::ParentOrigin);
    topLayout->addWidget(m_countDown);

    topLayout->addStretch(5);

    QBoxLayout *buttonRow = new QHBoxLayout( topLayout );

    m_miniButton = new QPushButton( i18n("Minimize"), this );
    buttonRow->addWidget( m_miniButton );
    connect( m_miniButton, SIGNAL( clicked() ), SLOT( slotStart() ) );

    m_accel = new KAccel(this);
    m_accel->insert("minimize", i18n("Minimize"),
                    i18n("Abort a break"),Qt::Key_Escape,
                    this, SLOT( slotStart() ));

    buttonRow->addStretch(10);

    QTime m_targetTime;

    m_timer_max = new QTimer(this);
    connect(m_timer_max, SIGNAL(timeout()), SLOT(slotMaximize()));

    m_timer_min = new QTimer(this);
    connect(m_timer_min, SIGNAL(timeout()),  SLOT(slotMinimize()));

    m_timer_max->start(m_timeMinimized, true);
    m_normalTimer = startTimer( 500 );

    readConfig();
    slotMinimize();
}

RSIWidget::~RSIWidget()
{
    kdDebug() << "Entering ~RSIWidget" << endl;
    delete m_timer_max;
    delete m_timer_min;
    delete m_tray;
}

void RSIWidget::startMinimizeTimer()
{
    kdDebug() << "Entering startMinimizeTimer" << endl;

    hide();
    m_timer_min->stop();
    m_targetTime = QTime::currentTime().addSecs(m_timeMinimized);
    m_timer_max->start(m_timeMinimized*1000, true);
}

void RSIWidget::slotMinimize()
{
    kdDebug() << "Entering slotMinimize" << endl;

    startMinimizeTimer();

    if (m_files.count() == 0)
        return;

    // reset if all images are shown
    if (m_files_done.count() == m_files.count())
        m_files_done.clear();

    // get a net yet used number
    int j = (int) ((m_files.count()) * (rand() / (RAND_MAX + 1.0)));
    while (m_files_done.findIndex( QString::number(j) ) != -1)
        j = (int) ((m_files.count()) * (rand() / (RAND_MAX + 1.0)));

    // Use that number to load the right image
    m_files_done.append(QString::number(j));

    kdDebug() << "Loading: " << m_files[j] << 
                    "( " << j << " / "  << m_files.count() << " ) " << endl;

    QImage m = QImage( m_files[ j ]).scale(
                        QApplication::desktop()->width(),
                        QApplication::desktop()->height(),
                        QImage::ScaleMax);

    if (m.isNull())
    {
        kdWarning() << "constructed a null-image" << endl;
        kdDebug() << "format: " << 
           QImageIO::imageFormat(m_files[j]) << endl;

        QImageIO iio;
        iio.setFileName(m_files[j]);   
        if ( iio.read() )
            kdDebug() << "Read is ok" << endl;
        else
            kdDebug() << "Read failed" << endl;

        kdDebug() << iio.status() << endl;
        return;
    }

    setPaletteBackgroundPixmap( QPixmap( m ) );
    m_countDown->setPaletteBackgroundPixmap( QPixmap( m ) );
}

void RSIWidget::slotMaximize()
// this shows a break request all over the screen. Do not be mislead by
// the name "slotMaximize", the user cannot maximize the window. This is
// exclusively triggered by the timer whose signal is connected.
{
    kdDebug() << "Entering slotMaximize" << endl;

    int totalIdle = 0;

#ifdef HAVE_LIBXSS      // Idle detection.
    XScreenSaverInfo*  _mit_info;
    _mit_info= XScreenSaverAllocInfo();
    XScreenSaverQueryInfo(qt_xdisplay(), qt_xrootwin(), _mit_info);
    totalIdle = (_mit_info->idle/1000);
#endif // HAVE_LIBXSS

    // if user has been idle since the last break, there is no needed
    // to have another one. Skip it.
    if (totalIdle >= m_timeMinimized)
    {
        kdDebug() << "No break needed, idle time: " << totalIdle << endl;
        startMinimizeTimer();

        // If user has been idle twice the time between the breaks, it will
        // get a bonus in the way that it gains a tinyBreak
        if (totalIdle >= m_timeMinimized*2)
        {
            m_idleLong=true;
            kdDebug() << "Next real break will be skipped, "
                         "you have been idle a while now" << endl;
            if (m_currentInterval < m_bigInterval)
            m_currentInterval++;
        }
        kdDebug() << "Next BigBreak in " << m_currentInterval << endl;
        return;
    }
    else if (m_idleLong)
    {
        // if user has been idle for at least two breaks, there is no
        // need to break immediatly, we can postpone the break for one time
        kdDebug() << "Skip this break, you have been idle for a while recently" << endl;
        startMinimizeTimer();
        m_idleLong=false;
        return;
    }

    // idle handling is done now. Time for a break.
    kdDebug() << "You have been idle for " << totalIdle << "s "
        << "(need "<< m_timeMinimized << "s to skip) -> time for a break." << endl;

    m_currentInterval--;
    m_timer_max->stop();

    if (m_currentInterval > 0)
    {
        kdDebug() << "TinyBreak, next BigBreak in " << m_currentInterval << endl;
        m_targetTime = QTime::currentTime().addSecs(m_timeMaximized);
        m_timer_min->start(m_timeMaximized*1000, true);
    }
    else
    {
        kdDebug() << "BigBreak" << endl;
        m_targetTime = QTime::currentTime().addSecs(m_bigTimeMaximized);
        m_timer_min->start(m_bigTimeMaximized*1000, true);
        m_currentInterval=m_bigInterval;
    }

    show(); // Keep it above the KWin calls.
    KWin::forceActiveWindow(winId());
    KWin::setOnAllDesktops(winId(),true);
    KWin::setState(winId(), NET::KeepAbove);
    KWin::setState(winId(), NET::FullScreen);
}

void RSIWidget::timerEvent( QTimerEvent* )
{
    // a bit to much debugging with kdDebug() << "Entering timerEvent" << endl;
    // TODO: tell something about tinyBreaks, bigBreaks.
    int s = QTime::currentTime().secsTo(m_targetTime) +1 ;
    m_countDown->setText( QString::number( s ) );
    QToolTip::add(m_tray, i18n("One second remaining",
                  "%n seconds remaining",s));
}

void RSIWidget::slotStop( )
{
    kdDebug() << "Entering slotStop" << endl;
    startMinimizeTimer();
    m_timer_max->stop();
}

void RSIWidget::slotStart( )
{
    kdDebug() << "Entering slotStart" << endl;
    // the interuption can not be considered a real break
    // only needed fot a big break of course
    if (m_currentInterval == m_bigInterval)
        m_currentInterval=0;

    slotMinimize();
}

void RSIWidget::findImagesInFolder(const QString& folder)
{
    kdDebug() << "Looking for pictures in " << folder << endl;
    QDir dir( folder);

    // TODO: make an automated filter, maybe with QImageIO.
    QString ext("*.png *.jpg *.jpeg *.tif *.tiff *.gif *.bmp *.xpm *.ppm *.pnm *.xcf *.pcx");
    dir.setNameFilter(ext + " " + ext.upper());
    dir.setMatchAllDirs ( true );

    if ( !dir.exists() or !dir.isReadable() )
    {
        kdWarning() << "Folder does not exist or is not readable: "
                << folder << endl;
        return;
    }

    const QFileInfoList *list = dir.entryInfoList();
    if (!list)
        return;

    QFileInfoListIterator it( *list );
    QFileInfo *fi;

    while ( (fi = it.current()) != 0 )
    {
        if ( fi->isFile())
            m_files.append(fi->filePath());
        else if (fi->isDir() && m_searchRecursive && 
                 fi->fileName() != "." &&  fi->fileName() != "..")
            findImagesInFolder(fi->absFilePath());
        ++it;
    }
}

void RSIWidget::readConfig()
{
    kdDebug() << "Entering readConfig" << endl;
    KConfig* config = kapp->config();
    QColor *Black = new QColor(Qt::black);
    QFont *t = new QFont( "Verdana", 40, 75, true );

    // Make something fake, don't load anyyhing if not configured
    QString d = QDir::home().path()+"noimagesfolderconfigured";

    config->setGroup("General Settings");
    m_countDown->setPaletteForegroundColor(
            config->readColorEntry("CounterColor", Black ) );
    m_miniButton->setHidden(
            config->readBoolEntry("HideMinimizeButton", false));
    m_countDown->setHidden(
            config->readBoolEntry("HideCounter", false));
    m_accel->setEnabled("minimize",
                        !config->readBoolEntry("DisableAccel", false));
    m_countDown->setFont(
            config->readFontEntry("CounterFont", t ) );

    bool recursive =
            config->readBoolEntry("SearchRecursiveCheck", false);
    QString path = config->readEntry("ImageFolder", d );
    if (m_basePath != path || m_searchRecursive != recursive)
    {
        m_files.clear();
        m_files_done.clear();
        m_basePath = path;
        m_searchRecursive = recursive;
        findImagesInFolder( path );
    }

    m_timeMinimized = (int)(QString(config->readEntry("TinyInterval", "10")).toFloat()*60);
    m_timeMaximized = QString(config->readEntry("TinyDuration", "20")).toInt();

    m_bigInterval = QString(config->readEntry("BigInterval", "3")).toInt();
    m_bigTimeMaximized = (int)(QString(config->readEntry("BigDuration", "1")).toFloat()*60);
    m_currentInterval = m_bigInterval;

    delete Black;
}

void RSIWidget::slotReadConfig()
{
    kdDebug() << "Config changed" << endl;
    readConfig();
    startMinimizeTimer();
}

#include "rsiwidget.moc"
