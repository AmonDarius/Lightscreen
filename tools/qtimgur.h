/*
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
#ifndef QTIMGUR_H
#define QTIMGUR_H

#include <QObject>
#include <QHash>
#include <QNetworkReply>
#include <QPointer>

class QNetworkAccessManager;

class QtImgur: public QObject {
  Q_OBJECT

public:
  enum Error {
    ErrorFile,
    ErrorNetwork,
    ErrorCredits,
    ErrorUpload,
    ErrorCancel
  };

  QtImgur(const QString &APIKey, QObject *parent);
  bool directUrl;

public slots:
  void cancelAll();
  void cancel(const QString &fileName);
  void upload(const QString &fileName);

protected slots:
  void progress(qint64, qint64);
  void reply(QNetworkReply* reply);

signals:
  void error(QString, QtImgur::Error);
  void uploadProgress(qint64, qint64);
  void uploaded(QString file, QString url, QString deleteHash);

private:
  QString mAPIKey;
  QNetworkAccessManager *mNetworkManager;
  QHash<QNetworkReply*, QString> mFiles;
};

#endif // QTIMGUR_H
