#include <kernel.h>
#include <appmgr.h>
#include <stdlib.h>
#include <string.h>
#include <libdbg.h>
#include <paf.h>

#include "common.h"
#include "utils.h"
#include "yt_utils.h"
#include "np_utils.h"
#include "dialog.h"
#include "tex_pool.h"
#include "invidious.h"
#include <paf_file_ext.h>
#include "option_menu.h"
#include "menus/menu_generic.h"
#include "menus/menu_youtube.h"
#include "menus/menu_player_youtube.h"
#include "menus/menu_settings.h"

using namespace paf;

menu::YouTube::Submenu::Submenu(YouTube *parentObj)
{
	m_currentPage = 0;
	m_baseParent = parentObj;
	m_interrupted = false;
	m_allJobsComplete = true;
}

menu::YouTube::Submenu::~Submenu()
{
	m_list->SetName(static_cast<uint32_t>(0));
	ReleaseCurrentPage();
	common::transition::DoReverse(0.0f, m_submenuRoot, common::transition::Type_Fadein1, true, false);
}

void menu::YouTube::Submenu::ReleaseCurrentPage()
{
	m_interrupted = true;
	while (!m_allJobsComplete)
	{
		thread::RMutex::main_thread_mutex.Unlock();
		thread::Sleep(10);
		thread::RMutex::main_thread_mutex.Lock();
	}
	m_interrupted = false;

	if (m_list->GetCellNum(0) > 0)
	{
		m_list->DeleteCell(0, 0, m_list->GetCellNum(0));
	}

	m_baseParent->m_texPool->SetAlive(false);
	m_baseParent->m_texPool->AddAsyncWaitComplete();
	m_baseParent->m_texPool->RemoveAll();
	m_baseParent->m_texPool->SetAlive(true);

	m_results.clear();
}

void menu::YouTube::Submenu::OnListButton(ui::Widget *wdg)
{
	uint32_t totalCount = m_results.size();
	uint32_t idhash = wdg->GetName().GetIDHash();

	if (m_currentPage == 0)
	{
		if (idhash == totalCount)
		{
			GoToNextPage();
			return;
		}
	}
	else
	{
		if (idhash == totalCount)
		{
			GoToPrevPage();
			return;
		}
		else if (idhash == totalCount + 1)
		{
			GoToNextPage();
			return;
		}
	}

	Submenu::Item item = m_results.at(idhash);

	switch (item.type)
	{
	case INV_ITEM_TYPE_VIDEO:
		ytutils::GetHistLog()->AddAsync(item.videoId.c_str());
		utils::SetDisplayResolution(ui::EnvironmentParam::RESOLUTION_HD_FULL);
		new menu::PlayerYoutube(item.videoId.c_str(), GetType() == Submenu::SubmenuType_Favourites);
		break;
	}
}

void menu::YouTube::Submenu::OnTexPoolAdd(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	Item *workItem = static_cast<Item *>(userdata);
	ui::Button *button = static_cast<ui::Button *>(self);

	if (e->GetValue(0) == workItem->texId.GetIDHash())
	{
		button->SetTexture(workItem->texPool->Get(workItem->texId));
		button->DeleteEventCallback(ui::Handler::CB_STATE_READY_CACHEIMAGE, OnTexPoolAdd, userdata);
	}
}

