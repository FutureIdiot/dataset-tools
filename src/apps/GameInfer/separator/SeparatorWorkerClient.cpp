#include "SeparatorWorkerClient.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QStandardPaths>

#include <algorithm>

namespace
{
    constexpr int StartTimeoutMs = 30000;
    constexpr int BootstrapTimeoutMs = 60 * 60 * 1000;
    constexpr int LoadTimeoutMs = 30 * 60 * 1000;
    constexpr int SeparateTimeoutMs = 24 * 60 * 60 * 1000;
    constexpr int ShutdownTimeoutMs = 5000;

    bool isExecutableFile(const QString &path) {
        const QFileInfo info(path);
        return info.isFile() && info.isExecutable();
    }

    QString firstExecutable(const QStringList &candidates) {
        for (const QString &candidate : candidates) {
            if (!candidate.isEmpty() && isExecutableFile(candidate)) {
                return QDir::cleanPath(candidate);
            }
        }
        return {};
    }
}

SeparatorWorkerClient::SeparatorWorkerClient() { m_process.setProcessChannelMode(QProcess::SeparateChannels); }

SeparatorWorkerClient::~SeparatorWorkerClient() { stop(); }

bool SeparatorWorkerClient::start(const SeparatorWorkerConfiguration &configuration,
                                  const QString &initialOutputDirectory, QString &error) {
    if (isRunning()) {
        error = QStringLiteral("separator worker is already running");
        return false;
    }
    if (!QFileInfo::exists(configuration.scriptPath)) {
        error = QStringLiteral("separator worker script not found: %1").arg(configuration.scriptPath);
        return false;
    }

    const QString workerDirectory = QFileInfo(configuration.scriptPath).absolutePath();
    const QString projectFile = QDir(workerDirectory).filePath(QStringLiteral("pyproject.toml"));
    const QString lockFile = QDir(workerDirectory).filePath(QStringLiteral("uv.lock"));
    if (!QFileInfo(projectFile).isFile() || !QFileInfo(lockFile).isFile()) {
        error = QStringLiteral("separator dependency files are missing from: %1").arg(workerDirectory);
        return false;
    }

    const QString applicationDirectory = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    const QString executableSuffix = QStringLiteral(".exe");
#else
    const QString executableSuffix;
#endif
    const QString bundledRuntimeDirectory =
        QDir(applicationDirectory).filePath(QStringLiteral("separator-runtime"));
    const QString bundledPython = firstExecutable({
        QDir(bundledRuntimeDirectory).filePath(QStringLiteral("python") + executableSuffix),
        QDir(bundledRuntimeDirectory).filePath(QStringLiteral("bin/python3") + executableSuffix),
        QDir(bundledRuntimeDirectory).filePath(QStringLiteral("Scripts/python") + executableSuffix),
    });

    QString program;
    QStringList command;
    bool managedByUv = false;
    if (!bundledPython.isEmpty()) {
        program = bundledPython;
        command.push_back(configuration.scriptPath);
    } else {
        program = firstExecutable({
            qEnvironmentVariable("GAMEINFER_SEPARATOR_UV"),
            QDir(bundledRuntimeDirectory).filePath(QStringLiteral("uv") + executableSuffix),
            QStandardPaths::findExecutable(QStringLiteral("uv")),
        });
        if (program.isEmpty()) {
            error = QStringLiteral(
                "managed separator runtime not found; reinstall GameInfer with its separator runtime");
            return false;
        }
        managedByUv = true;
        command = {
            QStringLiteral("run"),
            QStringLiteral("--locked"),
            QStringLiteral("--managed-python"),
            QStringLiteral("--project"),
            workerDirectory,
            QStringLiteral("--"),
            QStringLiteral("python"),
            configuration.scriptPath,
        };
    }

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    if (managedByUv) {
        const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        const QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (dataRoot.isEmpty() || !QDir().mkpath(dataRoot)) {
            error = QStringLiteral("failed to create the separator runtime data directory: %1").arg(dataRoot);
            return false;
        }
        const QString runtimeRoot = QDir(dataRoot).filePath(QStringLiteral("separator-runtime"));
        if (!QDir().mkpath(runtimeRoot)) {
            error = QStringLiteral("failed to create the separator runtime directory: %1").arg(runtimeRoot);
            return false;
        }
        environment.insert(QStringLiteral("UV_PROJECT_ENVIRONMENT"),
                           QDir(runtimeRoot).filePath(QStringLiteral("environment")));
        environment.insert(QStringLiteral("UV_PYTHON_INSTALL_DIR"),
                           QDir(runtimeRoot).filePath(QStringLiteral("python")));
        if (!cacheRoot.isEmpty()) {
            const QString uvCache = QDir(cacheRoot).filePath(QStringLiteral("separator-uv"));
            QDir().mkpath(uvCache);
            environment.insert(QStringLiteral("UV_CACHE_DIR"), uvCache);
        }
        environment.insert(QStringLiteral("UV_LINK_MODE"), QStringLiteral("copy"));
        environment.insert(QStringLiteral("UV_NO_PROGRESS"), QStringLiteral("1"));
    }
    if (configuration.backend == QStringLiteral("cpu") || configuration.backend == QStringLiteral("directml")) {
        environment.insert(QStringLiteral("CUDA_VISIBLE_DEVICES"), QStringLiteral("-1"));
    }
    m_process.setProcessEnvironment(environment);
    m_process.setWorkingDirectory(workerDirectory);
    m_stderr.clear();
    m_nextRequestId = 1;
    m_process.start(program, command, QIODevice::ReadWrite);
    if (!m_process.waitForStarted(StartTimeoutMs)) {
        error = QStringLiteral("failed to start separator worker: %1").arg(m_process.errorString());
        return false;
    }

    QJsonObject response;
    const int pingTimeout = managedByUv ? BootstrapTimeoutMs : StartTimeoutMs;
    if (!request({{QStringLiteral("command"), QStringLiteral("ping")}}, response, error, pingTimeout)) {
        stop();
        return false;
    }

    QJsonObject parameters = configuration.parameters;
    parameters.insert(QStringLiteral("use_directml"), configuration.backend == QStringLiteral("directml"));
    QJsonObject loadRequest{
        {QStringLiteral("command"), QStringLiteral("load_model")},
        {QStringLiteral("model_file_dir"), configuration.modelFileDir},
        {QStringLiteral("model_filename"), configuration.modelFilename},
        {QStringLiteral("output_mode"), configuration.outputMode},
        {QStringLiteral("backend"), configuration.backend},
        {QStringLiteral("output_dir"), initialOutputDirectory},
        {QStringLiteral("parameters"), parameters},
    };
    if (!request(loadRequest, response, error, LoadTimeoutMs)) {
        stop();
        return false;
    }
    return true;
}

