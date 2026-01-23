#pragma once

#include <string>
#include <vector>
#include <map>

class OrderManager {
public:
	OrderManager();
	~OrderManager();

	// Persistence
	void Load();
	void Save();

	// Current context management
	void SetCurrentCollection(const std::string &collectionName);
	void SetCurrentScene(const std::string &sceneName);
	std::string GetCurrentCollection() const { return currentCollection; }
	std::string GetCurrentScene() const { return currentScene; }

	// Order management (operates on current collection + scene)
	std::vector<std::string> GetOrder() const;
	void SetOrder(const std::vector<std::string> &uuids);
	void AddSource(const std::string &uuid);
	void RemoveSource(const std::string &uuid);

	// Layout preference (global, not per-scene)
	bool IsVerticalLayout() const { return verticalLayout; }
	void SetVerticalLayout(bool vertical) { verticalLayout = vertical; }

private:
	std::string GetConfigPath() const;
	void EnsureDirectory(const std::string &path) const;

private:
	// Order storage: collection -> scene -> ordered list of source UUIDs
	std::map<std::string, std::map<std::string, std::vector<std::string>>> orderByCollectionScene;
	std::string currentCollection;
	std::string currentScene;
	bool verticalLayout = false;
};
