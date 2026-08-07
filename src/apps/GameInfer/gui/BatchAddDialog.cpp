#include "BatchAddDialog.h"

#include "JobEditorDialog.h"

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

BatchAddDialog::BatchAddDialog(const QVector<QueueJob> &jobs,
                               const QVector<QPair<int, QString>> &languages,
                               QWidget *parent)
    : QDialog(parent), m_jobs(jobs), m_languages(languages) {
    setWindowTitle(tr("Confirm batch tasks"));

    auto *layout = new QVBoxLayout(this);
    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels(
        {tr("#"), tr("Input audio"), tr("Output MIDI"), tr("Song language"), tr("Tempo (BPM)"),
         tr("Parameters...")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    layout->addWidget(m_table);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &BatchAddDialog::validateAndAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    refreshTable();
    resize(900, 420);
}

QVector<QueueJob> BatchAddDialog::jobs() const { return m_jobs; }

void BatchAddDialog::editRow(const int row) {
    if (row < 0 || row >= m_jobs.size()) {
        return;
    }

    JobEditorDialog dialog(m_languages, this);
    dialog.setJob(m_jobs.at(row));
    if (dialog.exec() == QDialog::Accepted) {
        m_jobs[row] = dialog.job();
        refreshTable();
        m_table->selectRow(row);
    }
}

void BatchAddDialog::validateAndAccept() {
    QString message;
    if (!validate(&message)) {
        QMessageBox::warning(this, tr("Invalid batch"), message);
        return;
    }
    QDialog::accept();
}

void BatchAddDialog::refreshTable() {
    m_table->clearContents();
    m_table->setRowCount(m_jobs.size());

    for (int row = 0; row < m_jobs.size(); ++row) {
        const QueueJob &job = m_jobs.at(row);
        m_table->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        m_table->setItem(row, 1, new QTableWidgetItem(job.inputPath));
        m_table->setItem(row, 2, new QTableWidgetItem(job.outputPath));
        m_table->setItem(row, 3, new QTableWidgetItem(job.languageName));
        m_table->setItem(row, 4, new QTableWidgetItem(QString::number(job.tempo, 'f', 0)));

        auto *parametersButton = new QPushButton(tr("Parameters..."), m_table);
        m_table->setCellWidget(row, 5, parametersButton);
        connect(parametersButton, &QPushButton::clicked, this, [this, row] { editRow(row); });
    }
}

bool BatchAddDialog::validate(QString *message) const {
    for (int row = 0; row < m_jobs.size(); ++row) {
        const QueueJob &job = m_jobs.at(row);
        if (job.inputPath.trimmed().isEmpty()) {
            if (message != nullptr) {
                *message = tr("Task %1 is missing an input audio file.").arg(row + 1);
            }
            return false;
        }
        if (job.outputPath.trimmed().isEmpty()) {
            if (message != nullptr) {
                *message = tr("Task %1 is missing an output MIDI path.").arg(row + 1);
            }
            return false;
        }
    }
    return true;
}
