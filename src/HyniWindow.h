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
#ifdef ENABLE_AUDIO_STREAM
#include "AudioStreamer.h"
#endif

class ChatAPIWorker;

class HyniWindow : public QMainWindow {
    Q_OBJECT

public:
    HyniWindow(QWidget *parent = nullptr);
    ~HyniWindow();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void sendText(bool resend = false);
    void handleHighlightedText(const QString& texts);
    void onMessageReceived(const std::string& message);
    void onWebSocketConnected(bool connected);
    void onWebSocketError(const std::string& error);
    void handleAPIResponse(const QString& response);
    void handleAPIError(const QString& error);
    void handleNeedAPIKey();
    void captureScreen();
    void handleCapturedScreen(const QPixmap& pixmap);
    void resendCapturedScreen();
    void receiveAudioData(const QByteArray& data);
    void onAISelectionChanged(QAction* action);
    void zoomInResponseBox();
    void zoomOutResponseBox();
    void showAboutDialog();
    void onLanguageChanged(QAction* action);

private:
    void attemptReconnect();
    void setupAPIWorkers();
    void addResponseTab(const QString& language);
    void cleanupAPIWorkers();
    std::string sendToChatAPI(const QString& text, bool isStarQuestion);
    void toggleQuestionType();
    void handleTabNavigation(QKeyEvent* event);
    void setupMenuBar(QMenuBar* menuBar);

    HighlightTableWidget* highlightTableWidget;
    QTextEdit* promptTextBox;
    QRadioButton* generalOption;
    QRadioButton* amazonStarOption;
    QRadioButton* systemDesignOption;
    QRadioButton* codingOption;
    QString highlightedText;
    QTabWidget *tabWidget;

    QString m_sharedApiKey;
    bool m_apiKeyRequested{false};

    QVector<QTextEdit*> responseEditors;
    ChatAPIWorker* worker;
    QThread* thread;

    std::unique_ptr<QTimer> reconnectTimer;
    std::unique_ptr<boost::asio::io_context> io_context;
    std::shared_ptr<hyni_websocket_client> websocketClient;
    std::thread io_thread;

    PngMonitor m_png_monitor;
    QVector<QString> m_history;
#ifdef ENABLE_AUDIO_STREAM
    AudioStreamer m_streamer;
#endif
};

#endif
