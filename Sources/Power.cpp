/*
 File:		Power.cpp
 
 This module manages power up and down of the Mac
 
 Description:	This is an experimental driver that enables Mac OS X to communicate with a Sharp Zaurus
 over IP over USB.
 
 It is based on the USBCDCEthernet example which is
 Copyright 1998-2002 Apple Computer, Inc. All rights reserved.
 Portions have been ported from usbdnet.c (Linux driver) written by
 Stuart Lynne <sl@lineo.com> and Tom Rushworth <tbr@lineo.com> with some
 algorithms adopted from usbnet.c written by
 David Brownell <dbrownell@users.sourceforge.net>.
 
 Original description for USBCDCEthernet:
 This is a sample USB Communication Device Class (CDC) driver, Ethernet model.
 Note that this sample has not been tested against any actual hardware since there
 are very few CDC Ethernet devices currently in existence. 
 
 This sample requires Mac OS X 10.1 and later. If built on a version prior to 
 Mac OS X 10.2, a compiler warning "warning: ANSI C++ forbids data member `ip_opts'
 with same name as enclosing class" will be issued. This warning can be ignored.
 
 Copyright:		Copyright 2002, 2003 Andreas Junghans
 Copyright 2004-2006 H. Nikolaus Schaller
 
 Disclaimer:		This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 
 Change History (most recent first):
 
 <x>     2004-2005   Support for Zaurus C1000, C3000
 made compile on MacOS X 10.4 with Xcode 2.0
 power management added
 <7>  	07/04/03	Support for Zaurus C760.
 <6>  	02/18/03	Increased stability + support for iPaq/Opie.
 <5>  	02/10/03	Bug fix (introduces a workloop).
 <4>  	01/24/03	Support for more Zaurus models.
 <3>  	01/11/03	Modified to include latest changes from sample driver
 (provided by Russ Winsper <russw@apple.com>)
 <2>  	12/22/02	Modified to support the Sharp Zaurus (and some bug fixes)
 <1>	 	11/08/02	New sample (provided by Apple Computer, Inc.)
 
 */

#include "Driver.h"

static IOPMPowerState gOurPowerStates[kNumCDCStates] =
{
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
};


/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::initForPM
//
//		Inputs:		provider - my provider
//
//		Outputs:	return code - true(initialized), false(failed)
//
//		Desc:		Add ourselves to the power management tree so we can do
//				the right thing on sleep/wakeup. 
//
/****************************************************************************************************/

bool net_lucid_cake_driver_AJZaurusUSB::initForPM(IOService *provider)
{
    
    fPowerState = kCDCPowerOnState;				// init our power state to be 'on'
    PMinit();							// init power manager instance variables
    provider->joinPMtree(this);					// add us to the power management tree
    if (pm_vars != NULL)
		{
		
		// register ourselves with ourself as policy-maker
        
        registerPowerDriver(this, gOurPowerStates, kNumCDCStates);
        return true;
		}
	else
		{
		IOLog("AJZaurusUSB::initForPM - Initializing power manager failed\n");
		}
    return false;
    
}/* end initForPM */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::initialPowerStateForDomainState
//
//		Inputs:		flags - 
//
//		Outputs:	return code - Current power state
//
//		Desc:		Request for our initial power state. 
//
/****************************************************************************************************/

unsigned long net_lucid_cake_driver_AJZaurusUSB::initialPowerStateForDomainState(IOPMPowerFlags flags)
{
	
    return fPowerState;
    
}/* end initialPowerStateForDomainState */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::setPowerState
//
//		Inputs:		whatState - new state
//
//		Outputs:	Return code - IOPMAckImplied
//
//		Desc:		We are notified to turn power on/off
//
/****************************************************************************************************/

IOReturn net_lucid_cake_driver_AJZaurusUSB::setPowerState(unsigned long whatState, IOService *whatDevice)
{ // turn your device on/off here
    IOLog("AJZaurusUSB::setPowerState state:%lu\n", whatState);
	
    if(whatState == kCDCPowerOffState || whatState == kCDCPowerOnState)
        {
        IOLog("AJZaurusUSB::setPowerState - power %lu\n", whatState);
		if(whatState == fPowerState)
			return IOPMAckImplied;	// unchanged
		fPowerState=whatState;
		if(fPowerState == kCDCPowerOnState)
			{
			IOLog("AJZaurusUSB::setPowerState - powered on\n");
			// do whatever we need to do
			resetDevice();
			}
		return IOPMNoErr;
        }

    return IOPMNoSuchState;
}

/* EOF */