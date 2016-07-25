/*
* Copyright (C) 2012-2016 AirDC++ Project
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

#include "stdafx.h"
#include "Resource.h"
#include "WinUtil.h"
#include "RssConfigDlg.h"
#include "BrowseDlg.h"

#include <airdcpp/StringTokenizer.h>
#include <airdcpp/ResourceManager.h>

#define setMinMax(x, y, z) \
	updown.Attach(GetDlgItem(x)); \
	updown.SetRange32(y, z); \
	updown.Detach();

#define ATTACH(id, var) var.Attach(GetDlgItem(id))

RssDlg::RssDlg() { 
	loading = true;
}

RssDlg::~RssDlg() {

}

LRESULT RssDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {

	ATTACH(IDC_RSS_URL, ctrlUrl);
	::SetWindowText(GetDlgItem(IDC_RSS_URL_TEXT), CTSTRING(LINK));

	ATTACH(IDC_RSS_NAME, ctrlCategorie);
	::SetWindowText(GetDlgItem(IDC_RSS_NAME_TEXT), CTSTRING(CATEGORIES));

	ATTACH(IDC_RSS_AUTOSEARCH, ctrlAutoSearchPattern);
	::SetWindowText(GetDlgItem(IDC_RSS_AUTOSEARCH_TEXT), CTSTRING(RSS_MATCH_PATTERN));

	ATTACH(IDC_RSS_DOWNLOAD_PATH, ctrlTarget);
	::SetWindowText(GetDlgItem(IDC_RSS_DOWNLOAD_PATH_TEXT), CTSTRING(DOWNLOAD_TO));

	ATTACH(IDC_RSS_INTERVAL, ctrlInterval);
	::SetWindowText(GetDlgItem(IDC_INTERVAL_TEXT), CTSTRING(MINIMUM_UPDATE_INTERVAL_MIN));
	setMinMax(IDC_RSS_INT_SPIN, 10, 999);

	ATTACH(IDC_RSS_LIST, ctrlRssList);

	CRect rc;
	ctrlRssList.GetClientRect(rc);
	//ctrlRssList.SetExtendedListViewStyle(LVS_EX_LABELTIP | LVS_EX_FULLROWSELECT);
	ctrlRssList.InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, (rc.Width() - 17), 0);

	::SetWindowText(GetDlgItem(IDCANCEL), CTSTRING(CANCEL));
	::SetWindowText(GetDlgItem(IDC_RSS_REMOVE), CTSTRING(REMOVE));
	::SetWindowText(GetDlgItem(IDC_RSS_ADD), CTSTRING(ADD));
	::SetWindowText(GetDlgItem(IDC_RSS_GROUP_TEXT), CTSTRING(RSS_CONFIG));
	::SetWindowText(GetDlgItem(IDC_RSS_UPDATE), CTSTRING(UPDATE));

	::EnableWindow(GetDlgItem(IDC_RSS_REMOVE), false);
	::EnableWindow(GetDlgItem(IDC_RSS_UPDATE), false);

	auto tmpList = RSSManager::getInstance()->getRss();
	for (auto i : tmpList)
		rssList.emplace_back(i);

	loading = false;
	fillList();

	ctrlRssList.SelectItem(0);
	CenterWindow(GetParent());
	SetWindowText(CTSTRING(RSS_CONFIG));

	return TRUE;
}

LRESULT RssDlg::onSelectionChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	if (loading)
		return 0;

	NM_LISTVIEW* lv = (NM_LISTVIEW*)pnmh;
	::EnableWindow(GetDlgItem(IDC_RSS_REMOVE), (lv->uNewState & LVIS_FOCUSED));
	::EnableWindow(GetDlgItem(IDC_RSS_UPDATE), (lv->uNewState & LVIS_FOCUSED));
	loading = true;
	if (ctrlRssList.GetSelectedCount() == 1) {
		int i = ctrlRssList.GetSelectedIndex();
		auto item = rssList.begin() + i;
		ctrlUrl.SetWindowText(Text::toT(item->getUrl()).c_str());
		ctrlCategorie.SetWindowText(Text::toT(item->getCategories()).c_str());
		ctrlAutoSearchPattern.SetWindowText(Text::toT(item->getAutoSearchFilter()).c_str());
		ctrlTarget.SetWindowText(Text::toT(item->getDownloadTarget()).c_str());
		ctrlInterval.SetWindowText(Util::toStringW(item->getUpdateInterval()).c_str());
	} else {
		ctrlUrl.SetWindowText(_T(""));
		ctrlCategorie.SetWindowText(_T(""));
		ctrlAutoSearchPattern.SetWindowText(_T(""));
		ctrlTarget.SetWindowText(_T(""));
		ctrlInterval.SetWindowText(_T("30"));
	}
	loading = false;

	return 0;
}

LRESULT RssDlg::onIntervalChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL & /*bHandled*/) {
	if (loading)
		return 0;

	int value = Util::toInt(Text::fromT(WinUtil::getEditText(ctrlInterval)));
	if (value == 0) {
		value = 30; // use 30 minutes as default interval
	}
	else if (value < 10) {
		value = 10;
	}
	ctrlInterval.SetWindowText(Text::toT(Util::toString(value)).c_str());
	return 0;
}

