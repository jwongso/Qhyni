#include "ChatAPIWorker.h"
#include "chat_api.h"
#include "config.h"
#include <QTimer>
#include <QDebug>
#include <exception>
#include <qimage.h>
#include <qpixmap.h>
#include <qthread.h>
#include <QBuffer>

std::string encodePixmapToBase64(const QPixmap& pixmap, const char* format = "PNG", int quality = 90) {
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);

    if (!pixmap.save(&buffer, format, quality)) {
        throw std::runtime_error("Failed to save QPixmap to buffer.");
    }

    return byteArray.toBase64().toStdString();
}

ChatAPIWorker::ChatAPIWorker(QObject *parent)
    : QObject(parent),
    m_isBusy(false),
    m_cancelRequested(false) {
    try {
        m_chatAPI = std::make_unique<hyni::chat_api>(hyni::GPT_API_URL);
        if (!m_chatAPI->has_api_key()) {
            QTimer::singleShot(2000, [this]() {
                emit needApiKey();
            });
        }
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

hyni::chat_api::API_PROVIDER ChatAPIWorker::getProvider() const {
    return m_chatAPI ? m_chatAPI->get_api_provider() :
        hyni::chat_api::API_PROVIDER::Unknown;
}

void ChatAPIWorker::setProvider(hyni::chat_api::API_PROVIDER provider) {
    if (m_chatAPI->get_api_provider() != provider) {
        m_chatAPI.reset(new hyni::chat_api(provider));
    }
}

void ChatAPIWorker::sendImageRequest(const QPixmap& pixmap,
                                     const QString& language,
                                     hyni::chat_api::QUESTION_TYPE type) {
    if (m_isBusy.exchange(true)) {
        qDebug() << "Image request ignored - worker busy";
        return;
    }

    if (!m_chatAPI->has_api_key()) {
        emit needApiKey();
        return;
    }

    m_cancelRequested.store(false);

    try {
        // Encode QPixmap to base64 JPEG
        const std::string base64Image = encodePixmapToBase64(pixmap, "PNG", 90);

        const bool wasCancelled = [&]() {
            if (m_cancelRequested.load()) return true;

            QString enhancedPrompt;

            if (type == hyni::chat_api::QUESTION_TYPE::Coding) {
                enhancedPrompt += hyni::CODING_EXT;
                enhancedPrompt = enhancedPrompt.arg(language);
            } else if (type == hyni::chat_api::QUESTION_TYPE::SystemDesign) {
                enhancedPrompt += hyni::SYSTEM_DESIGN_EXT;
            } else {
                enhancedPrompt = "Solve the questions asked from the image.";
            }

            qDebug() << enhancedPrompt;

            auto response = m_chatAPI->send_image(
                base64Image,
                type,
                enhancedPrompt.toStdString(),
                1500,
                0.7,
                [this]() { return m_cancelRequested.load(); }
                );

            if (m_cancelRequested.load()) return true;

            response = m_chatAPI->get_assistant_reply(response);
            emit responseReceived(QString::fromStdString(response));
            return false;
        }();

        if (wasCancelled) {
            emit requestCancelled();
        }
    }
    catch (const std::exception& e) {
        if (!m_cancelRequested.load()) {
            qWarning() << "Image API error:" << e.what();
            emit errorOccurred(QString("Image API request failed: %1").arg(e.what()));
        }
    }

    m_isBusy.store(false); // Reset busy flag
}


void ChatAPIWorker::sendRequest(const QString& message, hyni::chat_api::QUESTION_TYPE type) {
    if (m_isBusy.exchange(true)) {
        qDebug() << "Request ignored - worker busy";
        return;
    }

    if (!m_chatAPI->has_api_key()) {
        emit needApiKey();
        return;
    }

    m_cancelRequested.store(false);

    try {
        const bool wasCancelled = [&]() {
            if (m_cancelRequested.load()) return true;

            qDebug() << message;

            auto response = m_chatAPI->send_message(
                message.toStdString(),
                type,
                1500,
                0.7,
                [this]() { return m_cancelRequested.load(); }
                );

            if (m_cancelRequested.load()) return true;

            response = m_chatAPI->get_assistant_reply(response);
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

void ChatAPIWorker::setAPIKey(const QString& apiKey) {
    if (m_chatAPI) {
        m_chatAPI->set_api_key(apiKey.toStdString());
    }
}
