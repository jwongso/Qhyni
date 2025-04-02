#include "ChatAPIWorker.h"
#include "config.h"
#include <QDebug>
#include <exception>
#include <qthread.h>

ChatAPIWorker::ChatAPIWorker(QObject *parent)
    : QObject(parent),
    m_isBusy(false),
    m_cancelRequested(false) {
    try {
        m_chatAPI = std::make_unique<hyni::ChatAPI>(hyni::DS_API_URL);
    } catch (const std::exception& e) {
        qCritical() << "API init failed:" << e.what();
        emit errorOccurred(QString("API initialization failed: %1").arg(e.what()));
    }
}

ChatAPIWorker::~ChatAPIWorker() {
    if (QThread::currentThread() == this->thread()) {
        // Direct deletion if already in correct thread
        m_chatAPI.reset();
    } else {
        // Non-blocking deferred deletion
        QMetaObject::invokeMethod(this, [this]() {
            m_chatAPI.reset();
        }, Qt::QueuedConnection);
    }
}

void ChatAPIWorker::sendRequest(const QString& message, bool isStarQuestion) {
    // Early return if busy (thread-safe)
    if (m_isBusy.exchange(true)) {
        qDebug() << "Request ignored - worker busy";
        return;
    }

    m_cancelRequested.store(false); // Reset cancellation flag

    try {
        const auto questionType = isStarQuestion
                                      ? hyni::ChatAPI::QuestionType::AmazonBehavioral
                                      : hyni::ChatAPI::QuestionType::General;

        // Single cancellation check point
        const bool wasCancelled = [&]() {
            if (m_cancelRequested.load()) return true;

            auto response = m_chatAPI->sendMessage(
                message.toStdString(),
                questionType,
                1500,
                0.7,
                [this]() { return m_cancelRequested.load(); }
                );

            if (m_cancelRequested.load()) return true;

            response = m_chatAPI->getAssistantReply(response);
            emit responseReceived(QString::fromStdString(response));
            return false;
        }();

        if (wasCancelled) {
            emit requestCancelled();
        }
    }
    catch (const std::exception& e) {
        if (!m_cancelRequested.load()) {
            qWarning() << "API error:" << e.what();
            emit errorOccurred(QString("API request failed: %1").arg(e.what()));
        }
    }

    m_isBusy.store(false); // Reset busy flag
}

void ChatAPIWorker::cancelCurrentRequest() {
    m_cancelRequested.store(true);
    if (m_chatAPI) {
        m_chatAPI->cancel();
    }
}
