#ifndef CHATAPI_WORKER_H
#define CHATAPI_WORKER_H

#include <QObject>
#include <memory>
#include "chat_api.h"
#include <atomic>

class ChatAPIWorker : public QObject {
    Q_OBJECT

public:
    explicit ChatAPIWorker(QObject *parent = nullptr);
    ~ChatAPIWorker();
    hyni::chat_api::API_PROVIDER getProvider() const;
    void setLanguage(const QString& language);
    void setProvider(hyni::chat_api::API_PROVIDER);

public slots:
    void sendImageRequest(const QPixmap& pixmap, hyni::chat_api::QUESTION_TYPE type);
    void sendRequest(const QString& message, hyni::chat_api::QUESTION_TYPE type);
    void cancelCurrentRequest();
    void setAPIKey(const QString& apiKey);

signals:
    void responseReceived(const QString& response);
    void errorOccurred(const QString& error);
    void requestCancelled();
    void needApiKey();

private:
    std::unique_ptr<hyni::chat_api> m_chatAPI;
    std::atomic<bool> m_isBusy{false};
    std::atomic<bool> m_cancelRequested{false};
    QString m_language;
};

#endif // CHATAPI_WORKER_H