ui::ListItem *menu::YouTube::Submenu::CreateListItem(ui::listview::ItemFactory::CreateParam& param)
{
	Plugin::TemplateOpenParam tmpParam;
	ui::Widget *item = NULL;
	ui::Widget *button = NULL;
	ui::Text *timeText = NULL;
	ui::Widget *subText = NULL;
	wstring text16;
	wchar_t numPageText[32];

	if (!param.list_view->GetName().GetIDHash())
	{
		return new ui::ListItem(param.parent, 0);
	}

	ui::Widget *targetRoot = param.parent;
	uint32_t totalCount = m_results.size();

	if (param.cell_index == totalCount || param.cell_index == totalCount + 1)
	{
		g_appPlugin->TemplateOpen(targetRoot, template_list_item_youtube_aligned, tmpParam);
		item = targetRoot->GetChild(targetRoot->GetChildrenNum() - 1);
		button = item->FindChild(image_button_list_item_youtube_aligned);
	}
	else
	{
		g_appPlugin->TemplateOpen(targetRoot, template_list_item_youtube, tmpParam);
		item = targetRoot->GetChild(targetRoot->GetChildrenNum() - 1);
		button = item->FindChild(image_button_list_item_youtube);
	}

	button->SetName(param.cell_index);

	intrusive_ptr<graph::Surface> tex;

	if (param.cell_index == totalCount)
	{
		if (m_currentPage == 0)
		{
			text16 = g_appPlugin->GetString(msg_next_page);
			sce_paf_swprintf(numPageText, sizeof(numPageText) / 2, L" (%d)", m_currentPage + 1);
			text16 += numPageText;
			button->SetString(text16);
			tex = g_appPlugin->GetTexture(tex_button_arrow_right);
			button->SetTexture(tex);
			goto serviceButton;
		}
		else
		{
			text16 = g_appPlugin->GetString(msg_previous_page);
			sce_paf_swprintf(numPageText, sizeof(numPageText) / 2, L" (%d)", m_currentPage - 1);
			text16 += numPageText;
			button->SetString(text16);
			tex = g_appPlugin->GetTexture(tex_button_arrow_left);
			button->SetTexture(tex);
			goto serviceButton;
		}
	}
	else if (param.cell_index == totalCount + 1)
	{
		text16 = g_appPlugin->GetString(msg_next_page);
		sce_paf_swprintf(numPageText, sizeof(numPageText) / 2, L" (%d)", m_currentPage + 1);
		text16 += numPageText;
		button->SetString(text16);
		tex = g_appPlugin->GetTexture(tex_button_arrow_right);
		button->SetTexture(tex);
		goto serviceButton;
	}

	Item *workItem = &m_results.at(param.cell_index);
	timeText = static_cast<ui::Text *>(button->FindChild(text_list_item_youtube_time));
	subText = button->FindChild(text_list_item_youtube_subtext);
	if (workItem->time == L"LIVE")
	{
		math::v4 col(1.0f, 0.0f, 0.0f, 0.5f);
		timeText->SetStyleAttribute(graph::TextStyleAttribute_BackColor, 0, 0, col);
	}
	timeText->SetString(workItem->time);
	subText->SetString(workItem->stat);
	button->SetString(workItem->name);
	if (m_baseParent->m_texPool->Exist(workItem->texId))
	{
		button->SetTexture(m_baseParent->m_texPool->Get(workItem->texId));
	}
	else
	{
		workItem->texPool = m_baseParent->m_texPool;
		button->AddEventCallback(ui::Handler::CB_STATE_READY_CACHEIMAGE, OnTexPoolAdd, workItem);
		m_baseParent->m_texPool->AddAsync(workItem->texId, workItem->texId.GetID().c_str());
	}

serviceButton:

	button->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<Submenu *>(userdata)->OnListButton(static_cast<ui::Widget *>(self));
	}, this);

	return static_cast<ui::ListItem *>(item);
}

