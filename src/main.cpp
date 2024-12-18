#include "Geode/cocos/cocoa/CCObject.h"
#include "Geode/loader/Event.hpp"
#include "Geode/loader/Loader.hpp"
#include "Geode/utils/general.hpp"
#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/ColorProvider.hpp>
#include <cstdint>
#include <unordered_set>
#include <winspool.h>

using namespace geode::prelude;

// little notes here, i am starting to move towards commenting code instead of removing it because sometimes i need to revert back (plus i havent commited in so long while writing this because im working on the fixing the uh idk transparent lists)

CCNode* modsLayerReference = nullptr;
std::vector<std::string> visited;

bool isGeodeTheme(bool forceDisableTheme = false) {
	return !forceDisableTheme && Loader::get()->getInstalledMod("geode.loader")->getSettingValue<bool>("enable-geode-theme");
}

// Average more janky solution

// Tried to use const char*, led to the value randomly changing after being return (i dont understand why)

// returns bytevector for scenario of getting Mod so i can get modid without another func
// std::pair<geode::ByteVector, bool> isOffsetValidMod(uintptr_t offset, uintptr_t pointerDepth = 0) {
// 	geode::ByteVector bytes;
// 	bool legal = true;
// 	bool hasMetNull = false;
// 	for (uintptr_t i = 0; i < 312; i++) { // 312 bytes cool
// 		// I know, great variable names
// 		auto v = *reinterpret_cast<unsigned char*>(offset + i); //I dont like reinterpret_cast but there is no other option?
// 		auto d = static_cast<uint8_t>(v);
// 		if (d == 0 && bytes.size() >= 4) {
// 			break;
// 		} else if (d != 46 && d != 45 && d != 95 && (d < 97 || d > 122) && (d < 48 || d > 57)) { // exit if not a char allowed in mod ids
// 			legal = false;
// 			if (bytes.size() >= 4) {
// 				geode::log::debug("invalid char: {} {}", v, d);
// 				break;
// 			}
// 		}
// 		bytes.push_back(d);
// 	}
// 	//bytes.push_back('\0'); //No longer needed yay!
//
// 	//geode::log::debug("bytes {} legal {}", bytes, legal);
//
// 	std::pair<geode::ByteVector, bool> pair = std::make_pair(bytes, legal);
// 	if (legal) {
// 		if (bytes.empty()) {
// 			pair.second = false;
// 			return pair;
// 		}
// 		return pair;
// 	} else {
// 		// pointers logic idk
// 		if (bytes.size() == 4 && pointerDepth < 2) {
// 			//geode::log::debug("average pointer {}", bytes);
// 			uintptr_t r = (bytes[3] << 24) | (bytes[2] << 16) | (bytes[1] << 8) | bytes[0];
// 			pair = isOffsetValidMod(r, pointerDepth + 1);
// 			return pair;
// 		}
// 		return pair;
// 	}
// }

struct ServerDateTime final {
	using Clock = std::chrono::system_clock;
	using Value = std::chrono::time_point<Clock>;

	Value value;

	std::string toAgoString() const;

	static Result<ServerDateTime> parse(std::string const& str);
};

struct ServerDeveloper final {
	std::string username;
	std::string displayName;
	bool isOwner;
};

struct ServerModVersion final {
	ModMetadata metadata;
	std::string downloadURL;
	std::string hash;
	size_t downloadCount;

	bool operator==(ServerModVersion const&) const = default;

	static Result<ServerModVersion> parse(matjson::Value const& json);
};

// Code WILL break if ServerModMetadata (or the other structs) is ever updated in geode (hopefully not anytime soon)
struct ServerModMetadata final {
	std::string id;
	bool featured;
	size_t downloadCount;
	std::vector<ServerDeveloper> developers;
	std::vector<ServerModVersion> versions;
	std::unordered_set<std::string> tags;
	std::optional<std::string> about;
	std::optional<std::string> changelog;
	std::optional<std::string> repository;
	std::optional<ServerDateTime> createdAt;
	std::optional<ServerDateTime> updatedAt;
};

