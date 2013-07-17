# Insomnia #

## What is it? ##

Insomnia is a Mac OS X Kernel extension that prevents the system to go to sleep when the lid
is closed on Mac Books.

## Usage ##
```sh
cd /path/to/repo/
xcodebuild -target Insomnia -configuration Release
sudo cp -R build/Release/Insomnia.kext /System/Library/Extensions/
sudo touch /System/Library/Extensions

kextstat | grep co.pabu.kext.insomnia
#   137    0 0xffffff7f8213e000 0x2000     0x2000     co.pabu.kext.insomnia (1.1.1d1) <4 3 1>
# If a line like above is printed out, you are done. Otherwise continue
sudo kextload /System/Library/Extensions/Insomnia.kext
```

Build the kernel extension with Xcode. In OS X 10.8, and even more so in 10.9, all kernel
extensions must be code signed by a certificate the system trusts. Refer to the 
[Apple Documents][apple_codesign_url] on how to generate a self-signed certificate if you are
not a registered Apple Developer.

By default, this version of the KEXT will disable sleep on clamshell close only when connected to
AC. When on battery, it will allow the system to sleep.  You can use `/usr/sbin/sysctl` to
control some of the settings. The kext is built to where you do not need to be 
root to change these settings.

Use `/etc/sysctl.conf` to have these values persist across reboots. [ref][apple_sysctl_conf]

To change this:
```sh
/usr/sbin/sysctl kern.insomnia.lidsleep
# => kern.insomnia.lidsleep: -1 1
# -1 is default, it means honor power state setings.

/usr/sbin/sysctl -w kern.insomnia.lidsleep=0
# 0 overrides power state settings and allows the system to sleep when the lid is closed
# 1 overrides power state settings and does not allow the system to sleep when the lid is closed

/usr/sbin/sysctl -w kern.insomnia.ac_state=1
# 0 allows the system to sleep when the lid is closed while connected to AC
# 1 does not allow the system to sleep when the lid is closed while connected to AC

/usr/sbin/sysctl -w kern.insomnia.battery_state=0
# 0 allows the system to sleep when the lid is closed while on battery
# 1 does not allow the system to sleep when the lid is closed while on battery

/usr/sbin/sysctl -w kern.insomnia.debug=1
# 0 less verbose Console.app output
# 1 more verbose Console.app output
```

## TODO: ##
* Actually implement settings
* Allow sleep on temperature threshold
* Allow sleep on battery threshold
* Add settings for idle sleep
* Insomnia.prefPane

## Credits & License ##

This project is a fork of an [original code][original_project_url] by Michael Roßberg.
As the downloads links provided on the official home page did not work, I cloned the project from [sourceforge][sourceforge_project_url].

The project is release under the GPL license.

[original_project_url]: https://binaervarianz.de/projekte/programmieren/meltmac/
[sourceforge_project_url]: http://insomnia-kext.sourceforge.net/
[apple_codesign_url]: https://developer.apple.com/library/mac/documentation/security/Conceptual/CodeSigningGuide/Procedures/Procedures.html#//apple_ref/doc/uid/TP40005929-CH4-SW1
[apple_sysctl_conf]: https://developer.apple.com/library/mac/documentation/darwin/reference/manpages/man5/sysctl.conf.5.html