void menu::YouTube::SearchSubmenu::Search()
{
	string text8;
	InvItem *items = NULL;
	int32_t ret = -1;
	bool isId = false;

	thread::RMutex::main_thread_mutex.Lock();
	m_baseParent->m_loaderIndicator->Start();
	thread::RMutex::main_thread_mutex.Unlock();

	if ((sce_paf_strstr(m_request.c_str(), "id:") == m_request.c_str()) && m_request.length() == 14)
	{
		isId = true;
	}

	if (!isId)
	{
		InvSort sort;
		InvDate date;
		char region[3];
		sce::AppSettings *settings = menu::Settings::GetAppSetInstance();
		settings->GetInt("yt_search_sort", (int32_t *)&sort, 0);
		settings->GetInt("yt_search_date", (int32_t *)&date, 0);
		region[0] = 0;
		settings->GetString("yt_search_region", region, sizeof(region), "");
		ret = invParseSearch(m_request.c_str(), m_currentPage, INV_ITEM_TYPE_VIDEO, sort, date, region, &items);
	}
	else
	{
		items = new InvItem();
		ret = invParseVideo(m_request.c_str() + 3, &items->videoItem);
		if (!items->videoItem->id)
		{
			invCleanupVideo(items->videoItem);
			ret = -1;
		}
	}

	if (ret <= 0)
	{
		dialog::OpenError(g_appPlugin, ret, Framework::Instance()->GetCommonString("msg_error_connect_server_peer"));
		thread::RMutex::main_thread_mutex.Lock();
		m_baseParent->m_loaderIndicator->Stop();
		thread::RMutex::main_thread_mutex.Unlock();
		m_allJobsComplete = true;
		return;
	}

	for (int i = 0; i < ret; i++)
	{
		Item item;
		item.type = items[i].type;
		switch (item.type)
		{
		case INV_ITEM_TYPE_VIDEO:
			item.videoId = items[i].videoItem->id;
			text8 = "\n";
			text8 += items[i].videoItem->title;
			common::Utf8ToUtf16(text8, &item.name);
			item.texId = items[i].videoItem->thmbUrl;
			if (items[i].videoItem->isLive || items[i].videoItem->lengthSec == 0)
			{
				text8 = "LIVE";
			}
			else
			{
				utils::ConvertSecondsToString(text8, items[i].videoItem->lengthSec, false);
			}
			common::Utf8ToUtf16(text8, &item.time);
			text8 = "by ";
			text8 += items[i].videoItem->author;
			text8 += "\n";
			text8 += items[i].videoItem->published;
			common::Utf8ToUtf16(text8, &item.stat);
			break;
		}

		m_results.push_back(item);
	}

	thread::RMutex::main_thread_mutex.Lock();
	m_baseParent->m_loaderIndicator->Stop();
	if (m_currentPage == 0)
	{
		if (!isId)
		{
			m_list->InsertCell(0, 0, m_results.size() + 1);
		}
		else
		{
			m_list->InsertCell(0, 0, m_results.size());
		}
	}
	else
	{
		m_list->InsertCell(0, 0, m_results.size() + 2);
	}
	thread::RMutex::main_thread_mutex.Unlock();

	if (!isId)
	{
		invCleanupSearch(items);
	}
	else
	{
		invCleanupVideo(items->videoItem);
		delete items;
	}

	m_allJobsComplete = true;
}

void menu::YouTube::SearchSubmenu::OnSearchButton()
{
	wstring text16;
	string text8;

	ReleaseCurrentPage();
	m_searchBox->EndEdit();

	m_request.clear();
	m_searchBox->GetString(text16);
	common::Utf16ToUtf8(text16, &m_request);

	if (m_request.empty())
	{
		return;
	}

	m_currentPage = 0;

	SearchJob *job = new SearchJob(this);
	common::SharedPtr<job::JobItem> itemParam(job);
	m_allJobsComplete = false;
	utils::GetJobQueue()->Enqueue(itemParam);
}

void menu::YouTube::SearchSubmenu::GoToNextPage()
{
	ReleaseCurrentPage();
	m_currentPage++;

	SearchJob *job = new SearchJob(this);
	common::SharedPtr<job::JobItem> itemParam(job);
	m_allJobsComplete = false;
	utils::GetJobQueue()->Enqueue(itemParam);
}

void menu::YouTube::SearchSubmenu::GoToPrevPage()
{
	ReleaseCurrentPage();
	m_currentPage--;

	SearchJob *job = new SearchJob(this);
	common::SharedPtr<job::JobItem> itemParam(job);
	m_allJobsComplete = false;
	utils::GetJobQueue()->Enqueue(itemParam);
}

