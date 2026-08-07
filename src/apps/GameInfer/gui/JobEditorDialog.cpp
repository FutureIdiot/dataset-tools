#include "JobEditorDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>

JobEditorDialog::JobEditorDialog(const QVector<QPair<int, QString>> &languages, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(tr("Task parameters"));

    auto *layout = new QFormLayout(this);

    auto *inputLayout = new QHBoxLayout();
    m_inputEdit = new QLineEdit(this);
    auto *inputButton = new QPushButton(tr("Browse..."), this);
    inputLayout->addWidget(m_inputEdit);
    inputLayout->addWidget(inputButton);
    layout->addRow(tr("Input audio:"), inputLayout);
    connect(inputButton, &QPushButton::clicked, this, &JobEditorDialog::browseInputFile);

    auto *outputLayout = new QHBoxLayout();
    m_outputEdit = new QLineEdit(this);
    auto *outputButton = new QPushButton(tr("Browse..."), this);
    outputLayout->addWidget(m_outputEdit);
    outputLayout->addWidget(outputButton);
    layout->addRow(tr("Output MIDI:"), outputLayout);
    connect(outputButton, &QPushButton::clicked, this, &JobEditorDialog::browseOutputFile);

    m_languageCombo = new QComboBox(this);
    layout->addRow(tr("Song language:"), m_languageCombo);

    m_tempoSpin = new QDoubleSpinBox(this);
    m_tempoSpin->setRange(1.0, 300.0);
    m_tempoSpin->setSingleStep(1.0);
    m_tempoSpin->setValue(120.0);
    m_tempoSpin->setSuffix(tr(" BPM"));
    layout->addRow(tr("Tempo (BPM):"), m_tempoSpin);

    m_errorEdit = new QPlainTextEdit(this);
    m_errorEdit->setReadOnly(true);
    m_errorEdit->setPlaceholderText(tr("No error details."));
    m_errorEdit->setMaximumHeight(100);
    layout->addRow(tr("Error details:"), m_errorEdit);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addRow(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &JobEditorDialog::validateAndAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    populateLanguages(languages);
    resize(560, 300);
}

void JobEditorDialog::setJob(const QueueJob &job) {
    m_job = job;
    m_inputEdit->setText(job.inputPath);
    m_outputEdit->setText(job.outputPath);
    m_tempoSpin->setValue(job.tempo);

    const int index = m_languageCombo->findData(job.languageId);
    if (index >= 0) {
        m_languageCombo->setCurrentIndex(index);
    } else if (!job.languageName.isEmpty()) {
        m_languageCombo->addItem(job.languageName, job.languageId);
        m_languageCombo->setCurrentIndex(m_languageCombo->count() - 1);
    } else {
        m_languageCombo->setCurrentIndex(0);
    }
    m_errorEdit->setPlainText(job.error);
}

QueueJob JobEditorDialog::job() const {
    QueueJob result = m_job;
    result.inputPath = m_inputEdit->text().trimmed();
    result.outputPath = m_outputEdit->text().trimmed();
    result.languageId = m_languageCombo->currentData().toInt();
    result.languageName = m_languageCombo->currentText();
    result.tempo = m_tempoSpin->value();
    return result;
}

void JobEditorDialog::browseInputFile() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select input audio"), m_inputEdit->text(),
        tr("Audio files (*.wav *.flac *.mp3);;All files (*)"));
    if (!path.isEmpty()) {
        m_inputEdit->setText(path);
    }
}

void JobEditorDialog::browseOutputFile() {
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Select output MIDI"), m_outputEdit->text(), tr("MIDI files (*.mid);;All files (*)"));
    if (!path.isEmpty()) {
        m_outputEdit->setText(path);
    }
}

void JobEditorDialog::validateAndAccept() {
    QString message;
    if (!validate(&message)) {
        QMessageBox::warning(this, tr("Invalid task"), message);
        return;
    }
    QDialog::accept();
}

void JobEditorDialog::populateLanguages(const QVector<QPair<int, QString>> &languages) {
    m_languageCombo->clear();
    for (const auto &[id, name] : languages) {
        m_languageCombo->addItem(name, id);
    }
    if (m_languageCombo->count() == 0) {
        m_languageCombo->addItem(tr("Default"), 0);
    }
}

bool JobEditorDialog::validate(QString *message) const {
    if (m_inputEdit->text().trimmed().isEmpty()) {
        if (message != nullptr) {
            *message = tr("Input audio is required.");
        }
        return false;
    }
    if (m_outputEdit->text().trimmed().isEmpty()) {
        if (message != nullptr) {
            *message = tr("Output MIDI path is required.");
        }
        return false;
    }
    return true;
}
