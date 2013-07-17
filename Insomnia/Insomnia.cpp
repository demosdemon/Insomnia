/*
    File:        Insomnia.cpp
    Program:     Insomnia
    Author:      Michael Roßberg/Alexey Manannikov/Dominik Wickenhauser/Andrew James
    Description: Insomnia is a kext module to disable sleep on ClamshellClosed
 
    Copyright (C) 2009 Michael Roßberg, Alexey Manannikov, Dominik Wickenhauser, Andrew James
        <https://code.google.com/p/insomnia-kext/>
    Copyright (C) 2013 François Lamboley
        <https://github.com/Frizlab/Insomnia>
    Copyright (C) 2013 Joachim B. LeBlanc
        <https://github.com/demosdemon/Insomnia>

    Insomnia is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 
    Insomnia is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with Insomnia; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <IOKit/IOLib.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <sys/kern_control.h>

#include <mach/mach_types.h>
#include <sys/systm.h>
#include <mach/mach_types.h>
#include <mach/kern_return.h>
#include <sys/kern_control.h>

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/kern_event.h>

#include "Insomnia.h"

#define CTLFLAG_ANYBODY_RW_NODE (CTLFLAG_ANYBODY|CTLFLAG_RW|CTLTYPE_NODE)
#define CTLFLAG_ANYBODY_RW_INT (CTLFLAG_ANYBODY|CTLFLAG_RW|CTLTYPE_INT)

#define INSOMNIA_SYSCTL_INT(name, desc, default, minval, maxval) \
    static int name = default; \
\
    static int sysctl_##name SYSCTL_HANDLER_ARGS; \
\
    SYSCTL_PROC(_kern_insomnia, \
                OID_AUTO, \
                name, \
                CTLFLAG_ANYBODY_RW_INT, \
                &name, 0, \
                &sysctl_##name, \
                "I", desc); \
\
    static int sysctl_##name SYSCTL_HANDLER_ARGS { \
        int error = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2, req); \
\
        if (name < minval || name > maxval) \
        return 1; \
\
        if (!error && req->newptr) { \
            myinstance->states_changed(); \
        } else if (req->newptr) { \
            DLog("error with write"); \
        } else { \
\
            SYSCTL_OUT(req, &name, sizeof(name)); \
        } \
\
        return error; \
    }

#define super IOService

void sysctl_register();
void sysctl_unregister();

#if DEBUG
    static int debug = 1;
#else
    static int debug = 0;
#endif

#define DLog(fmt, ...) IOLog("%s:%d " fmt "\n", __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__)
#if DEBUG
#   define Log DLog
#else
#   define Log(fmt, ...) do { if (debug) DLog(fmt, ##__VA_ARGS__); } while(0)
#endif

struct kern_ctl_reg ep_ctl; // Initialize control
kern_ctl_ref kctlref;
class Insomnia *myinstance;

#pragma mark - Sysctl Insomnia Settings
// After changing this list, make sure to update sysctl_register and sysctl_unregister!

SYSCTL_DECL(_kern_insomnia);
SYSCTL_NODE(_kern, OID_AUTO, insomnia, CTLFLAG_RW, 0, "Insomnia Settings");

INSOMNIA_SYSCTL_INT(lidsleep, "Override power states", -1, -1, 1);
INSOMNIA_SYSCTL_INT(ac_state, "Lid sleep for AC", 1, 0, 1);
INSOMNIA_SYSCTL_INT(battery_state, "Lid sleep for battery", 0, 0, 1);

// debug is just a regular sysctl managed int... we don't need to be notified when it changes
SYSCTL_INT(_kern_insomnia, OID_AUTO, debug, (CTLFLAG_ANYBODY|CTLFLAG_RW), &debug, debug, "");

#pragma mark - Init and Free

OSDefineMetaClassAndStructors(Insomnia, IOService);

bool Insomnia::init(OSDictionary* properties) {
    DLog("");
    
    if (super::init(properties) == false) {
        DLog("super::init failed");
        return false;
    }

    myinstance = this;

    sysctl_register();

    return true;
}

void Insomnia::free() {
    Log("");

    sysctl_unregister();
    
    super::free();
}

IOWorkLoop* Insomnia::getWorkLoop() {
    if (!_work_loop)
        _work_loop = IOWorkLoop::workLoop();
    
    return _work_loop;
}

# pragma mark - Start and Stop

bool Insomnia::start(IOService* provider) {
    DLog("");

    if (!super::start(provider)) {
        DLog("super::start failed");
        return false;
    }

    _last_lid_state = true; // Technically it could be closed...

    startPM(provider);

    Insomnia::send_event(kIOPMDisableClamshell);// | kIOPMPreventSleep);

    return true;
}

void Insomnia::stop(IOService* provider) {
    DLog("");

    if (_work_loop) {
        _work_loop->release();
        _work_loop = 0;
    }

    /* Reset the system to orginal state */
    stopPM();
    enable_lid_sleep();
    enable_idle_sleep();

    super::stop(provider);
}