menu::YouTube::SearchSubmenu::SearchSubmenu(YouTube *parentObj) : Submenu(parentObj)
{
	Plugin::TemplateOpenParam tmpParam;

	g_appPlugin->TemplateOpen(m_baseParent->m_browserRoot, template_list_view_youtube_search, tmpParam);
	m_submenuRoot = m_baseParent->m_browserRoot->FindChild(plane_list_view_youtube_search_root);
	m_list = static_cast<ui::ListView *>(m_submenuRoot->FindChild(list_view_youtube));
	ListViewCb *lwCb = new ListViewCb(this);
	m_list->SetItemFactory(lwCb);
	m_list->InsertSegment(0, 1);
	math::v4 sz(960.0f, 100.0f);
	m_list->SetCellSizeDefault(0, sz);
	m_list->SetSegmentLayoutType(0, ui::ListView::LAYOUT_TYPE_LIST);

	m_searchBox = static_cast<ui::TextBox *>(m_submenuRoot->FindChild(text_box_top_youtube_search));
	m_searchButton = m_submenuRoot->FindChild(button_top_youtube_search);

	m_searchBox->AddEventCallback(ui::TextBox::CB_TEXT_BOX_ENTER_PRESSED,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<SearchSubmenu *>(userdata)->OnSearchButton();
	}, this);

	m_searchButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<SearchSubmenu *>(userdata)->OnSearchButton();
	}, this);

	common::transition::Do(0.0f, m_submenuRoot, common::transition::Type_Fadein1, true, false);
}

menu::YouTube::SearchSubmenu::~SearchSubmenu()
{

}

void menu::YouTube::HistorySubmenu::Parse()
{
	string text8;
	char *entryData;
	int32_t ret = 0;
	InvItemVideo *invItem;
	char key[SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE];

	thread::RMutex::main_thread_mutex.Lock();
	m_baseParent->m_loaderIndicator->Start();
	thread::RMutex::main_thread_mutex.Unlock();

	ytutils::GetHistLog()->UpdateFromTUS();

	ytutils::GetHistLog()->Reset();
	int32_t totalNum = ytutils::GetHistLog()->GetSize();

	while (totalNum > 30)
	{
		ytutils::GetHistLog()->GetNext(key);
		ytutils::GetHistLog()->Remove(key);
		ytutils::GetHistLog()->Reset();
		totalNum = ytutils::GetHistLog()->GetSize();
	}

	entryData = static_cast<char *>(sce_paf_calloc(totalNum, SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE));

	for (int32_t i = 0; i < totalNum; i++)
	{
		ytutils::GetHistLog()->GetNext(entryData + (i * SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE));
	}

	for (int32_t i = totalNum; i > -1; i--)
	{
		if (m_interrupted)
		{
			break;
		}

		ret = invParseVideo(entryData + (i * SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE), &invItem);
		if (ret == true)
		{
			if (invItem->id)
			{
				Item item;
				item.type = INV_ITEM_TYPE_VIDEO;
				item.videoId = invItem->id;
				text8 = "\n";
				text8 += invItem->title;
				common::Utf8ToUtf16(text8, &item.name);
				item.texId = invItem->thmbUrl;
				if (invItem->isLive || invItem->lengthSec == 0)
				{
					text8 = "LIVE";
				}
				else
				{
					utils::ConvertSecondsToString(text8, invItem->lengthSec, false);
				}
				common::Utf8ToUtf16(text8, &item.time);
				text8 = "by ";
				text8 += invItem->author;
				text8 += "\n";
				text8 += invItem->published;
				common::Utf8ToUtf16(text8, &item.stat);

				m_results.push_back(item);
			}

			invCleanupVideo(invItem);
		}
	}

	thread::RMutex::main_thread_mutex.Lock();
	m_baseParent->m_loaderIndicator->Stop();
	if (!m_interrupted)
	{
		m_list->InsertCell(0, 0, m_results.size());
	}
	thread::RMutex::main_thread_mutex.Unlock();

	sce_paf_free(entryData);

	m_allJobsComplete = true;
}

void menu::YouTube::HistorySubmenu::GoToNextPage()
{

}

void menu::YouTube::HistorySubmenu::GoToPrevPage()
{

}

