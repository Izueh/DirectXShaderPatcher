#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace dxp {

/// @brief Severity levels for recipe execution logging. Higher values are more
/// verbose; a level filter emits messages at or below the configured level.
enum class LogLevel : std::uint8_t {
  Error = 0,
  Warning = 1,
  Info = 2,
  Debug = 3,
  Trace = 4,
};

/// @brief Optional logging sink installed per Execute call (via PatchOptions).
///
/// The host decides how messages are routed (stderr, a file, a framework
/// logger, ...). A null sink discards everything — the library never writes to
/// global streams on its own. Installed on the per-call options object, so
/// concurrent Execute calls on a shared recipe each log to their own sink.
using LogSink = std::function<void(LogLevel, const std::string&)>;

/// @brief Per-execution logger state: sink + level filter. Populated by the
/// recipe engine from PatchOptions before steps run; steps and the engine emit
/// via Log().
struct LogContext {
  LogSink sink;
  LogLevel level = LogLevel::Warning;

  /// @brief Emits a message when the sink is set and the level passes the filter.
  void Log(LogLevel msg_level, const std::string& message) const {
    if (sink && msg_level <= level) {
      sink(msg_level, message);
    }
  }
};

}  // namespace dxp
