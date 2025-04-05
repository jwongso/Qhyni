#ifndef PNG_MONITOR_H
#define PNG_MONITOR_H

#include <QObject>
#include <QDir>
#include <QPixmap>
#include <QDebug>
#include <QTimer>

class PngMonitor : public QObject
{
    Q_OBJECT
public:
    PngMonitor(const QString &folderPath, QObject *parent = nullptr)
        : QObject(parent), m_folderPath(folderPath)
{
    // Set up polling timer
    connect(&m_pollTimer, &QTimer::timeout, this, &PngMonitor::checkForNewPngs);
    m_pollTimer.setInterval(2500);
    m_pollTimer.setSingleShot(false);
    m_pollTimer.start();
}

signals:
    void sendImage(const QPixmap& pixmap);

private slots:
    void checkForNewPngs()
    {
        QDir dir(m_folderPath);
        QStringList pngFiles = dir.entryList(QStringList() << "*.png", QDir::Files);

        foreach (const QString &file, pngFiles) {
            QString fullPath = dir.filePath(file);

            // Load as QPixmap
            QPixmap pixmap;
            if (pixmap.load(fullPath)) {
                qDebug() << "Loaded PNG file:" << fullPath;

                emit sendImage(pixmap);

                // Delete the file
                if (QFile::remove(fullPath)) {
                    qDebug() << "Deleted file:" << fullPath;
                } else {
                    qDebug() << "Failed to delete file:" << fullPath;
                }
            } else {
                qDebug() << "Failed to load PNG file:" << fullPath;
            }
        }
    }

private:
    QTimer m_pollTimer;
    QString m_folderPath;
};

#endif // PNG_MONITOR_H
