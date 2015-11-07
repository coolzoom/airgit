/*
* Copyright (C) 2011-2015 AirDC++ Project
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef DCPLUSPLUS_DCPP_LISTVIEW_H
#define DCPLUSPLUS_DCPP_LISTVIEW_H

#include <web-server/stdinc.h>
#include <web-server/SessionListener.h>
#include <web-server/WebServerManager.h>

#include <airdcpp/TaskQueue.h>

#include <api/ApiModule.h>
#include <api/common/PropertyFilter.h>
#include <api/common/Serializer.h>

namespace webserver {

	template<class T, int PropertyCount>
	class ListViewController : private SessionListener {
	public:
		typedef typename PropertyItemHandler<T>::ItemList ItemList;
		typedef typename PropertyItemHandler<T>::ItemListFunction ItemListF;

		ListViewController(const string& aViewName, ApiModule* aModule, const PropertyItemHandler<T>& aItemHandler, ItemListF aItemListF) :
			module(aModule), viewName(aViewName), itemHandler(aItemHandler), filter(aItemHandler.properties), itemListF(aItemListF),
			timer(WebServerManager::getInstance()->addTimer([this] { runTasks(); }, 200))
		{
			aModule->getSession()->addListener(this);

			// Magic for the following defines
			auto& requestHandlers = aModule->getRequestHandlers();

			METHOD_HANDLER(viewName, ApiRequest::METHOD_POST, (EXACT_PARAM("filter")), true, ListViewController::handlePostFilter);
			METHOD_HANDLER(viewName, ApiRequest::METHOD_DELETE, (EXACT_PARAM("filter")), false, ListViewController::handleDeleteFilter);

			METHOD_HANDLER(viewName, ApiRequest::METHOD_POST, (), true, ListViewController::handlePostSettings);
			METHOD_HANDLER(viewName, ApiRequest::METHOD_DELETE, (), false, ListViewController::handleReset);

			METHOD_HANDLER(viewName, ApiRequest::METHOD_GET, (EXACT_PARAM("items"), NUM_PARAM, NUM_PARAM), false, ListViewController::handleGetItems);
		}

		~ListViewController() {
			module->getSession()->removeListener(this);

			timer->stop(true);
		}

		void stop() noexcept {
			active = false;
			timer->stop(true);

			clearItems();
			currentValues.reset();

			WLock l(cs);
			filter.clear();
		}

		void setResetItems() {
			clearItems();

			currentValues.set(IntCollector::TYPE_RANGE_START, 0);

			updateList();
		}

		void onItemAdded(const T& aItem) {
			if (!active) return;

			WLock l(cs);
			auto prep = filter.prepare();
			if (!matchesFilter(aItem, prep)) {
				return;
			}

			tasks.addItem(aItem);
		}

		void onItemRemoved(const T& aItem) {
			if (!active) return;

			WLock l(cs);
			auto pos = findItem(aItem, allItems);
			if (pos != allItems.end()) {
				tasks.removeItem(*pos);
			}
		}

		void onItemUpdated(const T& aItem, const PropertyIdSet& aUpdatedProperties) {
			if (!active) return;

			bool inList;
			{
				RLock l(cs);
				inList = isInList(aItem, allItems);
			}

			auto prep = filter.prepare();
			if (!matchesFilter(aItem, prep)) {
				if (inList) {
					tasks.removeItem(aItem);
				}

				return;
			} else if (!inList) {
				tasks.addItem(aItem);
				return;
			}

			tasks.updateItem(aItem, aUpdatedProperties);
		}

		void onItemsUpdated(const ItemList& aItems, const PropertyIdSet& aUpdatedProperties) {
			if (!active) return;

			for (const auto& item : aItems) {
				onItemUpdated(item, aUpdatedProperties);
			}
		}

		void resetFilter() {
			{
				WLock l(cs);
				filter.clear();
			}

			onFilterUpdated();
		}

		void setFilter(const string& aPattern, int aMethod, int aProperty) {
			{
				WLock l(cs);
				filter.setFilterMethod(static_cast<StringMatch::Method>(aMethod));
				filter.setFilterProperty(aProperty);
				filter.setText(aPattern);
			}

			onFilterUpdated();
		}
	private:
		api_return handlePostFilter(ApiRequest& aRequest) {
			const auto& reqJson = aRequest.getRequestBody();

			std::string pattern = reqJson["pattern"];
			if (pattern.empty()) {
				resetFilter();
			} else {
				setFilter(pattern, reqJson["method"], findPropertyByName(reqJson["property"], itemHandler.properties));
			}

			// TODO: support for multiple filters
			return websocketpp::http::status_code::no_content;
		}

		api_return handlePostSettings(ApiRequest& aRequest) {
			parseProperties(aRequest.getRequestBody());

			if (!active) {
				active = true;
				updateList();
				timer->start();
			}
			return websocketpp::http::status_code::no_content;
		}

		api_return handleReset(ApiRequest& aRequest) {
			stop();
			return websocketpp::http::status_code::no_content;
		}

		void parseProperties(const json& j) {
			typename IntCollector::ValueMap updatedValues;
			if (j.find("range_start") != j.end()) {
				int start = j["range_start"];
				if (start < 0) {
					throw std::invalid_argument("Negative range start not allowed");
				}

				updatedValues[IntCollector::TYPE_RANGE_START] = start;
			}

			if (j.find("max_count") != j.end()) {
				int end = j["max_count"];
				updatedValues[IntCollector::TYPE_MAX_COUNT] = end;
			}

			if (j.find("sort_property") != j.end()) {
				auto prop = findPropertyByName(j["sort_property"], itemHandler.properties);
				if (prop == -1) {
					throw std::invalid_argument("Invalid sort property");
				}

				updatedValues[IntCollector::TYPE_SORT_PROPERTY] = prop;
			}

			if (j.find("sort_ascending") != j.end()) {
				bool sortAscending = j["sort_ascending"];
				updatedValues[IntCollector::TYPE_SORT_ASCENDING] = sortAscending;
			}

			if (j.find("paused") != j.end()) {
				bool paused = j["paused"];
				if (paused && timer->isRunning()) {
					timer->stop(false);
				}
				else if (!paused && !timer->isRunning()) {
					timer->start();
				}
			}

			if (!updatedValues.empty()) {
				WLock l(cs);
				currentValues.set(updatedValues);
			}
		}

		api_return handleDeleteFilter(ApiRequest& aRequest) {
			resetFilter();
			return websocketpp::http::status_code::ok;
		}

		void on(SessionListener::SocketDisconnected) noexcept {
			stop();
		}

		void sendJson(const json& j) {
			if (j.is_null()) {
				return;
			}

			module->send(viewName + "_updated", j);
		}

		void onFilterUpdated() {
			ItemList itemsNew;
			auto prep = filter.prepare();
			{
				for (const auto& i : itemListF()) {
					if (matchesFilter(i, prep)) {
						itemsNew.push_back(i);
					}
				}
			}

			{
				WLock l(cs);
				allItems.swap(itemsNew);
				itemListChanged = true;
			}

			//resetRange();
		}

		void updateList() {
			WLock l(cs);
			allItems = itemListF();
			itemListChanged = true;
		}

		void clearItems() {
			WLock l(cs);
			tasks.clear();
			currentViewItems.clear();
			allItems.clear();
			prevTotalCount = -1;
		}

		static bool itemSort(const T& t1, const T& t2, const PropertyItemHandler<T>& aItemHandler, int aSortProperty, int aSortAscending) {
			int res = 0;
			switch (aItemHandler.properties[aSortProperty].sortMethod) {
			case SORT_NUMERIC: {
				res = compare(aItemHandler.numberF(t1, aSortProperty), aItemHandler.numberF(t2, aSortProperty));
				break;
			}
			case SORT_TEXT: {
				res = Util::stricmp(aItemHandler.stringF(t1, aSortProperty).c_str(), aItemHandler.stringF(t2, aSortProperty).c_str());
				break;
			}
			case SORT_CUSTOM: {
				res = aItemHandler.customSorterF(t1, t2, aSortProperty);
				break;
			}
			}

			return aSortAscending == 1 ? res < 0 : res > 0;
		}

		api_return handleGetItems(ApiRequest& aRequest) {
			auto start = aRequest.getRangeParam(1);
			auto end = aRequest.getRangeParam(2);
			decltype(allItems) allItemsCopy;

			{
				RLock l(cs);
				allItemsCopy = allItems;
			}

			auto j = Serializer::serializeFromPosition(start, end - start, allItemsCopy, [&](const T& i) {
				return Serializer::serializeItem(i, itemHandler);
			});

			aRequest.setResponseBody(j);
			return websocketpp::http::status_code::ok;
		}

		typename ItemList::iterator findItem(const T& aItem, ItemList& aItems) noexcept {
			return find_if(aItems.begin(), aItems.end(), [&](const T& i) { return aItem->getToken() == i->getToken(); });
		}

		typename ItemList::const_iterator findItem(const T& aItem, const ItemList& aItems) const noexcept {
			return find_if(aItems.begin(), aItems.end(), [&](const T& i) { return aItem->getToken() == i->getToken(); });
		}

		bool isInList(const T& aItem, const ItemList& aItems) const noexcept {
			return findItem(aItem, aItems) != aItems.end();
		}

		int64_t getPosition(const T& aItem, const ItemList& aItems) const noexcept {
			auto i = findItem(aItem, aItems);
			if (i == aItems.end()) {
				return -1;
			}

			return distance(aItems.begin(), i);
		}

		void runTasks() {
			typename ViewTasks::TaskMap tl;
			PropertyIdSet updatedProperties;
			tasks.get(tl, updatedProperties);

			if (tl.empty() && !currentValues.hasChanged() && !itemListChanged) {
				return;
			}

			typename IntCollector::ValueMap updateValues;
			int sortAscending = false;
			int sortProperty = -1;

			{
				WLock l(cs);
				updateValues = currentValues.getAll();
				sortAscending = updateValues[IntCollector::TYPE_SORT_ASCENDING];
				sortProperty = updateValues[IntCollector::TYPE_SORT_PROPERTY];
				if (sortProperty < 0) {
					return;
				}

				bool needSort = updatedProperties.find(sortProperty) != updatedProperties.end() ||
					prevValues[IntCollector::TYPE_SORT_ASCENDING] != sortAscending ||
					prevValues[IntCollector::TYPE_SORT_PROPERTY] != sortProperty ||
					itemListChanged;

				if (needSort) {
					std::sort(allItems.begin(), allItems.end(), 
						std::bind(&ListViewController::itemSort, 
							std::placeholders::_1, 
							std::placeholders::_2, 
							itemHandler, 
							sortProperty,
							sortAscending
							));
				}
			}

			auto newStart = updateValues[IntCollector::TYPE_RANGE_START];
			if (newStart < 0) {
				return;
			}

			itemListChanged = false;

			// Go through the tasks
			std::map<T, const PropertyIdSet&> updatedItems;
			for (auto& t : tl) {
				switch (t.second.type) {
					case ADD_ITEM: {
						handleAddItem(t.first, sortProperty, sortAscending, newStart);
						break;
					}
					case REMOVE_ITEM: {
						handleRemoveItem(t.first, newStart);
						break;
					}
					case UPDATE_ITEM: {
						updatedItems.emplace(t.first, t.second.updatedProperties);
						break;
					}
				}
			}

			int totalItemCount = 0;

			// Get the new visible items
			decltype(currentViewItems) viewItemsNew, oldViewItems;
			{
				RLock l(cs);
				totalItemCount = allItems.size();
				if (newStart >= totalItemCount) {
					newStart = 0;
				}

				auto count = min(totalItemCount - newStart, updateValues[IntCollector::TYPE_MAX_COUNT]);
				if (count < 0) {
					return;
				}


				auto startIter = allItems.begin();
				advance(startIter, newStart);

				auto endIter = startIter;
				advance(endIter, count);

				std::copy(startIter, endIter, back_inserter(viewItemsNew));
				oldViewItems = currentViewItems;
			}

			json j;

			// List items
			int pos = 0;
			for (const auto& item : viewItemsNew) {
				if (!isInList(item, oldViewItems)) {
					appendItem(item, j, pos);
				} else {
					// append position
					auto props = updatedItems.find(item);
					if (props != updatedItems.end()) {
						appendItem(item, j, pos, props->second);
					} else {
						appendItemPosition(item, j, pos);
					}
				}

				pos++;
			}

			if (totalItemCount != prevTotalCount) {
				prevTotalCount = totalItemCount;
				j["total_items"] = totalItemCount;
			}

			auto startOffset = newStart - updateValues[IntCollector::TYPE_RANGE_START];
			if (startOffset != 0) {
				j["range_offset"] = startOffset;
			}

			j["range_start"] = newStart;


			{
				WLock l(cs);
				//IntCollector::appendChanges(updateValues, prevValues, j)

				currentViewItems.swap(viewItemsNew);
				prevValues.swap(updateValues);
			}

			sendJson(j);
		}

		void handleAddItem(const T& aItem, int aSortProperty, int aSortAscending, int& rangeStart_) {
			WLock l(cs);
			auto iter = allItems.insert(std::lower_bound(
				allItems.begin(), 
				allItems.end(), 
				aItem, 
				std::bind(&ListViewController::itemSort, std::placeholders::_1, std::placeholders::_2, itemHandler, aSortProperty, aSortAscending)
			), aItem);

			auto pos = static_cast<int>(std::distance(allItems.begin(), iter));
			if (pos < rangeStart_) {
				// Update the range range positions
				rangeStart_++;
			}
		}

		void handleRemoveItem(const T& aItem, int& rangeStart_) {
			WLock l(cs);
			auto iter = findItem(aItem, allItems);
			auto pos = static_cast<int>(std::distance(allItems.begin(), iter));

			allItems.erase(iter);

			if (pos < rangeStart_) {
				// Update the range range positions
				rangeStart_--;
			}
		}

		bool matchesFilter(const T& aItem, const PropertyFilter::Preparation& prep) {
			return filter.match(prep,
				[&](size_t aProperty) { return itemHandler.numberF(aItem, aProperty); },
				[&](size_t aProperty) { return itemHandler.stringF(aItem, aProperty); }
				);
		}

		// JSON APPEND START
		void appendItem(const T& aItem, json& json_, int pos) {
			appendItem(aItem, json_, pos, toPropertyIdSet(itemHandler.properties));
		}

		void appendItem(const T& aItem, json& json_, int pos, const PropertyIdSet& aPropertyIds) {
			appendItemPosition(aItem, json_, pos);
			json_["items"][pos]["properties"] = Serializer::serializeItemProperties(aItem, aPropertyIds, itemHandler);
		}

		void appendItemPosition(const T& aItem, json& json_, int pos) {
			json_["items"][pos]["id"] = aItem->getToken();
		}

		PropertyFilter filter;
		const PropertyItemHandler<T>& itemHandler;

		ItemList currentViewItems;
		ItemList allItems;

		bool active = false;

		SharedMutex cs;

		ApiModule* module = nullptr;
		std::string viewName;

		// Must be in merging order (lower ones replace other)
		enum Tasks {
			UPDATE_ITEM = 0,
			ADD_ITEM,
			REMOVE_ITEM
		};

		TimerPtr timer;

		class ItemTasks {
		public:
			struct MergeTask {
				int8_t type;
				PropertyIdSet updatedProperties;

				MergeTask(int8_t aType, const PropertyIdSet& aUpdatedProperties = PropertyIdSet()) : type(aType), updatedProperties(aUpdatedProperties) {

				}

				void merge(const MergeTask& aTask) {
					// Ignore
					if (type < aTask.type) {
						return;
					}

					// Merge
					if (type == aTask.type) {
						updatedProperties.insert(aTask.updatedProperties.begin(), aTask.updatedProperties.end());
						return;
					}

					// Replace the task
					type = aTask.type;
					updatedProperties = aTask.updatedProperties;
				}
			};

			typedef map<T, MergeTask> TaskMap;

			void add(const T& aItem, MergeTask&& aData) {
				WLock l(cs);
				auto j = tasks.find(aItem);
				if (j != tasks.end()) {
					(*j).second.merge(aData);
					return;
				}

				tasks.emplace(aItem, move(aData));
			}

			void clear() {
				WLock l(cs);
				tasks.clear();
			}

			bool remove(const T& aItem) {
				WLock l(cs);
				return tasks.erase(aItem) > 0;
			}

			void get(TaskMap& map) {
				WLock l(cs);
				swap(tasks, map);
			}
		private:
			TaskMap tasks;

			SharedMutex cs;
		};

		class ViewTasks : public ItemTasks {
		public:
			void addItem(const T& aItem) {
				tasks.add(aItem, typename ViewTasks::MergeTask(ADD_ITEM));
			}

			void removeItem(const T& aItem) {
				tasks.add(aItem, typename ViewTasks::MergeTask(REMOVE_ITEM));
			}

			void updateItem(const T& aItem, const PropertyIdSet& aUpdatedProperties) {
				updatedProperties.insert(aUpdatedProperties.begin(), aUpdatedProperties.end());
				tasks.add(aItem, typename ViewTasks::MergeTask(UPDATE_ITEM, aUpdatedProperties));
			}

			void get(typename ItemTasks::TaskMap& map, PropertyIdSet& updatedProperties_) {
				tasks.get(map);
				updatedProperties_.swap(updatedProperties);
			}

			void clear() {
				updatedProperties.clear();
				tasks.clear();
			}
		private:
			PropertyIdSet updatedProperties;
			ItemTasks tasks;
		};

		ViewTasks tasks;

		class IntCollector {
		public:
			enum ValueType {
				TYPE_SORT_PROPERTY,
				TYPE_SORT_ASCENDING,
				TYPE_RANGE_START,
				TYPE_MAX_COUNT,
				TYPE_LAST
			};

			typedef std::map<ValueType, int> ValueMap;

			IntCollector() {
				reset();
			}

			void reset() noexcept {
				for (int i = 0; i < TYPE_LAST; i++) {
					values[static_cast<ValueType>(i)] = -1;
				}
			}

			void set(ValueType aType, int aValue) noexcept {
				changed = true;
				values[aType] = aValue;
			}

			/*void get(ValueType aType) noexcept {
				auto v = values.find(aType);
				if (v != values.end()) {
					return (*v).second;
				}

				return -1;
			}*/

			void set(const ValueMap& aMap) noexcept {
				changed = true;
				for (const auto& i : aMap) {
					values[i.first] = i.second;
				}
			}

			ValueMap getAll() noexcept {
				changed = false;
				return values;
			}

			bool hasChanged() const noexcept {
				return changed;
			}
		private:
			bool changed = true;
			ValueMap values;
		};

		bool itemListChanged = false;
		IntCollector currentValues;

		int prevTotalCount = -1;
		ItemListF itemListF;
		typename IntCollector::ValueMap prevValues;
	};
}

#endif