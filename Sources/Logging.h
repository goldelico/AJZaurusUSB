/*
    File:		Logging.h
	
    Description:	This is an experimental driver that enables Mac OS X to communicate with a Sharp Zaurus
	                    over IP over USB. It is based on the USBCDCEthernet example which is
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
        
			<7>  	07/04/03	Support for Zaurus C760.
			<6>  	02/18/03	Increased stability + support for iPaq/Opie.
			<5>  	02/10/03	Bug fix (introduced a workloop).
			<4>  	01/24/03	Support for more Zaurus models.
        	<3>  	01/11/03	Modified to include latest changes to the sample driver
								(provided by Russ Winsper <russw@apple.com>)
			<2>  	12/22/02	Modified to support the Sharp Zaurus
            <1>	 	11/08/02	New sample (provided by Apple Computer, Inc.)
        
*/

	/*****	For fans of kprintf, IOLog and debugging infrastructure of the	*****/
	/*****	string ilk, please modify the ELG and PAUSE macros or their	*****/
	/*****	associated EvLog and Pause functions to suit your taste. These	*****/
	/*****	macros currently are set up to log events to a wraparound	*****/
	/*****	buffer with minimal performance impact. They take 2 UInt32	*****/
	/*****	parameters so that when the buffer is dumped 16 bytes per line,	*****/
	/*****	time stamps (~1 microsecond) run down the left side while	*****/
	/*****	unique 4-byte ASCII codes can be read down the right side.	*****/
	/*****	Preserving this convention facilitates different maintainers	*****/
	/*****	using different debugging styles with minimal code clutter.	*****/

#ifndef INCLUDE_LOGGING_H
#define INCLUDE_LOGGING_H

#include <machine/limits.h>			/* UINT_MAX */
#include <libkern/OSByteOrder.h>

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>

#include <sys/kpi_mbuf.h>

#include <UserNotification/KUNCUserNotifications.h>

extern "C"
{
    #include <sys/param.h>
    #include <sys/mbuf.h>
}

#define LDEBUG		0			// for debugging
#define USE_ELG		0			// to event log - LDEBUG must also be set
#define kEvLogSize  (4096*16)			// 16 pages = 64K = 4096 events
#define	LOG_DATA	0			// logs data to the IOLog - LDEBUG must also be set

#define Sleep_Time	20

#if LDEBUG
    #if USE_ELG
        #define ELG(A,B,ASCI,STRING)    EvLog((UInt32)(A), (UInt32)(B), (UInt32)(ASCI), STRING)		
    #else /* not USE_ELG */
        #define ELG(A,B,ASCI,STRING)	{IOLog("AJZaurusUSB: %8x %8x " STRING "\n", (unsigned int)(A), (unsigned int)(B));IOSleep(Sleep_Time);}
    #endif /* USE_ELG */
    #if LOG_DATA
        #define LogData(D, C, b)	USBLogData((UInt8)D, (UInt32)C, (char *)b)
    #else /* not LOG_DATA */
        #define LogData(D, C, b)
    #endif /* LOG_DATA */
#else /* not LDEBUG */
    #define ELG(A,B,ASCI,S)
    #define LogData(D, C, b)
    #undef USE_ELG
    #undef LOG_DATA
#endif /* LDEBUG */

#if DEBUG
// nothing
#else
#define IOLog(...)	// remove all logging code
#endif

// #define ALERT(A,B,ASCI,STRING)	{IOLog("AJZaurusUSB: %8x %8x " STRING "\n", (unsigned int)(A), (unsigned int)(B));}

    // Globals

typedef struct globals      // Globals for this module (not per instance)
{
    UInt32      evLogFlag; 				// debugging only
    UInt8       *evLogBuf;
    UInt8       *evLogBufe;
    UInt8       *evLogBufp;
    UInt8       intLevel;
} globals;

#endif INCLUDE_LOGGING_H

/* EOF */
