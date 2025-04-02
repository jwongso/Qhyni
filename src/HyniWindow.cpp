#include "HyniWindow.h"
#include "ChatAPIWorker.h"
#include <QTimer>
#include <QThread>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <qapplication.h>
#include <qevent.h>

HyniWindow::HyniWindow(QWidget *parent)
    : QMainWindow(parent), reconnectTimer(std::make_unique<QTimer>(this)),
    io_context(std::make_unique<boost::asio::io_context>())
{
    setWindowTitle("Hyni - Real-time Transcription");
    resize(800, 500);
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

    starOption = new QRadioButton("STAR", this);
    generalOption = new QRadioButton("General (Coding/System Design)", this);
    generalOption->setChecked(true);  // Default selection

    questionTypeLayout->addWidget(starOption);
    questionTypeLayout->addWidget(generalOption);
    questionTypeBox->setLayout(questionTypeLayout);

    leftLayout->addWidget(leftSplitter);
    leftLayout->addWidget(questionTypeBox);

    leftWidget->setLayout(leftLayout);
    splitter->addWidget(leftWidget);

    // Right side: GPT response text box
    responseBox = new QTextEdit(this);
    responseBox->setPlaceholderText("GPT Response");
    responseBox->setReadOnly(true);
    splitter->addWidget(responseBox);

    // Adjust initial size proportions
    splitter->setSizes({400, 400});

    mainLayout->addWidget(splitter);

    QPushButton *sendButton = new QPushButton("Send Selection to GPT", this);
    mainLayout->addWidget(sendButton);
    connect(sendButton, &QPushButton::clicked, this, &HyniWindow::sendText);

    connect(highlightTableWidget, &HighlightTableWidget::textHighlighted, this, &HyniWindow::handleHighlightedText);

    setCentralWidget(centralWidget);

    // Initialize WebSocket client
    websocketClient = std::make_shared<HyniWebSocketClient>(*io_context, "localhost", "8080");
    websocketClient->setMessageHandler([this](const std::string& message) {
        QMetaObject::invokeMethod(this, [this, message]() {
            onMessageReceived(message);
        });
    });
    websocketClient->setConnectionHandler([this](bool connected) {
        QMetaObject::invokeMethod(this, [this, connected]() {
            onWebSocketConnected(connected);
        });
    });
    websocketClient->setErrorHandler([this](const std::string& error) {
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

    setupAPIWorker();
}

HyniWindow::~HyniWindow() {
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

    if (m_apiThread) {
        // 1. Signal shutdown
        m_apiThread->quit();

        // 2. Non-blocking worker cleanup
        m_apiWorker->deleteLater();

        // 3. Wait with timeout
        if (!m_apiThread->wait(500)) {
            qWarning() << "API thread not responding, terminating";
            m_apiThread->terminate();
            m_apiThread->wait();
        }

        // 4. Now safe to delete thread
        delete m_apiThread;
        m_apiThread = nullptr;
    }
}

void HyniWindow::setupAPIWorker() {
    m_apiThread = new QThread(this);
    m_apiWorker = new ChatAPIWorker(); // No parent!

    m_apiWorker->moveToThread(m_apiThread);

    // Automatic cleanup connections
    connect(m_apiThread, &QThread::finished,
            m_apiWorker, &QObject::deleteLater);
    connect(m_apiThread, &QThread::finished,
            m_apiThread, &QObject::deleteLater);

    m_apiThread->start();
}

void HyniWindow::handleAPIResponse(const QString& response) {
    responseBox->setMarkdown(response);
    statusBar()->showMessage("Response received", 3000);
}

void HyniWindow::handleAPIError(const QString& error) {
    QMessageBox::warning(this, "API Error", error);
    statusBar()->showMessage(error, 5000);
}

void HyniWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_S) {            /// Send as prompt
        sendText();
    } else if (event->key() == Qt::Key_C) {     /// Clear the transcribed text
        highlightTableWidget->clearRow();
    } else if (event->key() == Qt::Key_T) {     /// Toggle STAR and coding
        if (starOption->isChecked()) {
            generalOption->setChecked(true);
        }
        else {
            starOption->setChecked(true);
        }
    } else if (event->key() == Qt::Key_A) {     /// Add current transcribed text into the last prompt and send it again
        QString last = promptTextBox->toPlainText();
        last += ". ";
        last += highlightTableWidget->getLastRowString();
        highlightTableWidget->clearRow();
        highlightTableWidget->addText(last);
        sendText();
    } else if (event->key() == Qt::Key_R) {     /// Re-send again the same text from prompt box.
        sendText(true);
    }

    // Call the base class implementation to handle other key events
    QMainWindow::keyPressEvent(event);
}

void HyniWindow::sendText(bool repeat) {

    QString text = highlightTableWidget->getLastRowString();
    if (repeat) {
        text = promptTextBox->toPlainText();
    }

    if (text.isEmpty()) return;

    promptTextBox->setText(text);
    highlightTableWidget->setRowCount(0);

    responseBox->setPlainText("Processing...");
    QApplication::processEvents();

    // Queue the request
    QMetaObject::invokeMethod(m_apiWorker, "sendRequest",
                              Qt::QueuedConnection,
                              Q_ARG(QString, text),
                              Q_ARG(bool, starOption->isChecked()));
}

void HyniWindow::handleHighlightedText(const QString& texts) {
    highlightedText = texts;
}

void HyniWindow::onWebSocketConnected(bool connected) {
    if (connected) {
        statusBar()->showMessage("Connected to WebSocket server.");
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
    if (!websocketClient->isConnected()) {
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
