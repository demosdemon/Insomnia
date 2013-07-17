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

#define INSOMNIA_SYSCTL_INT(name, desc) \
    SYSCTL_PROC(_kern_insomnia, \
                OID_AUTO, \
                name, \
                CTLFLAG_ANYBODY_RW_INT, \
                &name, 0, \
                &sysctl_##name, \
                "I", desc)

#define super IOService

static int sysctl_lidsleep SYSCTL_HANDLER_ARGS;
static int sysctl_ac_state SYSCTL_HANDLER_ARGS;
static int sysctl_battery_state SYSCTL_HANDLER_ARGS;
static int sysctl_debug SYSCTL_HANDLER_ARGS;
void sysctl_register();
void sysctl_unregister();

static unsigned int lidvar = 1;
static unsigned int origstate = lidvar;

static unsigned int lidsleep = -1;
static unsigned int ac_state = 1;
static unsigned int battery_state = 0;

static unsigned int debug = 1;

#define DLog(fmt, ...) IOLog("%s:%d " fmt "\n", __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__)
#if DEBUG
#define Log DLog
#else
#define Log(fmt, ...) do { if (debug) DLog(fmt, ##__VA_ARGS__); } while(0)
#endif

SYSCTL_DECL(_kern_insomnia);
SYSCTL_NODE(_kern, OID_AUTO, insomnia, CTLFLAG_RW, 0, "Insomnia Settings");

INSOMNIA_SYSCTL_INT(lidsleep, "Override power states");
INSOMNIA_SYSCTL_INT(ac_state, "Lid sleep for AC");
INSOMNIA_SYSCTL_INT(battery_state, "Lid sleep for battery");
INSOMNIA_SYSCTL_INT(debug, "");


struct kern_ctl_reg ep_ctl; // Initialize control
kern_ctl_ref kctlref;
class Insomnia *myinstance;

OSDefineMetaClassAndStructors(Insomnia, IOService);

#pragma mark -

/* init function for Insomnia, unchanged from orginal Insomnia */
bool Insomnia::init(OSDictionary* properties) {
    DLog("");
    
    if (super::init(properties) == false) {
        DLog("super::init failed");
        return false;
    }
    
    myinstance = this;

    return true;
}


/* start function for Insomnia, fixed send_event to match other code */
bool Insomnia::start(IOService* provider) {
    
    DLog("");
    
    lastLidState = true;
    
    if (!super::start(provider)) {
        DLog("super::start failed");
        return false;
    }
    
    sysctl_register();
    
    Insomnia::send_event(kIOPMDisableClamshell);// | kIOPMPreventSleep);
    
    return true;
}


/* free function for Insomnia, fixed send_event to match other code */
void Insomnia::free() {
    IOPMrootDomain *root = NULL;
    
    root = getPMRootDomain();
    
    if (!root) {
        DLog("Fatal error could not get RootDomain.");
        return;
    }
    
    /* Reset the system to orginal state */
    Insomnia::send_event(kIOPMAllowSleep | kIOPMEnableClamshell);
    
    
    DLog("Lid close is now processed again.");
    
    super::free();
    return;
}

// ###########################################################################

IOWorkLoop* Insomnia::getWorkLoop() {
    if (!_workLoop)
        _workLoop = IOWorkLoop::workLoop();
    
    return _workLoop;
}

// ###########################################################################
void Insomnia::stop(IOService* provider) {
    if (_workLoop) {
        _workLoop->release();
        _workLoop = 0;
    }
    
    sysctl_unregister();
    
    super::stop(provider);
}

#pragma mark -

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
        Log("========================");
        Log("Clamshell State Changed");
        
        /* If lid was opened */
        if ( ( argument && kClamshellStateBit) & (!lastLidState)) {
            Log("kClamshellStateBit set - lid was opened");
            lastLidState = true;
            
            Insomnia::send_event( kIOPMClamshellOpened);
            
            /* If lastLidState is true - lid closed */
        } else if (lastLidState) {
            Log("kClamshellStateBit not set - lid was closed");
            lastLidState = false;
            
            // - send kIOPMDisableClamshell | kIOPMPreventSleep here?
            if(origstate==1)
                Insomnia::send_event(kIOPMDisableClamshell | kIOPMPreventSleep);
        }
        
        /*        detection of system sleep probably not needed ...
         
        if ( argument && kClamshellSleepBit) {
            Log("kClamshellSleepBit set - now awake");
        } else {
            Log("kClamshellSleepBit not set - now sleeping");
        }
        */
        
    }
    
    return super::message(type, provider, argument);
}

// ###########################################################################

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

static int sysctl_lidsleep SYSCTL_HANDLER_ARGS {
    int error;
    
    error = sysctl_handle_int(oidp, oidp->oid_arg1, oidp->oid_arg2,  req);
    
    if(lidvar!=1 && lidvar!=0)
        return 1;
    
    if (!error && req->newptr) {//we get value
        if(lidvar != origstate) {
            /*
            if(lidvar)//activate
                myinstance->send_event(kIOPMDisableClamshell | kIOPMPreventSleep);
                if(!lidvar)//deactivate
                    myinstance->send_event(kIOPMAllowSleep | kIOPMEnableClamshell);
            */
            origstate = lidvar;
        }
    } else if (req->newptr) {
        /* Something was wrong with the write request */
    } else {
        /* Read request.  */
        SYSCTL_OUT(req, &lidvar, sizeof(lidvar));
    }
    
    return error;
}

static int sysctl_ac_state SYSCTL_HANDLER_ARGS {
    
}

static int sysctl_battery_state SYSCTL_HANDLER_ARGS {
    
}

static int sysctl_debug SYSCTL_HANDLER_ARGS {
    
}
