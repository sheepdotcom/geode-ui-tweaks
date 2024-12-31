#include "Geode/cocos/CCDirector.h"
#include "Geode/cocos/cocoa/CCObject.h"
#include "Geode/cocos/draw_nodes/CCDrawNode.h"
#include "Geode/cocos/label_nodes/CCLabelBMFont.h"
#include "Geode/cocos/platform/win32/CCGL.h"
#include "Geode/loader/Event.hpp"
#include "Geode/loader/Loader.hpp"
#include "Geode/loader/Setting.hpp"
#include "Geode/ui/SceneManager.hpp"
#include "Geode/utils/addresser.hpp"
#include "Geode/utils/cocos.hpp"
#include "Geode/utils/web.hpp"
#include "Geode/utils/general.hpp"
#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/ColorProvider.hpp>
#include <Geode/loader/ModSettingsManager.hpp>
#include <chrono>
#include <date/date.h>
#include <cstdint>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include "ccTypes.h"
#include "picosha2.h"
#include "Server.hpp"
#include "Tooltip.hpp"

using namespace geode::prelude;

// little notes here, i am starting to move towards commenting code instead of removing it because sometimes i need to revert back (plus i havent commited in so long while writing this because im working on the fixing the uh idk transparent lists)
// IT IS GETTING SO UNREADABLE HELP

// the amount of global variables is crazy (and all but one are vectors)
// this might be the main thing stopping me from like separating all my stuff to different files
CCNode* modsLayerReference = nullptr;
CCLayerColor* modPopupReference = nullptr;
CCDrawNode* debugDrawNode = nullptr;
std::vector<size_t> proxyIDList; // Idk
std::vector<ServerModMetadata> serverModList;
std::vector<ServerModUpdate> serverModUpdateList;
std::vector<ServerTag> serverTagList;
std::vector<std::pair<std::string, ServerModVersion>> serverModVersionList; // oh god they keep appearing
//std::map<ServerModMetadata, DownloadStatus> serverModDownloadsList; // guh (this one wont build)
std::vector<std::pair<std::variant<Mod*, ServerModMetadata>, DownloadStatus>> serverModDownloadsList; // guh x2 (this builds but not map or unordered_map wtf)
std::vector<TooltipMetadata> nodesToHoverList; // should be reset every frame idk?
std::vector<Tooltip*> activeTooltipsList;
std::unordered_map<std::string, std::string> tagDescriptionMap; // d

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

void appendServerModUpdate(ServerModUpdate update) {
	size_t i = 0;
	for (auto m : serverModUpdateList) {
		if (m.id == update.id) {
			serverModUpdateList[i] = update;
			return;
		}
		i++;
	}
	serverModUpdateList.push_back(update);
}

void appendServerModVersion(std::string id, ServerModVersion version) {
	size_t i = 0;
	for (auto m : serverModVersionList) {
		if (m.second.downloadURL == version.downloadURL) {
			serverModVersionList[i] = std::make_pair(id, version);
			return;
		}
		i++;
	}
	serverModVersionList.push_back(std::make_pair(id, version));
}

void appendServerModDownloadsList(std::variant<Mod*, ServerModMetadata> mod, DownloadStatus status) {
	size_t i = 0;
	//geode::log::debug("APPENDING");
	for (auto m : serverModDownloadsList) {
		if ((std::holds_alternative<Mod*>(m.first) && std::holds_alternative<Mod*>(mod)) || (std::holds_alternative<ServerModMetadata>(m.first) && std::holds_alternative<ServerModMetadata>(mod))) {
			serverModDownloadsList[i] = std::make_pair(mod, status);
			return;
		}
		i++;
	}
	serverModDownloadsList.push_back(std::make_pair(mod, status));
}

CCNode* getTopNodeFromNode(CCNode* node) {
	if (node) {
		if (auto p = node->getParent()) {
			if (typeinfo_cast<CCScene*>(p)) {
				return node;
			} else {
				return getTopNodeFromNode(p);
			}
		}
	}
	return nullptr;
}

CCScene* getSceneFromNode(CCNode* node) {
	CCNode* topNode = getTopNodeFromNode(node);
	if (topNode) {
		if (auto p = topNode->getParent()) {
			if (typeinfo_cast<CCScene*>(p)) {
				return static_cast<CCScene*>(p);
			}
		}
	}
	return nullptr;
}

// ok GDIntercept
CCSize getRecursiveScale(CCNode* node, const CCSize accumulator) {
	if (node->getParent()) {
		return getRecursiveScale(node->getParent(), {accumulator.width * node->getScaleX(), accumulator.height * node->getScaleY()});
	} else {
		return accumulator;
	}
}

class comma_numpunct : public std::numpunct<char> {
protected:
	virtual char do_thousands_sep() const
	{
		return ',';
	}

	virtual std::string do_grouping() const
	{
		return "\03";
	}
};

