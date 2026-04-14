// FILE: buf_connect_server/include/buf_connect_server/session/session_event.hpp
#ifndef BUF_CONNECT_SERVER_SESSION_SESSION_EVENT_HPP
#define BUF_CONNECT_SERVER_SESSION_SESSION_EVENT_HPP

#include <cstdint>
#include <string>
#include <variant>

namespace buf_connect_server::session {

    struct AdminConflictEvent {
        uint32_t timeout_seconds = 20;
        bool     snoozeable      = true;
    };

    struct AdminPendingEvent {
        uint32_t occupied_for_seconds = 0;
    };

    struct ConflictResolvedEvent {
        std::string resolution;  // "KEEP" | "GRANT" | "SNOOZE"
    };

    struct ForcedLogoutEvent {
        std::string reason;
    };

    struct VacantRoleEvent {};
    struct OnServiceEvent {};
    struct ServiceEndedEvent {};

    struct RoleClaimedEvent {
        std::string user_id;
    };

    using SessionEventVariant = std::variant<
            AdminConflictEvent,
            AdminPendingEvent,
            ConflictResolvedEvent,
            ForcedLogoutEvent,
            VacantRoleEvent,
            OnServiceEvent,
            ServiceEndedEvent,
            RoleClaimedEvent
    >;

}  // namespace buf_connect_server::session

#endif  // BUF_CONNECT_SERVER_SESSION_SESSION_EVENT_HPP