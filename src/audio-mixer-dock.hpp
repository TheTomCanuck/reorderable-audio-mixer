#pragma once

#include <obs.hpp>

#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QMenu>

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

public slots:
	void OnSceneCollectionChanged();
	void OnFinishedLoading();
	void SaveOrder();
	void OnExit();

	void ActivateAudioSource(OBSSource source);
	void DeactivateAudioSource(OBSSource source);
	void OnSourceRenamed(QString newName, QString prevName);

	void MoveSourceUp(MixerItem *item);
	void MoveSourceDown(MixerItem *item);

	void HideSource(OBSSource source);
	void UnhideAllSources();

private slots:
	void ShowContextMenu(const QPoint &pos);

private:
	void SetupUI();
	void ConnectSignalHandlers();
	void DisconnectSignalHandlers();
	void EnumerateAudioSources();
	void ClearMixerItems();
	void RefreshMixerLayout();
	void UpdateButtonStates();

	MixerItem *FindMixerItem(obs_source_t *source);
	int GetItemIndex(MixerItem *item);

private:
	QVBoxLayout *mainLayout = nullptr;
	QScrollArea *scrollArea = nullptr;
	QWidget *scrollWidget = nullptr;
	QBoxLayout *mixerLayout = nullptr;
	QLabel *emptyLabel = nullptr;

	std::vector<MixerItem *> mixerItems;
	std::vector<OBSSignal> signalHandlers;

	OrderManager *orderManager = nullptr;
	bool vertical = false;
	bool shuttingDown = false;
};
