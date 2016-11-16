//
//  InsomniaSettings.hpp
//  Insomnia
//
//  Created by Brandon LeBlanc on 11/5/16.
//  Copyright © 2016 PabuCo. All rights reserved.
//

#ifndef InsomniaSettings_hpp
#define InsomniaSettings_hpp

#include <sys/sysctl.h>
#include "LinkedList.hpp"

#define PROPERTY(name, default, min, max) \
private: \
    int name = default; \
public: \
    virtual int get_##name () const { \
        return name; \
    } \
\
    virtual bool set_##name (int value) { \
        if (value < min || value > max) \
            return false; \
        PropertyChangeEvent event = { #name, this->name, value }; \
        bool save = dispatch(&event); \
        if (save) \
            name = value; \
        return save; \
    }


/*! kern.insomnia
 */
class InsomniaSettings {
private:
    typedef const char * string;

public:
    typedef bool (InsomniaSettings::*PropertySetter)(int value);

    struct PropertyChangeEvent {
        string setting;
        int oldValue;
        int newValue;
    };

    class Watcher {
    public:
        virtual bool property_changed(const struct PropertyChangeEvent * event) = 0;
    };

    InsomniaSettings();
    ~InsomniaSettings();

    void initialize();

    /*! kern.insomnia.debug
     0 => terse IOLog messages
     1 => verbose IOLog debug messages
     */
#if DEBUG
    PROPERTY(debug, 0, 0, 1);
#else
    PROPERTY(debug, 1, 0, 1);
#endif

    /*! kern.insomnia.lidsleep
     0 => default, does nothing: allows system to sleep when the lid is closed
     1 => does not allow the system to sleep when the lid is closed (except where configured)
     */
    PROPERTY(lidsleep, 0, 0, 1);

    /*! kern.insomnia.ac_state
     0 => sleep when connect to ac power
     1 => default, do not sleep when connect to ac power
     */
    PROPERTY(ac_state, 1, 0, 1);

    /*! kern.insomnia.battery_state
     -1 => default, sleep when on battery power
     0 => do not sleep when on battery, but sleep when battery drops below threshold
     1 => do not sleep when on battery power
     */
    PROPERTY(battery_state, 0, -1, 1);

    /*! kern.insomnia.battery_threshold
     default => 30
     */
    PROPERTY(battery_threshold, 30, 0, 100);

    /*! kern.insomnia.cpu_temp_state
     0 => do not sleep when cpu exceeds temperature threshold
     1 => default, sleep when cpu exceeds temperature threshold
     */
    PROPERTY(cpu_temp_state, 1, 0, 1);

    /*! kern.insomnia.cpu_threshold
     default => 80 degrees celsius
     max => 105 degrees celsius
     */
    PROPERTY(cpu_threshold, 80, 0, 105);

    void addWatcher(Watcher * watcher);
    bool removeWatcher(Watcher * watcher);

protected:
    class Setting {
        friend InsomniaSettings;

    private:
        InsomniaSettings * owner = NULL;
        PropertySetter setter = NULL;
        string name = NULL;
        string type = NULL;
        string description = NULL;

        struct sysctl_oid oid;

        void init();

    public:

        bool newValue(int value);
    };

    bool dispatch(PropertyChangeEvent * event) const;

    virtual Setting * addSetting(PropertySetter setter, string name, string type, string description);

private:
    LinkedList<Watcher *> watchers;

    struct sysctl_oid_list sysctl__kern_insomnia_children;
    LinkedList<Setting *> settings;

    void register_oids();
    void deregister_oids();

    static int sysctl_handle_proc SYSCTL_HANDLER_ARGS;
};

#undef PROPERTY
#endif /* InsomniaSettings_hpp */
