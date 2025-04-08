#include "HyniWindow.h"
#include "ChatAPIWorker.h"
#include "config.h"
#include <QTimer>
#include <QThread>
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>
#include <qapplication.h>
#include <qevent.h>
#include <QProcess>
#include <QFile>
#include <QMenuBar>
#include <QActionGroup>

HyniWindow::HyniWindow(QWidget *parent)
    : QMainWindow(parent), reconnectTimer(std::make_unique<QTimer>(this)),
    io_context(std::make_unique<boost::asio::io_context>()),
    m_png_monitor("/home/jwongso/Dropbox/BabaYaga")
{
    setWindowTitle("Qhyni - hyni UI with gen AI and real-time transcription");

    QMenuBar *menuBar = new QMenuBar(this);
    menuBar->setNativeMenuBar(false);
    setMenuBar(menuBar);
    setupMenuBar(menuBar);

    resize(900, 600);
    setFocusPolicy(Qt::StrongFocus);
    setFocus();

    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // Create splitter for adjustable layout
    QSplitter *leftSplitter = new QSplitter(Qt::Vertical, this);
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // Left side: Two text boxes stacked vertically
    QWidget *leftWidget = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);

    highlightTableWidget = new HighlightTableWidget(this);
    highlightTableWidget->setHorizontalHeaderLabels(QStringList() << "Transcribe");
    leftSplitter->addWidget(highlightTableWidget);

    promptTextBox = new QTextEdit(this);
    promptTextBox->setPlaceholderText("Selected Prompt");
    promptTextBox->setReadOnly(true);
    leftSplitter->addWidget(promptTextBox);

    leftSplitter->setSizes({300, 200});

    QGroupBox *questionTypeBox = new QGroupBox(this);
    QHBoxLayout *questionTypeLayout = new QHBoxLayout(questionTypeBox);

    amazonStarOption = new QRadioButton("Amazon Behavioral (STAR)", this);
    systemDesignOption = new QRadioButton("System Design", this);
    codingOption = new QRadioButton("Coding", this);
    codingOption->setChecked(true);  // Default selection

    amazonStarOption->setToolTip("Behavioral questions (STAR format)");
    systemDesignOption->setToolTip("System architecture questions");
    codingOption->setToolTip("Coding problems (multi-language)");

    questionTypeLayout->addWidget(amazonStarOption);
    questionTypeLayout->addWidget(systemDesignOption);
    questionTypeLayout->addWidget(codingOption);
    questionTypeBox->setLayout(questionTypeLayout);

    leftLayout->addWidget(leftSplitter);
    leftLayout->addWidget(questionTypeBox);

    leftWidget->setLayout(leftLayout);
    splitter->addWidget(leftWidget);

    // Right side: Tabs for multi-lang responses
    tabWidget = new QTabWidget(this);
    for (const auto& lang : hyni::PROG_LANGUAGES) {
        addResponseTab(QString::fromStdString(lang));
    }

    // Add history
    addResponseTab("History");

    splitter->addWidget(tabWidget);
    splitter->setSizes({400, 400});
    mainLayout->addWidget(splitter);

    QPushButton *sendButton = new QPushButton("Send Selection", this);
    mainLayout->addWidget(sendButton);
    connect(sendButton, &QPushButton::clicked, this, &HyniWindow::sendText);

    connect(highlightTableWidget, &HighlightTableWidget::textHighlighted, this, &HyniWindow::handleHighlightedText);

    setCentralWidget(centralWidget);

#ifdef ENABLE_AUDIO_STREAM
    websocketClient = std::make_shared<hyni_websocket_client>(*io_context, "localhost", "8765");
#else
    websocketClient = std::make_shared<hyni_websocket_client>(*io_context, "localhost", "8080");
#endif
    websocketClient->set_message_handler([this](const std::string& message) {
        QMetaObject::invokeMethod(this, [this, message]() {
            onMessageReceived(message);
        });
    });
    websocketClient->set_connection_handler([this](bool connected) {
        QMetaObject::invokeMethod(this, [this, connected]() {
            onWebSocketConnected(connected);
        });
    });
    websocketClient->set_error_handler([this](const std::string& error) {
        QMetaObject::invokeMethod(this, [this, error]() {
            onWebSocketError(error);
        });
    });

    // Start IO context in a separate thread
    io_thread = std::thread([this]() {
        boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(io_context->get_executor());
        io_context->run();
    });

    // Reconnect logic
    connect(reconnectTimer.get(), &QTimer::timeout, this, &HyniWindow::attemptReconnect);
    reconnectTimer->start(3000); // Try reconnecting every 3 seconds

    attemptReconnect(); // Try initial connection
    statusBar()->showMessage("Disconnected");

    setupAPIWorkers();

    connect(&m_png_monitor, &PngMonitor::sendImage, this, &HyniWindow::handleCapturedScreen);

