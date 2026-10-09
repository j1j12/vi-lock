#ifndef VISION_WORKER_H
#define VISION_WORKER_H
#include <QThread>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <atomic>
#include <opencv2/core.hpp>
#include <QImage>
#include <QElapsedTimer>
#include "enrollment_session.h"

// One bounded latest-frame slot, never a queued signal for every camera frame.
class CameraWorker : public QThread {
    Q_OBJECT
public:
    bool snapshot(cv::Mat &frame, quint64 &sequence, qint64 *ageMs = nullptr);
signals:
    void status(const QString &text);
protected:
    void run() override;
private:
    QMutex mutex_;
    cv::Mat latest_;
    quint64 sequence_ = 0;
    QElapsedTimer latestAge_;
};

class RecognitionWorker : public QThread {
    Q_OBJECT
public:
    explicit RecognitionWorker(CameraWorker &camera) : camera_(camera) {}
    bool trigger(quint64 ticket = 0); // Busy events are not queued.
    bool manage(const QString &operation, const QString &id = QString(), const QString &ruleJson = QString(), quint64 session = 0);
    quint64 beginEnrollment() { return enrollment_.begin(); }
    void cancelEnrollment(quint64 session) { enrollment_.cancel(session); }
    bool detectionSnapshot(quint64 session, QImage &image, QString &caption, qint64 &ageMs);
    bool canTrigger() const { return ready_.load() && !busy_.load(); }
signals:
    void decision(quint64 ticket, bool matched, const QString &personId);
    void status(const QString &text);
    void result(const QString &text, const QString &name);
    void people(const QStringList &ids, const QStringList &states, const QStringList &rules);
    void managementResult(const QString &operation, const QString &id, bool ok, const QString &detail);
    void enrollmentResult(quint64 session, bool ok, const QString &detail);
protected:
    void run() override;
private:
    CameraWorker &camera_;
    std::atomic<bool> ready_{false};
    std::atomic<bool> busy_{false};
    std::atomic<bool> pending_{false};
    quint64 ticket_ = 0; // Published by pending_ release/acquire; busy_ guards reuse.
    QString operation_, personId_; // Same publication protocol as ticket_.
    QString ruleJson_;
    EnrollmentSession enrollment_;
    quint64 session_ = 0;
    void publishDetection(quint64 session, const QImage &image, const QString &caption, qint64 inputAge);
    QMutex detectionMutex_;
    QImage detectionImage_;
    QString detectionCaption_;
    quint64 detectionSession_ = 0;
    QElapsedTimer detectionAge_;
    qint64 detectionInputAge_ = 0;
};
#endif