Tooltip::~Tooltip() { // hehe
	activeTooltipsList.erase(std::remove(activeTooltipsList.begin(), activeTooltipsList.end(), this), activeTooltipsList.end());
}

void makeDebugDrawNode() {
	if (!debugDrawNode) {
		debugDrawNode = CCDrawNode::create();
		debugDrawNode->retain();
		debugDrawNode->setID("debug-draw-node"_spr);
		debugDrawNode->setZOrder(99);
		_ccBlendFunc blendFunc;
		blendFunc.src = GL_SRC_ALPHA;
		blendFunc.dst = GL_ONE_MINUS_SRC_ALPHA;
		debugDrawNode->setBlendFunc(blendFunc);
	}
	SceneManager::get()->keepAcrossScenes(debugDrawNode);
}

void removeDebugDrawNode() {
	if (debugDrawNode) {
		debugDrawNode->clear();
		SceneManager::get()->forget(debugDrawNode);
		debugDrawNode->removeFromParentAndCleanup(false);
	}
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

	// this might be the only thing keeping this off the index lol since it'll crash if it points to the wrong type and I can't use try catch because android grr
	// though hopefully they NEVER add any members before m_source
	auto addr = reinterpret_cast<uintptr_t>(node) + 0x140;
	auto theStuff = reinterpret_cast<std::variant<Mod*, ServerModMetadata>*>(addr); // me when pointer needed but i dereference it hehe
	if (theStuff != nullptr) { // i dont think it could ever be nullptr?
		return *theStuff;
	}
	geode::log::warn("Geode UI Tweaks could not find ModItem::m_source.m_value, a recent geode update could be the cause of this."); // Imagine
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
	if (theStuff != nullptr) { // useless check ill replace it later
		return *theStuff;
	}
	geode::log::warn("Geode UI Tweaks could not find ModPopup::m_source.m_value, a recent geode update could be the cause of this.");
	return std::nullopt;
}

std::optional<ServerModUpdate> getModUpdateFromNode(CCNode* node) {
	auto addr = reinterpret_cast<uintptr_t>(node) + 0x140 + 0x140; // no way
	auto theStuff = reinterpret_cast<std::optional<ServerModUpdate>*>(addr);
	if (theStuff != nullptr) { // this one might not be possible to replace lol
		return *theStuff;
	}
	geode::log::warn("Geode UI Tweaks could not find ModItem::m_source.m_availableUpdate");
	return std::nullopt;
}

class ModListSource {};

class ModList : public CCNode {};

class ModItem : public CCNode {};

class ModPopup : public CCNode {};

class ModsLayer : public CCNode {};

// Soooo about the indentations, uh nullptr checking i guess? dont want game to randomly crash!
// especially on the geode ui, very important for managing mods
// Also getting certain things might be wierd but like i dont wanna make hacky way around a private class and private members

// This ~~gonna be~~ is re-done in a less jank way. (No more changing colors based on mod item color because i found out getModSource exists)
// OMG IS THIS A MESS
void modsLayerModify(CCNode* modsLayer) {
	auto mod = Mod::get();
	auto mladdr = reinterpret_cast<uintptr_t>(modsLayer);
	auto mTabs = *reinterpret_cast<std::vector<CCMenuItemSpriteExtra*>*>(mladdr + 0x1a8); //pointer to a vector is crazy
	auto mLists = *reinterpret_cast<std::unordered_map<ModListSource*, Ref<CCNode>>*>(mladdr + 0x1c8);
	if (auto modListFrame = modsLayer->getChildByID("mod-list-frame")) {
		//geode::log::debug("{}", mLists.size());
		//for (auto modListPair : mLists) {
		if (auto modList = modListFrame->getChildByID("ModList")) {
			//geode::log::debug("counter");
			//auto modList = modListPair.second.data();
			//auto page = *reinterpret_cast<size_t*>(reinterpret_cast<uintptr_t>(modList) + 0x148);
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
			if (auto scrollLayer = static_cast<ScrollLayer*>(modList->getChildByID("ScrollLayer"))) {
				auto scrollArr = CCArrayExt<CCNode*>(scrollLayer->m_contentLayer->getChildren());
				for (auto node : scrollArr) {
					// should be mod items right
					if (typeinfo_cast<ModItem*>(node)) {
						auto noMod = getModFromNode(node);
						if (noMod.has_value()) {
							auto nMod = noMod.value();
							ModMetadata nMetadata;
							std::visit(geode::utils::makeVisitor {
								[&](Mod* mod) {
									nMetadata = mod->getMetadata();
								},
								[&](ServerModMetadata metadata) {
									nMetadata = metadata.versions.front().metadata;
								}
							}, nMod);
							// empty string / only spaces checker
							std::optional<std::string> oDescription = nMetadata.getDescription();
							std::string description = oDescription.value_or("[No Description Provided]");
							std::string tempDesc = description;
							tempDesc.erase(std::remove(tempDesc.begin(), tempDesc.end(), ' '), tempDesc.end());
							if (tempDesc.empty()) {
								description = "[No Description Provided]";
							}

							if (mod->getSettingValue<bool>("tooltips-mod-list-description")) {
								CCRect rect = CCRect(scrollLayer->convertToWorldSpace(scrollLayer->getAnchorPoint()), scrollLayer->getContentSize() * getRecursiveScale(scrollLayer, {1.f, 1.f}));
								auto tm = TooltipMetadata(node, description, true, rect);
								nodesToHoverList.push_back(tm);
							}
						}
					}
				}
			}
		}
	}
}

