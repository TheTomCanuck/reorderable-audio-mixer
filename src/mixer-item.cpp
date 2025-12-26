#include "mixer-item.hpp"
#include "volume-meter.hpp"

#include <obs-module.h>

#include <cmath>

MixerItem::MixerItem(OBSSource source_, bool vertical_, QWidget *parent)
	: QFrame(parent),
	  source(source_),
	  vertical(vertical_)
{
	// Create OBS fader and volmeter
	obs_fader = obs_fader_create(OBS_FADER_LOG);
	obs_volmeter = obs_volmeter_create(OBS_FADER_LOG);

	obs_fader_attach_source(obs_fader, source);
	obs_volmeter_attach_source(obs_volmeter, source);

	SetupUI();
	SetupSignals();

	// Initialize volume display
	UpdateVolumeLabel();
}

MixerItem::~MixerItem()
{
	Cleanup();
}

void MixerItem::Cleanup(bool isShutdown)
{
	if (!obs_fader)
		return; // Already cleaned up

	DisconnectSignals();

	// During shutdown, don't touch fader/volmeter - sources are already
	// destroyed and calling detach/destroy will crash. Just null out
	// our pointers and let the process cleanup handle it.
	if (!isShutdown) {
		obs_fader_detach_source(obs_fader);
		obs_volmeter_detach_source(obs_volmeter);
		obs_fader_destroy(obs_fader);
		obs_volmeter_destroy(obs_volmeter);
	}

	obs_fader = nullptr;
	obs_volmeter = nullptr;
	source = nullptr;
}

QString MixerItem::GetSourceUUID() const
{
	const char *uuid = obs_source_get_uuid(source);
	return uuid ? QString::fromUtf8(uuid) : QString();
}

QString MixerItem::GetSourceName() const
{
	const char *name = obs_source_get_name(source);
	return name ? QString::fromUtf8(name) : QString();
}

void MixerItem::SetupUI()
{
	setFrameShape(QFrame::StyledPanel);
	setObjectName(GetSourceName());

	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(6, 6, 6, 6);
	mainLayout->setSpacing(4);

	// Top row: Name and reorder buttons
	QHBoxLayout *topRow = new QHBoxLayout();
	topRow->setSpacing(4);

	nameLabel = new QLabel(GetSourceName());
	nameLabel->setStyleSheet("font-weight: bold;");
	topRow->addWidget(nameLabel, 1);

	upButton = new QPushButton();
	upButton->setFixedSize(24, 24);
	upButton->setToolTip(obs_module_text("BetterAudioMixer.MoveUp"));
	upButton->setText(QString::fromUtf8("\u25B2")); // Up arrow
	connect(upButton, &QPushButton::clicked, this, &MixerItem::OnMoveUp);
	topRow->addWidget(upButton);

	downButton = new QPushButton();
	downButton->setFixedSize(24, 24);
	downButton->setToolTip(obs_module_text("BetterAudioMixer.MoveDown"));
	downButton->setText(QString::fromUtf8("\u25BC")); // Down arrow
	connect(downButton, &QPushButton::clicked, this, &MixerItem::OnMoveDown);
	topRow->addWidget(downButton);

	mainLayout->addLayout(topRow);

	// Middle row: Volume meter
	volMeter = new VolumeMeter(this);
	volMeter->setMinimumHeight(20);
	mainLayout->addWidget(volMeter);

	// Bottom row: Slider, volume label, mute
	QHBoxLayout *bottomRow = new QHBoxLayout();
	bottomRow->setSpacing(4);

	slider = new QSlider(Qt::Horizontal);
	slider->setMinimum(0);
	slider->setMaximum(static_cast<int>(FADER_PRECISION));
	slider->setValue(static_cast<int>(obs_fader_get_deflection(obs_fader) * FADER_PRECISION));
	connect(slider, &QSlider::valueChanged, this, &MixerItem::OnSliderChanged);
	bottomRow->addWidget(slider, 1);

	volLabel = new QLabel();
	volLabel->setFixedWidth(50);
	volLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	bottomRow->addWidget(volLabel);

	muteCheckbox = new QCheckBox();
	muteCheckbox->setToolTip("Mute");
	muteCheckbox->setChecked(obs_source_muted(source));
	connect(muteCheckbox, &QCheckBox::toggled, this, &MixerItem::OnMuteToggled);
	bottomRow->addWidget(muteCheckbox);

	mainLayout->addLayout(bottomRow);

	setLayout(mainLayout);
}

