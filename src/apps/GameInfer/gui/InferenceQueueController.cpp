#include "InferenceQueueController.h"

#include <QMetaObject>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <exception>
#include <utility>

InferenceQueueController::InferenceQueueController(QObject *parent) : QObject(parent) {}

InferenceQueueController::~InferenceQueueController() {
    m_stopRequested.store(true);
    if (m_future.isRunning()) {
        m_future.waitForFinished();
    }
}

const QVector<QueueJob> &InferenceQueueController::jobs() const { return m_jobs; }

bool InferenceQueueController::isRunning() const { return m_running; }

quint64 InferenceQueueController::addJob(QueueJob job) {
    if (m_running) {
        return 0;
    }

    job.id = m_nextId++;
    job.status = QueueJobStatus::Pending;
    job.failureStage = QueueFailureStage::None;
    job.progress = 0;
    job.error.clear();
    job.vocalsPath.clear();
    job.instrumentalPath.clear();
    m_jobs.push_back(std::move(job));
    emit jobsChanged();
    return m_jobs.constLast().id;
}

bool InferenceQueueController::updateJob(const quint64 id, const QueueJob &job) {
    if (m_running) {
        return false;
    }

    const int index = indexOf(id);
    if (index < 0) {
        return false;
    }

    QueueJob updated = job;
    updated.id = id;
    const QueueJob &previous = m_jobs[index];
    const bool separationArtifactStillApplies = updated.inputPath == previous.inputPath &&
                                                updated.outputPath == previous.outputPath &&
                                                !previous.vocalsPath.isEmpty();
    if (previous.status == QueueJobStatus::Completed) {
        updated.status = QueueJobStatus::Completed;
    } else if (separationArtifactStillApplies &&
               (previous.status == QueueJobStatus::Separated ||
                (previous.status == QueueJobStatus::Failed && previous.failureStage == QueueFailureStage::Midi))) {
        updated.status = QueueJobStatus::Separated;
    } else {
        updated.status = QueueJobStatus::Pending;
    }
    updated.failureStage = QueueFailureStage::None;
    updated.progress = updated.status == QueueJobStatus::Completed ? 100 : 0;
    updated.error.clear();
    if (updated.status == QueueJobStatus::Pending) {
        updated.vocalsPath.clear();
        updated.instrumentalPath.clear();
    }
    m_jobs[index] = std::move(updated);
    emit jobsChanged();
    return true;
}

bool InferenceQueueController::removeJob(const quint64 id) {
    if (m_running) {
        return false;
    }

    const int index = indexOf(id);
    if (index < 0) {
        return false;
    }
    m_jobs.removeAt(index);
    emit jobsChanged();
    return true;
}

bool InferenceQueueController::moveJob(const quint64 id, const int offset) {
    if (m_running || offset == 0) {
        return false;
    }

    const int from = indexOf(id);
    const int to = from + offset;
    if (from < 0 || to < 0 || to >= m_jobs.size()) {
        return false;
    }
    m_jobs.move(from, to);
    emit jobsChanged();
    return true;
}

int InferenceQueueController::applyDefaultsToEditableJobs(const int languageId,
                                                          const QString &languageName,
                                                          const double tempo) {
    if (m_running) {
        return 0;
    }

    int updatedCount = 0;
    for (auto &job : m_jobs) {
        if (job.status != QueueJobStatus::Pending && job.status != QueueJobStatus::Separated &&
            job.status != QueueJobStatus::Failed) {
            continue;
        }
        job.languageId = languageId;
        job.languageName = languageName;
        job.tempo = tempo;
        ++updatedCount;
    }
    if (updatedCount > 0) {
        emit jobsChanged();
    }
    return updatedCount;
}

void InferenceQueueController::resetFailedJobs() {
    if (m_running) {
        return;
    }

    bool changed = false;
    for (auto &job : m_jobs) {
        if (job.status == QueueJobStatus::Failed) {
            job.status = job.failureStage == QueueFailureStage::Midi && !job.vocalsPath.isEmpty()
                             ? QueueJobStatus::Separated
                             : QueueJobStatus::Pending;
            if (job.status == QueueJobStatus::Pending) {
                job.vocalsPath.clear();
                job.instrumentalPath.clear();
            }
            job.failureStage = QueueFailureStage::None;
            job.progress = 0;
            job.error.clear();
            changed = true;
        }
    }
    if (changed) {
        emit jobsChanged();
    }
}

bool InferenceQueueController::start(Processor processor) {
    return startPipeline(false, {}, std::move(processor));
}