// pair containing either Mod or ServerModMetadata and if bool is false then it not valid
std::variant<Mod*, ServerModMetadata> getModFromNode(CCNode* node) {
	auto something = node->getChildByID("info-container")->getChildByID("title-container")->getChildByID("title-label"); //unsure what this is for

	auto addr = reinterpret_cast<uintptr_t>(node) + 0x140;
	auto theStuff = *reinterpret_cast<std::variant<Mod*, ServerModMetadata>*>(addr); // me when pointer needed but i dereference it hehe
	return theStuff;

	// // sometimes mod id is found at 0x140 so we check that first (aka its probably ServerModMetadata)
	// // note: 0x140 can be the mod id on SERVERMODMETADATA WHYYYYYYYYYYYYYYYYYYY (easy i flipped the order)
	// auto addr = reinterpret_cast<uintptr_t>(node) + 0x140; // ServerModMetadata id
	// auto addr2 = reinterpret_cast<uintptr_t>(node) + 0x448; // Mod id
	// //geode::log::debug("0x140 time!");
	// auto pair = isOffsetValidMod(addr2);
	// auto bytes = pair.first;
	// auto valid = pair.second;
	// if (valid) { // if valid then mod instead lol
	// 	std::string id(bytes.begin(), bytes.end());
	// 	//geode::log::debug("Mod {} id: {}", static_cast<CCLabelBMFont*>(something)->getString(), id);
	// 	auto mod = Loader::get()->getInstalledMod(id);
	// 	if (mod == nullptr) {
	// 		//geode::log::debug("mod nullptr detected; {}", id);
	// 		valid = false;
	// 	}
	// 	if (valid) {
	// 		return std::make_pair(mod, true);
	// 	}
	// } else {
	// 	geode::log::debug("{}", bytes);
	// }
	// if (!valid) { // not valid then servermodmetadata (there is one case where valid can change in the previous if statement)
	// 	pair = isOffsetValidMod(addr);
	// 	bytes = pair.first;
	// 	valid = pair.second;
	// 	if (valid) { // make sure mod is valid, otherwise we grabbed wrong address idk
	// 		auto val = *reinterpret_cast<ServerModMetadata*>(addr);
	// 		//geode::log::debug("SMM id {} featured {} tags {} developer0u {}", val.id, val.featured, val.tags, val.developers[0].username);
	// 		return std::make_pair(val, true);
	// 	}
	// }
	// geode::log::debug("failed to find either; mod name: {}", static_cast<CCLabelBMFont*>(something)->getString());
	// return std::make_pair(nullptr, false);
}

class ModListSource {};

class ModList : CCNode {};

class ModItem : CCNode {};

// Soooo about the indentations, uh nullptr checking i guess? dont want game to randomly crash!
// especially on the geode ui, very important for managing mods
// Also getting certain things might be wierd but like i dont wanna make hacky way around a private class and private members

