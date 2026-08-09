#ifndef SEPARATORWORKERCLIENT_H
#define SEPARATORWORKERCLIENT_H

#include <QJsonObject>
#include <QProcess>
#include <QString>

struct SeparatorWorkerConfiguration {
    QString scriptPath;
    QString modelFileDir;
    QString modelFilename;
    QString backend;
    QString outputMode;
    QJsonObject parameters;
};

struct SeparatorWorkerOutput {
    QString vocalsPath;
    QString instrumentalPath;
};

class SeparatorWorkerClient final {
public:
    SeparatorWorkerClient();
    ~SeparatorWorkerClient();

    SeparatorWorkerClient(const SeparatorWorkerClient &) = delete;
    SeparatorWorkerClient &operator=(const SeparatorWorkerClient &) = delete;

    bool start(const SeparatorWorkerConfiguration &configuration, const QString &initialOutputDirectory,
               QString &error);
    bool separate(const QString &inputPath, const QString &outputDirectory, const QString &outputBasename,
                  SeparatorWorkerOutput &output, QString &error);
    void stop();

    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] QString diagnostics() const;

private:
    bool request(QJsonObject message, QJsonObject &response, QString &error, int timeoutMs);
    void collectStderr();

    QProcess m_process;
    QByteArray m_stderr;
    quint64 m_nextRequestId = 1;
};

#endif // SEPARATORWORKERCLIENT_H
