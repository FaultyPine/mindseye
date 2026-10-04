#pragma once

#include "core/containers/me_hybrid_array.h"
#include "core/me_defines.h"

struct meEventPayload
{
    void* payload = nullptr;
    meEventPayload() = default;
    meEventPayload(void* p)
    {
        payload = p;
    }
};

typedef void(*MeEventCb)(meEventPayload payload);

struct meEvent
{
    HybridArray<MeEventCb, 20> subscribers = {};
	void operator()(meEventPayload payload = {}) const;
};

MEAPI void meEventSubscribe(meEvent& event, MeEventCb callback);
MEAPI void meEventUnsubscribe(meEvent& event, MeEventCb callback);


#define MEEVENT_DECLARE_STATIC(name) \
meEvent& ME_MACRO_CONCAT_EX(name, Get)() { static meEvent event; return event; }

#define MEEVENT_REGISTER_STATIC(eventName, cb) \
extern meEvent& ME_MACRO_CONCAT_EX(eventName, Get)(); \
namespace { \
struct ME_MACRO_CONCAT_EX(MEEVENT_INTERNAL_STATIC_REGISTERER, __LINE__) { \
ME_MACRO_CONCAT_EX(MEEVENT_INTERNAL_STATIC_REGISTERER, __LINE__)() \
{ meEventSubscribe(ME_MACRO_CONCAT_EX(eventName, Get)(), cb); }\
} ME_MACRO_CONCAT_EX(meevent_registerer, __LINE__); }
