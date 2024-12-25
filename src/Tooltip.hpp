#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/GeodeUI.hpp>

using namespace geode::prelude;

// GDIntercept tooltip???
class Tooltip : public geode::Popup<CCLabelBMFont*, GLubyte> {
public:
    static Tooltip* create(const std::string& text, const float scale, const GLubyte opacity) {
        auto ret = new Tooltip();
        auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        label->setScale(scale);
        CCSize tooltipSize = label->getScaledContentSize() + ccp(6.f, 6.f);
        if (ret && ret->initAnchored(tooltipSize.width, tooltipSize.height, label, opacity, "square02_001.png")) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
protected:
    bool setup(CCLabelBMFont* label, GLubyte opacity) {
        this->ignoreAnchorPointForPosition(false);

        return true;
    }
};
