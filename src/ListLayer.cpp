#include "ListLayer.hpp"

#include "Icons.hpp"

#include <algorithm>

using namespace geode::prelude;

namespace
{
    // Layout

    constexpr int ROWS_PER_PAGE = 8;
    constexpr float ROW_HEIGHT = 27.f;
    constexpr float FILTER_BUTTON_HEIGHT = 21.f;

    constexpr char const *DISCORD_URL = "";

    // Filter options 

    enum FilterKind
    {
        FILTER_DIFFICULTY,
        FILTER_LENGTH,
        FILTER_STATUS,
        FILTER_COINS,
        FILTER_SORT,
    };

    constexpr int OPTION_COUNTS[5] = {9, 6, 3, 5, 6};

    char const *const DIFFICULTY_NAMES[9] = {"All", "Auto", "Easy", "Normal", "Hard", "Harder", "Insane", "Demon", "NA"};
    char const *const LENGTH_FILTER_NAMES[6] = {"All", "Short", "Medium", "Long", "XL", "?"};
    char const *const STATUS_NAMES[3] = {"All", "Done", "Not done"};
    char const *const COIN_NAMES[5] = {"Any", "0", "1", "2", "3"};
    char const *const SORT_NAMES[6] = {"ID asc", "ID desc", "Stars asc", "Stars desc", "Short 1st", "Long 1st"};

    char const *const LENGTH_NAMES[5] = {"?", "Short", "Medium", "Long", "XL"};

    // 1 Auto, 2 Easy, 3 Normal, 4 Hard, 5 Harder, 6 Insane, 7 Demon, 8 NA (unrated).
    int difficultyGroup(int stars)
    {
        switch (stars)
        {
        case 1:
            return 1;
        case 2:
            return 2;
        case 3:
            return 3;
        case 4:
        case 5:
            return 4;
        case 6:
        case 7:
            return 5;
        case 8:
        case 9:
            return 6;
        case 10:
            return 7;
        default:
            return 8;
        }
    }

    char const *const DIFFICULTY_ICONS[9] = {
        "",
        "diff_auto.png",
        "diff_easy.png",
        "diff_normal.png",
        "diff_hard.png",
        "diff_harder.png",
        "diff_insane.png",
        "diff_demon.png",
        "",
    };

    ccColor3B difficultyColor(int group)
    {
        switch (group)
        {
        case 1:
            return {255, 220, 60};
        case 2:
            return {90, 200, 255};
        case 3:
            return {120, 255, 120};
        case 4:
            return {255, 200, 60};
        case 5:
            return {255, 130, 60};
        case 6:
            return {255, 90, 200};
        case 7:
            return {255, 60, 60};
        default:
            return {200, 200, 200};
        }
    }

    // Filter state

    struct FilterState
    {
        int options[5] = {0, 0, 0, 0, 0};
        int page = 0;
    };
    FilterState g_state;

    // Helpers

    bool passesFilters(LevelEntry const &level, GameStatsManager *stats)
    {
        int difficulty = g_state.options[FILTER_DIFFICULTY];
        int length = g_state.options[FILTER_LENGTH];
        int status = g_state.options[FILTER_STATUS];
        int coins = g_state.options[FILTER_COINS];

        if (difficulty != 0 && difficultyGroup(level.stars) != difficulty)
            return false;

        if (length != 0)
        {
            int wantedLength = (length == 5) ? 0 : length;
            if (level.length != wantedLength)
                return false;
        }

        if (coins != 0 && level.coins != coins - 1)
            return false;

        if (status != 0)
        {
            bool done = stats->hasCompletedOnlineLevel(level.id);
            bool wantDone = (status == 1);
            if (done != wantDone)
                return false;
        }
        return true;
    }

    void sortLevels(std::vector<LevelEntry> &levels, int mode)
    {
        auto sortBy = [&](auto key, bool ascending)
        {
            std::stable_sort(levels.begin(), levels.end(), [&](LevelEntry const &a, LevelEntry const &b)
                             { return ascending ? key(a) < key(b) : key(a) > key(b); });
        };

        switch (mode)
        {
        case 1:
            sortBy([](LevelEntry const &l)
                   { return l.id; }, false);
            break;
        case 2:
            sortBy([](LevelEntry const &l)
                   { return l.stars; }, true);
            break;
        case 3:
            sortBy([](LevelEntry const &l)
                   { return l.stars; }, false);
            break;
        case 4:
            sortBy([](LevelEntry const &l)
                   { return l.length; }, true);
            break;
        case 5:
            sortBy([](LevelEntry const &l)
                   { return l.length; }, false);
            break;
        default:
            break;
        }
    }

