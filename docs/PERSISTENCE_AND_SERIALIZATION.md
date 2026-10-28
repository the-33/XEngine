# Persistence, serialization and recovery

The callable interfaces below also appear in the
[Doxygen public API reference](API_REFERENCE.md), including per-method and
per-enumerator documentation. Generate both profiles with
`python tools/build_doxygen.py` (the `Doxyfile_Games` and `Doxyfile_Engine`
configurations are processed automatically).

XEngine provides general-purpose data I/O. The **game** owns its save format, version,
magic bytes, checksums, migrations, file extensions, encryption, obfuscation and anti-cheat
rules. The engine never executes a type or constructor named by a file: there is no
`BinaryFormatter` or automatic polymorphic object materialization.

| Layer | Responsibility | Content-aware? |
|---|---|---|
| `FileSystem` | Direct read/write, copy, move/rename, delete, path/metadata, per-user data directory | No |
| `AtomicFile` | Stage, synchronize and publish **one** file; preserve backups; inspect/read recovery candidates | No |
| `BinaryWriter` / `BinaryReader` | Primitives, exact byte sequences, bit fields and .NET-style UTF-8 strings | No |
| `BinarySerializer` | Convenience defaults for primitive/container/registered types | Yes: developer-selected types |
| `JsonSerializer` | Thin `nlohmann/json` facade; optional direct/atomic file saving | Yes: developer-selected types |
| `PlayerPrefs` | Small Int/Float/String user preferences | Yes: its own key/value settings only |

The lightweight headers (`FileSystem`, `AtomicFile`, `BinaryIO`, `PlayerPrefs`)
are exported by `XEngine.h`. Include `<JsonSerializer.h>` or
`<Serialization.h>` **explicitly** in source files that use them, keeping
JSON's template-heavy implementation out of unrelated compilation units.
No project JSON configuration changes are needed to use these APIs.

## Per-user folders and files

```cpp
auto saves = FileSystem::GetPersistentDataPath("YourStudio", "YourGame");
if (saves.empty()) { /* Report storage configuration failure. */ }
auto slot = saves / "slot-01.custom"; // ANY extension, or none
FileSystem::CreateDirectories(saves);
```

`GetPersistentDataPath` uses SDL2 `SDL_GetPrefPath`, not the application's current
working directory. The build system deliberately runs games from the assets folder;
**do not store mutable saves under `assets/`, `games/` or beside the binary**.

### FileSystem: unrestricted direct operations

```cpp
std::vector<std::byte> raw = {std::byte{0x01}, std::byte{0x02}};
FileError written = FileSystem::WriteAllBytes(slot, raw); // direct, NOT atomic
FileReadResult read = FileSystem::ReadAllBytes(slot, 1024 * 1024);
if (read.Ok()) { /* read.data contains exactly the stored bytes */ }
FileSystem::Copy(slot, saves / "slot-copy.bin");
FileSystem::Rename(saves / "slot-copy.bin", saves / "renamed.bin");
// FileSystem::Remove(...) is explicit and destructive; your game chooses when.
```

Other methods: `WriteAllText`, `ReadAllText`, `Exists`, `IsRegularFile`, `IsDirectory`,
`CreateDirectories`, `Move`, `LastWriteTime`. Read size defaults to **64 MiB**, and can
be lowered or raised by the caller. These APIs have no save-specific semantics.

`FileError` distinguishes `None`, `NotFound`, `PermissionDenied`, `TooLarge`,
`InvalidPath`, `AlreadyExists`, `RecoveryRequired`, and `IoError`. A failed direct write
may have truncated its target; choose AtomicFile when crash recovery matters.

## AtomicFile: single-file publication

```cpp
FileError status = AtomicFile::Write(slot, raw); // "slot-01.custom.tmp", ".bak"
auto inspection = AtomicFile::Inspect(slot);
if (inspection.error != FileError::None) { /* Report permission/metadata error */ }

// The game, not the engine, chooses which source to inspect and validate:
auto old = AtomicFile::Read(slot, AtomicFileSource::Backup, 8 * 1024 * 1024);
if (old.Ok()) { /* Validate version/checksum/fields, then offer recovery. */ }
```

