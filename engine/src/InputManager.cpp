#include "InputManager.h"

#include <cstring>
#include <algorithm>
#include <cmath>

#include "WindowManager.h"
#include "SceneManager.h"
#include "Camera2D.h"
#include "Scene.h"
#include "SDL.h"

bool InputManager::Init(const Config& cfg) noexcept
{
    this->win = WindowManager::GetInstancePtr();
    this->scn = SceneManager::GetInstancePtr();

    if (win == nullptr) return false;
    if (scn == nullptr) return false;

    mCurrKeys = SDL_GetKeyboardState(nullptr);
    mPrevKeys = new uint8_t[SDL_NUM_SCANCODES];
    std::memset((void*)mPrevKeys, 0, SDL_NUM_SCANCODES);

    mCurrMouseButtons = 0;
    mPrevMouseButtons = 0;

    mMouseX = mMouseY = mPrevMouseX = mPrevMouseY = 0;
    mWheelX = mWheelY = 0;

    mMouseRelative = cfg.mouseLocked;
    SDL_ShowCursor(cfg.mouseHidden ? SDL_DISABLE : SDL_ENABLE);
    SDL_SetRelativeMouseMode(cfg.mouseLocked ? SDL_TRUE : SDL_FALSE);

    if (cfg.TextInput)
        SDL_StartTextInput();
    else
        SDL_StopTextInput();

    mTextInputBuffer.clear();
    mPads.clear();
    mPadOwners.clear();
    mLastPadOwner.clear();
    mPlayers.clear();
    mLastActive.clear();
    mPadEvents.clear();
    mJoinedThisFrame.clear();
    mPadListeners.clear();
    mActions.clear();
    mDisabledMaps.clear();
    mContextStacks.clear();
    mNamedContexts.clear();
    mFocused = true;
    mPreviousTick = SDL_GetTicks();
    PlayerInputSettings primary;
    primary.player = 0;
    primary.keyboard = true;
    primary.mouse = true;
    ConfigurePlayer(primary);
    SDL_GameControllerEventState(SDL_ENABLE);
    for (int index = 0; index < SDL_NumJoysticks(); ++index)
        AddGamepad(index);
    return true;
}

void InputManager::Shutdown() noexcept
{
    mActions.clear();
    mPadListeners.clear();
    for (auto& entry : mPads)
        if (entry.second.handle) SDL_GameControllerClose(entry.second.handle);
    mPads.clear();
    mPadOwners.clear();
    mPlayers.clear();
    mPadEvents.clear();
    mRebind.active = false;
    mContextStacks.clear();
    mNamedContexts.clear();
    if (mPrevKeys)
    {
        delete[] mPrevKeys;
        mPrevKeys = nullptr;
    }
    mCurrKeys = nullptr;
    SDL_StopTextInput();
}

