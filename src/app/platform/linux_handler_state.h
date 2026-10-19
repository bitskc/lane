#pragma once

#include "platform.h"

namespace Lane
{

// Maps two xdg-mime query results onto the shared default-handler states.
// Partial means Lane owns exactly one of http and https. A failed query is
// not treated as another browser, so a total miss stays Unknown.
inline Platform::DefaultHandlerState linuxDefaultHandlerState(bool httpOk, const QString &httpDesktop,
                                                             bool httpsOk, const QString &httpsDesktop)
{
    const auto laneDesktop = QLatin1String("app.lane.Lane.desktop");
    const bool httpUs = httpOk && httpDesktop == laneDesktop;
    const bool httpsUs = httpsOk && httpsDesktop == laneDesktop;

    Platform::DefaultHandlerState state;
    if (httpUs && httpsUs) {
        state.isDefault = true;
        state.status = Platform::DefaultHandlerState::Status::Registered;
        state.currentHandler = QStringLiteral("Lane");
        return state;
    }

    const bool httpOther = httpOk && !httpUs;
    const bool httpsOther = httpsOk && !httpsUs;
    if ((httpUs && httpsOther) || (httpsUs && httpOther)) {
        state.status = Platform::DefaultHandlerState::Status::Partial;
        if (httpOther && !httpDesktop.isEmpty()) {
            state.currentHandler = httpDesktop;
        } else if (httpsOther && !httpsDesktop.isEmpty()) {
            state.currentHandler = httpsDesktop;
        }
        return state;
    }

    state.status = Platform::DefaultHandlerState::Status::Unknown;
    if (httpOk && !httpUs && !httpDesktop.isEmpty()) {
        state.currentHandler = httpDesktop;
    } else if (httpsOk && !httpsUs && !httpsDesktop.isEmpty()) {
        state.currentHandler = httpsDesktop;
    }
    return state;
}

} // namespace Lane