An atomic write creates the same-directory `.tmp` **exclusively** (it will not overwrite
a leftover `.tmp`), writes the bytes and synchronizes the file, and only then publishes
it by filesystem rename/replacement. Windows uses `ReplaceFileW` for existing primary
files (and `MoveFileExW` for new files). Linux uses `rename()` and `fsync` for files and
parent directories. The exact ordering of directory-durability guarantees on Windows
is platform-dependent; no API can guarantee survival of all hardware/power failures.

By default, the **first previous primary** is preserved as `.bak`. If `.bak` already
exists, it is **not overwritten**: a broken current file must never displace the only
known good backup. If the developer has verified that backup is no longer needed,
they may remove or archive it through `FileSystem` before future writes. Passing
`keepBackup=false` disables creation of a new backup but does not delete old ones.

On Linux a backup is first created as a hard link when supported; otherwise a complete
copy is staged as `.bak.tmp`, synchronized, then renamed. The `.bak.tmp` can be left by
an interrupted backup operation: **do not automatically delete it** without deciding
whether it contains valuable bytes. The core three candidate APIs inspect the
confirmed `.bak`, not the auxiliary backup-copy stage. Developers can inspect the
additional file through `FileSystem`.

### Interpreting AtomicReadSituation

`Inspect` determines **presence only**, not data integrity. The enum members carry
Doxygen comments visible on hover in common C++ language servers.

| Situation | Suggested game-level behavior (not enforced) |
|---|---|
| `NoFiles` | Offer a new game. |
| `PrimaryOnly` | Validate and load the primary. |
| `BackupOnly` | Validate then offer backup restoration. |
| `TemporaryOnly` | Inspect/validate interrupted write; do not auto-promote. |
| `PrimaryAndBackup` | Validate primary; offer backup when primary is unusable. |
| `PrimaryAndTemporary` | Prefer confirmed valid primary; preserve temporary until resolved. |
| `BackupAndTemporary` | Evaluate both candidates; let the player/developer decide. |
| `AllPresent` | Validate all three and present an explicit recovery choice if needed. |
| `Inaccessible` | Report I/O or permissions failure, not an empty save slot. |

`.tmp` remaining from a crash causes `AtomicFile::Write` to return
`FileError::RecoveryRequired`. Once the game has validated and resolved it, it can
remove/archive that candidate using `FileSystem`. `AtomicFile::Read(path, source)`
returns `AtomicReadResult` with `.situation`, `.source`, `.error` and `.data`.
No default fallback, magic, version, length, checksum or file extension is imposed.
The **game must not overwrite a future-version save without explicit player consent**.

There is **no multi-file atomic transaction**. Games that split one save across images,
binary data, and metadata files can use a manifest or generation/version scheme to
ensure those files belong to the same committed snapshot. There is no safe guarantee
against concurrent writers to the same path; serialize access in your game.

## BinaryWriter and BinaryReader

These are in-memory, deterministic field readers/writers, modeled on .NET
`BinaryWriter`/`BinaryReader`, **not** .NET `BinaryFormatter`.

```cpp
BinaryWriter writer;
writer.WriteInt32(100);
writer.WriteFloat32(0.75f);
writer.WriteString("Zoë"); // 7-bit UTF-8 byte length, then UTF-8 bytes
writer.WriteBytes(std::span<const std::byte>(someBytes)); // exact raw bytes
writer.WriteByteArray(someBytes); // uint32 byte-count followed by bytes

std::vector<std::byte> serialized = writer.TakeData();
BinaryReader reader(serialized, BinaryLimits{ /* optional bounds */ });
const auto health = reader.ReadInt32();
const auto fraction = reader.ReadFloat32();
const auto name = reader.ReadString();
auto buffer = reader.ReadBytes(4);
```

Other primitives: `WriteByte`, `WriteBool`, `WriteUInt16`, `WriteInt16`,
`WriteUInt32`, `WriteInt32`, `WriteUInt64`, `WriteInt64`, `WriteFloat32`,
`WriteFloat64`, `Write7BitEncodedUInt32`; corresponding `Read*` methods and
`ReadByteArray`, `TryReadByte`, `TryReadBytes`. All multibyte numbers are explicit
**little-endian**; float/double use IEEE 754. `ReadString` enforces configurable
maximum length and rejects malformed 7-bit prefixes. The bytes are not executed.

