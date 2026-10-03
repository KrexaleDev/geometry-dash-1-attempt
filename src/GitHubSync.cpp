#include "GitHubSync.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <memory>

using namespace geode::prelude;

namespace
{
    constexpr char const *REPO_OWNER = "KrexaleDev";
    constexpr char const *REPO_NAME = "geometry-dash-1-attempt";
    constexpr char const *BRANCH = "main";
    constexpr char const *FILE_PATH = "levels.csv";

    constexpr std::time_t REFRESH_INTERVAL_SECONDS = 600;
    constexpr int MAX_EDIT_RETRIES = 2;

    bool g_isModerator = false;
    bool g_editInProgress = false;
    bool g_initialised = false;
    std::string g_lastToken;
    std::time_t g_lastDownload = 0;
    SyncListener *g_listener = nullptr;

    void notifyLevelsChanged()
    {
        if (g_listener)
            g_listener->onLevelsChanged();
    }

    void notifyModeratorChanged()
    {
        if (g_listener)
            g_listener->onModeratorChanged();
    }

    std::string readToken()
    {
        std::string token =
            Mod::get()->getSettingValue<std::string>("github-token");

        auto first = token.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return "";

        auto last = token.find_last_not_of(" \t\r\n");
        return token.substr(first, last - first + 1);
    }

    std::string repoUrl(std::string const &path = "")
    {
        std::string url =
            std::string("https://api.github.com/repos/") +
            REPO_OWNER + "/" + REPO_NAME;

        if (!path.empty())
            url += "/" + path;

        return url;
    }

    std::string fileUrl()
    {
        return repoUrl(
            std::string("contents/") + FILE_PATH + "?ref=" + BRANCH);
    }

    void prepareRequest(
        web::WebRequest &request,
        std::string const &accept,
        std::string const &token)
    {
        request.userAgent("krexale.geometry-dash-1-attempt");
        request.header("Accept", accept);
        request.header("X-GitHub-Api-Version", "2022-11-28");

        if (!token.empty())
            request.header("Authorization", "Bearer " + token);
    }

    std::string findJsonString(
        std::string const &json,
        std::string const &key)
    {
        auto keyPos = json.find("\"" + key + "\"");
        if (keyPos == std::string::npos)
            return {};

        auto colon = json.find(':', keyPos);
        if (colon == std::string::npos)
            return {};

        auto openQuote = json.find('"', colon + 1);
        if (openQuote == std::string::npos)
            return {};

        auto closeQuote = json.find('"', openQuote + 1);
        if (closeQuote == std::string::npos)
            return {};

        return json.substr(
            openQuote + 1,
            closeQuote - openQuote - 1);
    }

    std::string toBase64(std::string const &input)
    {
        static char const *alphabet =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        auto byteAt = [&](size_t i)
        {
            return static_cast<uint32_t>(
                static_cast<unsigned char>(input[i]));
        };

        std::string out;
        out.reserve(((input.size() + 2) / 3) * 4);

        size_t i = 0;

        for (; i + 2 < input.size(); i += 3)
        {
            uint32_t block =
                (byteAt(i) << 16) |
                (byteAt(i + 1) << 8) |
                byteAt(i + 2);

            out += alphabet[(block >> 18) & 63];
            out += alphabet[(block >> 12) & 63];
            out += alphabet[(block >> 6) & 63];
            out += alphabet[block & 63];
        }

        size_t left = input.size() - i;

        if (left == 1)
        {
            uint32_t block = byteAt(i) << 16;

            out += alphabet[(block >> 18) & 63];
            out += alphabet[(block >> 12) & 63];
            out += "==";
        }
        else if (left == 2)
        {
            uint32_t block =
                (byteAt(i) << 16) |
                (byteAt(i + 1) << 8);

            out += alphabet[(block >> 18) & 63];
            out += alphabet[(block >> 12) & 63];
            out += alphabet[(block >> 6) & 63];
            out += '=';
        }

        return out;
    }

    void showMessage(std::string const &title, std::string const &text)
    {
        FLAlertLayer::create(
            title.c_str(),
            text.c_str(),
            "OK")
            ->show();
    }

    void onModeratorCheckDone(
        std::string const &token,
        web::WebResponse const &response)
    {
        bool isModerator = false;

        if (response.ok())
        {
            auto text = response.string();

            if (text.isOk())
            {
                std::string body = text.unwrap();
                auto pos = body.find("\"permissions\"");

                if (pos != std::string::npos)
                {
                    std::string part = body.substr(pos, 200);
                    part.erase(
                        std::remove(part.begin(), part.end(), ' '),
                        part.end());

                    isModerator =
                        part.find("\"push\":true") != std::string::npos;
                }
            }
        }
        else
        {
            log::warn(
                "Moderator check failed: HTTP {}",
                response.code());
        }

        if (token != g_lastToken)
            return;

        if (g_isModerator != isModerator)
        {
            g_isModerator = isModerator;
            notifyModeratorChanged();
        }
    }

