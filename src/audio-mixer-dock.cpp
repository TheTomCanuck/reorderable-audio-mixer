#include "audio-mixer-dock.hpp"
#include "mixer-item.hpp"
#include "order-manager.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <set>

#include <QScrollBar>
#include <QCursor>
#include <QStyle>

AudioMixerDock::AudioMixerDock(QWidget *parent)
	: QFrame(parent),
	  orderManager(new OrderManager())
{
	SetupUI();
	ConnectSignalHandlers();

	// Load saved order
	orderManager->Load();

	// Get current scene collection name
	char *collection = obs_frontend_get_current_scene_collection();
	if (collection) {
		orderManager->SetCurrentCollection(collection);
		bfree(collection);
	}
}

AudioMixerDock::~AudioMixerDock()
{
	DisconnectSignalHandlers();
	ClearMixerItems();
	delete orderManager;
}

void AudioMixerDock::SetupUI()
{
	mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);

	scrollArea = new QScrollArea(this);
	scrollArea->setFrameShape(QFrame::NoFrame);
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	scrollWidget = new QWidget();
	mixerLayout = new QVBoxLayout(scrollWidget);
	mixerLayout->setContentsMargins(4, 4, 4, 4);
	mixerLayout->setSpacing(4);
	mixerLayout->setAlignment(Qt::AlignTop);

	// Empty state label
	emptyLabel = new QLabel(obs_module_text("BetterAudioMixer.NoAudioSources"));
	emptyLabel->setAlignment(Qt::AlignCenter);
	emptyLabel->setStyleSheet("color: gray; padding: 20px;");
	mixerLayout->addWidget(emptyLabel);

	scrollArea->setWidget(scrollWidget);
	mainLayout->addWidget(scrollArea, 1);

	// Toolbar at bottom
	toolbar = new QToolBar(this);
	toolbar->setObjectName(QStringLiteral("mixerToolbar"));
	toolbar->setIconSize(QSize(16, 16));
	toolbar->setFloatable(false);

	upAction = toolbar->addAction(
		QIcon(":/res/images/up.svg"),
		obs_module_text("BetterAudioMixer.MoveUp"),
		this, &AudioMixerDock::OnMoveUpClicked);
	toolbar->widgetForAction(upAction)->setProperty("themeID", QVariant(QString::fromUtf8("upArrowIconSmall")));
	toolbar->widgetForAction(upAction)->setProperty("class", "icon-up");
	upAction->setEnabled(false);

	downAction = toolbar->addAction(
		QIcon(":/res/images/down.svg"),
		obs_module_text("BetterAudioMixer.MoveDown"),
		this, &AudioMixerDock::OnMoveDownClicked);
	toolbar->widgetForAction(downAction)->setProperty("themeID", QVariant(QString::fromUtf8("downArrowIconSmall")));
	toolbar->widgetForAction(downAction)->setProperty("class", "icon-down");
	downAction->setEnabled(false);

	mainLayout->addWidget(toolbar);

	setLayout(mainLayout);

	// Enable context menu
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &QWidget::customContextMenuRequested, this, &AudioMixerDock::ShowContextMenu);
}