menu::YouTube::HistorySubmenu::HistorySubmenu(YouTube *parentObj) : Submenu(parentObj)
{
	Plugin::TemplateOpenParam tmpParam;

	g_appPlugin->TemplateOpen(m_baseParent->m_browserRoot, template_list_view_youtube_history, tmpParam);
	m_submenuRoot = m_baseParent->m_browserRoot->FindChild(plane_list_view_youtube_history_root);
	m_list = static_cast<ui::ListView *>(m_submenuRoot->FindChild(list_view_youtube));
	ListViewCb *lwCb = new ListViewCb(this);
	m_list->SetItemFactory(lwCb);
	m_list->InsertSegment(0, 1);
	math::v4 sz(960.0f, 100.0f);
	m_list->SetCellSizeDefault(0, sz);
	m_list->SetSegmentLayoutType(0, ui::ListView::LAYOUT_TYPE_LIST);

	wstring title = g_appPlugin->GetString(msg_youtube_history);
	m_baseParent->m_topText->SetString(title);

	common::transition::Do(0.0f, m_submenuRoot, common::transition::Type_Fadein1, true, false);

	HistoryJob *job = new HistoryJob(this);
	common::SharedPtr<job::JobItem> itemParam(job);
	m_allJobsComplete = false;
	utils::GetJobQueue()->Enqueue(itemParam);
}

menu::YouTube::HistorySubmenu::~HistorySubmenu()
{
	wstring title;
	m_baseParent->m_topText->SetString(title);
}

void menu::YouTube::FavouriteSubmenu::Parse()
{
	string text8;
	char *entryData;
	int32_t ret = 0;
	InvItemVideo *invItem = NULL;
	bool isLastPage = false;

	thread::RMutex::main_thread_mutex.Lock();
	m_baseParent->m_loaderIndicator->Start();
	thread::RMutex::main_thread_mutex.Unlock();

	ytutils::GetFavLog()->UpdateFromTUS();

	ytutils::GetFavLog()->Reset();
	int32_t totalNum = ytutils::GetFavLog()->GetSize();

	if (!m_request.empty())
	{
		isLastPage = true;
		char key[SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE];

		for (int32_t i = 0; i < totalNum; i++)
		{
			if (m_interrupted)
			{
				break;
			}

			ytutils::GetFavLog()->GetNext(key);
			ret = invParseVideo(key, &invItem);
			if (ret == true)
			{
				if (invItem->id && sce_paf_strstr(invItem->title, m_request.c_str()) && m_results.size() < 30)
				{
					Item item;
					item.type = INV_ITEM_TYPE_VIDEO;
					item.videoId = invItem->id;
					text8 = "\n";
					text8 += invItem->title;
					common::Utf8ToUtf16(text8, &item.name);
					item.texId = invItem->thmbUrl;
					if (invItem->isLive || invItem->lengthSec == 0)
					{
						text8 = "LIVE";
					}
					else
					{
						utils::ConvertSecondsToString(text8, invItem->lengthSec, false);
					}
					common::Utf8ToUtf16(text8, &item.time);
					text8 = "by ";
					text8 += invItem->author;
					text8 += "\n";
					text8 += invItem->published;
					common::Utf8ToUtf16(text8, &item.stat);

					m_results.push_back(item);
				}

				invCleanupVideo(invItem);
			}
		}
	}
	else
	{
		int32_t startNum = m_currentPage * 30;

		int32_t realNum = 30;
		if ((totalNum - startNum) < 30)
		{
			realNum = totalNum - startNum;
			isLastPage = true;
		}

		entryData = (char *)sce_paf_calloc(realNum, SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE);

		for (int32_t i = 0; i < startNum; i++)
		{
			ytutils::GetFavLog()->GetNext(entryData);
		}

		for (int32_t i = 0; i < realNum; i++)
		{
			ytutils::GetFavLog()->GetNext(entryData + (i * SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE));
		}

		for (int32_t i = 0; i < realNum; i++)
		{
			if (m_interrupted)
			{
				break;
			}

			ret = invParseVideo(entryData + (i * SCE_INI_FILE_PROCESSOR_KEY_BUFFER_SIZE), &invItem);
			if (ret == true)
			{
				if (invItem->id)
				{
					Item item;
					item.type = INV_ITEM_TYPE_VIDEO;
					item.videoId = invItem->id;
					text8 = "\n";
					text8 += invItem->title;
					common::Utf8ToUtf16(text8, &item.name);
					item.texId = invItem->thmbUrl;
					if (invItem->isLive || invItem->lengthSec == 0)
					{
						text8 = "LIVE";
					}
					else
					{
						utils::ConvertSecondsToString(text8, invItem->lengthSec, false);
					}
					common::Utf8ToUtf16(text8, &item.time);
					text8 = "by ";
					text8 += invItem->author;
					text8 += "\n";
					text8 += invItem->published;
					common::Utf8ToUtf16(text8, &item.stat);
					m_results.push_back(item);
				}

				invCleanupVideo(invItem);
			}
		}

		sce_paf_free(entryData);
	}

	thread::RMutex::main_thread_mutex.Lock();
	m_baseParent->m_loaderIndicator->Stop();
	if (!m_interrupted)
	{
		if (m_currentPage == 0)
		{
			if (!isLastPage)
			{
				m_list->InsertCell(0, 0, m_results.size() + 1);
			}
			else
			{
				m_list->InsertCell(0, 0, m_results.size());
			}
		}
		else
		{
			if (!isLastPage)
			{
				m_list->InsertCell(0, 0, m_results.size() + 2);
			}
			else
			{
				m_list->InsertCell(0, 0, m_results.size() + 1);
			}
		}
	}
	thread::RMutex::main_thread_mutex.Unlock();

	m_allJobsComplete = true;
}

