# Console logging {#xengine_console_logging}

XEngine provides three logging levels from any script that includes
`<XEngine.h>`. Messages go to the console without interrupting the game
with modal error dialogs.

```cpp
Console.Log("Player spawned", "Player");
Console.Warning("Low health", "PlayerHealth");
Console.Error("Could not open save", "SaveSystem");
```

The second argument is the log title. It is optional and defaults to `"Game"`
for user scripts. Each entry shows the level, icon and title; the message
appears on indented lines, including multiline messages.

```text
● [LOG] Player
    Player spawned
⚠ [WARNING] PlayerHealth
    Low health
✖ [ERROR] SaveSystem
    Could not open save
```

Engine-owned messages always use `Engine` for general messages or
`Engine - <System>` for system-specific messages. Examples include
`Engine - Render`, `Engine - Assets`, `Engine - Physics`, `Engine - Audio`
and `Engine - Animation`. This applies to startup/shutdown progress, renderer
backend diagnostics, profiler output and warnings/errors. The backend
diagnostic marker `[RENDER-BACKEND]` and profiler marker `[PROFILE]` remain
inside messages for troubleshooting. Game messages keep their `Game` title
or any custom title supplied by the game.

```text
● [LOG] Engine
    XEngine started successfully.
⚠ [WARNING] Engine - Render
    [RENDER-BACKEND] requested=opengl is not installed; trying fallback
```

On compatible terminals, LOG is cyan, WARNING is yellow and ERROR is red.
Redirected output has no color escape sequences. Icons and text use UTF-8.
On Windows, XEngine enables UTF-8 and ANSI support on an attached console
before emitting the very first startup message, so the `●`, `⚠` and `✖`
icons display correctly even before stdout capture begins. Redirected output
remains UTF-8 without changing the console settings.

While XEngine is running, legacy game calls to `printf`, `puts` and
`std::cout` are captured as LOG entries titled `Game`. Pending output is
flushed each frame and at shutdown. Game `stderr` is not redirected.

The compatibility API `LogError(caption, error)` remains available: its first
argument is the title and its second is the error message.
`LogWarning(caption, warning)` works similarly for warnings. Errors respect
`Engine::logErrors`.

`Debug::Log`, `Debug::LogWarning` and `Debug::LogError` expose the same
behavior if internal access is required.
