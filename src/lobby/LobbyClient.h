// LobbyClient.h: how the lobby talks back to a connected player.
//
// The lobby does not know about sockets. It only needs "send this message to that
// player", so a Session implements this small interface. That keeps the lobby testable
// and keeps networking out of it.
#pragma once

#include <string>

namespace matchforge::lobby {

class LobbyClient {
public:
    virtual ~LobbyClient() = default;

    // Sends one serialized Envelope to the client. The lobby calls this from its own
    // strand, so implementations must move to their own strand before touching their state.
    virtual void deliver(std::string serializedEnvelope) = 0;
};

}  // namespace matchforge::lobby
