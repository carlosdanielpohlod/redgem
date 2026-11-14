#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace redgem::load {

// Asks a dedicated server's status port on this machine (redgem-server --status-port) how busy
// the server is: an HTTP GET, its answer read whole (it closes the connection after it). The
// document, or nothing with `error` when the port did not answer one in time.
std::optional<nlohmann::json> read_status(std::uint16_t port, std::string& error);

}  // namespace redgem::load
