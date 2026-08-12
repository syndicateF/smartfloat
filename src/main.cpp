#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/desktop/Workspace.hpp>
#include <hyprland/src/layout/space/Space.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/output/Monitor.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <cmath>
#include <algorithm>
#include <lua.h>
#include <lauxlib.h>

inline HANDLE PHANDLE = nullptr;

static SP<Config::Values::CIntValue> g_pTargetWidth;
static SP<Config::Values::CIntValue> g_pTargetHeight;
static SP<Config::Values::CFloatValue> g_pCascadeStep;
static SP<Config::Values::CIntValue> g_pMaxAttempts;

static Vector2D findNonOverlappingPosition(PHLWINDOW pTargetWindow, Vector2D candidatePos, Vector2D targetSize, const CBox& workArea) {
    const auto PMONITOR = pTargetWindow->m_monitor;
    if (!PMONITOR)
        return candidatePos;

    const float STEP = g_pCascadeStep->value();
    const int MAX_ATTEMPTS = std::max(1, (int)g_pMaxAttempts->value());
    Vector2D pos = candidatePos;
    int wrapCount = 0;
    int attempts = 0;

    while (attempts < MAX_ATTEMPTS) {
        attempts++;
        bool collision = false;

        for (auto& w : Desktop::windowState()->windows()) {
            if (!w || w == pTargetWindow || !w->m_isMapped || !w->m_isFloating || w->workspaceID() != pTargetWindow->workspaceID())
                continue;

            const Vector2D otherPos = w->positionAnimation()->goal();

            if (std::abs(otherPos.x - pos.x) < 5.0 && std::abs(otherPos.y - pos.y) < 5.0) {
                collision = true;
                break;
            }
        }

        if (!collision) {
            const bool outOfBounds = 
                pos.x + targetSize.x > workArea.x + workArea.w ||
                pos.y + targetSize.y > workArea.y + workArea.h;

            if (!outOfBounds)
                return pos;
            
            wrapCount++;
            float maxOffsetX = std::max(1.0f, (float)(workArea.w - targetSize.x));
            float offsetX = std::fmod(STEP * (wrapCount - 1), maxOffsetX);

            pos = Vector2D{workArea.x + offsetX, workArea.y};
            continue;
        }

        pos.x += STEP;
        pos.y += STEP;
    }

    return candidatePos;
}

static SDispatchResult onSmartFloatToggle(std::string args) {
    PHLWINDOW pWindow = Desktop::focusState()->window();

    if (!pWindow || !pWindow->m_isMapped)
        return SDispatchResult{.success = true};

    if (Fullscreen::controller()->isFullscreen(pWindow) || pWindow->m_pinned)
        return SDispatchResult{.success = true};

    const bool wasFloating = pWindow->m_isFloating;

    g_layoutManager->changeFloatingMode(pWindow->layoutTarget());

    if (!wasFloating && pWindow->m_isFloating) {
        Desktop::windowState()->raise(pWindow);

        float targetW = (float)g_pTargetWidth->value();
        float targetH = (float)g_pTargetHeight->value();

        const auto PMONITOR = pWindow->m_monitor;
        if (!PMONITOR)
            return SDispatchResult{.success = true};

        CBox workArea;
        
        if (pWindow->m_workspace && pWindow->m_workspace->m_space) {
            workArea = pWindow->m_workspace->m_space->workArea(true);
        } else {
            workArea = {PMONITOR->m_position.x, PMONITOR->m_position.y, PMONITOR->m_size.x, PMONITOR->m_size.y};
        }

        targetW = std::min((float)workArea.w, targetW);
        targetH = std::min((float)workArea.h, targetH);
        const Vector2D targetSize = {targetW, targetH};

        const Vector2D centerPos = Vector2D{workArea.x, workArea.y} + (Vector2D{workArea.w, workArea.h} - targetSize) / 2.0;

        const Vector2D finalPos = findNonOverlappingPosition(pWindow, centerPos, targetSize, workArea);

        const CBox newBox{finalPos.x, finalPos.y, targetSize.x, targetSize.y};
        pWindow->layoutTarget()->setPositionGlobal(newBox);
        pWindow->layoutTarget()->rememberFloatingSize(targetSize);

        pWindow->updateWindowData();
    }

    return SDispatchResult{.success = true};
}

static int onSmartFloatToggleLua(lua_State* L) {
    onSmartFloatToggle("");
    return 0;
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

static void failNotif(const std::string& reason) {
    HyprlandAPI::addNotification(PHANDLE, "[smartfloat] Failure in initialization: " + reason, CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string COMPOSITOR_HASH = __hyprland_api_get_hash();
    const std::string CLIENT_HASH     = __hyprland_api_get_client_hash();

    if (COMPOSITOR_HASH != CLIENT_HASH) {
        failNotif("Header version mismatch! Plugin load aborted for safety. Please rebuild.");
        throw std::runtime_error("Version mismatch");
    }

    g_pTargetWidth = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:smartfloat:target_width", "Target width", 800);
    g_pTargetHeight = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:smartfloat:target_height", "Target height", 500);
    g_pCascadeStep = Hyprutils::Memory::makeShared<Config::Values::CFloatValue>("plugin:smartfloat:cascade_step", "Cascade step", 30.0f);
    g_pMaxAttempts = Hyprutils::Memory::makeShared<Config::Values::CIntValue>("plugin:smartfloat:max_attempts", "Max cascade attempts", 50);

    HyprlandAPI::addConfigValueV2(PHANDLE, g_pTargetWidth);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pTargetHeight);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pCascadeStep);
    HyprlandAPI::addConfigValueV2(PHANDLE, g_pMaxAttempts);

    HyprlandAPI::addDispatcherV2(PHANDLE, "smartfloat:toggle", onSmartFloatToggle);
    HyprlandAPI::addLuaFunction(PHANDLE, "smartfloat", "toggle", onSmartFloatToggleLua);

    return {"smartfloat", "Auto resize+center+cascade on float toggle", "darko-bunny", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
}
