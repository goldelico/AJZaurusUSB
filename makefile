include ../Versions.def

.PHONY:	all AJZaurusUSB tgz clean src pkg help check load unload install uninstall enable-kdb disable-kdb gdb

all:	AJZaurusUSB

patch-version-number:
	while read LINE; \
			do \
			case "$$LINE" in \
				"<key>CFBundleGetInfoString</key>" ) \
					echo "$$LINE"; \
					echo "<string>AJZaurusUSB version $(VERSION_AJZaurusUSB), Copyright 2002-2004 Andreas Junghans &amp; 2005-2009 Nikolaus Schaller, based on USBCDCEthernet, Copyright 1998-2002 Apple</string>"; \
					read IGNORE;; \
				"<key>CFBundleShortVersionString</key>" ) \
					echo "$$LINE"; \
					echo "<string>$(VERSION_AJZaurusUSB)</string>"; \
					read IGNORE;; \
				"<key>CFBundleVersion</key>" ) \
					echo "$$LINE"; \
					echo "<string>$(VERSION_AJZaurusUSB)</string>"; \
					read IGNORE;; \
				* ) \
					echo "$$LINE";; \
				esac; \
			done <Info.plist >Info.plist.tmp && mv -f Info.plist.tmp Info.plist

	# here we should patch the version number into Info.plist, and AJZaurusUSB.pmdoc (the latter is a binary PList: root/$objects/53 = version number)

AJZaurusUSB: patch-version-number
	@echo "Building AJZaurusUSB $(VERSION_AJZaurusUSB)"
	(sudo rm -rf build pkg && mkdir pkg && xcodebuild -target AJZaurusUSB)
	sudo chown -R root pkg/AJZaurusUSB.kext
	sudo chgrp -R wheel pkg/AJZaurusUSB.kext
	sudo chmod -R go-w pkg/AJZaurusUSB.kext

tgz: pkg src
	@echo "Packing AJZaurusUSB-$(VERSION_AJZaurusUSB).tgz"
	tar cvzf AJZaurusUSB-$(VERSION_AJZaurusUSB).tgz --exclude .svn ./AJZaurusUSB.pkg ./COPYING ./WELCOME.rtf ./README.rtfd ./HISTORY.rtf

clean:
	@echo "Cleaning AJZaurusUSB"
	sudo rm -rf build pkg
	sudo find . -name .DS_Store -exec rm {} \;

