#include "InferenceQueueController.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <iostream>
#include <vector>

namespace
{
    QueueJob makeJob(const QString &inputPath, const QString &outputPath) {
        QueueJob job;
        job.inputPath = inputPath;
        job.outputPath = outputPath;
        return job;
    }

    bool waitForQueue(InferenceQueueController &controller, const int timeoutMs = 5000) {
        QEventLoop loop;
        bool finished = false;
        QObject::connect(&controller, &InferenceQueueController::queueFinished, &loop, [&](const bool) {
            finished = true;
            loop.quit();
        });
        QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
        loop.exec();
        return finished;
    }

    bool testTwoPhaseOrdering() {
        InferenceQueueController controller;
        controller.addJob(makeJob("a.wav", "a.mid"));
        controller.addJob(makeJob("b.wav", "b.mid"));
        std::vector<QString> events;

        const bool started = controller.startPipeline(
            true,
            [&](QueueJob &job, const InferenceQueueController::ProgressCallback &, QString &) {
                events.push_back("separate:" + job.inputPath);
                job.vocalsPath = job.inputPath + ".vocals.wav";
                return true;
            },
            [&](const QueueJob &job, const InferenceQueueController::ProgressCallback &, QString &) {
                events.push_back("midi:" + job.inputPath);
                return !job.vocalsPath.isEmpty();
            });
        if (!started || !waitForQueue(controller)) {
            return false;
        }
        const std::vector<QString> expected{
            "separate:a.wav", "separate:b.wav", "midi:a.wav", "midi:b.wav",
        };
        if (events != expected) {
            return false;
        }
        for (const auto &job : controller.jobs()) {
            if (job.status != QueueJobStatus::Completed || job.vocalsPath.isEmpty()) {
                return false;
            }
        }
        return true;
    }

    bool testSeparationFailureIsolation() {
        InferenceQueueController controller;
        controller.addJob(makeJob("bad.wav", "bad.mid"));
        controller.addJob(makeJob("good.wav", "good.mid"));
        int midiRuns = 0;
        controller.startPipeline(
            true,
            [](QueueJob &job, const InferenceQueueController::ProgressCallback &, QString &error) {
                if (job.inputPath == "bad.wav") {
                    error = "separation failed";
                    return false;
                }
                job.vocalsPath = "good_vocals.wav";
                return true;
            },
            [&](const QueueJob &, const InferenceQueueController::ProgressCallback &, QString &) {
                ++midiRuns;
                return true;
            });
        if (!waitForQueue(controller) || midiRuns != 1) {
            return false;
        }
        const auto &jobs = controller.jobs();
        return jobs[0].status == QueueJobStatus::Failed &&
               jobs[0].failureStage == QueueFailureStage::Separation &&
               jobs[1].status == QueueJobStatus::Completed;
    }

    bool testMidiRetryKeepsSeparatedVocals() {
        InferenceQueueController controller;
        controller.addJob(makeJob("source.wav", "source.mid"));
        controller.startPipeline(
            true,
            [](QueueJob &job, const InferenceQueueController::ProgressCallback &, QString &) {
                job.vocalsPath = "source_vocals.wav";
                return true;
            },
            [](const QueueJob &, const InferenceQueueController::ProgressCallback &, QString &error) {
                error = "midi failed";
                return false;
            });
        if (!waitForQueue(controller)) {
            return false;
        }
        controller.resetFailedJobs();
        const auto &job = controller.jobs().front();
        return job.status == QueueJobStatus::Separated && job.vocalsPath == "source_vocals.wav";
    }
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (!testTwoPhaseOrdering()) {
        std::cerr << "two-phase ordering test failed" << std::endl;
        return 1;
    }
    if (!testSeparationFailureIsolation()) {
        std::cerr << "separation failure isolation test failed" << std::endl;
        return 1;
    }
    if (!testMidiRetryKeepsSeparatedVocals()) {
        std::cerr << "MIDI retry artifact test failed" << std::endl;
        return 1;
    }
    std::cout << "GameInfer queue tests passed" << std::endl;
    return 0;
}
