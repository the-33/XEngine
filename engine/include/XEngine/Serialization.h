#pragma once
/** @file XEngine/Serialization.h
 * @brief Opt-in JSON and binary object serialization.
 * @details Imports nlohmann/json; keep this out of gameplay headers that do
 * not actually serialize data.
 */
#include <Serialization.h>
#include "JsonSerializer.h"

/** @brief Optional serialization module with familiar legacy global names. */
namespace xengine::serialization {
using ::JsonWriteArchive;
using ::JsonReadArchive;
using ::BinaryWriteArchive;
using ::BinaryReadArchive;
using ::BinarySerializer;
using ::JsonSerializer;
using ::JsonWriteMode;
}
