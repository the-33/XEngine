# Game saves, settings, binary and JSON data

XEngine provides several independent tools: @ref FileSystem for direct files,
@ref AtomicFile for one-file crash-resistant publication, @ref BinaryWriter
and @ref BinaryReader for explicit binary fields, @ref JsonSerializer for
JSON, and @ref PlayerPrefs for small key/value settings.

`PlayerPrefs` is part of `<XEngine.h>`; low-level storage and serialization
are opt-in modules. Include `<XEngine/Storage.h>` for FileSystem/AtomicFile,
`<XEngine/BinaryIO.h>` for binary primitives, and
`<XEngine/Serialization.h>` for JSON and registered objects. Their original
direct headers continue to work for existing games.

## Choose the lowest layer you need

| Need | API | What the game still owns |
|---|---|---|
| Read/write arbitrary bytes | @ref FileSystem | Filename, directory, format and validation |
| Replace one file with backup candidates | @ref AtomicFile | Payload validity, recovery choice, backups and conflicts |
| Encode numeric fields, strings and bits | @ref BinaryWriter and @ref BinaryReader | Field order, version, limits and validation |
| Convert registered values to JSON | @ref JsonSerializer | Schema and save/recovery policy |
| Remember volume or menu preferences | @ref PlayerPrefs | Key names and when to save |

`AtomicFile` never guesses a save header, magic, checksum, version or extension.
The game can choose `.dat`, `.sav`, `.json`, any other extension or none. A
`.tmp` file must not be promoted simply because it exists; validate data
before offering to recover it. An old `.bak` may be the only good save.

## An explicit small data model

```cpp
#include <XEngine/Serialization.h>

struct Stats {
    int score = 0;
    std::vector<int> completedLevels;
    XENGINE_SERIALIZABLE(score, completedLevels)
};

Stats data{42, {1, 2}};
std::string jsonText = JsonSerializer::Serialize(data);
```

Nested registered classes, vectors, arrays, maps and optional values can be
serialized through the registered-field archives. The C++ macro only
registers explicitly named fields; it is **not** a reflection or execution
system like `BinaryFormatter`. The game should define its own migration rules
if the schema changes in later versions.

## File-system and preference paths

Use `FileSystem::GetPersistentDataPath(company, game)` for per-user data on
Windows and Linux. Keep runtime assets separate from mutable saves. Windows
`PlayerPrefs` use a dedicated per-user registry key; Linux preferences use a
per-user file. `PlayerPrefs` is for simple settings, not large save slots.

For multi-file transactions, encryption, checksums and anti-cheat measures,
combine the low-level APIs yourself; a single `AtomicFile::Write` only
publishes **one** file atomically. Validate all untrusted bytes before
constructing or applying gameplay state.
