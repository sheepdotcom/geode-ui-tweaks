#include "Geode/loader/Event.hpp"
#include "Geode/loader/Loader.hpp"
#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/ColorProvider.hpp>

using namespace geode::prelude;

CCNode* modsLayerReference = nullptr;

bool isGeodeTheme(bool forceDisableTheme = false) {
	return !forceDisableTheme && Loader::get()->getInstalledMod("geode.loader")->getSettingValue<bool>("enable-geode-theme");
}

// Soooo about the indentations, uh nullptr checking i guess? dont want game to randomly crash!
// Also getting certain things might be wierd but like i dont wanna make hacky way around a private class and private members

void modsLayerModify(CCNode* modsLayer) {
	auto mod = Mod::get();
	if (auto modListFrame = modsLayer->getChildByID("mod-list-frame")) {
		if (auto modList = modListFrame->getChildByID("ModList")) {
			auto searchMenu = modList->getChildByID("top-container")->getChildByID("search-menu");
			auto searchBG = searchMenu->getChildByID("search-id");
			auto frameBG = modListFrame->getChildByID("frame-bg");

			if (!searchMenu->getChildByID("search-bg"_spr)) {
				auto searchInput = searchMenu->getChildByID("search-input");
				auto searchFiltersMenu = searchMenu->getChildByID("search-filters-menu");

				auto someBG = CCLayerColor::create(ccColor4B{});
				someBG->setPosition(searchMenu->getScaledContentSize() / 2);
				someBG->ignoreAnchorPointForPosition(false);
				someBG->setScale(0.7f);
				someBG->setContentSize(ccp(350.f, 30.f) / someBG->getScale());
				someBG->setColor(ccColor3B{0, 0, 0});
				someBG->setOpacity(isGeodeTheme() ? 50 : 90);
				someBG->setID("search-bg"_spr);

				searchMenu->addChild(someBG);
			}

			auto someBG = searchMenu->getChildByID("search-bg"_spr);

			frameBG->setVisible(!mod->getSettingValue<bool>("transparent-lists"));
			searchBG->setVisible(!mod->getSettingValue<bool>("transparent-lists"));
			someBG->setVisible(mod->getSettingValue<bool>("transparent-lists"));

			if (auto scrollLayer = static_cast<ScrollLayer*>(modList->getChildByID("ScrollLayer"))) {
				CCObject* obj;
				CCARRAY_FOREACH(scrollLayer->m_contentLayer->getChildren(), obj) {
					// should be mod items right
					auto node = static_cast<CCNode*>(obj);
					if (node->getID() == "ModItem") {
						if (auto bg = static_cast<CCScale9Sprite*>(node->getChildByID("bg"))) {
							if (mod->getSettingValue<bool>("transparent-lists")) {
								auto rgb = bg->getColor();
								auto color = ccColor4B{rgb.b, rgb.g, rgb.b, bg->getOpacity()};
								if (color == ccColor4B{255, 255, 255, 25} || color == ccColor4B{0, 0, 0, 90}) { // Enabled
									bg->setOpacity(isGeodeTheme() ? 50 : 90);
									bg->setColor(ccColor3B{0, 0, 0});
								}
								else if (color == ccColor4B{153, 245, 245, 25}) { // Restart Required
									bg->setOpacity(90);
									bg->setColor(ccColor3B{123, 156, 163});
								}
								else if (color == ccColor4B{255, 255, 255, 10}) { // Disabled
									bg->setOpacity(50);
									bg->setColor(ccColor3B{205, 205, 205});
								}
								else if (color == ccColor4B{235, 35, 112, 25}) { // Error
									bg->setOpacity(90);
									bg->setColor(ccColor3B{245, 27, 27});
								}
								else if (color == ccColor4B{245, 153, 245, 25}) { // Outdated
									bg->setOpacity(90);
								}
								else if (color == ccColor4B{240, 211, 42, 65}) { // Featured
									bg->setOpacity(90);
								}
								else if (color == ccColor4B{63, 91, 138, 85}) { // Modtober
									bg->setOpacity(90);
									bg->setColor(ccColor3B{32, 102, 220});
								}
							}
						}
						if (auto logoSprite = node->getChildByID("logo-sprite")) {
							if (mod->getSettingValue<bool>("larger-logos") && (logoSprite->getScale() == 0.4f || logoSprite->getScale() == 0.6f)) {
								logoSprite->setScale(logoSprite->getScale() == 0.6f ? 0.7f : 0.5f);
							} else if (!mod->getSettingValue<bool>("larger-logos") && (logoSprite->getScale() == 0.5f || logoSprite->getScale() == 0.7f)) {
								logoSprite->setScale(logoSprite->getScale() == 0.7f ? 0.6f : 0.4f);
							}
						}
						if (auto infoContainer = node->getChildByID("info-container")) {
							if (mod->getSettingValue<bool>("fix-mod-info-size")) {
								auto titleContainer = infoContainer->querySelector("title-container");
								auto developersMenu = infoContainer->querySelector("developers-menu");
								if (titleContainer && developersMenu) {
									infoContainer->setContentWidth(525.f);
									infoContainer->updateLayout();
									titleContainer->setContentWidth(525.f);
									titleContainer->updateLayout();
									developersMenu->setContentWidth(525.f);
									developersMenu->updateLayout();
								}
							}
						}
					}
				}
			}
		}
	}
}