src: clean
	@echo "Packing AJZaurusUSB-$(VERSION_AJZaurusUSB)-src.tgz"
	tar cvzf AJZaurusUSB-$(VERSION_AJZaurusUSB)-src.tgz \
		 --exclude .svn \
		./COPYING ./*.rtf* \
		./HOWTO-MAKE ./makefile \
		./Sources \
		./*.plist \
		./*.xcode* ./*.lproj \
		./*.pmproj ./*.pmdoc \
		./*.term

help:
	@echo "Makefile for making AJZaurusUSB"
	@echo ""
	@echo "Supported targets:"
	@echo "make all     - build driver (asks for root password)"
	@echo "make check   - check driver dependencies"
	@echo "make load    - load driver"
	@echo "make unload  - unload driver"
	@echo "make install - permanently install"
	@echo "make uninstall - permanently uninstall"
	@echo "make pkg     - installer package"
	@echo "make src     - source distribution"
	@echo "make tgz     - full distribution file (incl. src)"
	@echo "make gdb     - debug driver on 2nd machine"
	
load:
	@echo "Loading AJZaurusUSB"
	sudo kextload pkg/AJZaurusUSB.kext
	
check:
	@echo "Checking AJZaurusUSB"
	sudo kextload -c -tn -v 3 pkg/AJZaurusUSB.kext
	
unload:
	@echo "Unloading AJZaurusUSB"
	sudo kextunload pkg/AJZaurusUSB.kext

install:
	# permanently install on your local machine
	@echo "Installing AJZaurusUSB"
	sudo sh -c "(cd pkg && tar cf - AJZaurusUSB.kext) | (cd /System/Library/Extensions && tar xvf -)"
	@echo "Loading AJZaurusUSB"
	sudo kextload /System/Library/Extensions/AJZaurusUSB.kext
	@echo "****************************************************"
	@echo "You should now reboot to load the driver permanently"
	@echo "****************************************************"

uninstall:
	# uninstall existing driver
	@echo "Unloading AJZaurusUSB"
	- sudo kextunload /System/Library/Extensions/AJZaurusUSB.kext
	@echo "Uninstalling AJZaurusUSB"
	sudo rm -rf /System/Library/Extensions/AJZaurusUSB.kext
	@echo "****************************************************"
	@echo "You should now reboot to really uninstall the driver"
	@echo "****************************************************"

pkg: AJZaurusUSB
	open AJZaurusUSB.pmdoc

# experimental - not working yet

.PHONY:	enable-kdb sudo copy2target loadremote getsymtab gdb

#
# remote debugging
#
# make sure you have the same MacOS X versions
# connect both machines by Ethernet (we will use ZeroConf)
# make sure that the Ethernet cable is the only connection (i.e. no connection through a common WLAN!)
# configure parameters for the debug machine

# ZeroConf name
#TARGET := PowerBook.local
#ARCH := ppc

#TARGET := MacBook.local
TARGET := MacBookPro.local
ARCH := i386

#
# run this makefile on the development machine!
#
# use 'make enable-kdb' to enable debugging, reboot debug machine and 'make gdb'
#
# please refer to http://developer.apple.com/documentation/Darwin/Conceptual/KEXTConcept/KEXTConceptDebugger/hello_debugger.html
#

# same user id on debug machine
USER := $(USER)

# sometimes ssh and/or sudo may execute the commands in a tcsh even if we expclicity have a /bin/bash
# if anyone knows how and where this can be controlled, please let me know

sudo:
	@echo "*** reset sudo timeout ***"
	@echo "first password is for authentication of debug ssh access, second one is for debug sudo"
	ssh $(USER)@$(TARGET) sudo -v	# update timestamp

# enable kernel debugging on debug machine (and switch a Core2 Duo to single processor mode)
enable-kdb: sudo
	open http://developer.apple.com/documentation/Darwin/Conceptual/KEXTConcept/KEXTConceptDebugger/hello_debugger.html
	ssh $(USER)@$(TARGET) sh -c 'cd; cat >~/.ssh/authorized_keys' <~/.ssh/id_rsa.pub
	ssh $(USER)@$(TARGET) sh -c 'cd; sudo nvram boot-args="cpus=1 debug=0x14e"'
	@echo "*** reboot target machine to activate changes ***"

# disable kernel debugging on debug machine
disable-kdb:
	ssh $(USER)@$(TARGET) sh -c 'cd; sudo nvram boot-args=""'
	@echo "*** reboot debug machine to complete changes ***"

copy2target: sudo
	@echo "*** check address and access to debug machine ***"
	ping -c 1 $(TARGET)
	@echo "*** copy kernel extension to debug machine ***"
	@echo "next password is for authentication of debug ssh access; may also ask for sudo password on debug machine"
	(cd pkg && tar czpf - AJZaurusUSB.kext) | ssh $(USER)@$(TARGET) sh -c 'cd; cd ~ && sudo rm -rf AJZaurusUSB.kext && tar xzf - && sudo chown -R root:wheel ~/AJZaurusUSB.kext && sudo chmod -R go-w ~/AJZaurusUSB.kext'
			
getsymtab: copy2target
	@echo "*** create symbol files, fetch from debug machine ***"
	@echo "next password is for authentication of debug ssh access"
	ssh $(USER)@$(TARGET) sh -c 'cd; mkdir -p /tmp/AJZaurusUSB && sudo kextload -lvs /tmp/AJZaurusUSB ~/AJZaurusUSB.kext'
	@mkdir -p symbols
	ssh $(USER)@$(TARGET) sh -c 'cd; cd /tmp/AJZaurusUSB && tar czf - .' | (cd symbols && tar xvzf - -m)

loadremote: sudo
	@echo "*** finally load/match driver on remote side ***"
	@echo "next password is for authentication of debug ssh access"
	ssh $(USER)@$(TARGET) sudo /bin/bash -c 'cd; kextload -m ~/AJZaurusUSB.kext'

# launch a complete debugging session
gdb:
	@echo "*** launch and attach debugger ***"
	@( echo make getsymtab loadremote; \
		echo add-symbol-file symbols/net.lucid-cake.driver.AJZaurusUSB.sym; \
		echo add-symbol-file symbols/com.apple.iokit.IONetworkingFamily.sym; \
		echo add-symbol-file symbols/com.apple.iokit.IOUSBFamily.sym; \
		echo add-symbol-file symbols/com.apple.kpi.bsd.sym; \
		echo add-symbol-file symbols/com.apple.kpi.iokit.sym; \
		echo add-symbol-file symbols/com.apple.kpi.libkern.sym; \
		echo add-symbol-file symbols/com.apple.kpi.mach.sym; \
		echo add-symbol-file symbols/com.apple.kpi.unsupported.sym; \
		echo target remote-kdp; \
		echo shell echo; \
		echo shell echo; \
		echo shell echo "Please break on debug machine into debugger and press Return on this machine (often NMI or Power button)."; \
		echo shell echo "If successful, a console message will be shown on the screen."; \
		echo shell read WAIT; \
		echo shell echo "Attaching..."; \
		echo attach $(TARGET); \
		: echo break 'net_lucid_cake_driver_AJZaurusUSB::start(IOService *)'; \
		echo shell echo "Remotely loading and starting the driver..."; \
		echo "shell ssh $(USER)@$(TARGET) sh -c 'cd; sudo kextload -m ~/AJZaurusUSB.kext' &"; \
		echo shell echo "Continuing the kernel..."; \
		echo shell echo "If successful, the clock in the Status bar will continue to run."; \
		echo continue ) >prepare.gdb

	gdb -arch $(ARCH) -x prepare.gdb /mach_kernel