bool SeparatorWorkerClient::separate(const QString &inputPath, const QString &outputDirectory,
                                     const QString &outputBasename, SeparatorWorkerOutput &output, QString &error) {
    QJsonObject response;
    const QJsonObject message{
        {QStringLiteral("command"), QStringLiteral("separate")},
        {QStringLiteral("input_path"), inputPath},
        {QStringLiteral("output_dir"), outputDirectory},
        {QStringLiteral("output_basename"), outputBasename},
    };
    if (!request(message, response, error, SeparateTimeoutMs)) {
        return false;
    }

    output.vocalsPath = response.value(QStringLiteral("vocals_path")).toString();
    output.instrumentalPath = response.value(QStringLiteral("instrumental_path")).toString();
    if (output.vocalsPath.isEmpty() || !QFileInfo::exists(output.vocalsPath)) {
        error = QStringLiteral("separator worker did not return an existing vocals file");
        return false;
    }
    return true;
}

void SeparatorWorkerClient::stop() {
    if (!isRunning()) {
        return;
    }
    QJsonObject response;
    QString ignored;
    request({{QStringLiteral("command"), QStringLiteral("shutdown")}}, response, ignored, ShutdownTimeoutMs);
    if (m_process.state() != QProcess::NotRunning && !m_process.waitForFinished(ShutdownTimeoutMs)) {
        m_process.terminate();
        if (!m_process.waitForFinished(ShutdownTimeoutMs)) {
            m_process.kill();
            m_process.waitForFinished(ShutdownTimeoutMs);
        }
    }
    collectStderr();
}

bool SeparatorWorkerClient::isRunning() const { return m_process.state() != QProcess::NotRunning; }

QString SeparatorWorkerClient::diagnostics() const { return QString::fromUtf8(m_stderr).trimmed(); }

bool SeparatorWorkerClient::request(QJsonObject message, QJsonObject &response, QString &error, const int timeoutMs) {
    if (!isRunning()) {
        error = QStringLiteral("separator worker is not running");
        return false;
    }

    const QString requestId = QString::number(m_nextRequestId++);
    message.insert(QStringLiteral("request_id"), requestId);
    QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
    payload.push_back('\n');
    if (m_process.write(payload) != payload.size() || !m_process.waitForBytesWritten(StartTimeoutMs)) {
        error = QStringLiteral("failed to send a command to separator worker: %1").arg(m_process.errorString());
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        collectStderr();
        while (m_process.canReadLine()) {
            const QByteArray line = m_process.readLine().trimmed();
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                continue;
            }
            const QJsonObject object = document.object();
            if (object.value(QStringLiteral("request_id")).toString() != requestId) {
                continue;
            }
            if (object.value(QStringLiteral("event")).toString() == QStringLiteral("error")) {
                error = object.value(QStringLiteral("error")).toString(QStringLiteral("separator worker failed"));
                const QString detail = diagnostics();
                if (!detail.isEmpty()) {
                    error += QStringLiteral("\n") + detail.right(4000);
                }
                return false;
            }
            response = object;
            return true;
        }
        if (m_process.state() == QProcess::NotRunning) {
            collectStderr();
            error = QStringLiteral("separator worker exited unexpectedly");
            const QString detail = diagnostics();
            if (!detail.isEmpty()) {
                error += QStringLiteral("\n") + detail.right(4000);
            }
            return false;
        }
        const int remaining = std::max(1, timeoutMs - static_cast<int>(timer.elapsed()));
        m_process.waitForReadyRead(std::min(250, remaining));
    }

    collectStderr();
    error = QStringLiteral("separator worker command timed out");
    return false;
}

void SeparatorWorkerClient::collectStderr() {
    const QByteArray chunk = m_process.readAllStandardError();
    if (!chunk.isEmpty()) {
        m_stderr += chunk;
        constexpr qsizetype MaxDiagnosticBytes = 64 * 1024;
        if (m_stderr.size() > MaxDiagnosticBytes) {
            m_stderr = m_stderr.right(MaxDiagnosticBytes);
        }
    }
}
