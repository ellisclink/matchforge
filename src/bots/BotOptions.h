// BotOptions.h: command-line settings for the bot simulator.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace matchforge::bots {

struct BotOptions {
    std::string host = "127.0.0.1";  // server address
    std::uint16_t port = 7777;       // server port
    int bots = 10;                   // how many fake players run at once
    int rounds = 1;                  // how many matches each bot plays
    std::string mode = "duel";       // which queue to join
    std::string region = "eu";
    std::string resultSecret;  // when set, bots also report a random result per match
    int timeoutSeconds = 30;   // how long a bot waits for any one reply
};

// The text printed by --help.
std::string botUsage();

// Reads "--name value" pairs, for example: --bots 50 --rounds 3 --mode duo.
// Returns the options, or nullopt with `error` set to a message for the user.
// "--help" also returns nullopt, with the usage text in `error`.
std::optional<BotOptions> parseBotOptions(const std::vector<std::string>& args, std::string& error);

}  // namespace matchforge::bots
