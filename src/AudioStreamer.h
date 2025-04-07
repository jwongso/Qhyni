#ifndef AUDIOSTREAMER_H
#define AUDIOSTREAMER_H

#include <QIODevice>
#include <QAudioSource>
#include <QMediaDevices>
#include <QAudioFormat>
#include <QByteArray>
#include <memory>

class AudioStreamer : public QIODevice
{
    Q_OBJECT

public:
    explicit AudioStreamer(QObject *parent = nullptr);
    ~AudioStreamer();

    void startRecording();
    void stopRecording();
    bool isRecording() const;

    void setSampleRate(int rate);
    int sampleRate() const;

signals:
    void audioDataReady(const QByteArray &data);
    void errorOccurred(const QString &message);

protected:
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;

private:
    std::unique_ptr<QAudioSource> m_audioInput;
    QAudioFormat m_format;
    QByteArray m_buffer;
};

#endif // AUDIOSTREAMER_H