void AudioMixerDock::ConnectSignalHandlers()
{
	signal_handler_t *handler = obs_get_signal_handler();

	// Source activated (becomes visible/active)
	signalHandlers.emplace_back(handler, "source_activate",
		[](void *data, calldata_t *params) {
			auto *dock = static_cast<AudioMixerDock *>(data);
			obs_source_t *source = static_cast<obs_source_t *>(calldata_ptr(params, "source"));

			uint32_t flags = obs_source_get_output_flags(source);
			if (flags & OBS_SOURCE_AUDIO) {
				QMetaObject::invokeMethod(dock, "ActivateAudioSource",
					Qt::QueuedConnection,
					Q_ARG(OBSSource, OBSSource(source)));
			}
		}, this);

	// Source deactivated
	signalHandlers.emplace_back(handler, "source_deactivate",
		[](void *data, calldata_t *params) {
			auto *dock = static_cast<AudioMixerDock *>(data);
			obs_source_t *source = static_cast<obs_source_t *>(calldata_ptr(params, "source"));

			uint32_t flags = obs_source_get_output_flags(source);
			if (flags & OBS_SOURCE_AUDIO) {
				QMetaObject::invokeMethod(dock, "DeactivateAudioSource",
					Qt::QueuedConnection,
					Q_ARG(OBSSource, OBSSource(source)));
			}
		}, this);

	// Audio specifically activated
	signalHandlers.emplace_back(handler, "source_audio_activate",
		[](void *data, calldata_t *params) {
			auto *dock = static_cast<AudioMixerDock *>(data);
			obs_source_t *source = static_cast<obs_source_t *>(calldata_ptr(params, "source"));

			QMetaObject::invokeMethod(dock, "ActivateAudioSource",
				Qt::QueuedConnection,
				Q_ARG(OBSSource, OBSSource(source)));
		}, this);

	// Audio specifically deactivated
	signalHandlers.emplace_back(handler, "source_audio_deactivate",
		[](void *data, calldata_t *params) {
			auto *dock = static_cast<AudioMixerDock *>(data);
			obs_source_t *source = static_cast<obs_source_t *>(calldata_ptr(params, "source"));

			QMetaObject::invokeMethod(dock, "DeactivateAudioSource",
				Qt::QueuedConnection,
				Q_ARG(OBSSource, OBSSource(source)));
		}, this);

	// Source renamed
	signalHandlers.emplace_back(handler, "source_rename",
		[](void *data, calldata_t *params) {
			auto *dock = static_cast<AudioMixerDock *>(data);
			const char *newName = calldata_string(params, "new_name");
			const char *prevName = calldata_string(params, "prev_name");

			QMetaObject::invokeMethod(dock, "OnSourceRenamed",
				Qt::QueuedConnection,
				Q_ARG(QString, QString::fromUtf8(newName)),
				Q_ARG(QString, QString::fromUtf8(prevName)));
		}, this);
}

void AudioMixerDock::DisconnectSignalHandlers()
{
	signalHandlers.clear();
}

void AudioMixerDock::EnumerateAudioSources()
{
	auto enumCallback = [](void *data, obs_source_t *source) -> bool {
		auto *dock = static_cast<AudioMixerDock *>(data);

		uint32_t flags = obs_source_get_output_flags(source);
		if (!(flags & OBS_SOURCE_AUDIO))
			return true;

		if (!obs_source_active(source))
			return true;

		// Skip sources hidden in mixer
		if (SourceMixerHidden(source))
			return true;

		dock->ActivateAudioSource(OBSSource(source));
		return true;
	};

	obs_enum_sources(enumCallback, this);
}

void AudioMixerDock::ClearMixerItems()
{
	selectedItem = nullptr;
	for (MixerItem *item : mixerItems) {
		mixerLayout->removeWidget(item);
		item->Cleanup(shuttingDown);
		delete item;
	}
	mixerItems.clear();
}

void AudioMixerDock::RefreshMixerLayout()
{
	// Remove all items from layout
	for (MixerItem *item : mixerItems) {
		mixerLayout->removeWidget(item);
	}

	// Sort by saved order
	std::vector<std::string> order = orderManager->GetOrder();

	std::sort(mixerItems.begin(), mixerItems.end(),
		[&order](MixerItem *a, MixerItem *b) {
			std::string uuidA = a->GetSourceUUID().toStdString();
			std::string uuidB = b->GetSourceUUID().toStdString();

			auto itA = std::find(order.begin(), order.end(), uuidA);
			auto itB = std::find(order.begin(), order.end(), uuidB);

			// Items not in order go to end, sorted alphabetically
			if (itA == order.end() && itB == order.end()) {
				return a->GetSourceName() < b->GetSourceName();
			}
			if (itA == order.end()) return false;
			if (itB == order.end()) return true;

			return itA < itB;
		});

	// Re-add in sorted order
	for (MixerItem *item : mixerItems) {
		mixerLayout->addWidget(item);
	}

	// Save order immediately
	orderManager->Save();

	// Update empty state
	emptyLabel->setVisible(mixerItems.empty());

	UpdateToolbarButtons();
}

