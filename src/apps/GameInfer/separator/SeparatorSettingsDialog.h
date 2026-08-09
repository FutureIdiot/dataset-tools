#ifndef SEPARATORSETTINGSDIALOG_H
#define SEPARATORSETTINGSDIALOG_H

#include <QDialog>
#include <QJsonObject>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSettings;
class QSpinBox;

class SeparatorSettingsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit SeparatorSettingsDialog(QSettings *settings, QWidget *parent = nullptr);

    [[nodiscard]] QJsonObject parameters() const;

private:
    void loadSettings();
    void saveSettings() const;

    QSettings *m_settings;

    QDoubleSpinBox *m_normalization = nullptr;
    QDoubleSpinBox *m_amplification = nullptr;
    QSpinBox *m_sampleRate = nullptr;
    QSpinBox *m_chunkDuration = nullptr;
    QCheckBox *m_useSoundFile = nullptr;
    QCheckBox *m_useAutocast = nullptr;

    QSpinBox *m_mdxcSegmentSize = nullptr;
    QSpinBox *m_mdxcOverlap = nullptr;
    QSpinBox *m_mdxcBatchSize = nullptr;
    QCheckBox *m_mdxcOverrideSegmentSize = nullptr;
    QSpinBox *m_mdxcPitchShift = nullptr;

    QComboBox *m_mdxHopLength = nullptr;
    QSpinBox *m_mdxSegmentSize = nullptr;
    QDoubleSpinBox *m_mdxOverlap = nullptr;
    QSpinBox *m_mdxBatchSize = nullptr;
    QCheckBox *m_mdxDenoise = nullptr;

    QSpinBox *m_vrBatchSize = nullptr;
    QComboBox *m_vrWindowSize = nullptr;
    QSpinBox *m_vrAggression = nullptr;
    QCheckBox *m_vrTta = nullptr;
    QCheckBox *m_vrPostProcess = nullptr;
    QDoubleSpinBox *m_vrPostProcessThreshold = nullptr;
    QCheckBox *m_vrHighEndProcess = nullptr;

    QSpinBox *m_demucsSegmentSize = nullptr;
    QSpinBox *m_demucsShifts = nullptr;
    QDoubleSpinBox *m_demucsOverlap = nullptr;
    QCheckBox *m_demucsSegmentsEnabled = nullptr;
};

#endif // SEPARATORSETTINGSDIALOG_H
