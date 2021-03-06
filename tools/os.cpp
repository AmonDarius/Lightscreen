﻿/*
 * Copyright (C) 2014  Christian Kaiser
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <QApplication>
#include <QBitmap>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QDialog>
#include <QDir>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QLibrary>
#include <QLocale>
#include <QMessageBox>
#include <QPixmap>
#include <QPointer>
#include <QProcess>
#include <QSettings>
#include <QTextEdit>
#include <QTimeLine>
#include <QTimer>
#include <QUrl>
#include <QWidget>
#include <string>

#include "qtwin.h"

#ifdef Q_WS_WIN
  #include <qt_windows.h>
  #include <shlobj.h>
#elif defined(Q_WS_X11)
  #include <QX11Info>
  #include <X11/X.h>
  #include <X11/Xlib.h>
  #include <X11/Xutil.h>
  #include <X11/Xatom.h>
#endif

#include "os.h"

void os::addToRecentDocuments(QString fileName)
{
#ifdef Q_WS_WIN
  QT_WA ( {
      SHAddToRecentDocs (0x00000003, QDir::toNativeSeparators(fileName).utf16());
    } , {
      SHAddToRecentDocs (0x00000002, QDir::toNativeSeparators(fileName).toLocal8Bit().data());
  } ); // QT_WA
#else
  Q_UNUSED(fileName)
#endif
}

QPixmap os::cursor()
{
#ifdef Q_WS_WIN
  /*
  * Taken from: git://github.com/arrai/mumble-record.git > src > mumble > Overlay.cpp
  * BSD License.
  */

  QPixmap pixmap;

  CURSORINFO cursorInfo;
  cursorInfo.cbSize = sizeof(cursorInfo);
  ::GetCursorInfo(&cursorInfo);

  HICON cursor = cursorInfo.hCursor;

  ICONINFO iconInfo;
  ::GetIconInfo(cursor, &iconInfo);

  ICONINFO info;
  ZeroMemory(&info, sizeof(info));

  if (::GetIconInfo(cursor, &info)) {
    if (info.hbmColor) {
      pixmap = QPixmap::fromWinHBITMAP(info.hbmColor, QPixmap::Alpha);
    }
    else {
      QBitmap orig(QPixmap::fromWinHBITMAP(info.hbmMask));
      QImage img = orig.toImage();

      int h = img.height() / 2;
      int w = img.bytesPerLine() / sizeof(quint32);

      QImage out(img.width(), h, QImage::Format_MonoLSB);
      QImage outmask(img.width(), h, QImage::Format_MonoLSB);

      for (int i=0;i<h; ++i) {
        const quint32 *srcimg = reinterpret_cast<const quint32 *>(img.scanLine(i + h));
        const quint32 *srcmask = reinterpret_cast<const quint32 *>(img.scanLine(i));

        quint32 *dstimg = reinterpret_cast<quint32 *>(out.scanLine(i));
        quint32 *dstmask = reinterpret_cast<quint32 *>(outmask.scanLine(i));

        for (int j=0;j<w;++j) {
          dstmask[j] = srcmask[j];
          dstimg[j] = srcimg[j];
        }
      }

      pixmap = QBitmap::fromImage(out, Qt::ColorOnly);
    }

    if (info.hbmMask)
      ::DeleteObject(info.hbmMask);

    if (info.hbmColor)
      ::DeleteObject(info.hbmColor);
  }

  return pixmap;
#else
  return QPixmap();
#endif
}

void os::effect(QObject* target, const char *slot, int frames, int duration, const char* cleanup)
{
  QTimeLine* timeLine = new QTimeLine(duration);
  timeLine->setFrameRange(0, frames);

  timeLine->connect(timeLine, SIGNAL(frameChanged(int)), target, slot);

  if (cleanup != 0)
    timeLine->connect(timeLine, SIGNAL(finished()), target, SLOT(cleanup()));

  timeLine->connect(timeLine, SIGNAL(finished()), timeLine, SLOT(deleteLater()));


  timeLine->start();
}

QString os::getDocumentsPath()
{
#ifdef Q_WS_WIN
  TCHAR szPath[MAX_PATH];

  if (SUCCEEDED(SHGetFolderPath(NULL,
                               CSIDL_PERSONAL|CSIDL_FLAG_CREATE,
                               NULL,
                               0,
                               szPath)))
  {
    std::wstring path(szPath);

    return QString::fromWCharArray(path.c_str());
  }

  return QDir::homePath() + QDir::separator() + "My Documents";
#else
  return QDir::homePath() + QDir::separator() + "Documents";
#endif
}