void modItemModify(CCNode* node) {
	auto mod = Mod::get();
	if (typeinfo_cast<ModItem*>(node)) {
		auto noMod = getModFromNode(node);
		if (noMod.has_value()) {
			auto nMod = noMod.value();
			ModMetadata nMetadata;
			std::string id;
			std::unordered_set<std::string> tags;
			bool isServerMod = false;
			bool isEnabled = true;
			bool isFeatured = false;
			size_t downloadCount = 0;
			size_t requestedAction = 0;
			bool restartRequired = false;
			std::optional<LoadProblem> targetsOutdated;
			bool hasLoadProblems;
			std::optional<ServerModUpdate> availableUpdate = getModUpdateFromNode(node);
			if (availableUpdate.has_value()) {
				appendServerModUpdate(availableUpdate.value());
			}
			bool isDownloading = false;
			bool hasDownloaded = false;
			std::visit(geode::utils::makeVisitor {
				[&](Mod* dMod) {
					nMetadata = dMod->getMetadata();
					id = dMod->getID();
					tags = dMod->getMetadata().getTags();
					isEnabled = dMod->isEnabled();
					//Mod doesnt store featured :(
					targetsOutdated = dMod->targetsOutdatedVersion();
					hasLoadProblems = dMod->hasLoadProblems();
				},
				[&](ServerModMetadata const& metadata) {
					nMetadata = metadata.versions.front().metadata;
					isServerMod = true;
					id = metadata.id;
					tags = metadata.tags;
					isFeatured = metadata.featured;
					downloadCount = metadata.downloadCount;
					//geode::log::debug("id: {} tags: {} featured: {}", id, tags, isFeatured);
					appendServerMod(metadata);
				}
			}, nMod);
			if (auto lMod = Loader::get()->getInstalledMod(id)) { // put some things in here to MAKE SURE nothing breaks
				requestedAction = static_cast<size_t>(lMod->getRequestedAction());
				restartRequired = (requestedAction != 0) || (ModSettingsManager::from(lMod)->restartRequired());
			}
			//geode::log::debug("nya_uwugayreal {} {} {}", id, requestedAction, restartRequired);
			for (auto m : serverModDownloadsList) {
				//geode::log::debug("huuuh");
				std::visit(geode::utils::makeVisitor {
					[&](Mod* mod) {
						//geode::log::debug("{} {}", mod->getID(), id);
						if (mod->getID() == id) {
							//geode::log::debug("guh guh guh guh");
							if (std::holds_alternative<DownloadStatusDone>(m.second)) {
								restartRequired = true;
								hasDownloaded = true;
							}
							if (!(std::holds_alternative<DownloadStatusDone>(m.second) || std::holds_alternative<DownloadStatusError>(m.second) || std::holds_alternative<DownloadStatusCancelled>(m.second))) {
								isDownloading = true;
							}
						}
					},
					[&](ServerModMetadata metadata) {
						//geode::log::debug("{} {}", mod->getID(), id);
						if (metadata.id == id) {
							//geode::log::debug("nya nya nya nya");
							if (std::holds_alternative<DownloadStatusDone>(m.second)) {
								restartRequired = true;
								hasDownloaded = true;
							}
							if (!(std::holds_alternative<DownloadStatusDone>(m.second) || std::holds_alternative<DownloadStatusError>(m.second) || std::holds_alternative<DownloadStatusCancelled>(m.second))) {
								isDownloading = true;
							}
						}
					}
				}, m.first);
			}
			static std::locale commaLocale(std::locale(), new comma_numpunct());
			auto bg = static_cast<CCScale9Sprite*>(node->getChildByID("bg"));
			auto logoSprite = node->getChildByID("logo-sprite");
			auto infoContainer = node->getChildByID("info-container");
			CCNode* titleContainer;
			CCMenu* developersMenu;
			if (infoContainer) {
				titleContainer = infoContainer->getChildByID("title-container");
				developersMenu = static_cast<CCMenu*>(infoContainer->getChildByID("developers-menu"));
			}
			auto viewMenu = static_cast<CCMenu*>(node->getChildByID("view-menu"));
			CCNode* downloadCountContainer;
			CCLabelBMFont* downloadsLabel;
			CCSprite* downloadsIcon;
			if (viewMenu) {
				downloadsLabel = static_cast<CCLabelBMFont*>(viewMenu->getChildByIDRecursive("downloads-label"));
				downloadsIcon = static_cast<CCSprite*>(viewMenu->getChildByIDRecursive("downloads-icon-sprite"));
				if (downloadsLabel) {
					downloadCountContainer = downloadsLabel->getParent();
				}
			}
			if (bg) {
				if (mod->getSettingValue<bool>("transparent-lists")) {
					auto color4 = mod->getSettingValue<ccColor4B>("tl-enabled-color");
					if (!isEnabled) {
						color4 = mod->getSettingValue<ccColor4B>("tl-disabled-color");
					}
					if (tags.contains("paid") && isServerMod) {
						color4 = mod->getSettingValue<ccColor4B>("tl-paid-color");
					}
					if (tags.contains("modtober24") && isServerMod) {
						color4 = mod->getSettingValue<ccColor4B>("tl-modtober-entry-color");
					}
					if ((tags.contains("modtober24winner") || id == "rainixgd.geome3dash") && isServerMod) {
						color4 = mod->getSettingValue<ccColor4B>("tl-modtober-winner-color");
					}
					if (isFeatured && isServerMod) {
						color4 = mod->getSettingValue<ccColor4B>("tl-featured-color");
					}
					//geode::log::debug("is {} has {}", isDownloading, hasDownloaded);
					if (availableUpdate.has_value() && !(isDownloading || hasDownloaded)) {
						color4 = mod->getSettingValue<ccColor4B>("tl-updates-available-color");
					}
					if (!isServerMod && hasLoadProblems) {
						color4 = mod->getSettingValue<ccColor4B>("tl-error-color");
					}
					if (!restartRequired && targetsOutdated.has_value() && !isDownloading) {
						color4 = mod->getSettingValue<ccColor4B>("tl-outdated-color");
					}
					if (restartRequired) { // Restart Required currently
						color4 = mod->getSettingValue<ccColor4B>("tl-restart-required-color");
					}
					auto color = ccColor3B{color4.r, color4.g, color4.b};
					bg->setColor(color);
					bg->setOpacity(color4.a);
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
			if (logoSprite) {
				if (mod->getSettingValue<bool>("larger-logos") && (logoSprite->getScale() == 0.4f || logoSprite->getScale() == 0.6f)) {
					logoSprite->setScale(logoSprite->getScale() == 0.6f ? 0.7f : 0.5f);
				} else if (!mod->getSettingValue<bool>("larger-logos") && (logoSprite->getScale() == 0.5f || logoSprite->getScale() == 0.7f)) {
					logoSprite->setScale(logoSprite->getScale() == 0.7f ? 0.6f : 0.4f);
				}
			}
			if (viewMenu) {
				if (downloadsLabel && downloadsIcon && downloadCountContainer) {
					if (mod->getSettingValue<bool>("dont-shorten-download-count")) {
						if (isServerMod) {
							downloadsLabel->setString(fmt::format(commaLocale, "{:L}", downloadCount).c_str());
							downloadCountContainer->setContentSize({downloadsLabel->getScaledContentWidth() + downloadsIcon->getScaledContentWidth(), 30});
							downloadCountContainer->updateLayout();
						}
					}
				}
			}
			if (infoContainer) {
				if (mod->getSettingValue<bool>("fix-mod-info-size") && node->getContentHeight() != 100.f) { // me when ModList::m_display is a private member grrr (now i do have bypasses but seeing as this can change uh idk)
					if (viewMenu) {
						auto updateBtn = viewMenu->getChildByID("update-button");
						if (titleContainer && developersMenu) {
							auto width = updateBtn->isVisible() ? 500.f : 525.f;
							if (isServerMod && downloadCountContainer) {
								auto leftPos = infoContainer->getPosition();
								auto viewMenuLeft = viewMenu->getPosition() - viewMenu->getScaledContentWidth();
								auto rightPos = (downloadCountContainer->getPosition() * viewMenu->getScale()) + viewMenuLeft;
								width = (std::abs(rightPos.x - leftPos.x) / infoContainer->getScaleX()) - 15.f;
							}
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

// this one runs every frame unlike the other one (for tooltips)
void otherModPopupModify(CCLayerColor* popup) {
	auto mod = Mod::get();
	// geode::log::debug("spam");
	CCNode* tagsContainer = popup->getChildByIDRecursive("tags-container"); // one of the only things with a node id like come on
	if (tagsContainer && !serverTagList.empty()) {
		// geode::log::debug("is that server tag list empty");
		auto arr = CCArrayExt<CCNode*>(tagsContainer->getChildren());
		for (CCNode* n : arr) {
			if (auto label = n->getChildByType<CCLabelBMFont*>(0)) {
				std::string tag = std::string(label->getString());
				// ah yes ModSource::fetchValidTags
				auto oServerTag = ranges::find(serverTagList, [&tag](ServerTag const& serverTag) {
					return serverTag.displayName == tag;
				});
				// geode::log::debug("wowie {}", stag.has_value());
				if (oServerTag.has_value()) {
					ServerTag serverTag = oServerTag.value();
					std::string tagDesc = fmt::format("No description found for tag {}", serverTag.displayName);
					auto oTagDescPair = ranges::find(tagDescriptionMap, [&serverTag](std::pair<std::string, std::string> const& tagDescPair) {
						return serverTag.name == tagDescPair.first;
					});
					if (oTagDescPair.has_value()) {
						tagDesc = oTagDescPair.value().second;
					}
					if (mod->getSettingValue<bool>("tooltips-tag-popup-description")) {
						auto tm = TooltipMetadata(n, tagDesc, true);
						nodesToHoverList.push_back(tm);
					}
				}
			}
		}
	}
}

#include <Geode/modify/CCLayerColor.hpp>

class $modify(CustomPopup, CCLayerColor) {
	bool initWithColor(ccColor4B const& color, GLfloat width, GLfloat height) {
		if (!CCLayerColor::initWithColor(color, width, height)) return false;

		if (typeinfo_cast<ModPopup*>(this)) {
			modPopupReference = this;
		}

		return true;
	}
};

#include <Geode/modify/CCLayer.hpp>

class $modify(CustomModsLayer, CCLayer) {
	bool init() {
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
		if (modsLayerReference) {
			CCScene* scene = CCDirector::sharedDirector()->getRunningScene();
			auto arr = CCArrayExt<CCNode*>(scene->getChildren());
			for (CCNode* n : arr) {
				if (typeinfo_cast<ModsLayer*>(n)) {
					modsLayerReference = nullptr;
					nodesToHoverList.clear();
					break;
				}
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

		if (modPopupReference) {
			otherModPopupModify(modPopupReference);
		}

		auto mod = Mod::get();
		CCSize winSize = CCDirector::sharedDirector()->getWinSize();

		if (mod->getSettingValue<bool>("debug-tooltips-draw")) makeDebugDrawNode();
		
		if (debugDrawNode) {
			debugDrawNode->clear();
			debugDrawNode->setVisible(true);
			debugDrawNode->setZOrder(CCDirector::sharedDirector()->getRunningScene()->getHighestChildZ());
		}

		const ccColor4F debugColor = ccColor4F{ 255/255.f, 175/255.f, 204/255.f, 255/255.f };

		// not even devs using cocos know how to code
		std::vector<Tooltip*> orphanedTooltips = activeTooltipsList;
		if (mod->getSettingValue<bool>("tooltips")) {
			for (auto mNode : nodesToHoverList) {
				CCNode* node = mNode.node;
				std::string text = mNode.text;
				bool zOrderCheck = mNode.zOrderCheck;
				std::optional<CCRect> oLimitedArea = mNode.limitedArea;
				CCRect limitedArea = oLimitedArea.has_value() ? oLimitedArea.value() : CCRect{{0, 0}, winSize};
				bool alreadyHovered = false;
				Tooltip* nodeTooltip = nullptr;
				for (auto t : activeTooltipsList) {
					if (t->m_nodeFrom == node) {
						alreadyHovered = true;
						nodeTooltip = t;
						orphanedTooltips.erase(std::remove(orphanedTooltips.begin(), orphanedTooltips.end(), t), orphanedTooltips.end());
						break;
					}
				}

				const CCPoint trueScale = getRecursiveScale(node, {1.f, 1.f});

				const CCPoint mousePos = geode::cocos::getMousePos();
				const CCPoint bottomLeft = node->convertToWorldSpace(node->getAnchorPoint());
				const CCPoint topRight = bottomLeft + node->getContentSize() * trueScale;

				//const CCPoint tooltipPos = {(bottomLeft.x + topRight.x) / 2, topRight.y};
				CCPoint tooltipPos = ccp(mousePos.x + 5.f, mousePos.y - 5.f);

				CCNode* topNode = getTopNodeFromNode(node);
				CCScene* scene = getSceneFromNode(topNode);

				bool isCoveredUp = false;
				if (zOrderCheck && scene && topNode) {
					auto arr = CCArrayExt<CCNode*>(scene->getChildren());
					std::reverse(arr.begin(), arr.end());
					for (CCNode* n : arr) {
						// cocos should guarantee that everything is sorted by zOrder (the one case is if someone is REALLY REALLY REALLY stupid and somehow manually appends a child without using addChild which auto re-orders everything)
						if (n->getZOrder() < topNode->getZOrder()) {
							break;
						}
						// check only for FLAlertLayer
						if (typeinfo_cast<FLAlertLayer*>(n) && n->getZOrder() > topNode->getZOrder()) {
							isCoveredUp = true;
							break;
						}
					}
					std::reverse(arr.begin(), arr.end()); // dont forget
				}

				bool withinLimitedArea = false;
				if (mousePos >= limitedArea.origin && mousePos <= limitedArea.origin + limitedArea.size) {
					withinLimitedArea = true;
				}

				if (mod->getSettingValue<bool>("debug-tooltips-draw") && debugDrawNode) {
					CCPoint maxTopRight = limitedArea.origin + limitedArea.size;
					bool shouldDrawDebug = true;
					if (isCoveredUp) {
						shouldDrawDebug = false;
					}
					if (topRight.x < limitedArea.origin.x || bottomLeft.x > maxTopRight.x) {
						shouldDrawDebug = false;
					} else if (topRight.y < limitedArea.origin.y || bottomLeft.y > maxTopRight.y) {
						shouldDrawDebug = false;
					}
					if (shouldDrawDebug) {
						CCPoint dTopRight = ccp(std::fmin(topRight.x, maxTopRight.x), std::fmin(topRight.y, maxTopRight.y));
						CCPoint dBottomLeft = ccp(std::fmax(bottomLeft.x, limitedArea.origin.x), std::fmax(bottomLeft.y, limitedArea.origin.y));
						CCPoint dTopLeft = ccp(dBottomLeft.x, dTopRight.y);
						CCPoint dBottomRight = ccp(dTopRight.x, dBottomLeft.y);
						float thickness = .5f;
						debugDrawNode->drawSegment(dTopLeft, dTopRight, thickness, debugColor);
						debugDrawNode->drawSegment(dTopRight, dBottomRight, thickness, debugColor);
						debugDrawNode->drawSegment(dBottomRight, dBottomLeft, thickness, debugColor);
						debugDrawNode->drawSegment(dBottomLeft, dTopLeft, thickness, debugColor);
					}
				}

				if ((mousePos >= bottomLeft && mousePos <= topRight) && withinLimitedArea && !isCoveredUp) {
					if (!alreadyHovered) {
						auto tooltip = Tooltip::create(node, text, 0.2f, 300.f, 150.f);
						if (tooltip) {
							tooltip->setID("Tooltip"_spr);
							tooltip->show(scene);
							activeTooltipsList.push_back(tooltip);
							nodeTooltip = tooltip;
						}
					}
					if (nodeTooltip) {
						// make it go on the left side if it goes offscreen on the right
						bool left = false;
						float width = nodeTooltip->m_bg->getScaledContentWidth();
						if (tooltipPos.x + width > winSize.width) {
							left = true;
						}
						// do something else if it goes offscreen on left aswell
						if (left && tooltipPos.x - 10.f - width < 0.f) {
							left = false;
							tooltipPos = ccp(winSize.width - width, tooltipPos.y);
						}
						nodeTooltip->setPosition(ccp(tooltipPos.x - (left ? 10.f : 0.f), tooltipPos.y));
						nodeTooltip->setAnchorPoint(ccp(left ? 1.f : 0.f, 0.f));
					}
				} else if (alreadyHovered && nodeTooltip) {
					activeTooltipsList.erase(std::remove(activeTooltipsList.begin(), activeTooltipsList.end(), nodeTooltip), activeTooltipsList.end()); // c++
					nodeTooltip->fadeOut();
				}
			}
		}
		// keep it outside so it can kill any tooltips that existed before turning off tooltips setting
		for (auto t : orphanedTooltips) {
			activeTooltipsList.erase(std::remove(activeTooltipsList.begin(), activeTooltipsList.end(), t), activeTooltipsList.end()); // c++
			t->fadeOut();
		}
		// if (debugDrawNode) debugDrawNode->draw();
		nodesToHoverList.clear();
		CCScheduler::update(dt);
	}
};

void modDownloadChecker(web::WebResponse* response, std::variant<Mod*, ServerModMetadata> mod, std::string_view url) {
	std::string id;
	std::optional<ServerModVersion> oVersion = std::nullopt; // im in love with std::optional
	std::visit(geode::utils::makeVisitor {
		[&](Mod* dMod) {
			id = dMod->getID();
			//geode::log::debug("oh no");
			for (auto v : serverModVersionList) {
				//geode::log::debug("it is better to not think about it {} {}", v.first, id);
				if (v.first == id) {
					oVersion = v.second;
				}
			}
		},
		[&](ServerModMetadata metadata) {
			id = metadata.id;
			for (ServerModVersion v : metadata.versions) {
				//geode::log::debug("version test lol {} {}", v.downloadURL, url);
				if (v.downloadURL == url) {
					oVersion = v;
				}
			}
		}
	}, mod);
	DownloadStatus status;
	if (response->ok() && oVersion.has_value()) {
		auto version = oVersion.value();
		// my recreation of m_downloadListener.bind in DownloadManager::confirm
		if (auto actualHash = calculateHash(response->data()); actualHash != version.hash) {
			geode::log::info("INVALID HASH");
			status = DownloadStatusError{.details="Hash mismatch, downloaded file did not match what was expected"};
			//serverModDownloadsList.insert_or_assign(serverMod, status);
			appendServerModDownloadsList(mod, status);
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
			geode::log::info("UNABLE TO WRITE GEODE FILE");
			status = DownloadStatusError{.details="Unable to open file"};
			//serverModDownloadsList.insert_or_assign(serverMod, status);
			appendServerModDownloadsList(mod, status);
			// oops it cant write!
			return;
		}
		file.close(); // am i writing too many comments?
		status = DownloadStatusDone{.version=version};
		//serverModDownloadsList.insert_or_assign(serverMod, status);
		//geode::log::debug("hey guys, pointcrow here, do you ever get collectors anxiety?");
		appendServerModDownloadsList(mod, status);
		return;
	}
	status = DownloadStatusError{.details=response->string().unwrapOr("Unknown error")};
	//serverModDownloadsList.insert_or_assign(serverMod, status);
	appendServerModDownloadsList(mod, status);
}

// should have called it mod!DownloadChecker
void modNotDownloadChecker(web::WebResponse* response, std::variant<Mod*, ServerModMetadata> mod, std::string_view url) {
	DownloadStatus status;
	if (response->ok()) {
		auto payload = parseServerPayload(*response);
		if (!payload) {
			// Oof
			status = DownloadStatusError{.details=payload.unwrapErr().details};
			appendServerModDownloadsList(mod, status);
			return;
		}
		auto list = ServerModVersion::parse(payload.unwrap());
		if (!list) {
			// Unable to parse response
			status = DownloadStatusError{.details=ServerError(response->code(), "Unable to parse response: {}", list.unwrapErr()).details};
			appendServerModDownloadsList(mod, status);
			return;
		}
		// yay!
		std::string modID;
		std::visit(geode::utils::makeVisitor {
			[&](Mod* mod) {
				modID = mod->getID();
			},
			[&](ServerModMetadata metadata) {
				modID = metadata.id;
			}
		}, mod);
		auto data = list.unwrap();
		appendServerModVersion(modID, data);
		status = DownloadStatusConfirm{.version=data};
		appendServerModDownloadsList(mod, status);
		return;
	}
	status = DownloadStatusError{.details=parseServerError(*response).details};
	appendServerModDownloadsList(mod, status);
}

void tagChecker(web::WebResponse* response) {
	if (response->ok()) {
		auto payload = parseServerPayload(*response);
		if (!payload) {
			return;
		}
		auto list = ServerTag::parseList(payload.unwrap());
		if (!list) {
			return;
		}
		serverTagList = list.unwrap();
	}
}

//https://api.geode-sdk.org/v1/mods/{id}/versions/{version}/download

// using GDIntercept code (kinda) so i can hook onto a SINGLE request omg (maybe more later idk)
// oops i found another request that needs hooking
// unsure about the need of this function but idc
web::WebTask doMyRequestsForMe(web::WebRequest* request, const std::string& method, const std::string& url) {
	//geode::log::debug("IS THE URL OK PLEASE BE OK {}", url);
	bool isGettingMod = false;
	bool isDownload = false;
	bool isTags = false;
	std::string modID;
	std::string sURL = url;
	//url = std::string_view(sURL);
	std::string apiURL = "https://api.geode-sdk.org/v1/mods/";
	std::string apiURL2 = "https://api.geode-sdk.org/v1/";
	// when you dont use regex
	if (sURL.starts_with(apiURL)) {
		//geode::log::debug("geode idk");
		auto nURL = sURL.substr(apiURL.size());
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
				if (slash2 == std::string::npos) slash2 = nURL.size();
				if (nURL.size() > 0) {
					auto version = nURL.substr(0, slash2);
					isGettingMod = true;
					//geode::log::debug("ver {}", version);
					if (nURL.ends_with("/download")) {
						//geode::log::debug("is download no way");
						isDownload = true;
					}
				}
			}
		}
	} else if (sURL.starts_with(apiURL2)) {
		auto nURL = sURL.substr(apiURL2.size());
		if (nURL.starts_with("detailed-tags")) {
			isTags = true;
		}
	}
	if (isDownload || isGettingMod || isTags) {
		bool shouldGoOn = false;
		std::variant<Mod*, ServerModMetadata> mod;
		if (isDownload || isGettingMod) {
			if (auto iMod = Loader::get()->getInstalledMod(modID)) { // average apple product lol
				mod = iMod;
				shouldGoOn = true;
			} else if (auto oServerMod = getServerMod(modID); oServerMod.has_value()) { // two statements, one line
				mod = oServerMod.value();
				shouldGoOn = true;
			}
		} else {
			shouldGoOn = true;
		}
		//geode::log::debug("hooking download hehehe");
		// this looks so much less complicated than in GDIntercept
		// soo its getting much worse i hate this
		if (shouldGoOn) {
			if (!isDownload && isGettingMod) {
				DownloadStatus status = DownloadStatusFetching{.percentage=0};
				appendServerModDownloadsList(mod, status);
			}
			auto newRequest = new web::WebRequest(*request);
			auto task = web::WebTask::run([request, method, url, modID, sURL, mod, isDownload, isGettingMod, isTags, newRequest](auto progress, auto cancelled) -> web::WebTask::Result {
				DownloadStatus status;

				web::WebResponse* response = nullptr;

				web::WebTask task = newRequest->send(method, url);

				task.listen([&response](const web::WebResponse* taskResponse) {
					response = new web::WebResponse(*taskResponse);
				}, [progress, cancelled, mod, &status, isDownload, isGettingMod](const web::WebProgress* taskProgress) {
					if (cancelled()) return;
					progress(*taskProgress);
					if (isDownload) {
						status = DownloadStatusDownloading{.percentage=static_cast<uint8_t>(taskProgress->downloadProgress().value_or(0))};
						appendServerModDownloadsList(mod, status);
					} else if (isGettingMod) {
						status = DownloadStatusFetching{.percentage=static_cast<uint8_t>(taskProgress->downloadProgress().value_or(0))};
						appendServerModDownloadsList(mod, status);
					}
				});

					while (!response && !cancelled()) std::this_thread::sleep_for(std::chrono::milliseconds(2)); // rest

					if (cancelled()) {
						task.cancel();

						if (isDownload) {
							status = DownloadStatusCancelled();
							appendServerModDownloadsList(mod, status);
						}

						return web::WebTask::Cancel();
					} else {
						if (isDownload) modDownloadChecker(response, mod, sURL);
						if (!isDownload && isGettingMod) modNotDownloadChecker(response, mod, sURL);
						if (isTags) tagChecker(response);

						return *response;
					}
			}, fmt::format("Proxy for {} {}", method, url));
			return task;
		}
	}
	return request->send(method, url);
}

web::WebTask GeodeWebHook(web::WebRequest* request, std::string_view method, std::string_view url) {
	//geode::log::debug("IS URL FINE? {}", url);
	//geode::log::debug("sURL {}", sURL);
	if (std::find(proxyIDList.begin(), proxyIDList.end(), request->getID()) == proxyIDList.end()) { // anti proxy thingie (no duplicates i guess?)
		proxyIDList.push_back(request->getID());
		return doMyRequestsForMe(request, std::string(method), std::string(url));
	}
	return request->send(method, url);
}

void popupRFPACHook(FLAlertLayer* self, bool cleanup) {
	if (typeinfo_cast<ModPopup*>(self)) {
		modPopupReference = nullptr;
	}
	self->removeFromParentAndCleanup(cleanup);
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

	(void) Mod::get()->hook(
		reinterpret_cast<void*>(geode::addresser::getVirtual(&FLAlertLayer::removeFromParentAndCleanup)),
		&popupRFPACHook,
		"FLAlertLayer::removeFromParentAndCleanup",
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

	listenForSettingChanges("debug-tooltips-draw", [](bool value) {
		if (value) {
			makeDebugDrawNode();
		} else {
			removeDebugDrawNode();
		}
	});
}

$on_mod(Loaded) {
	auto mod = Mod::get();

	if (mod->getSettingValue<bool>("debug-tooltips-draw")) makeDebugDrawNode();

	// guh
	tagDescriptionMap = {{"universal", "This mod affects the entire game"},
						{"gameplay", "This mod affects mainly gameplay"},
						{"editor", "This mod affects mainly the editor"},
						{"offline", "This mod does not require an internet connection to work"},
						{"online", "This mod requires an internet connection to work"},
						{"enhancement", "This mod enhances or expands upon an existing GD feature"},
						{"music", "This mod deals with music, such as adding more songs"},
						{"interface", "This mod modifies the GD UI in notable ways (beyond just adding a new button)"},
						{"bugfix", "This mod fixes existing bugs in the game"},
						{"utility", "This mod provides tools that simplify working with the game and its levels"},
						{"performance", "This mod optimizes existing GD features"},
						{"customization", "This mod adds new customization options to existing GD features"},
						{"content", "This mod adds new content (new levels, gamemodes, etc.)"},
						{"developer", "This mod is intended for mod developers only"},
						{"cheat", "This mod adds cheats like noclip"},
						{"paid", "This mod contains paid content, like a Pro tier, or if the mod acts as an installer for a fully paywalled mod"},
						{"joke", "This mod is a joke."},
						{"modtober24", "This mod is a part of the Modtober 2024 Contest"},
						{"modtober24winner", "This mod is the winner of the Modtober 2024 Contest"}};
}
