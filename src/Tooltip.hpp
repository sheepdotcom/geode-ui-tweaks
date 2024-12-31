#pragma once
#include "Geode/cocos/CCDirector.h"
#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>

using namespace geode::prelude;

struct TooltipMetadata { // guh
	CCNode* node;
	std::string text;
	bool zOrderCheck = true;
	std::optional<CCRect> limitedArea = std::nullopt;

	TooltipMetadata() = default;
	TooltipMetadata(CCNode* node, std::string text) : node(node), text(text) {}
	TooltipMetadata(CCNode* node, std::string text, bool zOrderCheck) : node(node), text(text), zOrderCheck(zOrderCheck) {}
	TooltipMetadata(CCNode* node, std::string text, std::optional<CCRect> limitedArea) : node(node), text(text), limitedArea(limitedArea) {}
	TooltipMetadata(CCNode* node, std::string text, bool zOrderCheck, std::optional<CCRect> limitedArea) : node(node), text(text), zOrderCheck(zOrderCheck), limitedArea(limitedArea) {}
};

// GDIntercept tooltip??? WRONG! MY TOOLTIP!
class Tooltip : public CCNode {
private:
    // oops
    bool init(CCNode* nodeFrom, const std::string& text, const float scale, const float maxWidth, GLubyte opacity) {
        if (!CCNode::init()) return false;

        m_nodeFrom = nodeFrom;
        m_text = text;
        m_textScale = scale;
        m_maxWidth = maxWidth;
        m_opacity = opacity;

        this->ignoreAnchorPointForPosition(false);
        m_mainLayer = CCNode::create();
        m_mainLayer->setContentSize({m_maxWidth, 20});
        m_mainLayer->setZOrder(2);

        // thanks alphalaneous for letting me know about this method
        RowLayout* layout = RowLayout::create();
        layout->setAxisAlignment(AxisAlignment::Start);
        layout->setCrossAxisAlignment(AxisAlignment::End);
        layout->setGrowCrossAxis(true);
        layout->setAutoScale(false);
        layout->setGap(0);
        m_mainLayer->setLayout(layout);
        m_mainLayer->setPosition({3.f, 3.f});
        this->addChild(m_mainLayer);

        m_bg = CCScale9Sprite::create("square02_001.png");
        m_bg->setScale(0.2f);
        m_bg->setOpacity(m_opacity);
        this->addChild(m_bg);

        this->setString(m_text);

        // lol
        // float farthestRight = 0.f;
        // auto arr = CCArrayExt<CCNode*>(m_mainLayer->getChildren());
        // for (CCNode* a : arr) {
        //     float right = a->getPositionX() + (a->getScaledContentWidth() * (1.f - a->getAnchorPoint().x));
        //     if (right > farthestRight) {
        //         farthestRight = right;
        //     } else {
        //         break;
        //     }
        // }

        return true;
    }

