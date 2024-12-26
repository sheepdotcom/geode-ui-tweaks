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

        // thanks alphalaneous for letting me know about this method
        this->ignoreAnchorPointForPosition(false);
        m_mainLayer = CCNode::create();
        m_mainLayer->setContentSize({maxWidth, 20});
        m_mainLayer->setZOrder(2);

        RowLayout* layout = RowLayout::create();
        layout->setAxisAlignment(AxisAlignment::Start);
        layout->setCrossAxisAlignment(AxisAlignment::End);
        layout->setGrowCrossAxis(true);
        layout->setAutoScale(false);
        layout->setGap(0);
        m_mainLayer->setLayout(layout);

        std::vector<std::string> splitText = geode::utils::string::split(text, " ");

        for (std::string t : splitText) {
            CCLabelBMFont* partLabel = CCLabelBMFont::create(fmt::format("{} ", t).c_str(), "bigFont.fnt");
            partLabel->setScale(scale);
            m_mainLayer->addChild(partLabel);
        }

        m_mainLayer->updateLayout();

        // lol
        float farthestRight = 0.f;
        auto arr = CCArrayExt<CCNode*>(m_mainLayer->getChildren());
        for (CCNode* a : arr) {
            float right = a->getPositionX() + (a->getScaledContentWidth() * (1.f - a->getAnchorPoint().x));
            if (right > farthestRight) {
                farthestRight = right;
            } else {
                break;
            }
        }

        this->addChild(m_mainLayer);

        CCSize size = ccp(farthestRight * m_mainLayer->getScale(), m_mainLayer->getScaledContentHeight()) + ccp(6.f, 6.f);

        this->setContentSize(size);
        m_bg = CCScale9Sprite::create("square02_001.png");
        m_bg->setScale(0.2f);
        m_bg->setOpacity(opacity);
        m_bg->setScaledContentSize(size);
        m_bg->setPosition(size * m_bg->getAnchorPoint());
        this->addChild(m_bg);

        m_mainLayer->setPosition({3.f, 3.f});

        return true;
    }

public:
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
