#include "SeparatorSettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace
{
    QSpinBox *integerSpin(QWidget *parent, const int minimum, const int maximum) {
        auto *spin = new QSpinBox(parent);
        spin->setRange(minimum, maximum);
        return spin;
    }

    QDoubleSpinBox *decimalSpin(QWidget *parent, const double minimum, const double maximum,
                                const double step, const int decimals) {
        auto *spin = new QDoubleSpinBox(parent);
        spin->setRange(minimum, maximum);
        spin->setSingleStep(step);
        spin->setDecimals(decimals);
        return spin;
    }

    QWidget *tabWithForm(QTabWidget *tabs, const QString &title, QFormLayout *&form) {
        auto *tab = new QWidget(tabs);
        form = new QFormLayout(tab);
        tabs->addTab(tab, title);
        return tab;
    }
}

SeparatorSettingsDialog::SeparatorSettingsDialog(QSettings *settings, QWidget *parent)
    : QDialog(parent), m_settings(settings) {
    setWindowTitle(tr("Advanced separator parameters"));
    resize(620, 520);

    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    layout->addWidget(tabs);

    QFormLayout *commonForm = nullptr;
    auto *commonTab = tabWithForm(tabs, tr("Common"), commonForm);
    m_normalization = decimalSpin(commonTab, 0.0, 1.0, 0.01, 2);
    m_amplification = decimalSpin(commonTab, 0.0, 1.0, 0.01, 2);
    m_sampleRate = integerSpin(commonTab, 8000, 192000);
    m_chunkDuration = integerSpin(commonTab, 0, 86400);
    m_chunkDuration->setSpecialValueText(tr("Disabled"));
    m_useSoundFile = new QCheckBox(commonTab);
    m_useAutocast = new QCheckBox(commonTab);
    commonForm->addRow(tr("Normalization:"), m_normalization);
    commonForm->addRow(tr("Amplification:"), m_amplification);
    commonForm->addRow(tr("Output sample rate:"), m_sampleRate);
    commonForm->addRow(tr("Long-audio chunk duration (seconds):"), m_chunkDuration);
    commonForm->addRow(tr("Use SoundFile for output:"), m_useSoundFile);
    commonForm->addRow(tr("Use autocast (CUDA only):"), m_useAutocast);

    QFormLayout *mdxcForm = nullptr;
    auto *mdxcTab = tabWithForm(tabs, tr("MDXC / RoFormer"), mdxcForm);
    m_mdxcSegmentSize = integerSpin(mdxcTab, 32, 4096);
    m_mdxcOverlap = integerSpin(mdxcTab, 2, 50);
    m_mdxcBatchSize = integerSpin(mdxcTab, 1, 64);
    m_mdxcOverrideSegmentSize = new QCheckBox(mdxcTab);
    m_mdxcPitchShift = integerSpin(mdxcTab, -12, 12);
    mdxcForm->addRow(tr("Segment size:"), m_mdxcSegmentSize);
    mdxcForm->addRow(tr("Overlap:"), m_mdxcOverlap);
    mdxcForm->addRow(tr("Batch size:"), m_mdxcBatchSize);
    mdxcForm->addRow(tr("Override model segment size:"), m_mdxcOverrideSegmentSize);
    mdxcForm->addRow(tr("Pitch shift (semitones):"), m_mdxcPitchShift);

    QFormLayout *mdxForm = nullptr;
    auto *mdxTab = tabWithForm(tabs, tr("MDX"), mdxForm);
    m_mdxHopLength = new QComboBox(mdxTab);
    for (const int value : {256, 512, 1024, 2048}) {
        m_mdxHopLength->addItem(QString::number(value), value);
    }
    m_mdxSegmentSize = integerSpin(mdxTab, 32, 4096);
    m_mdxOverlap = decimalSpin(mdxTab, 0.001, 0.999, 0.025, 3);
    m_mdxBatchSize = integerSpin(mdxTab, 1, 64);
    m_mdxDenoise = new QCheckBox(mdxTab);
    mdxForm->addRow(tr("Hop length:"), m_mdxHopLength);
    mdxForm->addRow(tr("Segment size:"), m_mdxSegmentSize);
    mdxForm->addRow(tr("Overlap:"), m_mdxOverlap);
    mdxForm->addRow(tr("Batch size:"), m_mdxBatchSize);
    mdxForm->addRow(tr("Enable denoise:"), m_mdxDenoise);

    QFormLayout *vrForm = nullptr;
    auto *vrTab = tabWithForm(tabs, tr("VR"), vrForm);
    m_vrBatchSize = integerSpin(vrTab, 1, 128);
    m_vrWindowSize = new QComboBox(vrTab);
    for (const int value : {320, 512, 1024}) {
        m_vrWindowSize->addItem(QString::number(value), value);
    }
    m_vrAggression = integerSpin(vrTab, -100, 100);
    m_vrTta = new QCheckBox(vrTab);
    m_vrPostProcess = new QCheckBox(vrTab);
    m_vrPostProcessThreshold = decimalSpin(vrTab, 0.1, 0.3, 0.01, 2);
    m_vrHighEndProcess = new QCheckBox(vrTab);
    vrForm->addRow(tr("Batch size:"), m_vrBatchSize);
    vrForm->addRow(tr("Window size:"), m_vrWindowSize);
    vrForm->addRow(tr("Aggression:"), m_vrAggression);
    vrForm->addRow(tr("Enable TTA:"), m_vrTta);
    vrForm->addRow(tr("Enable post-process:"), m_vrPostProcess);
    vrForm->addRow(tr("Post-process threshold:"), m_vrPostProcessThreshold);
    vrForm->addRow(tr("High-end process:"), m_vrHighEndProcess);

    QFormLayout *demucsForm = nullptr;
    auto *demucsTab = tabWithForm(tabs, tr("Demucs"), demucsForm);
    m_demucsSegmentSize = integerSpin(demucsTab, 0, 100);
    m_demucsSegmentSize->setSpecialValueText(tr("Model default"));
    m_demucsShifts = integerSpin(demucsTab, 0, 20);
    m_demucsOverlap = decimalSpin(demucsTab, 0.001, 0.999, 0.025, 3);
    m_demucsSegmentsEnabled = new QCheckBox(demucsTab);
    demucsForm->addRow(tr("Segment size:"), m_demucsSegmentSize);
    demucsForm->addRow(tr("Shifts:"), m_demucsShifts);
    demucsForm->addRow(tr("Overlap:"), m_demucsOverlap);
    demucsForm->addRow(tr("Segment-wise processing:"), m_demucsSegmentsEnabled);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        saveSettings();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    loadSettings();
}

