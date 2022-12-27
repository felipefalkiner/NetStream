#ifndef _MENU_LOCAL_H_
#define _MENU_LOCAL_H_

#include <kernel.h>
#include <paf.h>

#include "dialog.h"
#include "menu_generic.h"
#include "local_server_browser.h"
#include "menus/menu_player_simple.h"

using namespace paf;

namespace menu {
	class Local : public GenericMenu
	{
	public:

		static SceVoid PlayerBackCb(PlayerSimple *player, ScePVoid pUserArg);
		static SceVoid PlayerFailCb(PlayerSimple *player, ScePVoid pUserArg);
		static SceVoid BackButtonCbFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);
		static SceVoid ListButtonCbFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

		class ListViewCb : public ui::ListView::ItemCallback
		{
		public:

			~ListViewCb()
			{

			}

			ui::ListItem *Create(Param *info);

			SceVoid Start(Param *info)
			{
				info->parent->PlayEffect(0.0f, effect::EffectType_Popup1);
			}

			Local *workObj;
		};

		class GoToJob : public job::JobItem
		{
		public:

			using job::JobItem::JobItem;

			~GoToJob() {}

			SceVoid Run();

			SceVoid Finish() {}

			static SceVoid ConnectionFailedDialogHandler(dialog::ButtonCode buttonCode, ScePVoid pUserArg);

			Local *workObj;
			string targetRef;
		};

		class BrowserPage
		{
		public:

			BrowserPage() : isLoaded(SCE_FALSE)
			{

			}

			~BrowserPage()
			{

			}

			vector<LocalServerBrowser::Entry *> *itemList;
			ui::ListView *list;
			SceBool isLoaded;
		};

		Local();

		~Local();

		MenuType GetMenuType()
		{
			return MenuType_Http;
		}

		const SceUInt32 *GetSupportedSettingsItems(SceInt32 *count)
		{
			*count = sizeof(k_settingsIdList) / sizeof(char*);
			return k_settingsIdList;
		}

		SceBool PushBrowserPage(string *ref);

		SceBool PopBrowserPage();

		LocalServerBrowser *browser;
		ui::Widget *browserRoot;
		ui::BusyIndicator *loaderIndicator;
		ui::Text *topText;
		vector<BrowserPage *> pageList;
		SceBool firstBoot;

	private:

		const SceUInt32 k_settingsIdList[1] = {
			http_setting
		};
	};
}

#endif