// This gonna be re-done in a less jank way. (No more changing colors based on mod item color because i found out getModSource exists)
// OMG IS THIS A MESS
void modsLayerModify(CCNode* modsLayer) {
	auto mod = Mod::get();
	auto mladdr = reinterpret_cast<uintptr_t>(modsLayer);
	auto mTabs = *reinterpret_cast<std::vector<CCMenuItemSpriteExtra*>*>(mladdr + 0x1a8); //pointer to a vector is crazy
	auto mLists = *reinterpret_cast<std::unordered_map<ModListSource*, Ref<CCNode>>*>(mladdr + 0x1c8);
	if (auto modListFrame = modsLayer->getChildByID("mod-list-frame")) {
		//geode::log::debug("{}", mLists.size());
		for (auto modListPair : mLists) {
			//geode::log::debug("counter");
			auto modList = modListPair.second.data();
			auto page = *reinterpret_cast<size_t*>(reinterpret_cast<uintptr_t>(modList) + 0x148);
			//geode::log::debug("page ok {}", page);
			auto searchMenu = modList->getChildByID("top-container")->getChildByID("search-menu");
			CCLayerColor* searchBG = nullptr;
			if (searchMenu) {
				searchBG = static_cast<CCLayerColor*>(searchMenu->getChildByID("search-id"));
			}
			auto frameBG = static_cast<CCLayerColor*>(modListFrame->getChildByID("frame-bg"));

			if (searchMenu && searchBG && frameBG) {
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

				frameBG->setOpacity(mod->getSettingValue<bool>("transparent-lists") ? mod->getSettingValue<int64_t>("list-bg-transparency") : 255);
				searchBG->setVisible(!mod->getSettingValue<bool>("transparent-lists"));
				someBG->setVisible(mod->getSettingValue<bool>("transparent-lists"));
			}
			// if (auto scrollLayer = static_cast<ScrollLayer*>(modList->getChildByID("ScrollLayer"))) {
			// 	CCObject* obj;
			// 	CCARRAY_FOREACH(scrollLayer->m_contentLayer->getChildren(), obj) {
			// 		// should be mod items right
			// 		auto node = static_cast<CCNode*>(obj);
			// 		if (typeinfo_cast<ModItem*>(node)) {
   //
			// 		}
			// 	}
			// }
		}
	}
}

