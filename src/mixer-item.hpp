#pragma once

#include <obs.hpp>

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

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
	void UpdateButtons(bool canMoveUp, bool canMoveDown);
	void RefreshName();
	void Cleanup(bool isShutdown = false);

signals:
	void MoveUpRequested(MixerItem *item);
	void MoveDownRequested(MixerItem *item);

private slots:
	void OnMoveUp();
	void OnMoveDown();
	void OnMuteToggled(bool checked);
	void OnSliderChanged(int value);

	void VolumeChanged();
	void VolumeMuted(bool muted);

private:
	void SetupUI();
	void SetupSignals();
	void DisconnectSignals();
	void UpdateVolumeLabel();

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
	QPushButton *upButton = nullptr;
	QPushButton *downButton = nullptr;

	// OBS handles
	OBSFader obs_fader;
	OBSVolMeter obs_volmeter;

	bool vertical = false;

	static constexpr float FADER_PRECISION = 4096.0f;
};