#include <Geode/modify/CCLayer.hpp>

class ModsLayer : public CCNode {};

class $modify(CustomModsLayer, CCLayer) {
	bool init() {
		auto winSize = CCDirector::sharedDirector()->getWinSize();

		if (!CCLayer::init()) return false;

		if (auto modslayer = typeinfo_cast<ModsLayer*>(this)) {
			queueInMainThread([this] {
				modsLayerReference = this;
				modsLayerModify(this);
			});
		}

		return true;
	}
};

#include <Geode/modify/CCDirector.hpp>

class $modify(CCDirector) {
	bool replaceScene(CCScene* pScene) {
		// No crash when ModsLayer no longer exists
		if (CCDirector::sharedDirector()->getRunningScene()->getChildByID("ModsLayer")) {
			if (modsLayerReference) {
				modsLayerReference = nullptr;
			}
		}

		return CCDirector::replaceScene(pScene);
	}
};

#include <Geode/modify/CCScheduler.hpp>

class $modify(CCScheduler) {
	void update(float dt) {
		if (modsLayerReference) {
			modsLayerModify(modsLayerReference);
		}
		CCScheduler::update(dt);
	}
};

// Imagine using geode's little ui events (they also dont work for my problems hehe)

/*$execute {
	new EventListener<EventFilter<ModItemUIEvent>>(+[](ModItemUIEvent* event) {
		auto mod = Mod::get();
		auto modItem = event->getItem();
		if (auto bg = static_cast<CCScale9Sprite*>(modItem->querySelector("bg"))) {
			if (mod->getSettingValue<bool>("transparent-lists")) {
				if ((bg->getColor() == ccColor3B{255, 255, 255} && bg->getOpacity() == 25) || (bg->getColor() == ccColor3B{0, 0, 0} && bg->getOpacity() == 90)) {
					bg->setOpacity(isGeodeTheme() ? 50 : 90);
					bg->setColor(ccColor3B{0, 0, 0});
				}
			}
		}
		return ListenerResult::Propagate;
	});
	new EventListener<EventFilter<ModLogoUIEvent>>(+[](ModLogoUIEvent* event) {
		auto logo = event->getSprite();
		queueInMainThread([logo] {
			auto mod = Mod::get();
			if (mod->getSettingValue<bool>("larger-logos")) {
				logo->setScale(0.5f);
			}
		});
		return ListenerResult::Propagate;
	});
}*/
