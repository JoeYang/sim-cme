#pragma once

#include <memory>
#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace cme::sim {

// ---------------------------------------------------------------------------
// Logger categories matching exchange subsystems
// ---------------------------------------------------------------------------
namespace LogCategory {
    inline constexpr const char* FIXP    = "FIXP";
    inline constexpr const char* ENGINE  = "ENGINE";
    inline constexpr const char* GATEWAY = "GATEWAY";
    inline constexpr const char* MDATA   = "MDATA";
    inline constexpr const char* NETWORK = "NETWORK";
} // namespace LogCategory

// ---------------------------------------------------------------------------
// Get or create a named logger with standard formatting
// ---------------------------------------------------------------------------
inline std::shared_ptr<spdlog::logger> getLogger(const std::string& name) {
    auto logger = spdlog::get(name);
    if (logger) {
        return logger;
    }

    // Create a console + rotating-file logger
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);

    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "logs/" + name + ".log",
        5 * 1024 * 1024, // 5 MB per file
        3                 // keep 3 rotated files
    );
    file_sink->set_level(spdlog::level::trace);

    logger = std::make_shared<spdlog::logger>(
        name,
        spdlog::sinks_init_list{console_sink, file_sink});

    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%n] [%^%l%$] %v");
    logger->set_level(spdlog::level::debug);
    logger->flush_on(spdlog::level::warn);

    spdlog::register_logger(logger);
    return logger;
}

} // namespace cme::sim