#ifdef ENABLE_AUDIO_STREAM
    QObject::connect(&m_streamer, &AudioStreamer::audioDataReady, this, &HyniWindow::receiveAudioData);
    m_streamer.startRecording();
#endif
}

void HyniWindow::addResponseTab(const QString& language) {
    QTextEdit *editor = new QTextEdit(this);
    editor->setPlaceholderText(language + " Response");
    editor->setReadOnly(true);

    tabWidget->addTab(editor, language);
    responseEditors[language] = editor;
}

HyniWindow::~HyniWindow() {
#ifdef ENABLE_AUDIO_STREAM
    m_streamer.stopRecording();
#endif
    reconnectTimer->stop();
    reconnectTimer.reset();

    if (websocketClient) {
        websocketClient->shutdown(); // Your existing method
        websocketClient.reset();
    }

    if (io_context) {
        io_context->stop(); // Signal stop to ASIO

        if (io_thread.joinable()) {
            // Give 500ms for graceful shutdown
            for (int i = 0; i < 50; ++i) {
                if (!io_thread.joinable()) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            if (io_thread.joinable()) {
                // Last resort
                io_context.reset(); // Destroy io_context first
                io_thread.detach();
            }
        }
    }

    cleanupAPIWorkers();
}

void HyniWindow::cleanupAPIWorkers() {
    for (const QString& language : threads.keys()) {
        QThread *thread = threads[language];
        if (thread) {
            thread->quit();
            if (!thread->wait(500)) {
                thread->terminate();
                thread->wait();
            }
            delete thread;
        }
    }
    workers.clear();
    threads.clear();
}

void HyniWindow::setupAPIWorkers() {
    // Setup workers for all languages
    for (const QString& language : responseEditors.keys()) {

        if (language == "History") continue;

        QThread *thread = new QThread(this);
        ChatAPIWorker *worker = new ChatAPIWorker();
        worker->setLanguage(language);

        if (!m_sharedApiKey.isEmpty()) {
            worker->setAPIKey(m_sharedApiKey);
        }

        worker->moveToThread(thread);

        connect(worker, &ChatAPIWorker::responseReceived,
                this, [this, language](const QString& response) {
                    handleAPIResponse(response, language);
                });
        connect(worker, &ChatAPIWorker::errorOccurred,
                this, [this, language](const QString& error) {
                    handleAPIError(error, language);
                });
        connect(worker, &ChatAPIWorker::needApiKey,
                this, [this, language]() {
                    handleNeedAPIKey(language);
                });

        connect(thread, &QThread::finished, worker, &QObject::deleteLater);

        workers[language] = worker;
        threads[language] = thread;
        thread->start();
    }
}

void HyniWindow::handleAPIResponse(const QString& response, const QString& language) {
    if (responseEditors.contains(language)) {
        responseEditors[language]->setMarkdown(response);
        m_history.push_back(response);
        qDebug() << response;
    }

    if (responseEditors.contains("History")) {
        QString history;
        for (const auto& page : m_history) {
            history += page;
            history += "\n\n---\n\n";
        }

        if (!history.isEmpty()) {
            responseEditors["History"]->setMarkdown(history);
        }
    }

    statusBar()->showMessage(language + " response received", 3000);
}

void HyniWindow::handleAPIError(const QString& error, const QString& language) {
    QMessageBox::warning(this, language + " API Error", error);
    statusBar()->showMessage(error, 5000);
}

void HyniWindow::handleNeedAPIKey(const QString& language) {
    // If we already have a key or are in the process of requesting one
    if (!m_sharedApiKey.isEmpty() || m_apiKeyRequested) {
        if (!m_sharedApiKey.isEmpty()) {
            // Distribute the existing key to all workers
            for (ChatAPIWorker* worker : workers.values()) {
                worker->setAPIKey(m_sharedApiKey);
            }
        }
        return;
    }

    m_apiKeyRequested = true;

    bool ok;
    QString label = "Enter your API Key for ";
    if (workers[language]->getProvider() == hyni::chat_api::API_PROVIDER::OpenAI) {
        label += "Open AI";
    } else if (workers[language]->getProvider() == hyni::chat_api::API_PROVIDER::DeepSeek) {
        label += "DeepSeek";
    } else {
        label += "Unknown";
    }
    QString userKey = QInputDialog::getText(nullptr, "API Key", label,
                                            QLineEdit::Normal, "", &ok);

    m_apiKeyRequested = false;

    if (ok && !userKey.isEmpty()) {
        m_sharedApiKey = userKey;
        // Distribute the key to all workers
        for (ChatAPIWorker* worker : workers.values()) {
            worker->setAPIKey(m_sharedApiKey);
        }
    } else {
        statusBar()->showMessage("No API-Key available");
    }
}

void HyniWindow::toggleQuestionType() {
    if (amazonStarOption->isChecked()) {
        systemDesignOption->setChecked(true);
        statusBar()->showMessage("Mode: System Design", 2000);
    }
    else if (systemDesignOption->isChecked()) {
        codingOption->setChecked(true);
        statusBar()->showMessage("Mode: Coding", 2000);
    }
    else {
        amazonStarOption->setChecked(true);
        statusBar()->showMessage("Mode: Amazon Behavioral (STAR)", 2000);
    }
}

void HyniWindow::handleTabNavigation(QKeyEvent* event) {
    int direction = (event->key() == Qt::Key_Left) ? -1 : 1;
    int newIndex = (tabWidget->currentIndex() + direction + tabWidget->count()) % tabWidget->count();

    // Simple version:
    tabWidget->setCurrentIndex(newIndex);
}

void HyniWindow::setupMenuBar(QMenuBar* menuBar) {
    QMenu *aiMenu = menuBar->addMenu("&AI");

    // Create action group for AI selection
    QActionGroup *aiGroup = new QActionGroup(this);
    aiGroup->setExclusive(true);

    // ChatGPT option
    QAction *chatGPTAction = new QAction("&ChatGPT", this);
    chatGPTAction->setCheckable(true);
    chatGPTAction->setChecked(true);
    chatGPTAction->setData("ChatGPT");
    chatGPTAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_1));
    aiGroup->addAction(chatGPTAction);
    aiMenu->addAction(chatGPTAction);

    // DeepSeek option
    QAction *deepSeekAction = new QAction("&DeepSeek", this);
    deepSeekAction->setCheckable(true);
    deepSeekAction->setData("DeepSeek");
    deepSeekAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_2));
    aiGroup->addAction(deepSeekAction);
    aiMenu->addAction(deepSeekAction);

    // Add separator to visually group the exit action
    aiMenu->addSeparator();

    // Exit action
    QAction *exitAction = new QAction("E&xit", this);
    exitAction->setShortcut(QKeySequence::Quit);  // Standard quit shortcut (Ctrl+Q on most platforms)
    aiMenu->addAction(exitAction);

    QMenu *viewMenu = menuBar->addMenu("&View");
    QAction *zoomInAction = viewMenu->addAction("Zoom &In");
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    QAction *zoomOutAction = viewMenu->addAction("Zoom &Out");
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);

    QMenu *helpMenu = menuBar->addMenu("&Help");
    QAction *aboutAction = helpMenu->addAction("&About");

    // Connect signals
    connect(aiGroup, &QActionGroup::triggered, this, &HyniWindow::onAISelectionChanged);
    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);

    connect(zoomInAction, &QAction::triggered, this, &HyniWindow::zoomInResponseBox);
    connect(zoomOutAction, &QAction::triggered, this, &HyniWindow::zoomOutResponseBox);

    connect(aboutAction, &QAction::triggered, this, &HyniWindow::showAboutDialog);
}

