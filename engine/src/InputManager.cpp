#include "InputManager.h"

#include <cstring>

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
    return true;
}

void InputManager::Shutdown() noexcept
{
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
    // Copy previous state
    std::memcpy((void*)mPrevKeys, mCurrKeys, SDL_NUM_SCANCODES);
    mPrevMouseButtons = mCurrMouseButtons;
    mPrevMouseX = mMouseX;
    mPrevMouseY = mMouseY;
    mMouseDeltaX = mMouseDeltaY = 0;
    mWheelX = mWheelY = 0;
    mTextInputBuffer.clear();

    // Procesamos eventos SDL relevantes
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
        case SDL_MOUSEMOTION:
            if (mMouseRelative)
            {
                mMouseDeltaX = e.motion.xrel;
                mMouseDeltaY = e.motion.yrel;
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
            break;
        case SDL_MOUSEBUTTONUP:
            mCurrMouseButtons &= ~SDL_BUTTON(e.button.button);
            break;
        case SDL_MOUSEWHEEL:
            mWheelX = e.wheel.x;
            mWheelY = e.wheel.y;
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
    }

    // Update current keyboard state
    mCurrKeys = SDL_GetKeyboardState(nullptr);
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
    return mCurrKeys && mCurrKeys[ToSDL(key)];
}

bool InputManager::KeyPressed(Key key) const noexcept
{
    return mCurrKeys && mPrevKeys && mCurrKeys[ToSDL(key)] && !mPrevKeys[ToSDL(key)];
}

bool InputManager::KeyReleased(Key key) const noexcept
{
    return mCurrKeys && mPrevKeys && !mCurrKeys[ToSDL(key)] && mPrevKeys[ToSDL(key)];
}

bool InputManager::MouseDown(MouseButton button) const noexcept
{
    return (mCurrMouseButtons & SDL_BUTTON(ToSDL(button))) != 0;
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

    const Camera2D* cam = scn->GetActive()->camera;
    if (!cam) return { (float)mMouseX, (float)mMouseY };

    const Vec2I drawable = win->GetDrawableSize();
    
    return cam->ScreenToWorld((float)mMouseX, (float)mMouseY, drawable.x, drawable.y);
}

Vec2 InputManager::GetMouseDeltaWorld() const noexcept
{
    if (!scn || !win) return { (float)mMouseDeltaX, (float)mMouseDeltaY };

    const Camera2D* cam = scn->GetActive()->camera;
    if (!cam) return { (float)mMouseDeltaX, (float)mMouseDeltaY };

    const Vec2I drawable = win->GetDrawableSize();

    // Convertimos pos anterior y actual a mundo y restamos
    return cam->ScreenToWorld((float)mMouseDeltaX, (float)mMouseDeltaY, drawable.x, drawable.y);
}