void MixerItem::SetupSignals()
{
	// Fader callback for volume changes
	obs_fader_add_callback(obs_fader, OBSVolumeChanged, this);

	// Volmeter callback for level display
	obs_volmeter_add_callback(obs_volmeter, OBSVolumeLevel, this);

	// Source mute signal
	signal_handler_t *handler = obs_source_get_signal_handler(source);
	signalConnections.emplace_back(handler, "mute",
		[](void *data, calldata_t *cd) {
			bool muted = calldata_bool(cd, "muted");
			QMetaObject::invokeMethod(static_cast<MixerItem *>(data),
				"VolumeMuted", Qt::QueuedConnection,
				Q_ARG(bool, muted));
		}, this);
}

void MixerItem::DisconnectSignals()
{
	obs_fader_remove_callback(obs_fader, OBSVolumeChanged, this);
	obs_volmeter_remove_callback(obs_volmeter, OBSVolumeLevel, this);
	signalConnections.clear();
}

void MixerItem::OBSVolumeChanged(void *data, float db)
{
	Q_UNUSED(db);
	QMetaObject::invokeMethod(static_cast<MixerItem *>(data),
		"VolumeChanged", Qt::QueuedConnection);
}

void MixerItem::OBSVolumeMuted(void *data, calldata_t *calldata)
{
	bool muted = calldata_bool(calldata, "muted");
	QMetaObject::invokeMethod(static_cast<MixerItem *>(data),
		"VolumeMuted", Qt::QueuedConnection,
		Q_ARG(bool, muted));
}

void MixerItem::OBSVolumeLevel(void *data,
	const float magnitude[MAX_AUDIO_CHANNELS],
	const float peak[MAX_AUDIO_CHANNELS],
	const float inputPeak[MAX_AUDIO_CHANNELS])
{
	MixerItem *item = static_cast<MixerItem *>(data);
	if (item->volMeter) {
		QMetaObject::invokeMethod(item->volMeter, "setLevels",
			Qt::QueuedConnection,
			Q_ARG(float, magnitude[0]),
			Q_ARG(float, peak[0]),
			Q_ARG(float, inputPeak[0]));
	}
}

void MixerItem::VolumeChanged()
{
	float deflection = obs_fader_get_deflection(obs_fader);
	slider->blockSignals(true);
	slider->setValue(static_cast<int>(deflection * FADER_PRECISION));
	slider->blockSignals(false);

	UpdateVolumeLabel();
}

void MixerItem::VolumeMuted(bool muted)
{
	muteCheckbox->blockSignals(true);
	muteCheckbox->setChecked(muted);
	muteCheckbox->blockSignals(false);

	if (volMeter) {
		volMeter->setMuted(muted);
	}
}

void MixerItem::UpdateVolumeLabel()
{
	float db = obs_fader_get_db(obs_fader);

	if (db < -96.0f) {
		volLabel->setText("-inf dB");
	} else {
		volLabel->setText(QString::number(db, 'f', 1) + " dB");
	}
}

void MixerItem::OnSliderChanged(int value)
{
	float deflection = static_cast<float>(value) / FADER_PRECISION;
	obs_fader_set_deflection(obs_fader, deflection);
	UpdateVolumeLabel();
}

void MixerItem::OnMuteToggled(bool checked)
{
	obs_source_set_muted(source, checked);
}

void MixerItem::OnMoveUp()
{
	emit MoveUpRequested(this);
}

void MixerItem::OnMoveDown()
{
	emit MoveDownRequested(this);
}

void MixerItem::UpdateButtons(bool canMoveUp, bool canMoveDown)
{
	upButton->setEnabled(canMoveUp);
	downButton->setEnabled(canMoveDown);
}

void MixerItem::RefreshName()
{
	nameLabel->setText(GetSourceName());
	setObjectName(GetSourceName());
}

void MixerItem::SetVertical(bool vert)
{
	vertical = vert;
	// TODO: Implement layout change for vertical mode
}
