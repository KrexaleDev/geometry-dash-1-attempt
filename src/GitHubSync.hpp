#pragma once

#include "LevelData.hpp"

class SyncListener
{
public:
    virtual ~SyncListener() = default;

    virtual void onLevelsChanged() = 0;
    virtual void onModeratorChanged() = 0;
};

namespace GitHubSync
{

    enum class EditKind
    {
        Add,
        Remove
    };

    bool isModerator();

    void setListener(SyncListener *listener);
    void clearListener(SyncListener *listener);

    void refreshIfNeeded();

    void askAndEdit(EditKind kind, LevelEntry entry);

}