void menu::YouTube::FavouriteSubmenu::OnSearchButton()
{
	wstring text16;
	string text8;

	ReleaseCurrentPage();
	m_searchBox->EndEdit();

	m_request.clear();
	m_searchBox->GetString(text16);
	common::Utf16ToUtf8(text16, &m_request);

	if (m_request.empty() || ytutils::GetFavLog()->GetSize() == 0)
	{
		return;
	}

	FavouriteJob *job = new FavouriteJob(this);
	common::SharedPtr<job::JobItem> itemParam(job);
	m_allJobsComplete = false;
	utils::GetJobQueue()->Enqueue(itemParam);
}

void menu::YouTube::FavouriteSubmenu::GoToNextPage()
{
	ReleaseCurrentPage();
	m_currentPage++;

	FavouriteJob *job = new FavouriteJob(this);
	common::SharedPtr<job::JobItem> itemParam(job);
	m_allJobsComplete = false;
	utils::GetJobQueue()->Enqueue(itemParam);
}

void menu::YouTube::FavouriteSubmenu::GoToPrevPage()
{
	ReleaseCurrentPage();
	m_currentPage--;

	FavouriteJob *job = new FavouriteJob(this);
	common::SharedPtr<job::JobItem> itemParam(job);
	m_allJobsComplete = false;
	utils::GetJobQueue()->Enqueue(itemParam);
}

menu::YouTube::FavouriteSubmenu::FavouriteSubmenu(YouTube *parentObj) : Submenu(parentObj)
{
	Plugin::TemplateOpenParam tmpParam;

	g_appPlugin->TemplateOpen(m_baseParent->m_browserRoot, template_list_view_youtube_fav, tmpParam);
	m_submenuRoot = m_baseParent->m_browserRoot->FindChild(plane_list_view_youtube_fav_root);
	m_list = static_cast<ui::ListView *>(m_submenuRoot->FindChild(list_view_youtube));
	ListViewCb *lwCb = new ListViewCb(this);
	m_list->SetItemFactory(lwCb);
	m_list->InsertSegment(0, 1);
	math::v4 sz(960.0f, 100.0f);
	m_list->SetCellSizeDefault(0, sz);
	m_list->SetSegmentLayoutType(0, ui::ListView::LAYOUT_TYPE_LIST);

	m_searchBox = static_cast<ui::TextBox *>(m_submenuRoot->FindChild(text_box_top_youtube_search));
	m_searchButton = m_submenuRoot->FindChild(button_top_youtube_search);

	m_searchBox->AddEventCallback(ui::TextBox::CB_TEXT_BOX_ENTER_PRESSED,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<FavouriteSubmenu *>(userdata)->OnSearchButton();
	}, this);

	m_searchButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<FavouriteSubmenu *>(userdata)->OnSearchButton();
	}, this);

	common::transition::Do(0.0f, m_submenuRoot, common::transition::Type_Fadein1, true, false);

	FavouriteJob *job = new FavouriteJob(this);
	common::SharedPtr<job::JobItem> itemParam(job);
	m_allJobsComplete = false;
	utils::GetJobQueue()->Enqueue(itemParam);
}

