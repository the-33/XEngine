#pragma once
/** @file XEngine/BinaryIO.h
 * @brief Opt-in bounded binary reading, writing, and bit-level tools.
 */
#include <BinaryIO.h>

/** @brief Optional binary I/O module with aliases to existing public types. */
namespace xengine::binary {
using ::BitOrder;
using ::BinaryLimits;
using ::BinaryReadError;
using ::BinaryWriter;
using ::BinaryReader;
}
