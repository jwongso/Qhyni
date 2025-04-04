#ifndef HYNI_WINDOW_H
#define HYNI_WINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QStatusBar>
#include <QGroupBox>
#include <QRadioButton>
#include <QMap>
#include <boost/asio.hpp>
#include "PngMonitor.h"
#include "websocket_client.h"
#include "HighlightTableWidget.h"

class ChatAPIWorker;

class HyniWindow : public QMainWindow {
    Q_OBJECT

public:
    HyniWindow(QWidget *parent = nullptr);
    ~HyniWindow();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void sendText(bool repeat = false);
    void handleHighlightedText(const QString& texts);
    void onMessageReceived(const std::string& message);
    void onWebSocketConnected(bool connected);
    void onWebSocketError(const std::string& error);
    void handleAPIResponse(const QString& response, const QString& language);
    void handleAPIError(const QString& error, const QString& language);
    void handleNeedAPIKey(const QString& language);
    void captureScreen();
    void handleCapturedScreen(const QPixmap& pixmap);

private:
    void attemptReconnect();
    void setupAPIWorkers();
    void addResponseTab(const QString& language);
    void cleanupAPIWorkers();
    std::string sendToChatAPI(const QString& text, bool isStarQuestion);
    void toggleQuestionType();
    void handleTabNavigation(QKeyEvent* event);

    HighlightTableWidget* highlightTableWidget;
    QTextEdit* promptTextBox;
    QRadioButton* amazonStarOption;
    QRadioButton* systemDesignOption;
    QRadioButton* codingOption;
    QString highlightedText;
    QTabWidget *tabWidget;

    QString m_sharedApiKey;
    bool m_apiKeyRequested{false};

    QMap<QString, QTextEdit*> responseEditors;
    QMap<QString, ChatAPIWorker*> workers;
    QMap<QString, QThread*> threads;

    std::unique_ptr<QTimer> reconnectTimer;
    std::unique_ptr<boost::asio::io_context> io_context;
    std::shared_ptr<hyni_websocket_client> websocketClient;
    std::thread io_thread;

    PngMonitor m_png_monitor;
};

#endif