void modItemModify(CCNode* node) {
	auto mod = Mod::get();
	if (typeinfo_cast<ModItem*>(node)) {
		//auto something = node->getChildByID("info-container")->getChildByID("title-container")->getChildByID("title-label");
		//std::string wow = static_cast<CCLabelBMFont*>(something)->getString();
		//if (std::find(visited.begin(), visited.end(), wow) == visited.end() || visited.size() == 0) { // for anti debug spam
		auto nMod = getModFromNode(node);
		//	visited.push_back(wow);
		//}
		std::string id;
		std::unordered_set<std::string> tags;
		bool isServerMod = false;
		bool isEnabled = true;
		bool isFeatured = false;
		std::visit(geode::utils::makeVisitor {
			[&](Mod* mod) {
				id = mod->getID();
				tags = mod->getMetadata().getTags();
				isEnabled = mod->isEnabled();
				//Mod doesnt store featured :(
			},
			[&](ServerModMetadata const& metadata) {
				isServerMod = true;
				id = metadata.id;
				isServerMod = true;
				tags = metadata.tags;
				isFeatured = metadata.featured;
				//geode::log::debug("id: {} tags: {} featured: {}", id, tags, isFeatured);
			}
		}, nMod);
		if (auto bg = static_cast<CCScale9Sprite*>(node->getChildByID("bg"))) {
			if (mod->getSettingValue<bool>("transparent-lists")) {
				if (isEnabled) {
					auto color4 = mod->getSettingValue<ccColor4B>("tl-enabled-color");
					auto color = ccColor3B{color4.r, color4.g, color4.b};
					bg->setColor(color);
					bg->setOpacity(color4.a);
				} else {
					auto color4 = mod->getSettingValue<ccColor4B>("tl-disabled-color");
					auto color = ccColor3B{color4.r, color4.g, color4.b};
					bg->setColor(color);
					bg->setOpacity(color4.a);
				}
				if (isFeatured && isServerMod) {
					auto color4 = mod->getSettingValue<ccColor4B>("tl-featured-color");
					auto color = ccColor3B{color4.r, color4.g, color4.b};
					bg->setColor(color);
					bg->setOpacity(color4.a);
				}
				if (tags.contains("paid") && isServerMod) {
					auto color4 = mod->getSettingValue<ccColor4B>("tl-paid-color");
					auto color = ccColor3B{color4.r, color4.g, color4.b};
					bg->setColor(color);
					bg->setOpacity(color4.a);
				}
				if (tags.contains("modtober24") && isServerMod) {
					auto color4 = mod->getSettingValue<ccColor4B>("tl-modtober-entry-color");
					auto color = ccColor3B{color4.r, color4.g, color4.b};
					bg->setColor(color);
					bg->setOpacity(color4.a);
				}
				if ((tags.contains("modtober24winner") || id == "rainixgd.geome3dash") && isServerMod) {
					auto color4 = mod->getSettingValue<ccColor4B>("tl-modtober-winner-color");
					auto color = ccColor3B{color4.r, color4.g, color4.b};
					bg->setColor(color);
					bg->setOpacity(color4.a);
				}
				// auto rgb = bg->getColor();
				// auto color = ccColor4B{rgb.b, rgb.g, rgb.b, bg->getOpacity()};
				// if (color == ccColor4B{255, 255, 255, 25} || color == ccColor4B{0, 0, 0, 90}) { // Enabled
				// 	bg->setOpacity(isGeodeTheme() ? 50 : 90);
				// 	bg->setColor(ccColor3B{0, 0, 0});
				// }
				// else if (color == ccColor4B{153, 245, 245, 25}) { // Restart Required
				// 	bg->setOpacity(90);
				// 	bg->setColor(ccColor3B{123, 156, 163});
				// }
				// else if (color == ccColor4B{255, 255, 255, 10}) { // Disabled
				// 	bg->setOpacity(50);
				// 	bg->setColor(ccColor3B{205, 205, 205});
				// }
				// else if (color == ccColor4B{235, 35, 112, 25}) { // Error
				// 	bg->setOpacity(90);
				// 	bg->setColor(ccColor3B{245, 27, 27});
				// }
				// else if (color == ccColor4B{245, 153, 245, 25}) { // Outdated
				// 	bg->setOpacity(90);
				// }
				// else if (color == ccColor4B{240, 211, 42, 65}) { // Featured
				// 	bg->setOpacity(90);
				// }
				// else if (color == ccColor4B{63, 91, 138, 85}) { // Modtober
				// 	bg->setOpacity(90);
				// 	bg->setColor(ccColor3B{32, 102, 220});
				// }
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
			if (mod->getSettingValue<bool>("fix-mod-info-size") && node->getContentHeight() != 100.f) { // me when ModList::m_display is a private member grrr
				auto titleContainer = infoContainer->getChildByID("title-container");
				auto developersMenu = infoContainer->getChildByID("developers-menu");
				if (auto viewMenu = node->getChildByID("view-menu")) {
					auto updateBtn = viewMenu->getChildByID("update-button");
					if (titleContainer && developersMenu) {
						auto width = updateBtn->isVisible() ? 500.f : 525.f;
						infoContainer->setContentWidth(width);
						infoContainer->updateLayout();
						titleContainer->setContentWidth(width);
						titleContainer->updateLayout();
						developersMenu->setContentWidth(width);
						developersMenu->updateLayout();
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
		// No crash when ModsLayer no longer exists (ill keep this just in case)
		if (CCDirector::sharedDirector()->getRunningScene()->getChildByID("ModsLayer")) {
			if (modsLayerReference) {
				modsLayerReference = nullptr;
			}
		}

		return CCDirector::replaceScene(pScene);
	}
};

// I have tried using scheduleOnce but it caused minor graphical glitches (for 1 frame like it was running a little too soon somehow)
// Shut up

// Requires ONLY for making search bar transparent (im scared to try and address on functions since those could change each update (and nightly updates))
// better safe than sorry!
// btw the other option is to hook onto the function used by the tab buttons (extremely easy to access right?) and just if that is run do my thing as well but like what if something else loads another tab?

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
// Haha they actually work now time to remove that very stupid CCScheduler::update hook

$execute {
	new EventListener<EventFilter<ModItemUIEvent>>(+[](ModItemUIEvent* event) {
		modItemModify(event->getItem());
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
}
