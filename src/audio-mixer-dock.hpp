#pragma once

#include <obs.hpp>

#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>

#include <vector>

class MixerItem;
class OrderManager;

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
};