LRESULT RssDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	if (wID == IDOK) {
		update();

		for (auto url : removeList)
			RSSManager::getInstance()->removeFeedItem(url);

		for(auto i : rssList)
			RSSManager::getInstance()->updateFeedItem(i.getUrl(), i.getCategories(), i.getAutoSearchFilter(), 
				i.getDownloadTarget(), i.getUpdateInterval());
	}
	ctrlRssList.Detach();
	EndDialog(wID);
	return 0;
}

LRESULT RssDlg::onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	add();
	return 0;
}

LRESULT RssDlg::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	remove();
	return 0;
}

LRESULT RssDlg::onUpdate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	update();
	return 0;
}

LRESULT RssDlg::onBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {

	tstring dir = Text::toT(SETTING(DOWNLOAD_DIRECTORY));

	BrowseDlg dlg(m_hWnd, BrowseDlg::TYPE_GENERIC, BrowseDlg::DIALOG_SELECT_FOLDER);
	dlg.setPath(dir);
	if (dlg.show(dir)) {
		SetDlgItemText(IDC_RSS_DOWNLOAD_PATH, dir.c_str());
	}

	return 0;
}

void RssDlg::fillList() {

	sort(rssList.begin(), rssList.end(), [](const RSSConfigItem& a, const RSSConfigItem& b) { return compare(a.getCategories(), b.getCategories()) < 0; });
	ctrlRssList.DeleteAllItems();
	int pos = 0;
	for (auto& i : rssList) {
		ctrlRssList.InsertItem(pos++, Text::toT(i.getCategories()).c_str());
	}
}

void RssDlg::remove() {
	if (ctrlRssList.GetSelectedCount() == 1) {
		//TODO ask to remove data also.
		int i = ctrlRssList.GetSelectedIndex();
		ctrlRssList.DeleteItem(i);
		auto r = *rssList.erase(rssList.begin() + i);
		removeList.push_back(r.getUrl());
	}
}

bool RssDlg::add() {
	auto url = WinUtil::getEditText(ctrlUrl);
	auto categorie = WinUtil::getEditText(ctrlCategorie);
	if (url.empty() || categorie.empty()) {
		MessageBox(_T("URL and Name / Categorie must not be empty"));
		return false;
	}
	bool exists = find_if(rssList.begin(), rssList.end(), [url, categorie](const RSSConfigItem& item) 
	{ return Text::fromT(url) == item.getUrl() || Text::fromT(categorie) == item.getCategories(); }) != rssList.end();

	if (exists) {
		MessageBox(_T("An item with the same URL or Name / Categorie already exists"));
		return false;
	}

	auto asPattern = Text::fromT(WinUtil::getEditText(ctrlAutoSearchPattern));
	auto dlTarget = Text::fromT(WinUtil::getEditText(ctrlTarget));
	auto updateInt = Util::toInt(Text::fromT(WinUtil::getEditText(ctrlInterval)));

	rssList.emplace_back(RSSConfigItem(Text::fromT(url), Text::fromT(categorie), asPattern, dlTarget, updateInt));
	fillList();
	//Select the new item
	restoreSelection(categorie);

	return true;
}
void RssDlg::update() {
	if (ctrlRssList.GetSelectedCount() == 1) {
		int i = ctrlRssList.GetSelectedIndex();
		auto item = rssList[i];
		rssList.erase(rssList.begin() + i);
		if (!add()) { //Failed, restore the old item
			rssList.push_back(item);
			fillList();
			restoreSelection(Text::toT(item.getCategories()));
		}
	}
}

void RssDlg::restoreSelection(const tstring& curSel) {
	if (!curSel.empty()) {
		int i = ctrlRssList.find(curSel);
		if (i != -1)
			ctrlRssList.SelectItem(i);
	}
}