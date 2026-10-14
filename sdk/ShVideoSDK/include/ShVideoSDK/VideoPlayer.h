#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include <QWidget>
#include <QtGlobal>

// 20261013 hello: QObject metadata is data, so Windows DLL consumers need
// explicit import/export decoration as well as exported function symbols.
#if defined(_WIN32) && defined(VIDEOSDK_SHARED)
#  if defined(VIDEOSDK_BUILDING_RUNTIME)
#    define VIDEOSDK_API Q_DECL_EXPORT
#  else
#    define VIDEOSDK_API Q_DECL_IMPORT
#  endif
#else
#  define VIDEOSDK_API
#endif

namespace VideoSDK {

struct OpenOptions
{
    QString mediaUrl;
    QString transport = QStringLiteral("auto");
    QString hardwareDecoder = QStringLiteral("none");
    QString audioDevice;
    int connectTimeoutMs = 500;
    int readTimeoutMs = 0;
    int cacheSizeMb = 0;
    bool decodeAudio = true;
    bool playAudio = true;
    bool repeat = false;
};

class VIDEOSDK_API VideoPlayer final : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPlayer(QWidget *parent = nullptr);
    ~VideoPlayer() override;

    VideoPlayer(const VideoPlayer &) = delete;
    VideoPlayer &operator=(const VideoPlayer &) = delete;

    [[nodiscard]] static QString backendVersion();
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] bool isPaused() const;
    [[nodiscard]] bool isRecording() const;
    [[nodiscard]] bool isDigitalZoomEnabled() const;
    [[nodiscard]] bool isStretchToFill() const;
    [[nodiscard]] QSize videoSize() const;
    [[nodiscard]] QImage currentImage() const;

public slots:
    bool open(const VideoSDK::OpenOptions &options);
    void play();
    void pause();
    void resume();
    void stop();
    void snapshot(const QString &fileName = QString());
    void startRecording(const QString &fileName);
    void pauseRecording();
    void stopRecording();
    void setMuted(bool muted);
    void setVolume(int percent);
    void setDigitalZoomEnabled(bool enabled);
    void resetDigitalZoom();
    void setStretchToFill(bool enabled);

signals:
    void playbackStarted(int elapsedMs);
    void playbackStopped();
    void playbackError(int errorCode);
    void bitRateChanged(qreal kbps, int frameRate);

private:
    class Impl;
    Impl *m_impl = nullptr;
};

} // namespace VideoSDK

Q_DECLARE_METATYPE(VideoSDK::OpenOptions)
