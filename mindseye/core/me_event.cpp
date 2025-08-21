#include "me_event.h"


void meEvent::operator()(meEventPayload payload) const
{
    for (u32 i = 0; i < subscribers.size(); i++)
    {
        const MeEventCb& event = subscribers.at(i);
        event(payload);
    }
}

void meEventSubscribe(meEvent& event, MeEventCb callback)
{
    event.subscribers.push_back(callback);
}

void meEventUnsubscribe(meEvent& event, MeEventCb callback)
{
    for (s32 i = event.subscribers.size(); i >= 0; i--)
    {
        if (event.subscribers.at(i) == callback)
        {
            event.subscribers.erase_and_fill(i);
        }
    }
}
