#pragma once

#include <obs.hpp>

#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QMenu>
#include <QToolBar>
#include <QAction>

#include <vector>

class MixerItem;
class OrderManager;

// Helper functions for mixer hidden state (uses OBS's standard private settings)
static inline bool SourceMixerHidden(obs_source_t *source)
{
	OBSDataAutoRelease priv_settings = obs_source_get_private_settings(source);
	return obs_data_get_bool(priv_settings, "mixer_hidden");
}

static inline void SetSourceMixerHidden(obs_source_t *source, bool hidden)
{
	OBSDataAutoRelease priv_settings = obs_source_get_private_settings(source);
	obs_data_set_bool(priv_settings, "mixer_hidden", hidden);
}

class AudioMixerDock : public QFrame {
	Q_OBJECT

public:
	explicit AudioMixerDock(QWidget *parent = nullptr);
	~AudioMixerDock();

	bool IsVertical() const { return vertical; }
	void SetVerticalLayout(bool vert);

public slots:
	void OnSceneCollectionChanged();
	void OnFinishedLoading();
	void SaveOrder();
	void OnExit();

	void ActivateAudioSource(OBSSource source);
	void DeactivateAudioSource(OBSSource source);
	void OnSourceRenamed(QString newName, QString prevName);

	void HideSource(OBSSource source);
	void UnhideAllSources();

private slots:
	void ShowContextMenu(const QPoint &pos);
	void OnItemSelected(MixerItem *item);
	void OnMoveUpClicked();
	void OnMoveDownClicked();

private:
	void SetupUI();
	void ConnectSignalHandlers();
	void DisconnectSignalHandlers();
	void EnumerateAudioSources();
	void ClearMixerItems();
	void RefreshMixerLayout();
	void UpdateToolbarButtons();
	void SelectItem(MixerItem *item);

	MixerItem *FindMixerItem(obs_source_t *source);
	int GetItemIndex(MixerItem *item);

private:
	QVBoxLayout *mainLayout = nullptr;
	QScrollArea *scrollArea = nullptr;
	QWidget *scrollWidget = nullptr;
	QBoxLayout *mixerLayout = nullptr;
	QLabel *emptyLabel = nullptr;

	// Toolbar
	QToolBar *toolbar = nullptr;
	QAction *upAction = nullptr;
	QAction *downAction = nullptr;

	std::vector<MixerItem *> mixerItems;
	std::vector<OBSSignal> signalHandlers;

	OrderManager *orderManager = nullptr;
	MixerItem *selectedItem = nullptr;
	bool vertical = false;
	bool shuttingDown = false;
};