    float fitScale(CCNode *label, float maxWidth, float preferredScale)
    {
        float width = label->getContentSize().width;
        if (width > 0.f && width * preferredScale > maxWidth)
        {
            return maxWidth / width;
        }
        return preferredScale;
    }

    void openLevelPage(int levelId)
    {
        auto search = GJSearchObject::create(SearchType::Search, std::to_string(levelId));
        auto scene = LevelBrowserLayer::scene(search);
        CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(0.5f, scene));
    }

    struct RowBuilder
    {
        CCNode *parent;
        float left;
        float y;

        void addText(std::string const &text, float x, ccColor3B color, float scale)
        {
            auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
            label->setAnchorPoint({0.f, 0.5f});
            label->setScale(scale);
            label->setColor(color);
            label->setPosition({left + x, y});
            parent->addChild(label, 1);
        }

        bool addIcon(char const *fileName, float height, float x)
        {
            auto icon = loadIcon(fileName, height);
            if (!icon)
                return false;
            icon->setPosition({left + x, y});
            parent->addChild(icon, 1);
            return true;
        }
    };
}

// Creating and closing the screen

void OneAttemptListLayer::open()
{
    auto layer = OneAttemptListLayer::create();
    if (!layer)
        return;

    auto scene = CCScene::create();
    scene->addChild(layer);
    CCDirector::sharedDirector()->pushScene(CCTransitionFade::create(0.5f, scene));
}

OneAttemptListLayer *OneAttemptListLayer::create()
{
    auto layer = new OneAttemptListLayer();
    if (layer && layer->init())
    {
        layer->autorelease();
        return layer;
    }
    CC_SAFE_DELETE(layer);
    return nullptr;
}

OneAttemptListLayer::~OneAttemptListLayer()
{
    GitHubSync::clearListener(this);
}

bool OneAttemptListLayer::init()
{
    if (!CCLayer::init())
        return false;

    auto winSize = CCDirector::sharedDirector()->getWinSize();

    this->addBackground();
    this->addTitle();
    this->addBackButton();
    this->addFilterButtons();
    this->addPageButtons();
    this->addDiscordButton();

    m_listNode = CCNode::create();
    this->addChild(m_listNode, 1);

    m_pageLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_pageLabel->setScale(0.4f);
    m_pageLabel->setPosition({winSize.width / 2.f, 44.f});
    this->addChild(m_pageLabel, 2);

    GitHubSync::setListener(this);
    this->buildModeratorTools();

    this->setKeypadEnabled(true);
    this->updateFilterButtons();
    this->refreshList();
    return true;
}

void OneAttemptListLayer::keyBackClicked()
{
    this->onBack(nullptr);
}

void OneAttemptListLayer::onBack(CCObject *)
{
    CCDirector::sharedDirector()->popSceneWithTransition(0.5f, PopTransition::kPopTransitionFade);
}

// Building the fixed parts of the screen

void OneAttemptListLayer::addBackground()
{
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    auto background = CCSprite::create("GJ_gradientBG.png");
    if (!background)
        return;

    auto size = background->getContentSize();
    background->setAnchorPoint({0.f, 0.f});
    if (size.width > 0.f)
        background->setScaleX(winSize.width / size.width);
    if (size.height > 0.f)
        background->setScaleY(winSize.height / size.height);
    background->setColor({18, 50, 110});
    this->addChild(background, -10);
}

void OneAttemptListLayer::addTitle()
{
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    auto title = CCLabelBMFont::create("Geometry Dash 1 Attempt", "bigFont.fnt");
    title->setScale(0.7f);
    title->setPosition({winSize.width / 2.f, winSize.height - 20.f});
    this->addChild(title);
}

void OneAttemptListLayer::addBackButton()
{
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});

    auto arrow = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
    if (arrow)
    {
        auto button = CCMenuItemSpriteExtra::create(arrow, this, menu_selector(OneAttemptListLayer::onBack));
        button->setPosition({27.f, winSize.height - 25.f});
        menu->addChild(button);
    }
    this->addChild(menu, 5);
}