bool InferenceQueueController::startPipeline(const bool separationEnabled, SeparationProcessor separationProcessor,
                                             Processor midiProcessor) {
    if (m_running || m_future.isRunning() || !midiProcessor || (separationEnabled && !separationProcessor)) {
        return false;
    }

    QVector<QueueJob> pendingJobs;
    for (const auto &job : m_jobs) {
        if (job.status == QueueJobStatus::Pending || job.status == QueueJobStatus::Separated) {
            pendingJobs.push_back(job);
        }
    }
    if (pendingJobs.isEmpty()) {
        return false;
    }

    m_stopRequested.store(false);
    m_running = true;
    emit runningChanged(true);

    m_future = QtConcurrent::run([this, pendingJobs = std::move(pendingJobs), separationEnabled,
                                  separationProcessor = std::move(separationProcessor),
                                  midiProcessor = std::move(midiProcessor)]() mutable {
        bool stopped = false;

        if (separationEnabled) {
            for (auto &job : pendingJobs) {
                if (job.status == QueueJobStatus::Separated) {
                    continue;
                }
                QMetaObject::invokeMethod(
                    this,
                    [this, id = job.id] {
                        updateJobState(id, QueueJobStatus::Separating, 0, {});
                        emit currentJobChanged(id);
                    },
                    Qt::QueuedConnection);

                QString error;
                bool success = false;
                try {
                    success = separationProcessor(
                        job,
                        [this, id = job.id](const int progress) {
                            QMetaObject::invokeMethod(
                                this, [this, id, progress] { updateJobProgress(id, progress); }, Qt::QueuedConnection);
                        },
                        error);
                } catch (const std::exception &exception) {
                    error = QString::fromLocal8Bit(exception.what());
                } catch (...) {
                    error = tr("Unknown separation error");
                }
                if (!success && error.trimmed().isEmpty()) {
                    error = tr("The separation failed without an error message.");
                }
                job.status = success ? QueueJobStatus::Separated : QueueJobStatus::Failed;
                job.failureStage = success ? QueueFailureStage::None : QueueFailureStage::Separation;
                QMetaObject::invokeMethod(
                    this, [this, job, success, error] { updateJobSeparation(job.id, success, job, error); },
                    Qt::QueuedConnection);

                if (m_stopRequested.load()) {
                    stopped = true;
                    break;
                }
            }
        }

        if (!stopped) {
            for (const auto &job : pendingJobs) {
                if (separationEnabled && job.status != QueueJobStatus::Separated) {
                    continue;
                }
                if (!separationEnabled && job.status != QueueJobStatus::Pending &&
                    job.status != QueueJobStatus::Separated) {
                    continue;
                }
                QMetaObject::invokeMethod(
                    this,
                    [this, id = job.id] {
                        updateJobState(id, QueueJobStatus::Running, 0, {});
                        emit currentJobChanged(id);
                    },
                    Qt::QueuedConnection);

                QString error;
                bool success = false;
                try {
                    success = midiProcessor(
                        job,
                        [this, id = job.id](const int progress) {
                            QMetaObject::invokeMethod(
                                this, [this, id, progress] { updateJobProgress(id, progress); }, Qt::QueuedConnection);
                        },
                        error);
                } catch (const std::exception &exception) {
                    error = QString::fromLocal8Bit(exception.what());
                } catch (...) {
                    error = tr("Unknown conversion error");
                }
                if (!success && error.trimmed().isEmpty()) {
                    error = tr("The conversion failed without an error message.");
                }

                QMetaObject::invokeMethod(
                    this,
                    [this, id = job.id, success, error] {
                        updateJobState(id, success ? QueueJobStatus::Completed : QueueJobStatus::Failed,
                                       success ? 100 : 0, error,
                                       success ? QueueFailureStage::None : QueueFailureStage::Midi);
                    },
                    Qt::QueuedConnection);

                if (m_stopRequested.load()) {
                    stopped = true;
                    break;
                }
            }
        }

        QMetaObject::invokeMethod(
            this,
            [this, stopped] {
                m_running = false;
                emit currentJobChanged(0);
                emit runningChanged(false);
                emit queueFinished(stopped);
            },
            Qt::QueuedConnection);
    });

    return true;
}

void InferenceQueueController::stopAfterCurrent() {
    if (m_running) {
        m_stopRequested.store(true);
    }
}

int InferenceQueueController::indexOf(const quint64 id) const {
    for (int index = 0; index < m_jobs.size(); ++index) {
        if (m_jobs[index].id == id) {
            return index;
        }
    }
    return -1;
}

void InferenceQueueController::updateJobState(const quint64 id, const QueueJobStatus status, const int progress,
                                              const QString &error, const QueueFailureStage failureStage) {
    const int index = indexOf(id);
    if (index < 0) {
        return;
    }
    m_jobs[index].status = status;
    m_jobs[index].failureStage = failureStage;
    m_jobs[index].progress = std::clamp(progress, 0, 100);
    m_jobs[index].error = error;
    emit jobsChanged();
}

void InferenceQueueController::updateJobSeparation(const quint64 id, const bool success, const QueueJob &job,
                                                   const QString &error) {
    const int index = indexOf(id);
    if (index < 0) {
        return;
    }
    m_jobs[index].status = success ? QueueJobStatus::Separated : QueueJobStatus::Failed;
    m_jobs[index].failureStage = success ? QueueFailureStage::None : QueueFailureStage::Separation;
    m_jobs[index].progress = success ? 100 : 0;
    m_jobs[index].error = error;
    m_jobs[index].vocalsPath = job.vocalsPath;
    m_jobs[index].instrumentalPath = job.instrumentalPath;
    emit jobsChanged();
}

void InferenceQueueController::updateJobProgress(const quint64 id, const int progress) {
    const int index = indexOf(id);
    if (index < 0 || (m_jobs[index].status != QueueJobStatus::Separating &&
                      m_jobs[index].status != QueueJobStatus::Running)) {
        return;
    }
    m_jobs[index].progress = std::clamp(progress, 0, 100);
    emit jobsChanged();
}
