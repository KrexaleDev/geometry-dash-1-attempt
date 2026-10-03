#pragma once

#include <Geode/Geode.hpp>

#include <filesystem>

inline cocos2d::CCSprite *loadIcon(
    char const *fileName,
    float height)
{
    if (!fileName || fileName[0] == '\0')
        return nullptr;

    auto path = geode::Mod::get()->getResourcesDir() / fileName;

    if (!std::filesystem::exists(path))
    {
        geode::log::warn("Missing icon: {}", path.string());
        return nullptr;
    }

    auto sprite = cocos2d::CCSprite::create(path.string().c_str());

    if (!sprite)
    {
        geode::log::warn("Could not create sprite: {}", path.string());
        return nullptr;
    }

    auto size = sprite->getContentSize();

    if (size.height <= 0.f)
        return nullptr;

    sprite->setScale(height / size.height);

    return sprite;
}