void OneAttemptListLayer::addFilterButtons()
{
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    float leftLimit = 52.f;
    float rightLimit = winSize.width - 10.f;

    float gap = 4.f;
    float availableWidth = std::max(250.f, rightLimit - leftLimit);
    float slotWidth = std::min((availableWidth - gap * (FILTER_COUNT - 1)) / FILTER_COUNT, 90.f);
    float totalWidth = slotWidth * FILTER_COUNT + gap * (FILTER_COUNT - 1);

    float firstCenterX = (leftLimit + rightLimit) / 2.f - totalWidth / 2.f + slotWidth / 2.f;
    float y = winSize.height - 40.f;

    float sizeFactor = 0.9f;

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});

    for (int i = 0; i < FILTER_COUNT; i++)
    {
        auto background = ButtonSprite::create(
            " ", static_cast<int>(slotWidth), true, "bigFont.fnt", "GJ_button_01.png", 19.f, 0.35f);
        m_filterBackgrounds[i] = background;

        m_filterContents[i] = CCNode::create();
        background->addChild(m_filterContents[i], 5);

        auto button = CCMenuItemSpriteExtra::create(background, this, menu_selector(OneAttemptListLayer::onFilter));
        button->setTag(i);

        float realWidth = background->getContentSize().width;
        float scale = (realWidth > slotWidth) ? slotWidth / realWidth : 1.f;
        button->setScale(scale * sizeFactor);

        button->setPosition({firstCenterX + i * (slotWidth + gap), y});
        menu->addChild(button);
    }
    this->addChild(menu, 5);
}

void OneAttemptListLayer::addPageButtons()
{
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    float centerX = winSize.width / 2.f;

    int jumps[6] = {-100, -10, -1, 1, 10, 100};
    char const *texts[6] = {"-100", "-10", "-1", "+1", "+10", "+100"};
    float offsets[6] = {-120.f, -75.f, -30.f, 30.f, 75.f, 120.f};

    float scale = std::min(1.f, (winSize.width - 24.f) / 320.f);

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});

    for (int i = 0; i < 6; i++)
    {
        auto sprite = ButtonSprite::create(texts[i], 24, true, "bigFont.fnt", "GJ_button_01.png", 18.f, 0.35f);
        auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(OneAttemptListLayer::onPage));
        button->setTag(jumps[i]);
        button->setScale(scale);
        button->setPosition({centerX + offsets[i] * scale, 22.f});
        menu->addChild(button);
    }
    this->addChild(menu, 5);
}

void OneAttemptListLayer::addDiscordButton()
{
    if (DISCORD_URL[0] == '\0')
        return;

    auto icon = loadIcon("discord.png", 24.f);
    if (!icon)
        return;

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});

    auto button = CCMenuItemSpriteExtra::create(icon, this, menu_selector(OneAttemptListLayer::onDiscord));
    button->setPosition({24.f, 24.f});
    menu->addChild(button);
    this->addChild(menu, 5);
}

void OneAttemptListLayer::onDiscord(CCObject *)
{
    CCApplication::sharedApplication()->openURL(DISCORD_URL);
}

// Moderator tools

void OneAttemptListLayer::buildModeratorTools()
{
    if (m_moderatorNode)
    {
        m_moderatorNode->removeFromParent();
        m_moderatorNode = nullptr;
        m_idInput = nullptr;
    }
    if (!GitHubSync::isModerator())
        return;

    auto winSize = CCDirector::sharedDirector()->getWinSize();
    m_moderatorNode = CCNode::create();

    float inputWidth = std::min(80.f, winSize.width * 0.18f);
    float goWidth = 40.f;
    float gap = 2.f;
    float rightMargin = 18.f;
    float left = winSize.width - rightMargin - (inputWidth + gap + goWidth);
    float y = winSize.height - 22.f;

    m_idInput = TextInput::create(inputWidth, "Level ID", "bigFont.fnt");
    m_idInput->setFilter("0123456789");
    m_idInput->setMaxCharCount(9);
    m_idInput->setPosition({left + inputWidth / 2.f, y});
    m_moderatorNode->addChild(m_idInput);

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});

    auto goSprite = ButtonSprite::create("Go", 28, true, "bigFont.fnt", "GJ_button_01.png", 18.f, 0.35f);
    auto goButton = CCMenuItemSpriteExtra::create(goSprite, this, menu_selector(OneAttemptListLayer::onGoToId));
    goButton->setPosition({left + inputWidth + gap + goWidth / 2.f, y});
    menu->addChild(goButton);
    m_moderatorNode->addChild(menu);

    this->addChild(m_moderatorNode, 6);
}

void OneAttemptListLayer::onGoToId(CCObject *)
{
    if (!m_idInput)
        return;

    std::string text(m_idInput->getString());
    if (text.empty())
        return;

    openLevelPage(std::atoi(text.c_str()));
}

void OneAttemptListLayer::onRemove(CCObject *sender)
{
    LevelEntry level;
    level.id = static_cast<CCNode *>(sender)->getTag();
    GitHubSync::askAndEdit(GitHubSync::EditKind::Remove, level);
}

