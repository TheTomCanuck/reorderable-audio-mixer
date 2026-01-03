#pragma once

#include <obs.hpp>

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>

#include <vector>

class VolumeMeter;

class MixerItem : public QFrame {
	Q_OBJECT

public:
	explicit MixerItem(OBSSource source, bool vertical = false, QWidget *parent = nullptr);
	~MixerItem();

	obs_source_t *GetSource() const { return source; }
	QString GetSourceUUID() const;
	QString GetSourceName() const;

	void SetVertical(bool vertical);
	void RefreshName();
	void Cleanup(bool isShutdown = false);

	void SetSelected(bool selected);
	bool IsSelected() const { return selected; }

signals:
	void HideRequested(MixerItem *item);
	void Selected(MixerItem *item);

protected:
	void mousePressEvent(QMouseEvent *event) override;

private slots:
	void OnMuteToggled(bool checked);
	void OnSliderChanged(int value);
	void OnConfigClicked();
	void OnHideClicked();
	void OnFiltersClicked();
	void OnPropertiesClicked();
	void OnAdvancedAudioClicked();

	void VolumeChanged();
	void VolumeMuted(bool muted);

private:
	void SetupUI();
	void SetupSignals();
	void DisconnectSignals();
	void UpdateVolumeLabel();
	void UpdateSelectionStyle();

	static void OBSVolumeChanged(void *data, float db);
	static void OBSVolumeMuted(void *data, calldata_t *calldata);
	static void OBSVolumeLevel(void *data,
		const float magnitude[MAX_AUDIO_CHANNELS],
		const float peak[MAX_AUDIO_CHANNELS],
		const float inputPeak[MAX_AUDIO_CHANNELS]);

private:
	OBSSource source;
	std::vector<OBSSignal> signalConnections;

	// UI elements
	QLabel *nameLabel = nullptr;
	QLabel *volLabel = nullptr;
	VolumeMeter *volMeter = nullptr;
	QSlider *slider = nullptr;
	QCheckBox *muteCheckbox = nullptr;
	QPushButton *configButton = nullptr;

	// OBS handles
	OBSFader obs_fader;
	OBSVolMeter obs_volmeter;

	bool vertical = false;
	bool selected = false;

	static constexpr float FADER_PRECISION = 4096.0f;
};
