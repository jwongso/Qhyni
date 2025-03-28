#include "HyniWindow.h"
#include <QSplitter>
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStatusBar>
#include <QGroupBox>
#include <QRadioButton>

HyniWindow::HyniWindow(QWidget *parent)
: QMainWindow(parent)
, websocket(new QWebSocket)
, reconnectTimer(new QTimer(this))
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
    connect(highlightTableWidget, &HighlightTableWidget::triggerSendText, this, &HyniWindow::sendText);

    setCentralWidget(centralWidget);

    // WebSocket connection
    connect(websocket, &QWebSocket::connected, this, &HyniWindow::onConnected);
    connect(websocket, &QWebSocket::disconnected, this, &HyniWindow::onDisconnected);
    connect(websocket, &QWebSocket::textMessageReceived, this, &HyniWindow::onMessageReceived);
    connect(websocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &HyniWindow::onError);

    // Reconnect logic
    connect(reconnectTimer, &QTimer::timeout, this, &HyniWindow::attemptReconnect);
    reconnectTimer->start(3000); // Try reconnecting every 3 seconds

    attemptReconnect(); // Try initial connection
    statusBar()->showMessage("Disconnected");
}

HyniWindow::~HyniWindow() {
    delete websocket;
    delete reconnectTimer;
}

void HyniWindow::keyPressEvent(QKeyEvent* event) {
    // Check if the 's' key is pressed
    if (event->key() == Qt::Key_S) {
        sendText();
    } else if (event->key() == Qt::Key_C) {
        highlightTableWidget->clearRow();
    } else if (event->key() == Qt::Key_T) {
        if (starOption->isChecked()) {
            generalOption->setChecked(true);
        }
        else {
            starOption->setChecked(true);
        }
    }

    // Call the base class implementation to handle other key events
    QMainWindow::keyPressEvent(event);
}

void HyniWindow::sendText() {

    const QString text = highlightTableWidget->getLastRowString();

    if (websocket && websocket->isValid() && !text.isEmpty()) {
        // Create a JSON object with the highlighted text
        QJsonObject prompt_message;
        prompt_message["type"] = "prompt";
        prompt_message["content"] = text;
        prompt_message["star"] = starOption->isChecked();

        // Convert the JSON object to a JSON document
        QJsonDocument jsonDocument(prompt_message);

        // Convert the JSON document to a compact string
        QString jsonString = jsonDocument.toJson(QJsonDocument::Compact);

        // Send the JSON string over the WebSocket
        websocket->sendTextMessage(jsonString);

        // Display the highlighted text in promptTextBox
        promptTextBox->setText(text);

        highlightTableWidget->setRowCount(0);
    } else {
        qDebug() << "WebSocket is not valid or not connected.";
    }
}

void HyniWindow::handleHighlightedText(const QString& texts) {
    highlightedText = texts;
}

void HyniWindow::onConnected() {
    statusBar()->showMessage("Connected to WebSocket server.");
    reconnectTimer->stop();
}

void HyniWindow::onDisconnected() {
    statusBar()->showMessage("Disconnected. Attempting to reconnect...");
    reconnectTimer->start(3000); // Start trying to reconnect
}

void HyniWindow::attemptReconnect() {
    if (websocket->state() == QAbstractSocket::UnconnectedState) {
        statusBar()->showMessage("Trying to reconnect...");
        websocket->open(QUrl("ws://localhost:8080"));

        // Exponential backoff (e.g., 3s, 6s, 12s, ...)
        static int retryInterval = 3000;
        reconnectTimer->start(retryInterval);
        retryInterval = qMin(retryInterval * 2, 30000); // Cap at 30 seconds
    }
}

void HyniWindow::onError(QAbstractSocket::SocketError error) {
    statusBar()->showMessage("WebSocket error: " + websocket->errorString());
}

void HyniWindow::onMessageReceived(const QString& message) {
    qDebug() << "Message Received:" << message;

    // Parse the incoming message as JSON
    QJsonDocument jsonDocument = QJsonDocument::fromJson(message.toUtf8());
    if (jsonDocument.isNull() || !jsonDocument.isObject()) {
        qDebug() << "Invalid JSON message received.";
        return;
    }

    QJsonObject jsonObject = jsonDocument.object();

    // Check the message type
    if (jsonObject.contains("type")) {
        QString type = jsonObject["type"].toString();

        if (type == "response") {
            // Handle response messages
            QString content = jsonObject["content"].toString();
            qDebug() << "Response Content:" << content;

            // Display the response in promptTextBox
            responseBox->setMarkdown(content);
        } else if (type == "transcribe") {
            // Handle transcribe messages
            QString content = jsonObject["content"].toString();
            qDebug() << "Transcribe Content:" << content;

            // Add the transcribed text to the HighlightTableWidget
            highlightTableWidget->addText(content);
        } else {
            qDebug() << "Unknown message type received:" << type;
        }
    } else {
        qDebug() << "Message does not contain a 'type' field.";
    }
}
