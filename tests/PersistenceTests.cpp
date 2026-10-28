#include "AtomicFile.h"
#include "BinaryIO.h"
#include "FileSystem.h"
#include "JsonSerializer.h"
#include "PlayerPrefs.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <list>
#include <array>
#include <unordered_map>
#include <vector>

struct Wallet {
    std::int32_t coins = 0;
    std::int32_t gems = 0;
    XENGINE_SERIALIZABLE(coins, gems)
};
struct SaveData {
    std::string player;
    Wallet wallet;
    std::vector<Wallet> history;
    std::map<std::string, Vec3> points;
    std::optional<std::int32_t> checkpoint;
    XENGINE_SERIALIZABLE(player, wallet, history, points, checkpoint)
};

struct CollectionBag {
    std::array<int, 3> fixed{1, 2, 3};
    std::list<std::string> names{"Ada", "Lin"};
    std::unordered_map<std::string, std::vector<int>> groups{{"a", {1, 2}}};
    std::map<int, Wallet> numericKeys{{7, {30, 4}}};
    XENGINE_SERIALIZABLE(fixed, names, groups, numericKeys)
};

int main(int argc, char** argv) {
    assert(argc == 2 && "Pass an isolated temporary test directory");
    const std::filesystem::path directory = argv[1];
    assert(FileSystem::CreateDirectories(directory) == FileError::None);

    const SaveData original{"Zoë", {17, 4}, {{2, 3}, {6, 7}},
                            {{"start", {1.5f, -2.0f, 0.25f}}}, 42};
    const std::string json = JsonSerializer::Serialize(original);
    const auto restoredJson = JsonSerializer::Deserialize<SaveData>(json);
    assert(restoredJson.player == original.player);
    assert(restoredJson.wallet.coins == 17);
    assert(restoredJson.history.size() == 2);
    assert(restoredJson.points.at("start").z == 0.25f);
    assert(restoredJson.checkpoint == 42);

    const auto binary = BinarySerializer::Serialize(original);
    const auto restoredBinary = BinarySerializer::Deserialize<SaveData>(binary);
    assert(restoredBinary.player == original.player);
    assert(restoredBinary.wallet.gems == 4);
    assert(restoredBinary.history.at(1).coins == 6);
    assert(restoredBinary.points.at("start").x == 1.5f);

    const CollectionBag bag;
    const auto binaryBag = BinarySerializer::Deserialize<CollectionBag>(BinarySerializer::Serialize(bag));
    assert(binaryBag.fixed[2] == 3);
    assert(binaryBag.names.back() == "Lin");
    assert(binaryBag.groups.at("a").size() == 2);
    assert(binaryBag.numericKeys.at(7).gems == 4);
    const auto jsonBag = JsonSerializer::Deserialize<CollectionBag>(JsonSerializer::Serialize(bag));
    assert(jsonBag.numericKeys.at(7).coins == 30);

    BinaryWriter writer;
    writer.WriteString("Hello");
    writer.WriteInt32(-12345);
    writer.WriteFloat32(0.5f);
    assert(writer.Data().size() == 14);
    assert(writer.Data()[0] == std::byte{5});
    assert(writer.Data()[1] == std::byte{'H'});
    BinaryReader reader(writer.Data());
    assert(reader.ReadString() == "Hello");
    assert(reader.ReadInt32() == -12345);
    assert(reader.ReadFloat32() == 0.5f);
    assert(reader.RemainingBytes() == 0);
    bool truncated = false;
    try { (void)reader.ReadByte(); } catch (const BinaryReadError&) { truncated = true; }
    assert(truncated);

    BinaryWriter bitWriter;
    bitWriter.WriteBits(5, 3);
    bitWriter.WriteBits(0x66, 8);
    bool unalignedRejected = false;
    try { bitWriter.WriteInt32(1); } catch (const std::logic_error&) { unalignedRejected = true; }
    assert(unalignedRejected);
    bitWriter.AlignToByte();
    bitWriter.WriteByte(0xCA);
    BinaryReader bitReader(bitWriter.Data());
    assert(bitReader.ReadBits(3) == 5);
    assert(bitReader.ReadBits(8) == 0x66);
    bitReader.AlignToByte();
    assert(bitReader.ReadByte() == 0xCA);

    BinaryWriter msbWriter(BitOrder::MSBFirst);
    msbWriter.WriteBits(0b101, 3);
    assert(msbWriter.Data()[0] == std::byte{0xA0});
    BinaryReader msbReader(msbWriter.Data(), {}, BitOrder::MSBFirst);
    assert(msbReader.ReadBits(3) == 0b101);

    const auto path = directory / "slot.my-custom-extension";
    assert(AtomicFile::Inspect(path).situation == AtomicReadSituation::NoFiles);
    assert(AtomicFile::Write(path, binary) == FileError::None);
    assert(AtomicFile::Inspect(path).situation == AtomicReadSituation::PrimaryOnly);
    assert(AtomicFile::Write(path, std::span<const std::byte>(writer.Data())) == FileError::None);
    const auto inspection = AtomicFile::Inspect(path);
    assert(inspection.situation == AtomicReadSituation::PrimaryAndBackup);
    assert(AtomicFile::Read(path, AtomicFileSource::Backup).data == binary);
    assert(AtomicFile::Write(path, binary) == FileError::None);
    assert(AtomicFile::Read(path, AtomicFileSource::Backup).data == binary);
    const auto temp = AtomicFile::TemporaryPath(path);
    assert(FileSystem::WriteAllText(temp, "interrupted") == FileError::None);
    assert(AtomicFile::Inspect(path).situation == AtomicReadSituation::AllPresent);
    assert(AtomicFile::Write(path, binary) == FileError::RecoveryRequired);
    assert(FileSystem::Remove(temp) == FileError::None);

    const auto jsonPath = directory / "custom.json";
    assert(JsonSerializer::SaveToFile(jsonPath, original) == FileError::None);
    assert(AtomicFile::Read(jsonPath).Ok());
    assert(JsonSerializer::SaveToFile(directory / "direct.json", original,
                                      JsonWriteMode::Direct) == FileError::None);

#if defined(__linux__)
    // The test runner must set XDG_CONFIG_HOME to an isolated directory before startup.
    assert(PlayerPrefs::Initialize("XEngineTests", "Persistence"));
    assert(PlayerPrefs::SetInt("quality", 3));
    assert(PlayerPrefs::SetFloat("volume", 0.75f));
    assert(PlayerPrefs::SetString("username", "Zoë"));
    assert(PlayerPrefs::Save());
    assert(PlayerPrefs::Initialize("XEngineTests", "Persistence"));
    assert(PlayerPrefs::GetInt("quality") == 3);
    assert(PlayerPrefs::GetFloat("volume") == 0.75f);
    assert(PlayerPrefs::GetString("username") == "Zoë");
    assert(PlayerPrefs::DeleteAll());
    assert(PlayerPrefs::Save());
    assert(!PlayerPrefs::HasKey("quality"));
#endif

    std::cout << "Persistence regression tests passed\n";
}