    void checkModerator()
    {
        std::string token = readToken();
        g_lastToken = token;

        if (token.empty())
        {
            if (g_isModerator)
            {
                g_isModerator = false;
                notifyModeratorChanged();
            }

            return;
        }

        web::WebRequest request;
        prepareRequest(
            request,
            "application/vnd.github+json",
            token);

        async::spawn(
            request.get(repoUrl()),
            [token](web::WebResponse response)
            {
                onModeratorCheckDone(token, response);
            });
    }

    void downloadLevels(bool useToken);

    void onDownloadDone(
        bool usedToken,
        web::WebResponse const &response)
    {
        if (!response.ok())
        {
            log::warn(
                "Could not download levels.csv: HTTP {}",
                response.code());

            if (usedToken)
                downloadLevels(false);

            return;
        }

        auto text = response.string();
        if (text.isErr())
            return;

        auto levels = LevelData::parseCsv(text.unwrap());

        if (levels.empty())
            return;

        LevelData::replaceAll(std::move(levels));
        notifyLevelsChanged();
    }

    void downloadLevels(bool useToken)
    {
        std::string token =
            useToken ? readToken() : std::string();

        web::WebRequest request;
        std::string url;

        if (!token.empty())
        {
            // Using the API avoids the cache on the public raw URL.
            prepareRequest(
                request,
                "application/vnd.github.raw+json",
                token);

            url = fileUrl();
        }
        else
        {
            prepareRequest(request, "text/plain", "");

            url =
                std::string("https://raw.githubusercontent.com/") +
                REPO_OWNER + "/" + REPO_NAME + "/" +
                BRANCH + "/" + FILE_PATH +
                "?t=" + std::to_string(std::time(nullptr) / 60);
        }

        bool usedToken = !token.empty();

        async::spawn(
            request.get(url),
            [usedToken](web::WebResponse response)
            {
                onDownloadDone(usedToken, response);
            });
    }

    struct EditJob
    {
        GitHubSync::EditKind kind = GitHubSync::EditKind::Add;
        LevelEntry level;
        std::string token;
        std::string sha;
        std::vector<LevelEntry> newList;
        int retries = 0;
    };

    using EditJobPtr = std::shared_ptr<EditJob>;

    void startEditStep1(EditJobPtr job);
    void startEditStep2(EditJobPtr job);
    void startEditStep3(EditJobPtr job);

    void finishEdit(
        bool success,
        std::string const &message)
    {
        g_editInProgress = false;

        showMessage(
            success ? "Done" : "Error",
            message);
    }

    void onStep1Done(
        EditJobPtr job,
        web::WebResponse const &response)
    {
        if (!response.ok())
        {
            finishEdit(
                false,
                "Could not read the list on GitHub (HTTP " +
                    std::to_string(response.code()) + ").");
            return;
        }

        auto text = response.string();

        job->sha = text.isOk()
                       ? findJsonString(text.unwrap(), "sha")
                       : std::string();

        if (job->sha.empty())
        {
            finishEdit(false, "Unexpected answer from GitHub.");
            return;
        }

        startEditStep2(job);
    }

    void onStep2Done(
        EditJobPtr job,
        web::WebResponse const &response)
    {
        if (!response.ok())
        {
            finishEdit(
                false,
                "Could not download the list (HTTP " +
                    std::to_string(response.code()) + ").");
            return;
        }

        auto text = response.string();

        if (text.isErr())
        {
            finishEdit(false, "Could not read the list.");
            return;
        }

        auto list = LevelData::parseCsv(text.unwrap());

        if (list.empty())
        {
            finishEdit(
                false,
                "The list on GitHub looks empty, aborting to be safe.");
            return;
        }

        int id = job->level.id;

        list.erase(
            std::remove_if(
                list.begin(),
                list.end(),
                [id](LevelEntry const &entry)
                {
                    return entry.id == id;
                }),
            list.end());

        if (job->kind == GitHubSync::EditKind::Add)
            list.push_back(job->level);

        std::sort(
            list.begin(),
            list.end(),
            [](LevelEntry const &a, LevelEntry const &b)
            {
                return a.id < b.id;
            });

        job->newList = std::move(list);
        startEditStep3(job);
    }

