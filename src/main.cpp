#include "Geode/cocos/cocoa/CCObject.h"
#include "Geode/loader/Event.hpp"
#include "Geode/loader/Loader.hpp"
#include "Geode/utils/web.hpp"
#include "Geode/utils/general.hpp"
#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/ColorProvider.hpp>
#include <Geode/loader/ModSettingsManager.hpp>
#include <chrono>
#include <cstdint>
#include <thread>
#include <unordered_set>
#include <winspool.h>
#include "picosha2.h"

using namespace geode::prelude;

// little notes here, i am starting to move towards commenting code instead of removing it because sometimes i need to revert back (plus i havent commited in so long while writing this because im working on the fixing the uh idk transparent lists)
// IT IS GETTING SO UNREADABLE HELP

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

CCNode* modsLayerReference = nullptr;
std::vector<size_t> proxyIDList; // Idk
std::vector<ServerModMetadata> serverModList;
std::vector<ServerModMetadata> serverModDownloadedList; // guh

bool isGeodeTheme(bool forceDisableTheme = false) {
	return !forceDisableTheme && Loader::get()->getInstalledMod("geode.loader")->getSettingValue<bool>("enable-geode-theme");
}

std::string calculateHash(std::span<const uint8_t> data) {
	std::vector<uint8_t> hash(picosha2::k_digest_size);
	picosha2::hash256(data.begin(), data.end(), hash);
	return picosha2::bytes_to_hex_string(hash.begin(), hash.end());
}

void appendServerMod(ServerModMetadata metadata) {
	//geode::log::info("good job UwU");
	size_t i = 0;
	// should i be using std::find? idk i am really inconsistent with my coding
	for (ServerModMetadata m : serverModList) {
		if (m.id == metadata.id) {
			serverModList[i] = metadata; // update value :3
			return;
		}
		i++;
	}
	serverModList.push_back(metadata);
}