menu::YouTube::FavouriteSubmenu::~FavouriteSubmenu()
{

}

void menu::YouTube::OnSettingsButton()
{
	vector<OptionMenu::Button> buttons;
	OptionMenu::Button bt;
	bt.label = g_appPlugin->GetString(msg_settings);
	buttons.push_back(bt);
	bt.label = g_appPlugin->GetString(msg_settings_youtube_clean_history);
	buttons.push_back(bt);
	bt.label = g_appPlugin->GetString(msg_settings_youtube_clean_fav);
	buttons.push_back(bt);

	new OptionMenu(g_appPlugin, m_root, &buttons);
}

void menu::YouTube::OnOptionMenuEvent(int32_t type, int32_t subtype)
{
	if (menu::GetTopMenu()->GetMenuType() != menu::MenuType_Youtube)
	{
		return;
	}

	if (type == OptionMenu::OptionMenuEvent_Close)
	{
		return;
	}

	switch (subtype)
	{
	case 0:
		menu::SettingsButtonCbFun(ui::Button::CB_BTN_DECIDE, NULL, 0, NULL);
		break;
	case 1:
		dialog::OpenYesNo(g_appPlugin, NULL, g_appPlugin->GetString(msg_settings_youtube_clean_history_confirm));
		m_dialogIdx = subtype;
		break;
	case 2:
		dialog::OpenYesNo(g_appPlugin, NULL, g_appPlugin->GetString(msg_settings_youtube_clean_fav_confirm));
		m_dialogIdx = subtype;
		break;
	}
}

void menu::YouTube::OnDialogEvent(int32_t type)
{
	if (menu::GetTopMenu()->GetMenuType() != menu::MenuType_Youtube)
	{
		return;
	}

	if (type == dialog::ButtonCode_Yes)
	{
		LogClearJob::Type clearType = LogClearJob::Type_Hist;
		switch (m_dialogIdx)
		{
		case 1:
			clearType = LogClearJob::Type_Hist;
			break;
		case 2:
			clearType = LogClearJob::Type_Fav;
			break;
		}

		LogClearJob *job = new LogClearJob(clearType);
		common::SharedPtr<job::JobItem> itemParam(job);
		job::JobQueue::default_queue->Enqueue(itemParam);
	}
}

void menu::YouTube::OnSettingsEvent(int32_t type)
{
	if (type == Settings::SettingsEvent_ValueChange)
	{
		// Bad solution but it shouldn't be noticable by the user
		char instance[256];
		sce_paf_memset(instance, 0, sizeof(instance));
		menu::Settings::GetAppSetInstance()->GetString("inv_instance", instance, sizeof(instance), "");
		invSetInstanceUrl(instance);
	}
}

void menu::YouTube::OnBackButton()
{
	delete this;
}

void menu::YouTube::OnSubmenuButton(ui::Widget *wdg)
{
	SwitchSubmenu(static_cast<Submenu::SubmenuType>(wdg->GetName().GetIDHash()));
}

