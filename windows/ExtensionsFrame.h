/*
*
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

#ifndef EXTENSIONS_FRAME
#define EXTENSIONS_FRAME

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "FlatTabCtrl.h"
#include "TypedListViewCtrl.h"

#include <web-server/WebServerManager.h>
#include <web-server/ExtensionManager.h>

class ExtensionsFrame : public MDITabChildWindowImpl<ExtensionsFrame>, public StaticFrame<ExtensionsFrame, ResourceManager::SETTINGS_EXTENSIONS, IDC_EXTENSIONS>,
	private SettingsManagerListener, private webserver::ExtensionManagerListener, private Async<ExtensionsFrame>
{
public:
	typedef MDITabChildWindowImpl<ExtensionsFrame> baseClass;

	ExtensionsFrame();
	~ExtensionsFrame() { };

	DECLARE_FRAME_WND_CLASS_EX(_T("ExtensionsFrame"), IDR_EXTENSIONS, 0, COLOR_3DFACE);


	BEGIN_MSG_MAP(ExtensionsFrame)
		NOTIFY_HANDLER(IDC_EXTENSIONS_LIST, LVN_GETDISPINFO, ctrlList.onGetDispInfo)
		NOTIFY_HANDLER(IDC_EXTENSIONS_LIST, LVN_COLUMNCLICK, ctrlList.onColumnClick)
		NOTIFY_HANDLER(IDC_EXTENSIONS_LIST, LVN_GETINFOTIP, ctrlList.onInfoTip)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		CHAIN_MSG_MAP(baseClass)
	END_MSG_MAP()

	LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
	void UpdateLayout(BOOL bResizeBars = TRUE);

	LRESULT onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL & /*bHandled*/);
	LRESULT onSetFocus(UINT /* uMsg */, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/);

	void createColumns();

	static string id;
private:
	class ItemInfo;
	TypedListViewCtrl<ItemInfo, IDC_EXTENSIONS_LIST> ctrlList;

	class ItemInfo {
	public:
		ItemInfo(const webserver::ExtensionPtr& aExtension) : item(aExtension) { }
		~ItemInfo() { }

		const tstring getText(int col) const;

		static int compareItems(const ItemInfo* a, const ItemInfo* b, int col) {
			return Util::DefaultSort(a->getText(col).c_str(), b->getText(col).c_str());
		
		}

		int getImageIndex() const { return 0; }

		webserver::ExtensionPtr item;

	};

	typedef vector<unique_ptr<ItemInfo>> ItemInfoList;
	ItemInfoList itemInfos;

	enum {
		COLUMN_FIRST,
		COLUMN_NAME = COLUMN_FIRST,
		COLUMN_DESCRIPTION,
		COLUMN_LAST
	};

	bool closed;

	static int columnSizes[COLUMN_LAST];
	static int columnIndexes[COLUMN_LAST];

	void updateList();
	void addEntry(ItemInfo* ii);

	CStatusBarCtrl ctrlStatus;
	int statusSizes[2];

	void on(SettingsManagerListener::Save, SimpleXML& /*xml*/) noexcept override;
	void on(webserver::ExtensionManagerListener::ExtensionAdded, const webserver::ExtensionPtr& e) noexcept;
	void on(webserver::ExtensionManagerListener::ExtensionRemoved, const webserver::ExtensionPtr& e) noexcept;

};
#endif