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

	// Current collection management
	void SetCurrentCollection(const std::string &collectionName);
	std::string GetCurrentCollection() const { return currentCollection; }

	// Order management
	std::vector<std::string> GetOrder() const;
	void SetOrder(const std::vector<std::string> &uuids);
	void AddSource(const std::string &uuid);
	void RemoveSource(const std::string &uuid);

private:
	std::string GetConfigPath() const;
	void EnsureDirectory(const std::string &path) const;

private:
	// Order storage: maps scene collection name to ordered list of source UUIDs
	std::map<std::string, std::vector<std::string>> orderByCollection;
	std::string currentCollection;
};