QJsonObject SeparatorSettingsDialog::parameters() const {
    const QJsonObject mdx{
        {QStringLiteral("hop_length"), m_mdxHopLength->currentData().toInt()},
        {QStringLiteral("segment_size"), m_mdxSegmentSize->value()},
        {QStringLiteral("overlap"), m_mdxOverlap->value()},
        {QStringLiteral("batch_size"), m_mdxBatchSize->value()},
        {QStringLiteral("enable_denoise"), m_mdxDenoise->isChecked()},
    };
    const QJsonObject mdxc{
        {QStringLiteral("segment_size"), m_mdxcSegmentSize->value()},
        {QStringLiteral("override_model_segment_size"), m_mdxcOverrideSegmentSize->isChecked()},
        {QStringLiteral("batch_size"), m_mdxcBatchSize->value()},
        {QStringLiteral("overlap"), m_mdxcOverlap->value()},
        {QStringLiteral("pitch_shift"), m_mdxcPitchShift->value()},
    };
    const QJsonObject vr{
        {QStringLiteral("batch_size"), m_vrBatchSize->value()},
        {QStringLiteral("window_size"), m_vrWindowSize->currentData().toInt()},
        {QStringLiteral("aggression"), m_vrAggression->value()},
        {QStringLiteral("enable_tta"), m_vrTta->isChecked()},
        {QStringLiteral("enable_post_process"), m_vrPostProcess->isChecked()},
        {QStringLiteral("post_process_threshold"), m_vrPostProcessThreshold->value()},
        {QStringLiteral("high_end_process"), m_vrHighEndProcess->isChecked()},
    };
    const QJsonObject demucs{
        {QStringLiteral("segment_size"), m_demucsSegmentSize->value() == 0
                                                     ? QJsonValue(QStringLiteral("Default"))
                                                     : QJsonValue(m_demucsSegmentSize->value())},
        {QStringLiteral("shifts"), m_demucsShifts->value()},
        {QStringLiteral("overlap"), m_demucsOverlap->value()},
        {QStringLiteral("segments_enabled"), m_demucsSegmentsEnabled->isChecked()},
    };
    return {
        {QStringLiteral("normalization"), m_normalization->value()},
        {QStringLiteral("amplification"), m_amplification->value()},
        {QStringLiteral("sample_rate"), m_sampleRate->value()},
        {QStringLiteral("chunk_duration"), m_chunkDuration->value()},
        {QStringLiteral("use_soundfile"), m_useSoundFile->isChecked()},
        {QStringLiteral("use_autocast"), m_useAutocast->isChecked()},
        {QStringLiteral("mdx"), mdx},
        {QStringLiteral("mdxc"), mdxc},
        {QStringLiteral("vr"), vr},
        {QStringLiteral("demucs"), demucs},
    };
}