void AudioMixerDock::UpdateToolbarButtons()
{
	if (!selectedItem) {
		upAction->setEnabled(false);
		downAction->setEnabled(false);
	} else {
		int index = GetItemIndex(selectedItem);
		upAction->setEnabled(index > 0);
		downAction->setEnabled(index >= 0 && index < static_cast<int>(mixerItems.size()) - 1);
	}

	// Refresh toolbar styling after enabling/disabling actions
	for (QAction *action : toolbar->actions()) {
		QWidget *widget = toolbar->widgetForAction(action);
		if (widget) {
			widget->style()->unpolish(widget);
			widget->style()->polish(widget);
		}
	}
}

void AudioMixerDock::SelectItem(MixerItem *item)
{
	if (selectedItem == item)
		return;

	// Deselect previous
	if (selectedItem) {
		selectedItem->SetSelected(false);
	}

	// Select new
	selectedItem = item;
	if (selectedItem) {
		selectedItem->SetSelected(true);
	}

	UpdateToolbarButtons();
}

void AudioMixerDock::OnItemSelected(MixerItem *item)
{
	SelectItem(item);
}

void AudioMixerDock::OnMoveUpClicked()
{
	if (!selectedItem)
		return;

	int index = GetItemIndex(selectedItem);
	if (index <= 0)
		return;

	// Swap in our list
	std::swap(mixerItems[index], mixerItems[index - 1]);

	// Update order manager - preserve inactive sources
	std::vector<std::string> newOrder;
	std::set<std::string> visibleUuids;
	for (MixerItem *mi : mixerItems) {
		std::string uuid = mi->GetSourceUUID().toStdString();
		newOrder.push_back(uuid);
		visibleUuids.insert(uuid);
	}
	// Append inactive sources (in saved order) that aren't currently visible
	for (const std::string &uuid : orderManager->GetOrder()) {
		if (visibleUuids.find(uuid) == visibleUuids.end()) {
			newOrder.push_back(uuid);
		}
	}
	orderManager->SetOrder(newOrder);

	// Refresh layout (also saves)
	RefreshMixerLayout();
}

void AudioMixerDock::OnMoveDownClicked()
{
	if (!selectedItem)
		return;

	int index = GetItemIndex(selectedItem);
	if (index < 0 || index >= static_cast<int>(mixerItems.size()) - 1)
		return;

	// Swap in our list
	std::swap(mixerItems[index], mixerItems[index + 1]);

	// Update order manager - preserve inactive sources
	std::vector<std::string> newOrder;
	std::set<std::string> visibleUuids;
	for (MixerItem *mi : mixerItems) {
		std::string uuid = mi->GetSourceUUID().toStdString();
		newOrder.push_back(uuid);
		visibleUuids.insert(uuid);
	}
	// Append inactive sources (in saved order) that aren't currently visible
	for (const std::string &uuid : orderManager->GetOrder()) {
		if (visibleUuids.find(uuid) == visibleUuids.end()) {
			newOrder.push_back(uuid);
		}
	}
	orderManager->SetOrder(newOrder);

	// Refresh layout (also saves)
	RefreshMixerLayout();
}

MixerItem *AudioMixerDock::FindMixerItem(obs_source_t *source)
{
	for (MixerItem *item : mixerItems) {
		if (item->GetSource() == source)
			return item;
	}
	return nullptr;
}

int AudioMixerDock::GetItemIndex(MixerItem *item)
{
	for (size_t i = 0; i < mixerItems.size(); i++) {
		if (mixerItems[i] == item)
			return static_cast<int>(i);
	}
	return -1;
}

void AudioMixerDock::ActivateAudioSource(OBSSource source)
{
	// Check if already tracked
	if (FindMixerItem(source))
		return;

	// Verify it's actually active and has audio
	if (!obs_source_active(source))
		return;

	// Skip sources hidden in mixer
	if (SourceMixerHidden(source))
		return;

	// Create mixer item
	MixerItem *item = new MixerItem(source, vertical, scrollWidget);

	// Connect signals
	connect(item, &MixerItem::Selected, this, &AudioMixerDock::OnItemSelected);
	connect(item, &MixerItem::HideRequested, this, [this](MixerItem *item) {
		// Capture the source before any cleanup happens
		OBSSource source = OBSSource(item->GetSource());
		// Defer the hide operation to ensure all signal handlers complete first
		QMetaObject::invokeMethod(this, [this, source]() {
			HideSource(source);
		}, Qt::QueuedConnection);
	});

	// Add to list and order manager
	mixerItems.push_back(item);
	orderManager->AddSource(item->GetSourceUUID().toStdString());

	// Refresh layout
	RefreshMixerLayout();
}

