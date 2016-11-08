//
//  InsomniaSettings.cpp
//  Insomnia
//
//  Created by Brandon LeBlanc on 11/5/16.
//  Copyright © 2016 PabuCo. All rights reserved.
//

#include <IOKit/IOLib.h>

#include <string.h>
#include "InsomniaSettings.hpp"

#define DLog(fmt, ...) IOLog("%s:%d " fmt "\n", __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__)
#if DEBUG
#   define Log DLog
#else
#   define Log(fmt, ...) do { if (debug) DLog(fmt, ##__VA_ARGS__); } while(0)
#endif

int InsomniaSettings::sysctl_handle_proc SYSCTL_HANDLER_ARGS {
    if (arg1 == NULL) {
        return -1;
    }

    int newValue = 0;
    int error = sysctl_handle_int(oidp, &newValue, 0, req);

    if (!error) {
        Setting * setting = (Setting *)arg1;
        bool success = setting->newValue(newValue);
        if (!success)
            error = 1;
            }

    return error;
}

void InsomniaSettings::register_oids() {
    auto end = ++(settings.end());
    for (auto it = settings.begin(); it != end; ++it) {
        sysctl_register_oid(&it->oid);
    }
}

void InsomniaSettings::deregister_oids() {
    auto begin = --(settings.begin());
    for (auto it = settings.end(); it != begin; --it) {
        sysctl_unregister_oid(&it->oid);
    }
}


#define CTLFLAG_RW_ANYBODY CTLFLAG_OID2 | CTLFLAG_RW | CTLFLAG_ANYBODY
void InsomniaSettings::Setting::init() {
    int kind = CTLFLAG_RW_ANYBODY;
    if (strcmp("A", type) == 0) {
        kind |= CTLTYPE_STRING;
    } else if (strcmp("I", type) == 0 || strcmp("IU", type) == 0 || strcmp("L", type) == 0) {
        kind |= CTLTYPE_INT;
    } else if (strcmp("Q", type)) {
        kind |= CTLTYPE_QUAD;
    } else if (strcmp("N", type)) {
        kind |= CTLTYPE_NODE;
    }

    this->oid = {
        &owner->sysctl__kern_insomnia_children,
        { 0 },
        OID_AUTO,
        kind,
        (void*)this,
        0,
        name,
        &InsomniaSettings::sysctl_handle_proc,
        type,
        description,
        SYSCTL_OID_VERSION,
        0
    };
}
#undef CTLFLAG_RW_ANYBODY

bool InsomniaSettings::Setting::newValue(int value) {
    if (strcmp("N", type))
        return false;

    bool retval = (*owner.*setter)(value);

    if (!retval)
        Log("Setting of property %s to %d failed", oid.oid_name, value);

    return retval;
}

bool InsomniaSettings::dispatch(InsomniaSettings::PropertyChangeEvent *event) const {
    if (!event)
        return false;

    auto end = ++watchers.end();
    for (auto it = watchers.begin(); it != end; ++it) {
        bool result = it->property_changed(event);
        if (!result)
            return false;
    }

    return true;
}

InsomniaSettings::Setting * InsomniaSettings::addSetting(PropertySetter setter,
                                                         string name, string type, string description) {
    Setting * setting = new Setting();
    setting->setter = setter;
    setting->name = name;
    setting->type = type;
    setting->description = description;
    setting->init();

    settings.insertAtBack(setting);

    return setting;
}

InsomniaSettings::InsomniaSettings() {
    Setting * node = addSetting(NULL, "insomnia", "N", "Insomnia Settings");
    // addSetting defaults to this node as the parent, node can't be a child of itself.
    node->oid.oid_parent = &sysctl__kern_children;
    // addSetting defaults to anybody... unset anybody
    node->oid.oid_kind = node->oid.oid_kind | ~CTLFLAG_ANYBODY;

#define ADD_SETTING(property, description) addSetting(&InsomniaSettings::set_##property, #property, "I", description)
    ADD_SETTING(debug, "");
    ADD_SETTING(lidsleep, "");
    ADD_SETTING(ac_state, "");
    ADD_SETTING(battery_state, "");
    ADD_SETTING(battery_threshold, "");
    ADD_SETTING(cpu_temp_state, "");
    ADD_SETTING(cpu_threshold, "");
#undef ADD_SETTING
}

InsomniaSettings::~InsomniaSettings() {
    deregister_oids();
}

void InsomniaSettings::initialize() {
    register_oids();
}

void InsomniaSettings::addWatcher(InsomniaSettings::Watcher *watcher) {
    watchers.insertAtBack(watcher);
}

bool InsomniaSettings::removeWatcher(InsomniaSettings::Watcher *watcher) {
    if (!watcher)
        return false;

    for (auto it = watchers.begin(); it.valid(); ++it) {
        if (watcher == *it) {
            watchers.removeAt(it);
            return true;
        }
    }
    
    return false;
}