#pragma mark - Instance operations

void Insomnia::states_changed() {
    Log("\n"
        "lidsleep == %d\n"
        "ac_state == %d\n"
        "battery_state == %d\n"
        "battery_percent_remaining() == %d\n"
        "is_on_AC() == %s",
        lidsleep, ac_state, battery_state, battery_percent_remaining(),
        is_on_AC() ? "true" : "false");

//    if (battery_percent_remaining() >= bat_threshold || cpu_temp < temp_threshold) {

    if (lidsleep == 1) {
        _lid_sleep_disabled = true;
    } else if (lidsleep == 0) {
        _lid_sleep_disabled = false;
    } else {
        if ((is_on_AC() && ac_state) || (!is_on_AC() && battery_state))
            _lid_sleep_disabled = true;
        else
            _lid_sleep_disabled = false;
    }

//    if (idlesleep == 1) {
//        _idle_sleep_disabled = true;
//    } else if (idlesleep == 0) {
//        _idle_sleep_disabled = false;
//    } else {
//        if ((is_on_AC() && ac_idle_state) || (!is_on_AC() && battery_idle_state))
//            _idle_sleep_disabled = true;
//        else
//            _idle_sleep_disabled = false;
//    }

//    } else {
//        _lid_sleep_disabled = true;
//        _idle_sleep_disabled = true;
//    }

    if (_lid_sleep_disabled) {
        disable_lid_sleep();
    } else {
        enable_lid_sleep();
        if (!_last_lid_state) {
            DLog("Sleeping because lid was already closed and now we don't want to be awake\n"
                 "Probably because we got unplugged or the battery dropped too low or the CPU"
                 " got too hot");
            send_event(kIOPMSleepNow);
        }
    }

//    if (_idle_sleep_disabled) {
//        disable_idle_sleep();
//    } else {
//        enable_idle_sleep();
//    }
}

/* Send power messages to rootDomain */
bool Insomnia::send_event(UInt32 msg) {
    IOPMrootDomain *root = NULL;
    IOReturn        ret=kIOReturnSuccess;
    
    DLog("");
    
    root = getPMRootDomain();
    if (!root) {
        DLog("Fatal error could not get RootDomain.");
        return false;
    }

    ret = root->receivePowerNotification(msg);
    
    Log("root returns %d", ret);
    
    if(ret!=kIOReturnSuccess)
    {
        DLog("Error sending event: %d", ret);
    }
    else
        Log("Message sent to root");
    
    return true;
}


/* kIOPMMessageClamshallStateChange Notification */
IOReturn Insomnia::message(UInt32 type, IOService * provider, void * argument) {
    
    if (type == kIOPMMessageClamshellStateChange) {
        bool state = (((long)argument) & kClamshellStateBit);
        clamshell_state_changed(state);

        if (state) // Opened... be a good citizen and let everyone know
            send_event(kIOPMClamshellOpened);
        // You know, I'm not even sure if this is necessary...
        // I think it causes a weird dispatch loop
//        
//        /* If lid was opened */
//        if ( ( argument && kClamshellStateBit) & (!lastLidState)) {
//            Log("kClamshellStateBit set - lid was opened");
//            lastLidState = true;
//            
//            Insomnia::send_event( kIOPMClamshellOpened);
//            
//            /* If lastLidState is true - lid closed */
//        } else if (lastLidState) {
//            Log("kClamshellStateBit not set - lid was closed");
//            lastLidState = false;
//            
//            // - send kIOPMDisableClamshell | kIOPMPreventSleep here?
//            if(origstate==1)
//                Insomnia::send_event(kIOPMDisableClamshell | kIOPMPreventSleep);
//        }
//        
//        /*        detection of system sleep probably not needed ...
//         
//        if ( argument && kClamshellSleepBit) {
//            Log("kClamshellSleepBit set - now awake");
//        } else {
//            Log("kClamshellSleepBit not set - now sleeping");
//        }
//        */
//        
    }
    
    return super::message(type, provider, argument);
}