void InputManager::Update() noexcept
{
    if (!mPrevKeys || !mCurrKeys) return;
    const uint32_t now = SDL_GetTicks();
    const float dt = std::min(0.25f, (now - mPreviousTick) / 1000.f);
    mPreviousTick = now;
    // Copy previous state
    std::memcpy((void*)mPrevKeys, mCurrKeys, SDL_NUM_SCANCODES);
    mPrevMouseButtons = mCurrMouseButtons;
    mPrevMouseX = mMouseX;
    mPrevMouseY = mMouseY;
    mMouseDeltaX = mMouseDeltaY = 0;
    mWheelX = mWheelY = 0;
    mTextInputBuffer.clear();
    mPadEvents.clear();
    mJoinedThisFrame.clear();
    for (auto& [id, pad] : mPads)
        pad.previousButtons = pad.buttons;

    // Procesamos eventos SDL relevantes
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        try {
        switch (e.type)
        {
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                mFocused = false;
                mCurrMouseButtons = 0;
                mPrevMouseButtons = 0;
                std::memset(mPrevKeys, 0, SDL_NUM_SCANCODES);
#if SDL_VERSION_ATLEAST(2,0,24)
                SDL_ResetKeyboard();
#endif
            } else if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                mFocused = true;
            }
            if (win) win->ProcessEvent(e);
            break;
        case SDL_KEYDOWN:
            if (mFocused && !e.key.repeat) {
                MarkActive({InputDeviceKind::Keyboard, -1});
                HandleRebindPath(std::string("Keyboard/") + SDL_GetScancodeName(e.key.keysym.scancode));
            }
            break;
        case SDL_MOUSEMOTION:
            if (e.motion.xrel || e.motion.yrel)
                MarkActive({InputDeviceKind::Mouse, -1});
            if (mMouseRelative)
            {
                // Several motion events can arrive in the same frame.
                mMouseDeltaX += e.motion.xrel;
                mMouseDeltaY += e.motion.yrel;
            }
            else
            {
                mMouseX = e.motion.x;
                mMouseY = e.motion.y;

                mMouseDeltaX = mMouseX - mPrevMouseX;
                mMouseDeltaY = mMouseY - mPrevMouseY;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            mCurrMouseButtons |= SDL_BUTTON(e.button.button);
            MarkActive({InputDeviceKind::Mouse, -1});
            HandleRebindPath("Mouse/" + std::to_string(e.button.button));
            break;
        case SDL_MOUSEBUTTONUP:
            mCurrMouseButtons &= ~SDL_BUTTON(e.button.button);
            break;
        case SDL_MOUSEWHEEL:
            mWheelX = e.wheel.x;
            mWheelY = e.wheel.y;
            if (mWheelX || mWheelY) MarkActive({InputDeviceKind::Mouse, -1});
            break;
        case SDL_CONTROLLERDEVICEADDED:
            AddGamepad(e.cdevice.which); // Device index, valid only at arrival.
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            RemoveGamepad(e.cdevice.which); // Joystick instance ID.
            break;
        case SDL_CONTROLLERBUTTONDOWN:
            MarkActive({InputDeviceKind::Gamepad, e.cbutton.which});
            if (e.cbutton.button == SDL_CONTROLLER_BUTTON_START)
                HandleJoin(e.cbutton.which);
            if (e.cbutton.button < SDL_CONTROLLER_BUTTON_MAX)
                HandleRebindPath("Gamepad/" + std::to_string(e.cbutton.button));
            break;
        case SDL_CONTROLLERAXISMOTION:
            if (std::abs(static_cast<int>(e.caxis.value)) > 15000)
                MarkActive({InputDeviceKind::Gamepad, e.caxis.which});
            break;
        case SDL_TEXTINPUT:
            mTextInputBuffer += e.text.text;
            break;
        case SDL_QUIT:
            WindowManager::GetInstancePtr()->ProcessEvent(e);
            break;
        default:
            break;
        }
        } catch (...) {
            // Preserve the noexcept game loop if input capture/allocation fails.
        }
    }

    // Update current keyboard state
    mCurrKeys = SDL_GetKeyboardState(nullptr);
    SDL_GameControllerUpdate();
    for (auto& [id, pad] : mPads) {
        if (!pad.handle) continue;
        for (int b = 0; b < 15; ++b)
            pad.buttons[b] = static_cast<uint8_t>(SDL_GameControllerGetButton(
                pad.handle, static_cast<SDL_GameControllerButton>(b)));
        for (int a = 0; a < 6; ++a) {
            const int16_t raw = SDL_GameControllerGetAxis(
                pad.handle, static_cast<SDL_GameControllerAxis>(a));
            pad.axes[a] = a >= 4 ? std::max(0.f, raw / 32767.f) :
                (raw < 0 ? raw / 32768.f : raw / 32767.f);
        }
    }
    EvaluateActions(dt);
}

static constexpr SDL_Scancode ToSDL(Key k) noexcept
{
    return static_cast<SDL_Scancode>(static_cast<uint16_t>(k));
}
static constexpr uint8_t ToSDL(MouseButton b) noexcept
{
    return static_cast<uint8_t>(b);
}

bool InputManager::KeyDown(Key key) const noexcept
{
    const auto scan = static_cast<unsigned>(ToSDL(key));
    return mFocused && scan < SDL_NUM_SCANCODES && mCurrKeys && mCurrKeys[scan];
}

bool InputManager::KeyPressed(Key key) const noexcept
{
    const auto scan = static_cast<unsigned>(ToSDL(key));
    return mFocused && scan < SDL_NUM_SCANCODES && mCurrKeys && mPrevKeys && mCurrKeys[scan] && !mPrevKeys[scan];
}

