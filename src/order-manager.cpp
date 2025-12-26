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

	orderByCollection.clear();

	obs_data_t *collections = obs_data_get_obj(data, "collections");
	if (collections) {
		// Iterate through collections
		obs_data_item_t *item = obs_data_first(collections);
		while (item) {
			const char *collectionName = obs_data_item_get_name(item);
			obs_data_t *collectionData = obs_data_item_get_obj(item);

			if (collectionData) {
				obs_data_array_t *orderArray = obs_data_get_array(collectionData, "order");
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
					orderByCollection[collectionName] = order;
					obs_data_array_release(orderArray);
				}
				obs_data_release(collectionData);
			}

			obs_data_item_next(&item);
		}
		obs_data_release(collections);
	}

	obs_data_release(data);
	blog(LOG_INFO, "[Reorderable Audio Mixer] Loaded order for %zu collections",
		orderByCollection.size());
}

void OrderManager::Save()
{
	std::string path = GetConfigPath();
	if (path.empty())
		return;

	EnsureDirectory(path);

	obs_data_t *data = obs_data_create();
	obs_data_set_int(data, "version", 1);

	obs_data_t *collections = obs_data_create();

	for (const auto &pair : orderByCollection) {
		obs_data_t *collectionData = obs_data_create();
		obs_data_array_t *orderArray = obs_data_array_create();

		for (const std::string &uuid : pair.second) {
			obs_data_t *entry = obs_data_create();
			obs_data_set_string(entry, "uuid", uuid.c_str());
			obs_data_array_push_back(orderArray, entry);
			obs_data_release(entry);
		}

		obs_data_set_array(collectionData, "order", orderArray);
		obs_data_set_obj(collections, pair.first.c_str(), collectionData);

		obs_data_array_release(orderArray);
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

std::vector<std::string> OrderManager::GetOrder() const
{
	auto it = orderByCollection.find(currentCollection);
	if (it != orderByCollection.end()) {
		return it->second;
	}
	return {};
}

void OrderManager::SetOrder(const std::vector<std::string> &uuids)
{
	orderByCollection[currentCollection] = uuids;
}

void OrderManager::AddSource(const std::string &uuid)
{
	auto &order = orderByCollection[currentCollection];

	// Don't add duplicates
	if (std::find(order.begin(), order.end(), uuid) == order.end()) {
		order.push_back(uuid);
	}
}

void OrderManager::RemoveSource(const std::string &uuid)
{
	auto it = orderByCollection.find(currentCollection);
	if (it != orderByCollection.end()) {
		auto &order = it->second;
		order.erase(std::remove(order.begin(), order.end(), uuid), order.end());
	}
}
