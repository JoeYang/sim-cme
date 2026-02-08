#pragma once
#include "exchange_config.h"
#include <string>
#include <stdexcept>

namespace cme::sim::config {

class ConfigValidationError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Load exchange configuration from a YAML file.
// If the file does not exist, returns a default configuration with standard
// channel and instrument setup for channels 310-313.
ExchangeConfig loadConfig(const std::string& path);

// Validate an ExchangeConfig, throwing ConfigValidationError on problems.
void validateConfig(const ExchangeConfig& config);

} // namespace cme::sim::config
