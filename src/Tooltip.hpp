#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>

using namespace geode::prelude;

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
        if (scene) {
            scene->addChild(this);
        }
    }

    void fadeOut() {
        this->removeFromParent(); // lol
    }
};
