#include "plugin-main.hpp"
#include "audio-mixer-dock.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QMainWindow>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("obs-better-audio-mixer")
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static AudioMixerDock *mixer_dock = nullptr;

static void frontend_event_callback(obs_frontend_event event, void *)
{
	if (!mixer_dock)
		return;

	switch (event) {
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		QMetaObject::invokeMethod(mixer_dock, "OnSceneCollectionChanged");
		break;
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		QMetaObject::invokeMethod(mixer_dock, "OnFinishedLoading");
		break;
	case OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN:
		// Must clean up BEFORE ClearSceneData() destroys sources
		// OBS_FRONTEND_EVENT_EXIT fires AFTER sources are destroyed
		mixer_dock->OnExit();
		break;
	default:
		break;
	}
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "[Better Audio Mixer] loaded version %s", PROJECT_VERSION);
	obs_frontend_add_event_callback(frontend_event_callback, nullptr);
	return true;
}

void obs_module_post_load(void)
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());

	mixer_dock = new AudioMixerDock(main_window);

	const QString title = QString::fromUtf8(obs_module_text("BetterAudioMixer"));
	obs_frontend_add_dock_by_id(PLUGIN_NAME, title.toUtf8().constData(), mixer_dock);

	blog(LOG_INFO, "[Better Audio Mixer] Dock registered");
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontend_event_callback, nullptr);
	blog(LOG_INFO, "[Better Audio Mixer] unloaded");
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Better Audio Mixer - Reorderable audio mixer dock";
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("BetterAudioMixer");
}
