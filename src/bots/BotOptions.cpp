// BotOptions.cpp: the command-line parser for the bot simulator.
#include "bots/BotOptions.h"

#include <cstdlib>

namespace matchforge::bots {

namespace {

// Parses a whole decimal number in [min, max]. Rejects text like "12abc" or "".
bool parseInt(const std::string& text, long min, long max, long& out) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (*end != '\0' || value < min || value > max) {
        return false;
    }
    out = value;
    return true;
}

}  // namespace

std::string botUsage() {
    return "usage: matchforge_bots [options]\n"
           "  --host <address>     server address (default 127.0.0.1)\n"
           "  --port <number>      server port (default 7777)\n"
           "  --bots <number>      fake players running at once, 1-5000 (default 10)\n"
           "  --rounds <number>    matches each bot plays, 1-1000 (default 1)\n"
           "  --mode <name>        queue to join (default duel)\n"
           "  --region <name>      region to join (default eu)\n"
           "  --secret <text>      result secret; when set, bots report random results\n"
           "  --timeout <seconds>  how long to wait for any reply, 1-600 (default 30)\n";
}

std::optional<BotOptions> parseBotOptions(const std::vector<std::string>& args,
                                          std::string& error) {
    BotOptions options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& flag = args[i];
        if (flag == "--help" || flag == "-h") {
            error = botUsage();
            return std::nullopt;
        }
        // Every other flag takes exactly one value after it.
        if (i + 1 >= args.size()) {
            error = "missing value after " + flag;
            return std::nullopt;
        }
        const std::string& value = args[++i];
        long number = 0;

        if (flag == "--host") {
            options.host = value;
        } else if (flag == "--port") {
            if (!parseInt(value, 1, 65535, number)) {
                error = "--port must be a number from 1 to 65535";
                return std::nullopt;
            }
            options.port = static_cast<std::uint16_t>(number);
        } else if (flag == "--bots") {
            if (!parseInt(value, 1, 5000, number)) {
                error = "--bots must be a number from 1 to 5000";
                return std::nullopt;
            }
            options.bots = static_cast<int>(number);
        } else if (flag == "--rounds") {
            if (!parseInt(value, 1, 1000, number)) {
                error = "--rounds must be a number from 1 to 1000";
                return std::nullopt;
            }
            options.rounds = static_cast<int>(number);
        } else if (flag == "--mode") {
            options.mode = value;
        } else if (flag == "--region") {
            options.region = value;
        } else if (flag == "--secret") {
            options.resultSecret = value;
        } else if (flag == "--timeout") {
            if (!parseInt(value, 1, 600, number)) {
                error = "--timeout must be a number from 1 to 600";
                return std::nullopt;
            }
            options.timeoutSeconds = static_cast<int>(number);
        } else {
            error = "unknown option " + flag + "\n" + botUsage();
            return std::nullopt;
        }
    }
    return options;
}

}  // namespace matchforge::bots