bool InputManager::KeyReleased(Key key) const noexcept
{
    const auto scan = static_cast<unsigned>(ToSDL(key));
    return mFocused && scan < SDL_NUM_SCANCODES && mCurrKeys && mPrevKeys && !mCurrKeys[scan] && mPrevKeys[scan];
}

bool InputManager::MouseDown(MouseButton button) const noexcept
{
    return mFocused && (mCurrMouseButtons & SDL_BUTTON(ToSDL(button))) != 0;
}

bool InputManager::MousePressed(MouseButton button) const noexcept
{
    return ((mCurrMouseButtons & SDL_BUTTON(ToSDL(button))) != 0) &&
        ((mPrevMouseButtons & SDL_BUTTON(ToSDL(button))) == 0);
}

bool InputManager::MouseReleased(MouseButton button) const noexcept
{
    return ((mCurrMouseButtons & SDL_BUTTON(ToSDL(button))) == 0) &&
        ((mPrevMouseButtons & SDL_BUTTON(ToSDL(button))) != 0);
}

Vec2I InputManager::GetMousePos() const noexcept
{
    return { mMouseX, mMouseY };
}

Vec2I InputManager::GetMouseDelta() const noexcept
{
    return { mMouseDeltaX, mMouseDeltaY };
}

Vec2I InputManager::GetMouseWheel() const noexcept
{
    return { mWheelX, mWheelY };
}

void InputManager::SetMouseMode(bool relativeLocked, bool visible) noexcept
{
    mMouseRelative = relativeLocked;
    SDL_SetRelativeMouseMode(relativeLocked ? SDL_TRUE : SDL_FALSE);
    SDL_ShowCursor(visible ? SDL_ENABLE : SDL_DISABLE);
}

void InputManager::SetTextInput(bool enabled) noexcept
{
    if (enabled) SDL_StartTextInput();
    else SDL_StopTextInput();
}

void InputManager::ClearTextInput() noexcept
{
    mTextInputBuffer.clear();
}

Vec2 InputManager::GetMousePosWorld() const noexcept
{
    if (!scn || !win) return { (float)mMouseX, (float)mMouseY };

    const Scene* active = scn->GetActive();
    if (!active) return { (float)mMouseX, (float)mMouseY };
    const Camera2D* cam = active->camera;
    if (!cam) return { (float)mMouseX, (float)mMouseY };

    const Vec2I logical = win->GetSize();
    const Vec2I drawable = win->GetDrawableSize();
    if (logical.x <= 0 || logical.y <= 0 || drawable.x <= 0 || drawable.y <= 0)
        return { (float)mMouseX, (float)mMouseY };

    // SDL motion positions are in logical window units. Camera2D uses the
    // renderer's drawable pixel dimensions, which can differ on HiDPI.
    const float x = static_cast<float>(mMouseX) * drawable.x / logical.x;
    const float y = static_cast<float>(mMouseY) * drawable.y / logical.y;
    return cam->ScreenToWorld(x, y, drawable.x, drawable.y);
}

Vec2 InputManager::GetMouseDeltaWorld() const noexcept
{
    if (!scn || !win) return { (float)mMouseDeltaX, (float)mMouseDeltaY };

    const Scene* active = scn->GetActive();
    if (!active) return { (float)mMouseDeltaX, (float)mMouseDeltaY };
    const Camera2D* cam = active->camera;
    if (!cam) return { (float)mMouseDeltaX, (float)mMouseDeltaY };

    const Vec2I logical = win->GetSize();
    const Vec2I drawable = win->GetDrawableSize();
    if (logical.x <= 0 || logical.y <= 0 || drawable.x <= 0 || drawable.y <= 0)
        return { (float)mMouseDeltaX, (float)mMouseDeltaY };

    const float dx = static_cast<float>(mMouseDeltaX) * drawable.x / logical.x;
    const float dy = static_cast<float>(mMouseDeltaY) * drawable.y / logical.y;
    const Vec2 origin = cam->ScreenToWorld(0.f, 0.f, drawable.x, drawable.y);
    const Vec2 moved = cam->ScreenToWorld(dx, dy, drawable.x, drawable.y);
    return {moved.x - origin.x, moved.y - origin.y};
}
