#include "mixer-item.hpp"
#include "volume-meter.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QCursor>
#include <QAction>
#include <QMainWindow>
#include <QMouseEvent>
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
		// OBSFader and OBSVolMeter are RAII wrappers (OBSPtr) that auto-destroy
		// when assigned nullptr - do NOT manually call obs_fader_destroy/obs_volmeter_destroy
		// as that causes a double-free crash!
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
	mainLayout->setSpacing(2);

	// Row 1: Name
	QHBoxLayout *nameRow = new QHBoxLayout();
	nameRow->setSpacing(4);

	nameLabel = new QLabel(GetSourceName());
	nameLabel->setStyleSheet("font-weight: bold;");
	nameRow->addWidget(nameLabel, 1);

	mainLayout->addLayout(nameRow);

	// Row 2: Config button + Volume meter + dB label
	// [config] [========meter========] [dB]
	QHBoxLayout *meterRow = new QHBoxLayout();
	meterRow->setSpacing(4);

	configButton = new QPushButton();
	configButton->setProperty("class", "icon-dots-vert");
	configButton->setFixedSize(22, 22);
	configButton->setFlat(true);
	configButton->setToolTip(obs_module_text("BetterAudioMixer.Config"));
	connect(configButton, &QPushButton::clicked, this, &MixerItem::OnConfigClicked);
	meterRow->addWidget(configButton);

	volMeter = new VolumeMeter(this);
	volMeter->setMinimumHeight(20);
	volMeter->muted = obs_source_muted(source);
	meterRow->addWidget(volMeter, 1);

	volLabel = new QLabel();
	volLabel->setFixedWidth(50);
	volLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	meterRow->addWidget(volLabel);

	mainLayout->addLayout(meterRow);

	// Row 3: Mute checkbox + Slider
	// [mute  ] [========slider=======]
	QHBoxLayout *sliderRow = new QHBoxLayout();
	sliderRow->setSpacing(4);

	muteCheckbox = new QCheckBox();
	muteCheckbox->setProperty("class", "indicator-mute");
	muteCheckbox->setFixedSize(22, 22);
	muteCheckbox->setToolTip(obs_module_text("BetterAudioMixer.Mute"));
	muteCheckbox->setChecked(obs_source_muted(source));
	connect(muteCheckbox, &QCheckBox::toggled, this, &MixerItem::OnMuteToggled);
	sliderRow->addWidget(muteCheckbox);

	slider = new QSlider(Qt::Horizontal);
	slider->setMinimum(0);
	slider->setMaximum(static_cast<int>(FADER_PRECISION));
	slider->setValue(static_cast<int>(obs_fader_get_deflection(obs_fader) * FADER_PRECISION));
	connect(slider, &QSlider::valueChanged, this, &MixerItem::OnSliderChanged);
	sliderRow->addWidget(slider, 1);

	// Spacer to align slider end with meter end (same width as volLabel)
	QWidget *sliderSpacer = new QWidget();
	sliderSpacer->setFixedWidth(50);
	sliderRow->addWidget(sliderSpacer);

	mainLayout->addLayout(sliderRow);

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
		// setLevels is thread-safe (uses mutex internally)
		item->volMeter->setLevels(magnitude, peak, inputPeak);
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
		volMeter->muted = muted;
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

void MixerItem::SetSelected(bool sel)
{
	if (selected == sel)
		return;

	selected = sel;
	UpdateSelectionStyle();
}

void MixerItem::UpdateSelectionStyle()
{
	if (selected) {
		setStyleSheet("MixerItem { border: 2px solid palette(highlight); }");
	} else {
		setStyleSheet("");
	}
}

void MixerItem::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		emit Selected(this);
	}
	QFrame::mousePressEvent(event);
}

void MixerItem::OnConfigClicked()
{
	QMenu menu(this);

	QAction *hideAction = menu.addAction(obs_module_text("BetterAudioMixer.Hide"));
	connect(hideAction, &QAction::triggered, this, &MixerItem::OnHideClicked);

	menu.addSeparator();

	QAction *filtersAction = menu.addAction(obs_module_text("BetterAudioMixer.Filters"));
	connect(filtersAction, &QAction::triggered, this, &MixerItem::OnFiltersClicked);

	QAction *propertiesAction = menu.addAction(obs_module_text("BetterAudioMixer.Properties"));
	connect(propertiesAction, &QAction::triggered, this, &MixerItem::OnPropertiesClicked);

	menu.addSeparator();

	QAction *advAudioAction = menu.addAction(obs_module_text("BetterAudioMixer.AdvancedAudio"));
	connect(advAudioAction, &QAction::triggered, this, &MixerItem::OnAdvancedAudioClicked);

	menu.exec(QCursor::pos());
}

void MixerItem::OnHideClicked()
{
	// Defer the hide operation until after the menu has closed
	// and the stack is fully unwound. This prevents crashes from
	// destroying OBS objects while still inside OnConfigClicked().
	QMetaObject::invokeMethod(this, [this]() {
		emit HideRequested(this);
	}, Qt::QueuedConnection);
}

void MixerItem::OnFiltersClicked()
{
	obs_frontend_open_source_filters(source);
}

void MixerItem::OnPropertiesClicked()
{
	obs_frontend_open_source_properties(source);
}

void MixerItem::OnAdvancedAudioClicked()
{
	// Open the Advanced Audio Properties dialog
	QMainWindow *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main)
		return;

	QAction *action = main->findChild<QAction *>("actionAdvAudioProperties");
	if (action)
		action->trigger();
}
