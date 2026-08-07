#ifndef GAMEINFER_JOBEDITORDIALOG_H
#define GAMEINFER_JOBEDITORDIALOG_H

#include <QDialog>
#include <QPair>
#include <QVector>

#include "InferenceQueueController.h"

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPlainTextEdit;

class JobEditorDialog final : public QDialog {
    Q_OBJECT

public:
    explicit JobEditorDialog(const QVector<QPair<int, QString>> &languages, QWidget *parent = nullptr);

    void setJob(const QueueJob &job);
    [[nodiscard]] QueueJob job() const;

private slots:
    void browseInputFile();
    void browseOutputFile();
    void validateAndAccept();

private:
    void populateLanguages(const QVector<QPair<int, QString>> &languages);
    [[nodiscard]] bool validate(QString *message = nullptr) const;

    QLineEdit *m_inputEdit = nullptr;
    QLineEdit *m_outputEdit = nullptr;
    QComboBox *m_languageCombo = nullptr;
    QDoubleSpinBox *m_tempoSpin = nullptr;
    QPlainTextEdit *m_errorEdit = nullptr;
    QueueJob m_job;
};

#endif // GAMEINFER_JOBEDITORDIALOG_H
