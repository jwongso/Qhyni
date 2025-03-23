#ifndef HYNIWINDOW_H
#define HYNIWINDOW_H

#include <QMainWindow>
#include <QWebSocket>
#include <QKeyEvent>
#include <QTimer>
#include "HighlightTableWidget.h"

class QWebSocket;
class QTextEdit;

class HyniWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit HyniWindow(QWidget *parent = nullptr);
    virtual ~HyniWindow();

private slots:
    void handleHighlightedText(const QString& text);
    void onConnected();
    void onDisconnected();
    void attemptReconnect();
    void onMessageReceived(const QString &message);
    void onError(QAbstractSocket::SocketError error);

private:
    void keyPressEvent(QKeyEvent *event) override;
    void sendText();

private:
    QWebSocket *websocket;
    HighlightTableWidget *highlightTableWidget;
    QTextEdit *promptTextBox;
    QTextEdit *responseBox;
    QTimer *reconnectTimer;
    QString highlightedText;
};

#endif // HYNIWINDOW_H