```cpp
BinaryWriter packed(BitOrder::LSBFirst);
packed.WriteBits(0b101u, 3);   // exactly three bits
packed.WriteBit(true);
packed.WriteBits(myBuffer, 19); // arbitrary count spanning several bytes
packed.AlignToByte();           // explicitly pad remaining bits before primitives
packed.WriteInt32(42);

BinaryReader input(packed.Data());
auto flags = input.ReadBits(3);
bool enabled = input.ReadBit();
input.ReadBits(destinationBuffer, 19);
input.AlignToByte();
auto value = input.ReadInt32();
```

`BitOrder::MSBFirst` is also available; it controls the bit position *within*
each byte and the iteration direction of `WriteBits(value, count)`. It does not
change little-endian byte ordering of integers. Reading or writing byte-aligned
primitives mid-byte raises an error until `AlignToByte()` is called. All reader
methods throw `BinaryReadError` on malformed/truncated input (or a standard
exception on an allocation failure); wrap parsing of unknown files in `try/catch`.
`BinaryLimits` controls total bytes, string bytes, container element count and
maximum nesting depth.

## Collections and custom types: one field declaration for JSON and binary

The registered-field system supports built-in arithmetic types, booleans,
strings, enums, `TVec2`/`TVec3` (including `Vec2`, `Vec3`, integer variants),
`std::array`, `std::vector`, `std::list`, `std::map`, `std::unordered_map`,
`std::optional`, and recursively registered user types.

```cpp
struct Wallet {
    int coins = 0;
    int gems = 0;
    XENGINE_SERIALIZABLE(coins, gems)
};

struct PlayerSave {
    std::string playerName;
    Wallet wallet;
    std::vector<Wallet> pastWallets;
    std::map<std::string, Vec3> namedPositions;
    XENGINE_SERIALIZABLE(playerName, wallet, pastWallets, namedPositions)
};

PlayerSave player{ /* fill values */ };
std::string json = JsonSerializer::Serialize(player);
PlayerSave jsonCopy = JsonSerializer::Deserialize<PlayerSave>(json);

std::vector<std::byte> raw = BinarySerializer::Serialize(player);
PlayerSave binaryCopy = BinarySerializer::Deserialize<PlayerSave>(raw);
```

`XENGINE_SERIALIZABLE(...)` belongs **inside** the class/struct. The arguments
must be plain field identifiers in the desired order. C++20 cannot infer all
members from a marker such as C#'s `[Serializable]` alone, so the macro explicitly
registers the fields. Nested types work recursively, with no runtime type registry
or pointer reconstruction.

For classes with resources, invariants or computed data, customize the fields:

```cpp
class PlayerState {
    std::string name_;
    int lives_ = 3;
    void Fields(auto& ar) { ar.Field("name", name_); ar.Field("lives", lives_); }
    void Fields(auto& ar) const { ar.Field("name", name_); ar.Field("lives", lives_); }
public:
    template<class Archive> void Serialize(Archive& ar) { Fields(ar); }
    template<class Archive> void Serialize(Archive& ar) const { Fields(ar); }
};
```

You control *which* fields are persisted. For example, serialize an asset ID,
not a live `SDL_Texture*`; rebuild references after loading. A field with an
unsupported type fails **at compile time** in the binary serializer, rather than
falling back to copying raw C++ memory. You can always drop down to the primitive
writer/reader and define your own exact wire format (including checksums,
compression, graphics embedding, encryption, etc.).

Default binary collection framing is **engine-defined**, not .NET's generic
object format: `std::vector`, `std::list`, `std::map`, and `std::unordered_map`
use a `uint32` element count followed by elements; `std::array<T,N>` has exactly
`N` consecutive elements and **no count prefix**; `optional` has one boolean
presence byte before its value. Dictionaries write consecutive key/value pairs.
Unordered maps may produce different byte ordering across runs/platforms;
use `std::map` or sort entries yourself for canonical binary data.

JSON uses field names and JSON objects/arrays. Maps with string keys are JSON
objects; maps with other key types are arrays of `[key, value]` pairs. The
library `nlohmann/json` also still accepts custom `to_json`/`from_json`
conversions. Missing registered JSON fields cause an error by default.