void HyniWindow::onAISelectionChanged(QAction* action) {
    hyni::chat_api::API_PROVIDER current = workers.first()->getProvider();
    hyni::chat_api::API_PROVIDER newProvider;
    QString selectedAI = action->data().toString();
    if (selectedAI == "ChatGPT") {
        newProvider = hyni::chat_api::API_PROVIDER::OpenAI;
    }
    else {
        newProvider = hyni::chat_api::API_PROVIDER::DeepSeek;
    }

    if (current != newProvider) {
        for (const QString& language : workers.keys()) {
            if (language == "History") continue;
            workers[language]->setProvider(newProvider);
        }
    }

    // You could also update the status bar
    statusBar()->showMessage(selectedAI + " selected", 2000);
}

void HyniWindow::zoomInResponseBox() {
    for (const QString& language : responseEditors.keys()) {
        if (language == "History") continue;
        QFont font = responseEditors[language]->font();
        font.setPointSize(font.pointSize() + 1);
        responseEditors[language]->setFont(font);
    }
}

void HyniWindow::zoomOutResponseBox() {
    for (const QString& language : responseEditors.keys()) {
        if (language == "History") continue;
        QFont font = responseEditors[language]->font();
        font.setPointSize(std::max(font.pointSize() - 1, 6));
        responseEditors[language]->setFont(font);
    }
}