void menu::YouTube::LogClearJob::Run()
{
	dialog::OpenPleaseWait(g_appPlugin, NULL, Framework::Instance()->GetCommonString("msg_wait"));

	switch (m_type)
	{
	case Type_Hist:
		ytutils::HistLog::Clean();
		break;
	case Type_Fav:
		ytutils::FavLog::Clean();
		break;
	}

	dialog::Close();
}

menu::YouTube::YouTube() :
	GenericMenu("page_youtube",
	MenuOpenParam(false, 200.0f, Plugin::TransitionType_SlideFromBottom),
	MenuCloseParam(false, 200.0f, Plugin::TransitionType_SlideFromBottom))
{
	m_currentSubmenu = NULL;

	m_root->AddEventCallback(OptionMenu::OptionMenuEvent,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<YouTube *>(userdata)->OnOptionMenuEvent(e->GetValue(0), e->GetValue(1));
	}, this);

	m_root->AddEventCallback(dialog::DialogEvent,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<YouTube *>(userdata)->OnDialogEvent(e->GetValue(0));
	}, this);

	m_root->AddEventCallback(Settings::SettingsEvent,
		[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<YouTube *>(userdata)->OnSettingsEvent(e->GetValue(0));
	}, this);

	ui::Widget *settingsButton = m_root->FindChild(button_settings_page_youtube);
	settingsButton->Show(common::transition::Type_Reset);
	settingsButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<YouTube *>(userdata)->OnSettingsButton();
	}, this);

	ui::Widget *backButton = m_root->FindChild(button_back_page_youtube);
	backButton->Show(common::transition::Type_Reset);
	backButton->AddEventCallback(ui::Button::CB_BTN_DECIDE,
	[](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<YouTube *>(userdata)->OnBackButton();
	}, this);

	m_browserRoot = m_root->FindChild(plane_browser_root_page_youtube);
	m_loaderIndicator = static_cast<ui::BusyIndicator *>(m_root->FindChild(busyindicator_loader_page_youtube));
	m_topText = static_cast<ui::Text *>(m_root->FindChild(text_top));
	m_btMenu = static_cast<ui::Box *>(m_root->FindChild(box_bottommenu_page_youtube));

	m_searchBt = m_btMenu->FindChild(button_yt_btmenu_search);
	m_histBt = m_btMenu->FindChild(button_yt_btmenu_history);
	m_favBt = m_btMenu->FindChild(button_yt_btmenu_favourite);

	auto submenuButtonCb = [](int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
	{
		reinterpret_cast<YouTube *>(userdata)->OnSubmenuButton(static_cast<ui::Widget *>(self));
	};
	m_searchBt->AddEventCallback(ui::Button::CB_BTN_DECIDE, submenuButtonCb, this);
	m_histBt->AddEventCallback(ui::Button::CB_BTN_DECIDE, submenuButtonCb, this);
	m_favBt->AddEventCallback(ui::Button::CB_BTN_DECIDE, submenuButtonCb, this);

	char instance[256];
	sce_paf_memset(instance, 0, sizeof(instance));
	menu::Settings::GetAppSetInstance()->GetString("inv_instance", instance, sizeof(instance), "");
	invSetInstanceUrl(instance);

	m_texPool = new TexPool(g_appPlugin);
	m_texPool->SetShare(utils::GetShare());

	SwitchSubmenu(Submenu::SubmenuType_Search);
}

menu::YouTube::~YouTube()
{
	delete m_currentSubmenu;
	m_texPool->DestroyAsync();
}

void menu::YouTube::SwitchSubmenu(Submenu::SubmenuType type)
{
	if (m_currentSubmenu)
	{
		if (m_currentSubmenu->GetType() == type)
		{
			return;
		}
		delete m_currentSubmenu;
		m_currentSubmenu = NULL;
	}

	switch (type)
	{
	case Submenu::SubmenuType_Search:
		m_currentSubmenu = new SearchSubmenu(this);
		break;
	case Submenu::SubmenuType_History:
		m_currentSubmenu = new HistorySubmenu(this);
		break;
	case Submenu::SubmenuType_Favourites:
		m_currentSubmenu = new FavouriteSubmenu(this);
		break;
	}
}