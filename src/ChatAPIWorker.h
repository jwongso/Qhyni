#ifndef CHATAPI_WORKER_H
#define CHATAPI_WORKER_H

#include <QObject>
#include <memory>
#include "chatapi.h"
#include <atomic>

class ChatAPIWorker : public QObject {
    Q_OBJECT

public:
    explicit ChatAPIWorker(QObject *parent = nullptr);
    ~ChatAPIWorker() {}

public slots:
    void sendRequest(const QString& message, bool isStarQuestion);
    void cancelCurrentRequest();

signals:
    void responseReceived(const QString& response);
    void errorOccurred(const QString& error);
    void requestCancelled();

private:
    std::unique_ptr<hyni::ChatAPI> m_chatAPI;
    std::atomic<bool> m_isBusy{false};
    std::atomic<bool> m_cancelRequested{false};
};

#endif // CHATAPI_WORKER_H
