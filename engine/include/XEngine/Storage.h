#pragma once
/** @file Storage.h
 * @brief Opt-in filesystem and atomic single-file persistence utilities.
 * @details Low-level file handling is intentionally not part of XEngine.h.
 * The original global names remain supported for existing games.
 */
#include "FileSystem.h"
#include "AtomicFile.h"

/** @brief Optional storage module; its aliases match the legacy global types. */
namespace xengine::io {
using ::FileError;
using ::FileReadResult;
using ::FileSystem;
using ::AtomicFile;
using ::AtomicFileSource;
using ::AtomicReadSituation;
using ::AtomicFileInspection;
using ::AtomicReadResult;
}