void HyniWindow::keyPressEvent(QKeyEvent* event) {
    switch(event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Right:
        handleTabNavigation(event);
        return;
    case Qt::Key_S:            // Send as prompt
        sendText();
        break;
    case Qt::Key_C:            // Clear transcribed text
        highlightTableWidget->clearRow();
        break;
    case Qt::Key_T:            // Toggle question types
        toggleQuestionType();
        break;
    case Qt::Key_A: {          // Append and send
        QString last = promptTextBox->toPlainText();
        last += ". " + highlightTableWidget->getLastRowString();
        highlightTableWidget->clearRow();
        highlightTableWidget->addText(last);
        sendText();
        break;
    }
    case Qt::Key_R:            // Resend
        sendText(true);
        break;
    case Qt::Key_P:            // Screenshot
        QTimer::singleShot(3000, this, &HyniWindow::captureScreen);
        break;
    default:
        QMainWindow::keyPressEvent(event);
    }
}

void HyniWindow::captureScreen() {
    QString savePath = "test.png";
    QStringList args = {"-a", "-b", "--nonotify", "-o", savePath};

    QProcess spectacle;
    spectacle.start("spectacle", args);

    if (spectacle.waitForFinished(3000)) {  // Wait up to 3 seconds
        if (!QFile::exists(savePath)) {
            qDebug() << "Error: File not found at" << savePath;
            return;
        }

        QPixmap pixmap;
        if (!pixmap.load(savePath)) {
            qDebug() << "Error: Failed to load image from" << savePath;
            return;
        }

        for (const auto& [language, editor] : responseEditors.toStdMap()) {
            if (language == "History") continue;
            editor->setPlainText("Processing...");
        }
        QApplication::processEvents();

        if (codingOption->isChecked()) {
            // Send to all workers
            for (const QString& language : workers.keys()) {
                if (language == "History") continue;
                QMetaObject::invokeMethod(workers[language], "sendImageRequest",
                                          Qt::QueuedConnection,
                                          Q_ARG(QPixmap, pixmap),
                                          Q_ARG(hyni::chat_api::QUESTION_TYPE, hyni::chat_api::QUESTION_TYPE::Coding));
            }
        }
        else {
            QMetaObject::invokeMethod(workers.first(), "sendImageRequest",
                                      Qt::QueuedConnection,
                                      Q_ARG(QPixmap, pixmap),
                                      Q_ARG(hyni::chat_api::QUESTION_TYPE, hyni::chat_api::QUESTION_TYPE::SystemDesign));
        }
    }
}

void HyniWindow::handleCapturedScreen(const QPixmap& pixmap) {
    for (const auto& [language, editor] : responseEditors.toStdMap()) {
        if (language == "History") continue;
        editor->setPlainText("Processing...");
    }
    QApplication::processEvents();

    if (codingOption->isChecked()) {
        // Send to all workers
        for (const QString& language : workers.keys()) {
            if (language == "History") continue;
            QMetaObject::invokeMethod(workers[language], "sendImageRequest",
                                      Qt::QueuedConnection,
                                      Q_ARG(QPixmap, pixmap),
                                      Q_ARG(hyni::chat_api::QUESTION_TYPE, hyni::chat_api::QUESTION_TYPE::Coding));
        }
    }
    else {
        QMetaObject::invokeMethod(workers.first(), "sendImageRequest",
                                  Qt::QueuedConnection,
                                  Q_ARG(QPixmap, pixmap),
                                  Q_ARG(hyni::chat_api::QUESTION_TYPE, hyni::chat_api::QUESTION_TYPE::SystemDesign));
    }
}