    CCNode* getTopNodeFromNode(CCNode* node) { // code duplication goes crazy
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

public:
    std::string m_text;
    float m_textScale;
    float m_maxWidth;
    GLubyte m_opacity;
    CCScale9Sprite* m_bg;
    CCNode* m_mainLayer;
    CCNode* m_nodeFrom;

    ~Tooltip();

    static Tooltip* create(CCNode* nodeFrom, const std::string& text, const float scale, const float maxWidth, const GLubyte opacity) {
        auto ret = new Tooltip();
        if (ret && ret->init(nodeFrom, text, scale, maxWidth, opacity)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    void setString(std::string text) {
        std::vector<std::string> splitText = geode::utils::string::split(text, " ");

        size_t i = 0;
        float currentWidth = 0.f;
        float farthestRight = 0.f;
        bool hadToOverflow = false;
        std::string prevStr2;
        CCLabelBMFont* prevLabel = nullptr;
        for (std::string t : splitText) {
            std::string str = fmt::format("{} ", t);
            std::string str2 = fmt::format("{}", t); // non-space version because
            std::string theText = (((splitText.size() - 1) <= i) ? str2 : str);
            CCLabelBMFont* partLabel = CCLabelBMFont::create(theText.c_str(), "bigFont.fnt");
            partLabel->setScale(m_textScale);

            // DO NOT USE THIS CODE AS REFERENCE, IT IS TERRIBLE AND JUST SIMPLY MAKE TEXT AND PUT THEM IN A LAYOUT
            // this looks weird but its just to remove the space when wrapping text lol
            if (theText == str && currentWidth + partLabel->getScaledContentWidth() > m_maxWidth) {
                if (str2.size() <= 0) {
                    continue; // skip because there is no need to generate a label with no text (the label will autorelease, i dont want to deal with memory management right now ok)
                }
                theText = str2;
                partLabel->setString(theText.c_str());
            }
            // test if removing the space still doesnt let it fit
            if (theText == str2 && currentWidth + partLabel->getScaledContentWidth() > m_maxWidth) {
                hadToOverflow = true;
                partLabel->setString(str.c_str()); // let it be wrapped around
                auto lastStr = std::string(prevLabel->getString());
                if (lastStr.size() > 0 && lastStr.ends_with(" ") && prevLabel) {
                    if (prevStr2.size() <= 0) {
                        prevLabel->removeFromParent(); // empty label must be gone
                    } else {
                        prevLabel->setString(prevStr2.c_str());
                    }
                }
                currentWidth = 0.f; // set to 0 here so it later is set to the partLabel's width
            }
            currentWidth += partLabel->getScaledContentWidth();
            if (currentWidth > farthestRight) {
                farthestRight = currentWidth;
            }
            m_mainLayer->addChild(partLabel);
            prevStr2 = str2; // hehe
            prevLabel = partLabel;
            i++;
        }

        m_mainLayer->updateLayout();

        CCSize size = ccp(farthestRight * m_mainLayer->getScale(), m_mainLayer->getScaledContentHeight()) + ccp(6.f, 6.f);

        this->setContentSize(size);

        m_bg->setScaledContentSize(size);
        m_bg->setPosition(size * m_bg->getAnchorPoint());
    }

    void show(CCScene* scene = nullptr) {
        // does anyone do nullptr checks on this?
        if (!scene) {
            scene = CCDirector::sharedDirector()->getRunningScene();
        }
        CCNode* topNode = this->getTopNodeFromNode(m_nodeFrom);
        if (scene && topNode) {
            scene->addChild(this, topNode->getZOrder());
        }
    }

    void fadeOut() {
        this->removeFromParent(); // lol
    }
};

// i dont think this would work :(
// // GDIntercept what the hell is this?
// template<typename T>
// concept Node = std::is_base_of<CCNode, T>::value;

// template<Node T = CCNode>
// class HoverNode : public T {
// protected:
//     bool m_hovered = false;
//     TooltipMetadata m_metadata;
//     Tooltip* m_tooltip = nullptr;


//     ~HoverNode() {
//         this->unHover();
//     }

//     virtual bool init() override {
//         if (!CCNode::init()) return false;

//         this->scheduleUpdate();

//         return true;
//     }

//     virtual void onHover(CCScene* scene = nullptr) {
//         m_tooltip = Tooltip::create(m_metadata.node, m_metadata.text, 0.2f, 300.f, 150.f);
//         m_tooltip->setID("Tooltip"_spr);
//         m_tooltip->show(scene);
//     }

//     virtual void unHover() {
//         if (m_tooltip) {
//             m_tooltip->fadeOut();
//         }
//         m_tooltip = nullptr;
//     }

//     CCNode* getTopNodeFromNode(CCNode* node) {
//         if (node) {
//             if (auto p = node->getParent()) {
//                 if (typeinfo_cast<CCScene*>(p)) {
//                     return node;
//                 } else {
//                     return getTopNodeFromNode(p);
//                 }
//             }
//         }
//         return nullptr;
//     }

//     CCScene* getSceneFromNode(CCNode* node) {
//         CCNode* topNode = getTopNodeFromNode(node);
//         if (topNode) {
//             if (auto p = topNode->getParent()) {
//                 if (typeinfo_cast<CCScene*>(p)) {
//                     return static_cast<CCScene*>(p);
//                 }
//             }
//         }
//         return nullptr;
//     }

//     CCSize getRecursiveScale(CCNode* node, const CCSize accumulator) {
//         if (node->getParent()) {
//             return getRecursiveScale(node->getParent(), {accumulator.width * node->getScaleX(), accumulator.height * node->getScaleY()});
//         } else {
//             return accumulator;
//         }
//     }

//     void update(float delta) override {
//         auto winSize = CCDirector::sharedDirector()->getWinSize();

//         CCNode* node = m_metadata.node;
//         std::string text = m_metadata.text;
//         bool zOrderCheck = m_metadata.zOrderCheck;
//         std::optional<CCRect> oLimitedArea = m_metadata.limitedArea;
//         CCRect limitedArea = oLimitedArea.has_value() ? oLimitedArea.value() : CCRect{{0, 0}, winSize};
//         bool alreadyHovered = false;

//         const CCPoint trueScale = getRecursiveScale(node, {1.f, 1.f});

//         const CCPoint mousePos = geode::cocos::getMousePos();
//         const CCPoint bottomLeft = node->convertToWorldSpace(node->getAnchorPoint());
//         const CCPoint topRight = bottomLeft + node->getContentSize() * trueScale;

//         //const CCPoint tooltipPos = {(bottomLeft.x + topRight.x) / 2, topRight.y};
//         CCPoint tooltipPos = ccp(mousePos.x + 5.f, mousePos.y - 5.f);

//         CCNode* topNode = getTopNodeFromNode(node);
//         CCScene* scene = getSceneFromNode(topNode);

//         CCDrawNode* debugDrawNode = static_cast<CCDrawNode*>(CCDirector::sharedDirector()->getRunningScene()->getChildByID("debug-draw-node"_spr));
//         if (!debugDrawNode) {
//             geode::log::debug("no draw node hmm");
//             debugDrawNode = static_cast<CCDrawNode*>(scene->getChildByID("debug-draw-node"_spr));
//             if (debugDrawNode) {
//                 geode::log::debug("draw node get off the other scene lol");
//             }
//         }

//         bool isCoveredUp = false;
//         if (zOrderCheck && scene && topNode) {
//             auto arr = CCArrayExt<CCNode*>(scene->getChildren());
//             std::reverse(arr.begin(), arr.end());
//             for (CCNode* n : arr) {
//                 // cocos should guarantee that everything is sorted by zOrder (the one case is if someone is REALLY REALLY REALLY stupid and somehow manually appends a child without using addChild which auto re-orders everything)
//                 if (n->getZOrder() < topNode->getZOrder()) {
//                     break;
//                 }
//                 // check only for FLAlertLayer
//                 if (typeinfo_cast<FLAlertLayer*>(n) && n->getZOrder() > topNode->getZOrder()) {
//                     isCoveredUp = true;
//                     break;
//                 }
//             }
//             std::reverse(arr.begin(), arr.end());
//         }

//         bool withinLimitedArea = false;
//         if (mousePos >= limitedArea.origin && mousePos <= limitedArea.origin + limitedArea.size) {
//             withinLimitedArea = true;
//         }

//         if (mod->getSettingValue<bool>("debug-tooltips-draw") && debugDrawNode) {
//             CCPoint maxTopRight = limitedArea.origin + limitedArea.size;
//             bool shouldDrawDebug = true;
//             if (isCoveredUp) {
//                 shouldDrawDebug = false;
//             }
//             if (topRight.x < limitedArea.origin.x || bottomLeft.x > maxTopRight.x) {
//                 shouldDrawDebug = false;
//             } else if (topRight.y < limitedArea.origin.y || bottomLeft.y > maxTopRight.y) {
//                 shouldDrawDebug = false;
//             }
//             if (shouldDrawDebug) {
//                 CCPoint dTopRight = ccp(std::fmin(topRight.x, maxTopRight.x), std::fmin(topRight.y, maxTopRight.y));
//                 CCPoint dBottomLeft = ccp(std::fmax(bottomLeft.x, limitedArea.origin.x), std::fmax(bottomLeft.y, limitedArea.origin.y));
//                 CCPoint dTopLeft = ccp(dBottomLeft.x, dTopRight.y);
//                 CCPoint dBottomRight = ccp(dTopRight.x, dBottomLeft.y);
//                 float thickness = .5f;
//                 debugDrawNode->drawSegment(dTopLeft, dTopRight, thickness, debugColor);
//                 debugDrawNode->drawSegment(dTopRight, dBottomRight, thickness, debugColor);
//                 debugDrawNode->drawSegment(dBottomRight, dBottomLeft, thickness, debugColor);
//                 debugDrawNode->drawSegment(dBottomLeft, dTopLeft, thickness, debugColor);
//             }
//         }

//         if ((mousePos >= bottomLeft && mousePos <= topRight) && withinLimitedArea && !isCoveredUp) {
//             if (!alreadyHovered) {
//                 this->onHover(scene);
//             }
//             if (m_tooltip) {
//                 // make it go on the left side if it goes offscreen on the right
//                 bool left = false;
//                 float width = m_tooltip->m_bg->getScaledContentWidth();
//                 if (tooltipPos.x + width > winSize.width) {
//                     left = true;
//                 }
//                 // do something else if it goes offscreen on left aswell
//                 if (left && tooltipPos.x - 10.f - width < 0.f) {
//                     left = false;
//                     tooltipPos = ccp(winSize.width - width, tooltipPos.y);
//                 }
//                 m_tooltip->setPosition(ccp(tooltipPos.x - (left ? 10.f : 0.f), tooltipPos.y));
//                 m_tooltip->setAnchorPoint(ccp(left ? 1.f : 0.f, 0.f));
//             }
//         } else if (alreadyHovered && m_tooltip) {
//             m_tooltip->fadeOut();
//         }
//     }
// };