std::optional<ServerModMetadata> getServerMod(std::string id) {
	for (ServerModMetadata m : serverModList) {
		//geode::log::debug("getServerMod {} {}", m.id, id);
		if (m.id == id) {
			return m;
		}
	}
	return std::nullopt;
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

// pair containing either Mod or ServerModMetadata and if bool is false then it not valid
std::optional<std::variant<Mod*, ServerModMetadata>> getModFromNode(CCNode* node) {
	auto something = node->getChildByID("info-container")->getChildByID("title-container")->getChildByID("title-label"); //unsure what this is for

	auto addr = reinterpret_cast<uintptr_t>(node) + 0x140;
	auto theStuff = reinterpret_cast<std::variant<Mod*, ServerModMetadata>*>(addr); // me when pointer needed but i dereference it hehe
	if (theStuff != nullptr) { // i dont think it could ever be nullptr?
		return *theStuff;
	}
	geode::log::warn("Geode UI Tweaks could not find ModItem::m_source, a recent geode update could be the cause of this."); // Imagine
	return std::nullopt;

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

std::optional<std::variant<Mod*, ServerModMetadata>> getModFromPopup(CCNode* node) {
	auto addr = reinterpret_cast<uintptr_t>(node) + 0x2a8;
	auto theStuff = reinterpret_cast<std::variant<Mod*, ServerModMetadata>*>(addr);
	if (theStuff != nullptr) {
		return *theStuff;
	}
	geode::log::warn("Geode UI Tweaks could not find ModPopup::m_source, a recent geode update could be the cause of this.");
	return std::nullopt;
}

class ModListSource {};

class ModList : CCNode {};

class ModItem : CCNode {};

class ModPopup : CCNode {};

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
		auto noMod = getModFromNode(node);
		if (noMod.has_value()) {
			auto nMod = noMod.value();
			std::string id;
			std::unordered_set<std::string> tags;
			bool isServerMod = false;
			bool isEnabled = true;
			bool isFeatured = false;
			size_t requestedAction = 0;
			bool restartRequired = false;
			std::visit(geode::utils::makeVisitor {
				[&](Mod* mod) {
					id = mod->getID();
					tags = mod->getMetadata().getTags();
					isEnabled = mod->isEnabled();
					requestedAction = static_cast<size_t>(mod->getRequestedAction());
					restartRequired = (requestedAction != 0) || (ModSettingsManager::from(mod)->restartRequired());
					//Mod doesnt store featured :(
				},
				[&](ServerModMetadata const& metadata) {
					isServerMod = true;
					id = metadata.id;
					tags = metadata.tags;
					isFeatured = metadata.featured;
					//geode::log::debug("id: {} tags: {} featured: {}", id, tags, isFeatured);
					for (auto m : serverModDownloadedList) {
						if (m.id == id) {
							restartRequired = true;
							break;
						}
					}
					appendServerMod(metadata);
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
					if (restartRequired) { // Restart Required currently
						auto color4 = mod->getSettingValue<ccColor4B>("tl-restart-required-color");
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
}

void modPopupModify(CCNode* popup) {
	auto mod = Mod::get();
	if (typeinfo_cast<ModPopup*>(popup)) {
		auto noMod = getModFromPopup(popup);
		if (noMod.has_value()) {
			auto nMod = noMod.value();
			std::visit(geode::utils::makeVisitor {
				[&](Mod* mod) {

				},
				[&](ServerModMetadata const& metadata) {
					appendServerMod(metadata);
				}
			}, nMod);
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

void modDownloadChecker(web::WebResponse* response, std::string id, std::string url) {
	auto oServerMod = getServerMod(id);
	//geode::log::debug("I PROBABLY BROKE SOMETHING {}", oServerMod.has_value());
	if (oServerMod.has_value()) {
		auto serverMod = oServerMod.value();
		std::optional<ServerModVersion> oVersion = std::nullopt; // im in love with std::optional
		for (ServerModVersion v : serverMod.versions) {
			//geode::log::debug("version test lol {} {}", v.downloadURL, url);
			if (v.downloadURL == url) {
				oVersion = v;
			}
		}
		if (response->ok() && oVersion.has_value()) {
			auto version = oVersion.value();
			// my recreation of m_downloadListener.bind in DownloadManager::confirm
			if (auto actualHash = calculateHash(response->data()); actualHash != version.hash) {
				//geode::log::info("INVALID HASH");
				// invalid hash!
				return;
			}
			// there is something for if it cant delete existing .geode package but we only care about installing not updating plus there is no solution besides overwriting (guh)
			// also this is stupid lol but idk im not writing anything!
			auto dir = dirs::getModsDir() / (id + ".geode");
			std::ofstream file;
#ifdef GEODE_IS_WINDOWS
			file.open(dir.wstring(), std::ios::out | std::ios::binary);
#else
			file.open(dir.string(), std::ios::out | std::ios::binary);
#endif
			if (!file.is_open()) {
				//geode::log::info("UNABLE TO WRITE GEODE FILE");
				// oops it cant write!
				return;
			}
			file.close(); // am i writing too many comments?
			serverModDownloadedList.push_back(serverMod);
		}
	}
}

//https://api.geode-sdk.org/v1/mods/{id}/versions/{version}/download

// using GDIntercept code so i can hook onto a SINGLE request omg
web::WebTask GeodeWebHook(web::WebRequest* request, std::string_view method, std::string_view url) {
	if (std::find(proxyIDList.begin(), proxyIDList.end(), request->getID()) == proxyIDList.end()) { // anti proxy thingie (no duplicates i guess?)
		proxyIDList.push_back(request->getID());
		bool isDownload = false;
		std::string modID;
		std::string sURL = std::string(url);
		//geode::log::debug("{}", std::string(url));
		std::string apiURL = "https://api.geode-sdk.org/v1/mods/";
		// when you dont use regex
		if (sURL.starts_with(apiURL)) {
			//geode::log::debug("geode idk");
			std::string nURL = sURL.substr(apiURL.size());
			//geode::log::debug("{}", nURL);
			size_t slash1 = nURL.find("/");
			if (slash1 != std::string::npos) {
				modID = nURL.substr(0, slash1);
				//geode::log::debug("wha {}", modID);
				nURL = nURL.substr(slash1); // hmm
				//geode::log::debug("insane {}", nURL);
				std::string vURL = "/versions/";
				if (nURL.starts_with(vURL)) {
					//geode::log::debug("INCORRECT!!!");
					nURL = nURL.substr(vURL.size());
					//geode::log::debug("new {}", nURL);
					size_t slash2 = nURL.find("/");
					if (slash2 != std::string::npos) {
						std::string version = nURL.substr(0, slash2);
						//geode::log::debug("ver {}", version);
						if (nURL.ends_with("/download")) {
							//geode::log::debug("is download no way");
							isDownload = true;
						}
					}
				}
			}
		}
		if (isDownload && !Loader::get()->isModInstalled(modID)) {
			//geode::log::debug("hooking download hehehe");
			// this looks so much less complicated than in GDIntercept
			auto task = web::WebTask::run([request, method, url, modID, sURL](auto progress, auto cancelled) -> web::WebTask::Result {
				web::WebResponse* response = nullptr;

				web::WebTask task = request->send(method, url);

				task.listen([&response](const web::WebResponse* taskResponse) {
					response = new web::WebResponse(*taskResponse);
				}, [progress, cancelled](const web::WebProgress* taskProgress) {
					if (!cancelled()) progress(*taskProgress);
				});

				while (!response && !cancelled()) std::this_thread::sleep_for(std::chrono::milliseconds(2)); // rest

				if (cancelled()) {
					task.cancel();

					return web::WebTask::Cancel();
				} else {
					//geode::log::debug("response gotten yay!");

					modDownloadChecker(response, modID, sURL);

					return *response;
				}
			}, fmt::format("Proxy for {} {}", method, url));
			return task;
		}
	}
	return request->send(method, url);
}

// Imagine using geode's little ui events (they also dont work for my problems hehe)
// Haha they actually work now time to remove that very stupid CCScheduler::update hook

$execute {
	// GDIntercept
	(void) Mod::get()->hook(
		reinterpret_cast<void*>(addresser::getNonVirtual(&web::WebRequest::send)),
		&GeodeWebHook,
		"geode::web::WebRequest::send",
		tulip::hook::TulipConvention::Thiscall
	);

	new EventListener<EventFilter<ModItemUIEvent>>(+[](ModItemUIEvent* event) {
		modItemModify(event->getItem());
		return ListenerResult::Propagate;
	});
	new EventListener<EventFilter<ModPopupUIEvent>>(+[](ModPopupUIEvent* event) {
		modPopupModify(event->getPopup());
		return ListenerResult::Propagate;
	});
}