void HyniWindow::sendText(bool repeat) {

    QString text = highlightTableWidget->getLastRowString();
    if (repeat) {
        QString last(text);
        text = promptTextBox->toPlainText();
        text += ". ";
        text += last;
    }

    if (text.isEmpty()) return;

    promptTextBox->setText(text);
    highlightTableWidget->setRowCount(0);

    hyni::chat_api::QUESTION_TYPE qType;
    QString enhancedPrompt;
    bool multiLanguage = true;

    // Set "Processing..." for relevant editors
    if (amazonStarOption->isChecked()) {
        // STAR question - only first tab (C++)
        qType = hyni::chat_api::QUESTION_TYPE::Behavioral;
        responseEditors.first()->setPlainText("Processing...");
        multiLanguage = false;
    }
    else if (systemDesignOption->isChecked()) {
        qType = hyni::chat_api::QUESTION_TYPE::SystemDesign;
        enhancedPrompt = text;
        enhancedPrompt += hyni::SYSTEM_DESIGN_EXT;
        multiLanguage = false;
    }
    else { // Coding option
        qType = hyni::chat_api::QUESTION_TYPE::Coding;
        multiLanguage = true;
    }

    if (!multiLanguage) {
        responseEditors.first()->setPlainText("Processing...");
    } else {
        for (const auto& [language, editor] : responseEditors.toStdMap()) {
            if (language == "History") continue;
            editor->setPlainText("Processing...");
        }
    }
    QApplication::processEvents();

    // Send requests
    if (!multiLanguage) {
        // For STAR and System Design (single response)
        QMetaObject::invokeMethod(workers.first(), "sendRequest",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, enhancedPrompt),
                                  Q_ARG(hyni::chat_api::QUESTION_TYPE, qType));
    }
    else {
        // For Coding (language-specific responses)
        for (const auto &[language, worker] : workers.asKeyValueRange()) {
            if (language == "History") continue;
            QString langSpecificPrompt = text;
            langSpecificPrompt += hyni::CODING_EXT;
            langSpecificPrompt = langSpecificPrompt.arg(language);

            QMetaObject::invokeMethod(worker, "sendRequest",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, langSpecificPrompt),
                                      Q_ARG(hyni::chat_api::QUESTION_TYPE, qType));
        }
    }
}

void HyniWindow::handleHighlightedText(const QString& texts) {
    highlightedText = texts;
}

void HyniWindow::onWebSocketConnected(bool connected) {
    if (connected) {
        statusBar()->showMessage("Connected to wstream server.");
        reconnectTimer->stop();
    } else {
        statusBar()->showMessage("Disconnected. Attempting to reconnect...");
        reconnectTimer->start(3000); // Start trying to reconnect
    }
}

void HyniWindow::onWebSocketError(const std::string& error) {
    statusBar()->showMessage(QString::fromStdString("WebSocket error: " + error));
}

void HyniWindow::attemptReconnect() {
    if (!websocketClient->is_connected()) {
        statusBar()->showMessage("Trying to reconnect...");
        websocketClient->connect();
    }
}

void HyniWindow::onMessageReceived(const std::string& message) {
    qDebug() << "Message Received:" << QString::fromStdString(message);

    try {
        auto json = nlohmann::json::parse(message);

        // Check the message type
        if (json.contains("type")) {
            std::string type = json["type"];

            if (type == "transcribe") {
                // Handle transcribe messages
                std::string content = json["content"];
                qDebug() << "Transcribe Content:" << QString::fromStdString(content);

                // Add the transcribed text to the HighlightTableWidget
                highlightTableWidget->addText(QString::fromStdString(content));
            } else {
                qDebug() << "Unknown message type received:" << QString::fromStdString(type);
            }
        } else {
            qDebug() << "Message does not contain a 'type' field.";
        }
    } catch (const std::exception& e) {
        qDebug() << "Invalid JSON message received:" << e.what();
    }
}

void HyniWindow::receiveAudioData(const QByteArray& data) {
    std::vector<uint8_t> audioData(data.begin(), data.end());
    websocketClient->sendAudioBuffer(audioData);
}

void HyniWindow::showAboutDialog()
{
    QMessageBox::about(this, "About Qhyni",
                       "<h2>Qhyni</h2>"
                       "<p>Version 1.0</p>"
                       "<p>hyni UI with gen AI and real-time transcription</p>");
}