void AudioMixerDock::DeactivateAudioSource(OBSSource source)
{
	MixerItem *item = FindMixerItem(source);
	if (!item)
		return;

	// Clear selection if this item was selected
	if (selectedItem == item) {
		selectedItem = nullptr;
	}

	// Remove from list
	auto it = std::find(mixerItems.begin(), mixerItems.end(), item);
	if (it != mixerItems.end()) {
		mixerItems.erase(it);
	}

	// Remove from layout and cleanup
	// Call Cleanup() immediately to detach from OBS objects while they're still valid
	// Then use deleteLater() because this may be called from a signal handler
	// while the item's context menu is still on the stack
	mixerLayout->removeWidget(item);
	item->Cleanup();
	item->hide();
	item->deleteLater();

	// Update empty state and buttons
	emptyLabel->setVisible(mixerItems.empty());
	UpdateToolbarButtons();
}

void AudioMixerDock::OnSourceRenamed(QString newName, QString prevName)
{
	Q_UNUSED(newName);
	Q_UNUSED(prevName);

	// Just refresh the display - names are fetched dynamically
	for (MixerItem *item : mixerItems) {
		item->RefreshName();
	}
}

void AudioMixerDock::OnSceneCollectionChanged()
{
	// Save current order before switching
	orderManager->Save();

	// Clear current items
	ClearMixerItems();

	// Update collection name
	char *collection = obs_frontend_get_current_scene_collection();
	if (collection) {
		orderManager->SetCurrentCollection(collection);
		bfree(collection);
	}

	// Re-enumerate sources
	EnumerateAudioSources();
}

void AudioMixerDock::OnFinishedLoading()
{
	EnumerateAudioSources();
}

void AudioMixerDock::SaveOrder()
{
	orderManager->Save();
}

void AudioMixerDock::OnExit()
{
	// Save order first
	orderManager->Save();

	// Mark as shutting down - this prevents MixerItem from trying to
	// detach/destroy faders which crashes when sources are already gone
	shuttingDown = true;

	// Disconnect signal handlers to prevent callbacks during cleanup
	DisconnectSignalHandlers();

	// Clear all mixer items - with shuttingDown=true, they won't touch OBS objects
	ClearMixerItems();
}

void AudioMixerDock::HideSource(OBSSource source)
{
	if (!SourceMixerHidden(source)) {
		SetSourceMixerHidden(source, true);
		// Remove from saved order - hidden sources lose their position
		const char *uuid = obs_source_get_uuid(source);
		if (uuid) {
			orderManager->RemoveSource(uuid);
			orderManager->Save();
		}
		DeactivateAudioSource(source);
	}
}

void AudioMixerDock::UnhideAllSources()
{
	auto unhideCallback = [](void *data, obs_source_t *source) -> bool {
		auto *dock = static_cast<AudioMixerDock *>(data);

		// Only process audio sources
		uint32_t flags = obs_source_get_output_flags(source);
		if (!(flags & OBS_SOURCE_AUDIO))
			return true;

		// Only unhide if currently hidden
		if (!SourceMixerHidden(source))
			return true;

		// Unhide
		SetSourceMixerHidden(source, false);

		// Re-activate if source is active
		if (obs_source_active(source)) {
			dock->ActivateAudioSource(OBSSource(source));
		}

		return true;
	};

	obs_enum_sources(unhideCallback, this);
}

void AudioMixerDock::ShowContextMenu(const QPoint &pos)
{
	Q_UNUSED(pos);

	QMenu menu(this);

	QAction *unhideAllAction = menu.addAction(obs_module_text("BetterAudioMixer.UnhideAll"));
	connect(unhideAllAction, &QAction::triggered, this, &AudioMixerDock::UnhideAllSources);

	menu.exec(QCursor::pos());
}