void SeparatorSettingsDialog::loadSettings() {
    m_normalization->setValue(m_settings->value("Separator/normalization", 0.9).toDouble());
    m_amplification->setValue(m_settings->value("Separator/amplification", 0.0).toDouble());
    m_sampleRate->setValue(m_settings->value("Separator/sampleRate", 44100).toInt());
    m_chunkDuration->setValue(m_settings->value("Separator/chunkDuration", 600).toInt());
    m_useSoundFile->setChecked(m_settings->value("Separator/useSoundFile", true).toBool());
    m_useAutocast->setChecked(m_settings->value("Separator/useAutocast", false).toBool());

    m_mdxcSegmentSize->setValue(m_settings->value("Separator/mdxcSegmentSize", 256).toInt());
    m_mdxcOverlap->setValue(m_settings->value("Separator/mdxcOverlap", 8).toInt());
    m_mdxcBatchSize->setValue(m_settings->value("Separator/mdxcBatchSize", 1).toInt());
    m_mdxcOverrideSegmentSize->setChecked(m_settings->value("Separator/mdxcOverrideSegmentSize", false).toBool());
    m_mdxcPitchShift->setValue(m_settings->value("Separator/mdxcPitchShift", 0).toInt());

    m_mdxHopLength->setCurrentIndex(
        qMax(0, m_mdxHopLength->findData(m_settings->value("Separator/mdxHopLength", 1024).toInt())));
    m_mdxSegmentSize->setValue(m_settings->value("Separator/mdxSegmentSize", 256).toInt());
    m_mdxOverlap->setValue(m_settings->value("Separator/mdxOverlap", 0.25).toDouble());
    m_mdxBatchSize->setValue(m_settings->value("Separator/mdxBatchSize", 1).toInt());
    m_mdxDenoise->setChecked(m_settings->value("Separator/mdxDenoise", false).toBool());

    m_vrBatchSize->setValue(m_settings->value("Separator/vrBatchSize", 1).toInt());
    m_vrWindowSize->setCurrentIndex(
        qMax(0, m_vrWindowSize->findData(m_settings->value("Separator/vrWindowSize", 512).toInt())));
    m_vrAggression->setValue(m_settings->value("Separator/vrAggression", 5).toInt());
    m_vrTta->setChecked(m_settings->value("Separator/vrTta", false).toBool());
    m_vrPostProcess->setChecked(m_settings->value("Separator/vrPostProcess", false).toBool());
    m_vrPostProcessThreshold->setValue(m_settings->value("Separator/vrPostProcessThreshold", 0.2).toDouble());
    m_vrHighEndProcess->setChecked(m_settings->value("Separator/vrHighEndProcess", false).toBool());

    m_demucsSegmentSize->setValue(m_settings->value("Separator/demucsSegmentSize", 0).toInt());
    m_demucsShifts->setValue(m_settings->value("Separator/demucsShifts", 2).toInt());
    m_demucsOverlap->setValue(m_settings->value("Separator/demucsOverlap", 0.25).toDouble());
    m_demucsSegmentsEnabled->setChecked(m_settings->value("Separator/demucsSegmentsEnabled", true).toBool());
}

void SeparatorSettingsDialog::saveSettings() const {
    m_settings->setValue("Separator/normalization", m_normalization->value());
    m_settings->setValue("Separator/amplification", m_amplification->value());
    m_settings->setValue("Separator/sampleRate", m_sampleRate->value());
    m_settings->setValue("Separator/chunkDuration", m_chunkDuration->value());
    m_settings->setValue("Separator/useSoundFile", m_useSoundFile->isChecked());
    m_settings->setValue("Separator/useAutocast", m_useAutocast->isChecked());
    m_settings->setValue("Separator/mdxcSegmentSize", m_mdxcSegmentSize->value());
    m_settings->setValue("Separator/mdxcOverlap", m_mdxcOverlap->value());
    m_settings->setValue("Separator/mdxcBatchSize", m_mdxcBatchSize->value());
    m_settings->setValue("Separator/mdxcOverrideSegmentSize", m_mdxcOverrideSegmentSize->isChecked());
    m_settings->setValue("Separator/mdxcPitchShift", m_mdxcPitchShift->value());
    m_settings->setValue("Separator/mdxHopLength", m_mdxHopLength->currentData().toInt());
    m_settings->setValue("Separator/mdxSegmentSize", m_mdxSegmentSize->value());
    m_settings->setValue("Separator/mdxOverlap", m_mdxOverlap->value());
    m_settings->setValue("Separator/mdxBatchSize", m_mdxBatchSize->value());
    m_settings->setValue("Separator/mdxDenoise", m_mdxDenoise->isChecked());
    m_settings->setValue("Separator/vrBatchSize", m_vrBatchSize->value());
    m_settings->setValue("Separator/vrWindowSize", m_vrWindowSize->currentData().toInt());
    m_settings->setValue("Separator/vrAggression", m_vrAggression->value());
    m_settings->setValue("Separator/vrTta", m_vrTta->isChecked());
    m_settings->setValue("Separator/vrPostProcess", m_vrPostProcess->isChecked());
    m_settings->setValue("Separator/vrPostProcessThreshold", m_vrPostProcessThreshold->value());
    m_settings->setValue("Separator/vrHighEndProcess", m_vrHighEndProcess->isChecked());
    m_settings->setValue("Separator/demucsSegmentSize", m_demucsSegmentSize->value());
    m_settings->setValue("Separator/demucsShifts", m_demucsShifts->value());
    m_settings->setValue("Separator/demucsOverlap", m_demucsOverlap->value());
    m_settings->setValue("Separator/demucsSegmentsEnabled", m_demucsSegmentsEnabled->isChecked());
}