// Reacting to changes from GitHubSync

void OneAttemptListLayer::onLevelsChanged()
{
    this->refreshList();
}

void OneAttemptListLayer::onModeratorChanged()
{
    this->buildModeratorTools();
    this->refreshList();
}

// Filter buttons

void OneAttemptListLayer::updateFilterButtons()
{
    std::string values[FILTER_COUNT] = {
        DIFFICULTY_NAMES[g_state.options[FILTER_DIFFICULTY]],
        LENGTH_FILTER_NAMES[g_state.options[FILTER_LENGTH]],
        STATUS_NAMES[g_state.options[FILTER_STATUS]],
        COIN_NAMES[g_state.options[FILTER_COINS]],
        SORT_NAMES[g_state.options[FILTER_SORT]],
    };

    char const *prefixes[FILTER_COUNT] = {"Diff: ", "Len: ", "Status: ", "Coins: ", "Sort: "};

    char const *icons[FILTER_COUNT] = {
        DIFFICULTY_ICONS[g_state.options[FILTER_DIFFICULTY]],
        "clock.png",
        "",
        "coin.png",
        "",
    };

    for (int i = 0; i < FILTER_COUNT; i++)
    {
        auto content = m_filterContents[i];
        content->removeAllChildren();

        auto size = m_filterBackgrounds[i]->getContentSize();
        float centerY = size.height / 2.f;

        auto icon = loadIcon(icons[i], 14.f);
        if (icon)
        {
            icon->setPosition({11.f, centerY});
            content->addChild(icon);
        }

        std::string text = icon ? values[i] : std::string(prefixes[i]) + values[i];
        auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");

        float maxTextWidth = icon ? size.width - 28.f : size.width - 8.f;
        label->setScale(fitScale(label, maxTextWidth, 0.38f));
        label->setPosition({icon ? 22.f + maxTextWidth / 2.f : size.width / 2.f, centerY});
        content->addChild(label);
    }
}

void OneAttemptListLayer::onFilter(CCObject *sender)
{
    int filter = static_cast<CCNode *>(sender)->getTag();
    if (filter < 0 || filter >= FILTER_COUNT)
        return;

    int &option = g_state.options[filter];
    option = (option + 1) % OPTION_COUNTS[filter];

    g_state.page = 0;
    this->updateFilterButtons();
    this->refreshList();
}

void OneAttemptListLayer::onPage(CCObject *sender)
{
    g_state.page += static_cast<CCNode *>(sender)->getTag();
    this->refreshList();
}

// The list

void OneAttemptListLayer::applyFilters()
{
    m_shownLevels.clear();

    auto stats = GameStatsManager::sharedState();
    for (auto const &level : LevelData::all())
    {
        if (passesFilters(level, stats))
        {
            m_shownLevels.push_back(level);
        }
    }
    sortLevels(m_shownLevels, g_state.options[FILTER_SORT]);
}

