#include "order-manager.hpp"

#include <obs-module.h>
#include <util/platform.h>

#include <algorithm>

OrderManager::OrderManager()
{
}

OrderManager::~OrderManager()
{
}

std::string OrderManager::GetConfigPath() const
{
	char *path = obs_module_config_path("order.json");
	std::string result = path ? path : "";
	bfree(path);
	return result;
}

void OrderManager::EnsureDirectory(const std::string &path) const
{
	// Find last separator
	size_t pos = path.find_last_of("/\\");
	if (pos != std::string::npos) {
		std::string dir = path.substr(0, pos);
		os_mkdirs(dir.c_str());
	}
}

void OrderManager::Load()
{
	std::string path = GetConfigPath();
	if (path.empty())
		return;

	obs_data_t *data = obs_data_create_from_json_file_safe(path.c_str(), "bak");
	if (!data) {
		blog(LOG_INFO, "[Reorderable Audio Mixer] No saved order found");
		return;
	}

	orderByCollectionScene.clear();

	// Load global preferences
	verticalLayout = obs_data_get_bool(data, "verticalLayout");

	int version = (int)obs_data_get_int(data, "version");

	if (version >= 2) {
		// Version 2: per-scene ordering
		obs_data_t *collections = obs_data_get_obj(data, "collections");
		if (collections) {
			obs_data_item_t *collItem = obs_data_first(collections);
			while (collItem) {
				const char *collectionName = obs_data_item_get_name(collItem);
				obs_data_t *collectionData = obs_data_item_get_obj(collItem);

				if (collectionData) {
					obs_data_t *scenes = obs_data_get_obj(collectionData, "scenes");
					if (scenes) {
						obs_data_item_t *sceneItem = obs_data_first(scenes);
						while (sceneItem) {
							const char *sceneName = obs_data_item_get_name(sceneItem);
							obs_data_t *sceneData = obs_data_item_get_obj(sceneItem);

							if (sceneData) {
								obs_data_array_t *orderArray = obs_data_get_array(sceneData, "order");
								if (orderArray) {
									std::vector<std::string> order;
									size_t count = obs_data_array_count(orderArray);
									for (size_t i = 0; i < count; i++) {
										obs_data_t *entry = obs_data_array_item(orderArray, i);
										const char *uuid = obs_data_get_string(entry, "uuid");
										if (uuid && *uuid) {
											order.push_back(uuid);
										}
										obs_data_release(entry);
									}
									orderByCollectionScene[collectionName][sceneName] = order;
									obs_data_array_release(orderArray);
								}
								obs_data_release(sceneData);
							}
							obs_data_item_next(&sceneItem);
						}
						obs_data_release(scenes);
					}
					obs_data_release(collectionData);
				}
				obs_data_item_next(&collItem);
			}
			obs_data_release(collections);
		}
		blog(LOG_INFO, "[Reorderable Audio Mixer] Loaded per-scene order config (v%d)", version);
	} else {
		// Version 1 (old global order format) - ignore old data, start fresh
		blog(LOG_INFO, "[Reorderable Audio Mixer] Old config format (v%d), starting fresh with per-scene ordering", version);
	}

	obs_data_release(data);
}

void OrderManager::Save()
{
	std::string path = GetConfigPath();
	if (path.empty())
		return;

	EnsureDirectory(path);

	obs_data_t *data = obs_data_create();
	obs_data_set_int(data, "version", 2);
	obs_data_set_bool(data, "verticalLayout", verticalLayout);

	obs_data_t *collections = obs_data_create();

	for (const auto &collPair : orderByCollectionScene) {
		obs_data_t *collectionData = obs_data_create();
		obs_data_t *scenes = obs_data_create();

		for (const auto &scenePair : collPair.second) {
			obs_data_t *sceneData = obs_data_create();
			obs_data_array_t *orderArray = obs_data_array_create();

			for (const std::string &uuid : scenePair.second) {
				obs_data_t *entry = obs_data_create();
				obs_data_set_string(entry, "uuid", uuid.c_str());
				obs_data_array_push_back(orderArray, entry);
				obs_data_release(entry);
			}

			obs_data_set_array(sceneData, "order", orderArray);
			obs_data_set_obj(scenes, scenePair.first.c_str(), sceneData);

			obs_data_array_release(orderArray);
			obs_data_release(sceneData);
		}

		obs_data_set_obj(collectionData, "scenes", scenes);
		obs_data_set_obj(collections, collPair.first.c_str(), collectionData);

		obs_data_release(scenes);
		obs_data_release(collectionData);
	}

	obs_data_set_obj(data, "collections", collections);
	obs_data_release(collections);

	if (obs_data_save_json_safe(data, path.c_str(), "tmp", "bak")) {
		blog(LOG_INFO, "[Reorderable Audio Mixer] Saved order config");
	} else {
		blog(LOG_ERROR, "[Reorderable Audio Mixer] Failed to save order config");
	}

	obs_data_release(data);
}

void OrderManager::SetCurrentCollection(const std::string &collectionName)
{
	currentCollection = collectionName;
}

void OrderManager::SetCurrentScene(const std::string &sceneName)
{
	currentScene = sceneName;
}

std::vector<std::string> OrderManager::GetOrder() const
{
	auto collIt = orderByCollectionScene.find(currentCollection);
	if (collIt != orderByCollectionScene.end()) {
		auto sceneIt = collIt->second.find(currentScene);
		if (sceneIt != collIt->second.end()) {
			return sceneIt->second;
		}
	}
	return {};
}

void OrderManager::SetOrder(const std::vector<std::string> &uuids)
{
	orderByCollectionScene[currentCollection][currentScene] = uuids;
}

void OrderManager::AddSource(const std::string &uuid)
{
	auto &order = orderByCollectionScene[currentCollection][currentScene];

	// Don't add duplicates
	if (std::find(order.begin(), order.end(), uuid) == order.end()) {
		order.push_back(uuid);
	}
}

void OrderManager::RemoveSource(const std::string &uuid)
{
	auto collIt = orderByCollectionScene.find(currentCollection);
	if (collIt != orderByCollectionScene.end()) {
		auto sceneIt = collIt->second.find(currentScene);
		if (sceneIt != collIt->second.end()) {
			auto &order = sceneIt->second;
			order.erase(std::remove(order.begin(), order.end(), uuid), order.end());
		}
	}
}
