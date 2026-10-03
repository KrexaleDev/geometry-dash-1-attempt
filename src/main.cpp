#include <Geode/Geode.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "GitHubSync.hpp"
#include "LevelData.hpp"
#include "ListLayer.hpp"

using namespace geode::prelude;

class $modify(OneAttemptMenuLayer, MenuLayer)
{
    bool init()
    {
        if (!MenuLayer::init())
            return false;

        GitHubSync::refreshIfNeeded();

        auto icon = CCSprite::create(
            (Mod::get()->getResourcesDir() / "logo.png").string().c_str());

        if (icon)
        {
            icon->setScale(0.65f);
        }
        else
        {
            icon = CCSprite::createWithSpriteFrameName("GJ_likeBtn_001.png");
        }

        auto button = CCMenuItemSpriteExtra::create(
            icon,
            this,
            menu_selector(OneAttemptMenuLayer::onOpenList));

        button->setID("open-list-button"_spr);

        if (auto menu = this->getChildByID("bottom-menu"))
        {
            menu->addChild(button);
            menu->updateLayout();
        }

        return true;
    }

    void onOpenList(CCObject *)
    {
        OneAttemptListLayer::open();
    }
};

class $modify(OneAttemptInfoLayer, LevelInfoLayer)
{
    bool init(GJGameLevel *level, bool challenge)
    {
        if (!LevelInfoLayer::init(level, challenge))
            return false;

        if (!GitHubSync::isModerator())
            return true;

        bool inList = LevelData::contains(level->m_levelID.value());
        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto sprite = ButtonSprite::create(
            inList ? "Remove" : "Add",
            54,
            true,
            "bigFont.fnt",
            inList ? "GJ_button_06.png" : "GJ_button_01.png",
            25.f,
            0.5f);

        auto button = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(OneAttemptInfoLayer::onToggleInList));

        button->setID("one-attempt-toggle"_spr);

        float x = std::max(
            65.f,
            std::min(85.f, winSize.width * 0.17f));

        button->setPosition({x,
                             winSize.height - 22.f});

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        menu->addChild(button);

        this->addChild(menu, 100);

        return true;
    }

    void onToggleInList(CCObject *)
    {
        LevelEntry entry;
        entry.id = m_level->m_levelID.value();
        entry.stars = m_level->m_stars.value();
        entry.length = std::max(
            1,
            std::min(static_cast<int>(m_level->m_levelLength), 4));
        entry.coins = m_level->m_coins;

        auto action = LevelData::contains(entry.id)
                          ? GitHubSync::EditKind::Remove
                          : GitHubSync::EditKind::Add;

        GitHubSync::askAndEdit(action, entry);
    }
};