**Format migration is a game responsibility.** Adding, removing or reordering
binary registered fields changes the binary layout; supply explicit versioning
and per-version readers if you need backwards compatibility. Existing .NET
`BinaryWriter` files should be read with explicit `BinaryReader` field calls,
matching the exact legacy record order, rather than assuming the new generic
collection framing matches them.

### Optional atomic JSON convenience

```cpp
JsonSerializer::SaveToFile(slot, player, JsonWriteMode::Atomic); // default
JsonSerializer::SaveToFile(slot, player, JsonWriteMode::Direct); // opt out

// Equivalent explicit workflow:
const auto text = JsonSerializer::Serialize(player);
AtomicFile::Write(slot, {reinterpret_cast<const std::byte*>(text.data()), text.size()});
```

`nlohmann/json` handles in-memory conversion/`parse()`; it does **not** make
disk writes atomic. File persistence is provided by XEngine's optional adapter
or the developer's chosen `FileSystem`/`AtomicFile` calls.

## PlayerPrefs

```cpp
if (PlayerPrefs::Initialize("YourStudio", "YourGame")) {
    PlayerPrefs::SetInt("quality", 2);
    PlayerPrefs::SetFloat("masterVolume", 0.8f);
    PlayerPrefs::SetString("language", "es");
    if (!PlayerPrefs::Save()) { /* Report failure; do not assume persistence. */ }
}

int quality = PlayerPrefs::GetInt("quality", 1);
float volume = PlayerPrefs::GetFloat("masterVolume", 1.f);
```

Additional methods: `GetString`, `HasKey`, `DeleteKey`, `DeleteAll`,
`GetStoragePath` (Linux only). Initialize explicitly; no global app identity
is guessed from runtime assets or build settings.

- **Windows:** `HKEY_CURRENT_USER\Software\YourStudio\YourGame\XEngine`,
  using Windows Registry DWORD, binary float and Unicode string values.
  Changes write to Registry on `Set*`; `Save()` calls `RegFlushKey`.
- **Linux:** `$XDG_CONFIG_HOME/unity3d/YourStudio/YourGame/xengine-playerprefs.json`,
  falling back to `$HOME/.config/unity3d/YourStudio/YourGame/` when XDG is unset.
  Changes remain in memory until `Save()`; it publishes the JSON with `AtomicFile`.

These are **Unity-inspired locations**, not a promise to read Unity's private
value encoding. The XEngine subkey/filename isolates XEngine from actual Unity
preferences with the same company and product names. The preferences are not
encrypted or suited for secrets; avoid storing full game saves in them.

## Validate application data before applying it

Neither a successful filesystem read nor `AtomicReadSituation::PrimaryOnly`
means a save is safe to use. For a game-defined binary format, validate your
magic bytes, format version, record lengths, checksum and ranges **before**
mutating gameplay objects. Make all reads bounded and handle exceptions:

```cpp
auto candidate = AtomicFile::Read(slot, AtomicFileSource::Primary, 8 * 1024 * 1024);
if (candidate.Ok()) {
    try {
        // For the BinarySerializer format used in this game's own PlayerSave type.
        PlayerSave parsed = BinarySerializer::Deserialize<PlayerSave>(candidate.data);
        // Only now apply parsed state to the active scene or game session.
    } catch (const std::exception&) {
        // Reject this candidate; inspect and validate backup/temp separately.
    }
}
```

`BinarySerializer` does **not** inject a magic, version or checksum for you.
The example assumes the game has already chosen that serializer's format; if
you need a versioned save, build the framing yourself using `BinaryWriter`
and `BinaryReader` or your preferred format library.

When handling recoveries:

1. Call `AtomicFile::Inspect()` to learn which filenames exist.
2. Read each candidate the game considers relevant with an explicit size cap.
3. Validate contents and compatibility using the game's schema and integrity rules.
4. Ask the player before promoting or replacing a save when appropriate.
5. Use `FileSystem` to archive/remove stale candidates **only after** a
   successful recovery decision. Keep the known-good `.bak` until it is safe
   to discard.

If you use more than one file for a single game state, store a generation ID
or manifest and commit it last; `AtomicFile` only publishes one target at a
time. Do not allow two independent writers to operate on one save slot.

For development checks, see [Testing in the Developer Guide](DEVELOPER_GUIDE.md#testing).
For graphical window settings, see the separate
[Window, camera and UI guide](WINDOW_AND_CAMERA.md).
