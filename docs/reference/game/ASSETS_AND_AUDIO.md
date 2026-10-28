# Textures, fonts, audio and resource ownership

@ref AssetManager is the game's resource cache. The `Assets` accessor is
available through `<XEngine.h>`. @ref SoundManager plays loaded audio and
controls master volume, music and mixer channels.

## Load and reuse assets

```cpp
#include <XEngine.h>

Texture* portrait = Assets->LoadTexture("./textures/portrait.png");
Font* labelFont = Assets->LoadFont("./fonts/ui-default.ttf", 18);
SoundEffect* click = Assets->LoadSFX("./audio/click.wav");
Music* soundtrack = Assets->LoadMusic("./audio/theme.ogg");
```

These paths are relative to the runtime assets directory, not the source
tree or the persistent user save directory. The generated game's startup
logic sets the working directory for Debug and packaged Release. Repeated
requests can reuse cached resources; do not delete a pointer returned by
`AssetManager`, and avoid retaining it across engine shutdown.

## Sound playback

```cpp
Sound->SetMasterVolume(0.75f);
int channel = Sound->PlaySFX(click);
if (channel >= 0) Sound->StopSFXChannel(channel);
Sound->PlayMusic(soundtrack, -1); // Loop indefinitely.
```

Music and sound-effect channels have different lifetimes. The channel
returned by `PlaySFX` is useful for stopping one effect; `StopAllSFX` stops
them all, while `StopMusic` stops background playback. A missing resource,
unsupported format or mixer error can cause playback to fail; check return
values when sound is important to gameplay.

## Rendering and packaging

The `SpriteRenderer` component references an already loaded texture. UI can
draw fonts and textures in screen coordinates through @ref UIManager. Runtime
resources under `assets/` must be included in your game release; save files
belong under the per-user path provided by @ref FileSystem, not in these
read-only shipped assets.
