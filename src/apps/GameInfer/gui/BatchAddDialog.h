#ifndef GAMEINFER_BATCHADDDIALOG_H
#define GAMEINFER_BATCHADDDIALOG_H

#include <QDialog>
#include <QPair>
#include <QVector>

#include "InferenceQueueController.h"

class QTableWidget;

class BatchAddDialog final : public QDialog {
    Q_OBJECT

public:
    explicit BatchAddDialog(const QVector<QueueJob> &jobs,
                            const QVector<QPair<int, QString>> &languages,
                            QWidget *parent = nullptr);

    [[nodiscard]] QVector<QueueJob> jobs() const;

private slots:
    void editRow(int row);
    void validateAndAccept();

private:
    void refreshTable();
    [[nodiscard]] bool validate(QString *message = nullptr) const;

    QTableWidget *m_table = nullptr;
    QVector<QueueJob> m_jobs;
    QVector<QPair<int, QString>> m_languages;
};

#endif // GAMEINFER_BATCHADDDIALOG_H