    void onStep3Done(
        EditJobPtr job,
        web::WebResponse const &response)
    {
        int code = response.code();

        if (code == 200 || code == 201)
        {
            LevelData::replaceAll(std::move(job->newList));
            notifyLevelsChanged();

            bool added =
                job->kind == GitHubSync::EditKind::Add;

            finishEdit(
                true,
                std::string(
                    added
                        ? "Level added to"
                        : "Level removed from") +
                    " the list. Other players will see it within a few minutes.");
        }
        else if (
            (code == 409 || code == 422) &&
            job->retries < MAX_EDIT_RETRIES)
        {
            job->retries++;
            job->newList.clear();

            startEditStep1(job);
        }
        else if (code == 401 || code == 403 || code == 404)
        {
            finishEdit(
                false,
                "GitHub refused the change. Your token may be invalid, "
                "expired, or you lost write access.");
        }
        else
        {
            finishEdit(
                false,
                "Update failed (HTTP " +
                    std::to_string(code) + ").");
        }
    }

    void startEditStep1(EditJobPtr job)
    {
        web::WebRequest request;
        prepareRequest(
            request,
            "application/vnd.github+json",
            job->token);

        async::spawn(
            request.get(fileUrl()),
            [job](web::WebResponse response)
            {
                onStep1Done(job, response);
            });
    }

    void startEditStep2(EditJobPtr job)
    {
        web::WebRequest request;
        prepareRequest(
            request,
            "application/vnd.github.raw+json",
            job->token);

        async::spawn(
            request.get(
                repoUrl("git/blobs/" + job->sha)),
            [job](web::WebResponse response)
            {
                onStep2Done(job, response);
            });
    }

    void startEditStep3(EditJobPtr job)
    {
        bool added =
            job->kind == GitHubSync::EditKind::Add;

        std::string commitMessage =
            (added ? "Add level " : "Remove level ") +
            std::to_string(job->level.id);

        std::string body =
            "{\"message\":\"" + commitMessage + "\"," +
            "\"content\":\"" +
            toBase64(LevelData::toCsv(job->newList)) +
            "\"," +
            "\"sha\":\"" + job->sha + "\"," +
            "\"branch\":\"" + BRANCH + "\"}";

        web::WebRequest request;

        prepareRequest(
            request,
            "application/vnd.github+json",
            job->token);

        request.header("Content-Type", "application/json");
        request.bodyString(body);

        async::spawn(
            request.put(
                repoUrl(std::string("contents/") + FILE_PATH)),
            [job](web::WebResponse response)
            {
                onStep3Done(job, response);
            });
    }

    void editLevel(
        GitHubSync::EditKind kind,
        LevelEntry level)
    {
        std::string token = readToken();

        if (!g_isModerator || token.empty())
        {
            showMessage(
                "Error",
                "You are not a moderator. Set a valid GitHub token in the mod settings.");
            return;
        }

        if (g_editInProgress)
        {
            showMessage(
                "Please wait",
                "Another update is still in progress.");
            return;
        }

        g_editInProgress = true;

        auto job = std::make_shared<EditJob>();
        job->kind = kind;
        job->level = level;
        job->token = token;

        startEditStep1(job);
    }
}

namespace GitHubSync
{

    bool isModerator()
    {
        return g_isModerator;
    }

    void setListener(SyncListener *listener)
    {
        g_listener = listener;
    }

    void clearListener(SyncListener *listener)
    {
        if (g_listener == listener)
            g_listener = nullptr;
    }

    void refreshIfNeeded()
    {
        std::string token = readToken();
        std::time_t now = std::time(nullptr);

        bool tokenChanged =
            !g_initialised || token != g_lastToken;

        bool listIsOld =
            now - g_lastDownload >= REFRESH_INTERVAL_SECONDS;

        if (!tokenChanged && !listIsOld)
            return;

        g_initialised = true;
        g_lastDownload = now;

        if (tokenChanged)
            checkModerator();

        downloadLevels(true);
    }

    void askAndEdit(
        EditKind kind,
        LevelEntry entry)
    {
        std::string question;

        if (kind == EditKind::Add)
        {
            question =
                "Add level " +
                std::to_string(entry.id) +
                " (" +
                std::to_string(entry.stars) +
                " stars) to the 1 Attempt list?";
        }
        else
        {
            question =
                "Remove level " +
                std::to_string(entry.id) +
                " from the 1 Attempt list?";
        }

        createQuickPopup(
            kind == EditKind::Add ? "Add level" : "Remove level",
            question,
            "Cancel",
            kind == EditKind::Add ? "Add" : "Remove",
            [kind, entry](FLAlertLayer *, bool confirmed)
            {
                if (confirmed)
                    editLevel(kind, entry);
            });
    }

}