QPixmap os::grabWindow(WId winId)
{
#ifdef Q_WS_WIN
  RECT rcWindow;
  GetWindowRect(winId, &rcWindow);

  if (IsZoomed(winId)) {
    int margin = GetSystemMetrics(SM_CXSIZEFRAME);

    rcWindow.right -= margin;
    rcWindow.left += margin;
    rcWindow.top += margin;
    rcWindow.bottom -= margin;
  }

  int width, height;
  width = rcWindow.right - rcWindow.left;
  height = rcWindow.bottom - rcWindow.top;

  RECT rcScreen;
  GetWindowRect(GetDesktopWindow(), &rcScreen);

  RECT rcResult;
  UnionRect(&rcResult, &rcWindow, &rcScreen);

  QPixmap pixmap;

  // Comparing the rects to determine if the window is outside the boundaries of the screen,
  // the window DC method has the disadvantage that it does not show Aero glass transparency,
  // so we'll avoid it for the screenshots that don't need it.

  HDC hdcMem;
  HBITMAP hbmCapture;

  if (EqualRect(&rcScreen, &rcResult)) {
    // Grabbing the window from the Screen DC.
    HDC hdcScreen = GetDC(NULL);

    BringWindowToTop(winId);

    hdcMem = CreateCompatibleDC(hdcScreen);
    hbmCapture = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectObject(hdcMem, hbmCapture);

    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, rcWindow.left, rcWindow.top, SRCCOPY);
  }
  else {
    // Grabbing the window by its own DC
    HDC hdcWindow = GetWindowDC(winId);

    hdcMem = CreateCompatibleDC(hdcWindow);
    hbmCapture = CreateCompatibleBitmap(hdcWindow, width, height);
    SelectObject(hdcMem, hbmCapture);

    BitBlt(hdcMem, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY);
  }

  ReleaseDC(winId, hdcMem);
  DeleteDC(hdcMem);

  pixmap = QPixmap::fromWinHBITMAP(hbmCapture);

  DeleteObject(hbmCapture);

  return pixmap;

#else
  return QPixmap::grabWindow(winId);
#endif
}

void os::setForegroundWindow(QWidget *window)
{
#ifdef Q_WS_WIN
  ShowWindow(window->winId(), SW_RESTORE);
  SetForegroundWindow(window->winId());
#else
  Q_UNUSED(window)
#endif
}

void os::setStartup(bool startup, bool hide)
{
  QString lightscreen = QDir::toNativeSeparators(qApp->applicationFilePath());

  if (hide)
    lightscreen.append(" -h");

#ifdef Q_WS_WIN
  // Windows startup settings
  QSettings init("Microsoft", "Windows");
  init.beginGroup("CurrentVersion");
  init.beginGroup("Run");

  if (startup) {
    init.setValue("Lightscreen", lightscreen);
  }
  else {
    init.remove("Lightscreen");
  }

  init.endGroup();
  init.endGroup();
#endif

#if defined(Q_WS_X11)
  QFile desktopFile(QDir::homePath() + "/.config/autostart/lightscreen.desktop");

  desktopFile.remove();

  if (startup) {
    desktopFile.open(QIODevice::WriteOnly);
    desktopFile.write(QString("[Desktop Entry]\nExec=%1\nType=Application").arg(lightscreen).toAscii());
  }
#endif
}

QGraphicsEffect* os::shadow(QColor color, int blurRadius, int offset) {
  QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect;
  shadowEffect->setBlurRadius(blurRadius);
  shadowEffect->setOffset(offset);
  shadowEffect->setColor(color);

  return shadowEffect;
}

void os::translate(QString language)
{
  /*
  static QTranslator *translator = 0;
  static QTranslator *translator_qt = 0;

  if ((language.compare("English", Qt::CaseInsensitive) == 0
      || language.isEmpty()) && translator) {
    qApp->removeTranslator(translator);
    qApp->removeTranslator(translator_qt);
    QLocale::setDefault(QLocale::c());
    return;
  }

  if (translator) {
    delete translator;
    delete translator_qt;
  }

  translator    = new QTranslator(qApp);
  translator_qt = new QTranslator(qApp);

  if (language == "Español")
    QLocale::setDefault(QLocale::Spanish);

  if (translator->load(language, ":/translations")) {
    qApp->installTranslator(translator);
  }

  if (translator_qt->load(language, ":/translations_qt")) {
    qApp->installTranslator(translator_qt);
  }
  */
}

QIcon os::icon(QString name)
{
  static int value = -1;

  if (value < 0) {
    value = qApp->desktop()->palette().color(QPalette::Button).value();
  }

  if (value > 125) {
    return QIcon(":/icons/" + name);
  }
  else {
    return QIcon(":/icons/inv/" + name);
  }
}

#ifdef Q_WS_X11
// Taken from KSnapshot. Oh KDE, what would I do without you :D
Window os::findRealWindow(Window w, int depth)
{
    if( depth > 5 ) {
        return None;
    }

    static Atom wm_state = XInternAtom( QX11Info::display(), "WM_STATE", False );
    Atom type;
    int format;
    unsigned long nitems, after;
    unsigned char* prop;

    if( XGetWindowProperty( QX11Info::display(), w, wm_state, 0, 0, False, AnyPropertyType,
                            &type, &format, &nitems, &after, &prop ) == Success ) {
        if( prop != NULL ) {
            XFree( prop );
        }

        if( type != None ) {
            return w;
        }
    }

    Window root, parent;
    Window* children;
    unsigned int nchildren;
    Window ret = None;

    if( XQueryTree( QX11Info::display(), w, &root, &parent, &children, &nchildren ) != 0 ) {
        for( unsigned int i = 0;
             i < nchildren && ret == None;
             ++i ) {
            ret = os::findRealWindow( children[ i ], depth + 1 );
        }

        if( children != NULL ) {
            XFree( children );
        }
    }

    return ret;
}

Window os::windowUnderCursor(bool includeDecorations)
{
    Window root;
    Window child;
    uint mask;
    int rootX, rootY, winX, winY;

    XQueryPointer( QX11Info::display(), QX11Info::appRootWindow(), &root, &child,
           &rootX, &rootY, &winX, &winY, &mask );

    if( child == None ) {
        child = QX11Info::appRootWindow();
    }

    if( !includeDecorations ) {
        Window real_child = os::findRealWindow( child );

        if( real_child != None ) { // test just in case
            child = real_child;
        }
    }

    return child;
}
#endif
