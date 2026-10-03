#pragma once

#include <Geode/Geode.hpp>

#include "GitHubSync.hpp"
#include "LevelData.hpp"

class OneAttemptListLayer : public cocos2d::CCLayer, public SyncListener
{
public:
    static void open();

    ~OneAttemptListLayer() override;

    void onLevelsChanged() override;
    void onModeratorChanged() override;

private:
    static OneAttemptListLayer *create();

    bool init() override;
    void keyBackClicked() override;

    void addBackground();
    void addTitle();
    void addBackButton();
    void addFilterButtons();
    void addPageButtons();
    void addDiscordButton();

    void buildModeratorTools();
    void updateFilterButtons();

    void applyFilters();
    void refreshList();
    void showEmptyMessage(int shownCount);
    void addLevelRow(
        int slot,
        LevelEntry const &level,
        cocos2d::CCMenu *buttonMenu);

    void onBack(cocos2d::CCObject *);
    void onDiscord(cocos2d::CCObject *);
    void onFilter(cocos2d::CCObject *sender);
    void onPage(cocos2d::CCObject *sender);
    void onView(cocos2d::CCObject *sender);
    void onRemove(cocos2d::CCObject *sender);
    void onGoToId(cocos2d::CCObject *);

    static constexpr int FILTER_COUNT = 5;

    std::vector<LevelEntry> m_shownLevels;

    cocos2d::CCNode *m_listNode = nullptr;
    cocos2d::CCNode *m_moderatorNode = nullptr;
    geode::TextInput *m_idInput = nullptr;
    cocos2d::CCLabelBMFont *m_pageLabel = nullptr;

    ButtonSprite *m_filterBackgrounds[FILTER_COUNT] = {};
    cocos2d::CCNode *m_filterContents[FILTER_COUNT] = {};
};