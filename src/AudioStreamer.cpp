#include "AudioStreamer.h"
#include <QDebug>
#include <QMediaDevices>

AudioStreamer::AudioStreamer(QObject *parent)
    : QIODevice(parent)
{
    // Initialize with optimal settings for speech recognition
    m_format.setSampleRate(16000);  // Standard for speech processing
    m_format.setChannelCount(1);    // Mono
    m_format.setSampleFormat(QAudioFormat::Int16);

    // Verify and adjust format based on device capabilities
    const QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (!device.isFormatSupported(m_format)) {
        qWarning() << "Default format not supported, using nearest supported format";
        m_format = device.preferredFormat();
    }

    m_audioInput = std::make_unique<QAudioSource>(device, m_format);
    m_audioInput->setBufferSize(m_format.bytesForDuration(100000)); // 100ms buffer
}

AudioStreamer::~AudioStreamer()
{
    stopRecording();
}

void AudioStreamer::startRecording()
{
    if (!isOpen()) {
        open(QIODevice::WriteOnly);
        m_audioInput->start(this);
        qDebug() << "Recording started with sample rate:" << m_format.sampleRate();
    }
}

void AudioStreamer::stopRecording()
{
    if (isOpen()) {
        m_audioInput->stop();
        close();
        qDebug() << "Recording stopped";
    }
}

bool AudioStreamer::isRecording() const
{
    return isOpen() && (m_audioInput->state() == QAudio::ActiveState);
}

void AudioStreamer::setSampleRate(int rate)
{
    if (m_format.sampleRate() != rate) {
        m_format.setSampleRate(rate);
        const QAudioDevice device = QMediaDevices::defaultAudioInput();
        if (!device.isFormatSupported(m_format)) {
            qWarning() << "Requested sample rate not supported, using device default";
            m_format = device.preferredFormat();
        }

        if (isRecording()) {
            stopRecording();
            m_audioInput = std::make_unique<QAudioSource>(device, m_format);
            startRecording();
        }
    }
}

int AudioStreamer::sampleRate() const
{
    return m_format.sampleRate();
}

qint64 AudioStreamer::readData(char *data, qint64 maxlen)
{
    Q_UNUSED(data);
    Q_UNUSED(maxlen);
    return 0;  // We're only capturing audio, not providing it
}

qint64 AudioStreamer::writeData(const char *data, qint64 len)
{
    // Store the incoming audio data
    const QByteArray newData(data, len);
    m_buffer.append(newData);

    // Emit signal with the new data
    emit audioDataReady(newData);

    // Return number of bytes processed
    return len;
}