void Insomnia::disable_lid_sleep()  { send_event(kIOPMDisableClamshell); }
void Insomnia::disable_idle_sleep() { send_event(kIOPMPreventSleep);     }
void Insomnia::enable_lid_sleep()   { send_event(kIOPMEnableClamshell);  }
void Insomnia::enable_idle_sleep()  { send_event(kIOPMAllowSleep);       }

void Insomnia::clamshell_state_changed(bool state) {
    states_changed();
    _last_lid_state = state;
}

# pragma mark - Power Management

void Insomnia::startPM(IOService *provider)
{
    static const int kMyNumberOfStates = 2;
    static IOPMPowerState myPowerStates[kMyNumberOfStates] = {
        {kIOPMPowerStateVersion1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {kIOPMPowerStateVersion1, kIOPMPowerOn, kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}
    };

    PMinit();
    provider->joinPMtree(this);
    registerPowerDriver(this, myPowerStates, kMyNumberOfStates);

    if (OSDictionary *tmpDict = serviceMatching("IOPMPowerSource"))
    {
        addMatchingNotification(gIOFirstPublishNotification, tmpDict,
                                &Insomnia::_power_source_published, this);

        tmpDict->release();
    }
}

void Insomnia::stopPM()
{
    if (_power_source) {
        _power_source->release();
        _power_source = NULL;
    }

    if (_power_state_notifier) {
        _power_state_notifier->remove();
        _power_state_notifier = NULL;
    }

    PMstop();
}

bool Insomnia::power_source_published(IOService *newService, IONotifier *notifier) {
    if (_power_source)
        _power_source->release();

    _power_source = (IOPMPowerSource *)newService;
    _power_source->retain();

    if (_power_state_notifier)
        _power_state_notifier->remove();

    _power_state_notifier = _power_source->registerInterest(gIOGeneralInterest, Insomnia::_power_source_state_changed, this); // auto-retained

    states_changed();

    return true;
}

IOReturn Insomnia::power_source_state_changed(UInt32 messageType,
                                              __unused IOService *provider,
                                              __unused void *messageArgument,
                                              __unused vm_size_t argSize)
{
    states_changed();
    // Don't filter because we're interested in usage updates and source changes
    return kIOPMAckImplied;
}

bool Insomnia::is_on_AC() {
    if (_power_source) {
        return _power_source->externalChargeCapable();
    }

    return false; // Assume battery until we know for sure
}

int Insomnia::battery_percent_remaining() {
    if (_power_source) {
        unsigned int current = _power_source->currentCapacity();
        unsigned int max     = _power_source->maxCapacity();

        int percent = (int)((((float)current)/((float)max)) * 100);

        if (percent < 0 || percent > 100)
            percent = -1;
        
        return percent;
    }

    return -1;
}

bool Insomnia::_power_source_published(void *target, __unused void *refCon,
                                       IOService *newService, IONotifier *notifier) {
    return ((Insomnia*)target)->power_source_published(newService, notifier);
}

IOReturn Insomnia::_power_source_state_changed(void *target, __unused void *refCon,
                                               UInt32 messageType, IOService *provider,
                                               void *messageArgument, vm_size_t argSize) {
    return ((Insomnia*)target)->power_source_state_changed(messageType, provider,
                                                           messageArgument, argSize);
}

IOReturn Insomnia::setPowerState(__unused unsigned long whichState,
                                 __unused IOService * whatDevice) {
    states_changed();
    return IOPMAckImplied;
}

# pragma mark - sysctl setup

void sysctl_register() {
    sysctl_register_oid(&sysctl__kern_insomnia);
    sysctl_register_oid(&sysctl__kern_insomnia_lidsleep);
    sysctl_register_oid(&sysctl__kern_insomnia_ac_state);
    sysctl_register_oid(&sysctl__kern_insomnia_battery_state);
    sysctl_register_oid(&sysctl__kern_insomnia_debug);
}

void sysctl_unregister() {
    sysctl_unregister_oid(&sysctl__kern_insomnia_debug);
    sysctl_unregister_oid(&sysctl__kern_insomnia_battery_state);
    sysctl_unregister_oid(&sysctl__kern_insomnia_ac_state);
    sysctl_unregister_oid(&sysctl__kern_insomnia_lidsleep);
    sysctl_unregister_oid(&sysctl__kern_insomnia);
}