void OneAttemptListLayer::refreshList()
{
    this->applyFilters();

    int total = static_cast<int>(m_shownLevels.size());
    int pageCount = std::max(1, (total + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE);
    g_state.page = std::max(0, std::min(g_state.page, pageCount - 1));

    m_listNode->removeAllChildren();
    this->showEmptyMessage(total);

    auto buttonMenu = CCMenu::create();
    buttonMenu->setPosition({0.f, 0.f});
    m_listNode->addChild(buttonMenu, 2);

    for (int slot = 0; slot < ROWS_PER_PAGE; slot++)
    {
        int index = g_state.page * ROWS_PER_PAGE + slot;
        if (index >= total)
            break;
        this->addLevelRow(slot, m_shownLevels[index], buttonMenu);
    }

    m_pageLabel->setString((
                               "Page " + std::to_string(g_state.page + 1) + " / " + std::to_string(pageCount) +
                               "  (" + std::to_string(total) + " levels)")
                               .c_str());
}

void OneAttemptListLayer::showEmptyMessage(int shownCount)
{
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    char const *text = nullptr;
    ccColor3B color = {255, 255, 255};

    if (LevelData::all().empty())
    {
        text = "levels.csv not found or empty";
        color = {255, 90, 90};
    }
    else if (shownCount == 0)
    {
        text = "No level matches these filters";
    }
    if (!text)
        return;

    auto label = CCLabelBMFont::create(text, "bigFont.fnt");
    label->setScale(0.5f);
    label->setColor(color);
    label->setPosition({winSize.width / 2.f, winSize.height / 2.f});
    m_listNode->addChild(label);
}

void OneAttemptListLayer::addLevelRow(int slot, LevelEntry const &level, CCMenu *buttonMenu)
{
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    float rowWidth = std::min(480.f, std::max(300.f, winSize.width - 20.f));
    float left = winSize.width / 2.f - rowWidth / 2.f;
    float y = winSize.height - 78.f - slot * ROW_HEIGHT;

    float s = rowWidth / 480.f;
    float idX = 12.f * s;
    float difficultyX = 105.f * s;
    float starsX = 145.f * s;
    float lengthX = 205.f * s;
    float coinsX = 275.f * s;
    float statusX = 340.f * s;
    float viewButtonX = rowWidth - 58.f;
    float removeButtonX = rowWidth - 16.f;

    auto strip = CCLayerColor::create(ccc4(0, 0, 0, 100), rowWidth, ROW_HEIGHT - 2.f);
    strip->setPosition({left, y - (ROW_HEIGHT - 2.f) / 2.f});
    m_listNode->addChild(strip);

    RowBuilder row{m_listNode, left, y};
    ccColor3B white = {255, 255, 255};
    ccColor3B grey = {170, 170, 170};

    int group = difficultyGroup(level.stars);
    bool done = GameStatsManager::sharedState()->hasCompletedOnlineLevel(level.id);

    if (row.addIcon("id.png", 11.f, idX))
    {
        row.addText(std::to_string(level.id), idX + 12.f, white, 0.35f);
    }
    else
    {
        row.addText("#" + std::to_string(level.id), idX, white, 0.35f);
    }

    if (!row.addIcon(DIFFICULTY_ICONS[group], 16.f, difficultyX))
    {
        row.addText(DIFFICULTY_NAMES[group], difficultyX - 12.f, difficultyColor(group), 0.32f);
    }

    if (row.addIcon("star.png", 10.f, starsX))
    {
        row.addText(std::to_string(level.stars), starsX + 9.f, white, 0.35f);
    }
    else
    {
        row.addText(std::to_string(level.stars) + "*", starsX, difficultyColor(group), 0.35f);
    }

    char const *lengthName = LENGTH_NAMES[std::max(0, std::min(level.length, 4))];
    if (row.addIcon("clock.png", 12.f, lengthX))
    {
        row.addText(lengthName, lengthX + 10.f, white, 0.32f);
    }
    else
    {
        row.addText(lengthName, lengthX, white, 0.32f);
    }

    if (level.coins <= 0)
    {
        row.addText("-", coinsX, grey, 0.35f);
    }
    else
    {
        bool iconsWorked = true;
        for (int coin = 0; coin < std::min(level.coins, 3); coin++)
        {
            if (!row.addIcon("coin.png", 11.f, coinsX + coin * 14.f))
            {
                iconsWorked = false;
                break;
            }
        }
        if (!iconsWorked)
        {
            row.addText("Coins: " + std::to_string(level.coins), coinsX, {255, 220, 60}, 0.32f);
        }
    }

    auto statusLabel = CCLabelBMFont::create(done ? "Done" : "Not done", "bigFont.fnt");
    float maxStatusWidth = viewButtonX - statusX - 38.f;
    statusLabel->setAnchorPoint({0.f, 0.5f});
    statusLabel->setScale(fitScale(statusLabel, maxStatusWidth, 0.31f));
    statusLabel->setColor(done ? ccColor3B{90, 255, 90} : grey);
    statusLabel->setPosition({left + statusX, y});
    m_listNode->addChild(statusLabel, 1);

    float buttonScale = std::min(1.f, rowWidth / 420.f);

    auto viewSprite = ButtonSprite::create("View", 25, true, "bigFont.fnt", "GJ_button_01.png", 18.f, 0.35f);
    auto viewButton = CCMenuItemSpriteExtra::create(viewSprite, this, menu_selector(OneAttemptListLayer::onView));
    viewButton->setTag(level.id);
    viewButton->setScale(buttonScale);
    viewButton->setPosition({left + viewButtonX, y});
    buttonMenu->addChild(viewButton);

    if (GitHubSync::isModerator())
    {
        auto removeSprite = ButtonSprite::create("X", 18, true, "bigFont.fnt", "GJ_button_06.png", 18.f, 0.35f);
        auto removeButton = CCMenuItemSpriteExtra::create(removeSprite, this, menu_selector(OneAttemptListLayer::onRemove));
        removeButton->setTag(level.id);
        removeButton->setScale(buttonScale);
        removeButton->setPosition({left + removeButtonX, y});
        buttonMenu->addChild(removeButton);
    }
}

void OneAttemptListLayer::onView(CCObject *sender)
{
    openLevelPage(static_cast<CCNode *>(sender)->getTag());
}