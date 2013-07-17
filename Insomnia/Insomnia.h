/*
    File:        Insomnia.h
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

#include <IOKit/IOService.h>
#include <IOKit/pwr_mgt/IOPMPowerSource.h>

#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>


class Insomnia : public IOService {
    OSDeclareDefaultStructors(Insomnia);

public:
    virtual bool init(OSDictionary * = 0);
    virtual void free();
    
    virtual bool start(IOService* provider);
    virtual void stop(IOService* provider);

    virtual IOReturn message(UInt32 type, IOService *provider, void *argument = 0);
    virtual IOWorkLoop* getWorkLoop();

    bool send_event(UInt32 msg);

    void states_changed();

protected:
    void disable_lid_sleep();
    void disable_idle_sleep();

    void enable_lid_sleep();
    void enable_idle_sleep();

    void clamshell_state_changed(bool state);

    bool power_source_published(IOService * newService, IONotifier * notifier);
	IOReturn power_source_state_changed(UInt32 messageType, IOService * provider,
                                        void * messageArgument, vm_size_t argSize);
    
private:
    bool        _idle_sleep_disabled;
    bool        _lid_sleep_disabled;
    bool        _last_lid_state;
    IOWorkLoop* _work_loop;

    IONotifier *_power_state_notifier;
	IOPMPowerSource *_power_source;

	bool is_on_AC();
    int battery_percent_remaining();

	void startPM(IOService *provider);
	void stopPM();

    static bool _power_source_published(void * target, void * refCon,
                                        IOService * newService, IONotifier * notifier);

    static IOReturn _power_source_state_changed(void * target, void * refCon,
                                                UInt32 messageType, IOService * provider,
                                                void * messageArgument, vm_size_t argSize);

    virtual IOReturn setPowerState(unsigned long whichState, IOService * whatDevice);

};
