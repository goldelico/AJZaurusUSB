/*
 File:		Logging.cpp
 
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

#if USE_ELG

static globals	g;	// Instantiate the globals
/****************************************************************************************************/
//
//		Function:	AllocateEventLog
//
//		Inputs:		size - amount of memory to allocate
//
//		Outputs:	None
//
//		Desc:		Allocates the event log buffer
//
/****************************************************************************************************/

static void AllocateEventLog(UInt32 size)
{
    if (g.evLogBuf)
        return;
    
    g.evLogFlag = 0;            // assume insufficient memory
    g.evLogBuf = (UInt8*)IOMalloc(size);
    if (!g.evLogBuf)
        {
        kprintf("net_lucid_cake_driver_AJZaurusUSB evLog allocation failed ");
        return;
        }
    
    bzero(g.evLogBuf, size);
    g.evLogBufp	= g.evLogBuf;
    g.evLogBufe	= g.evLogBufp + kEvLogSize - 0x20; // ??? overran buffer?
    g.evLogFlag  = 0xFEEDBEEF;	// continuous wraparound
                                //	g.evLogFlag  = 'step';		// stop at each ELG
                                //	g.evLogFlag  = 0x0333;		// any nonzero - don't wrap - stop logging at buffer end
    
    IOLog("AllocateEventLog - &globals=%8x buffer=%8x\n", (unsigned int)&g, (unsigned int)g.evLogBuf);
    
    return;
    
}/* end AllocateEventLog */

/****************************************************************************************************/
//
//		Function:	EvLog
//
//		Inputs:		a - anything, b - anything, ascii - 4 charater tag, str - any info string			
//
//		Outputs:	None
//
//		Desc:		Writes the various inputs to the event log buffer
//
/****************************************************************************************************/

static void EvLog(UInt32 a, UInt32 b, UInt32 ascii, char* str)
{
    register UInt32	*lp;           // Long pointer
    mach_timespec_t	time;
    
    if (g.evLogFlag == 0)
        return;
    
    IOGetTime(&time);
    
    lp = (UInt32*)g.evLogBufp;
    g.evLogBufp += 0x10;
    
    if (g.evLogBufp >= g.evLogBufe)       // handle buffer wrap around if any
        {    
        g.evLogBufp  = g.evLogBuf;
        if (g.evLogFlag != 0xFEEDBEEF)    // make 0xFEEDBEEF a symbolic ???
            g.evLogFlag = 0;                // stop tracing if wrap undesired
        }
    
    // compose interrupt level with 3 byte time stamp:
    
    *lp++ = (g.intLevel << 24) | ((time.tv_nsec >> 10) & 0x003FFFFF);   // ~ 1 microsec resolution
    *lp++ = a;
    *lp++ = b;
    *lp   = ascii;
    
    if(g.evLogFlag == 'step')
        {	
        static char	code[ 5 ] = {0,0,0,0,0};
        *(UInt32*)&code = ascii;
        IOLog("%8x AJZaurusUSB: %8x %8x %s\n", time.tv_nsec>>10, (unsigned int)a, (unsigned int)b, code);
        }
    
    return;
    
}/* end EvLog */
#endif // USE_ELG

#if LOG_DATA
/****************************************************************************************************/
//
//		Function:	Asciify
//
//		Inputs:		i - the nibble
//
//		Outputs:	return byte - ascii byte
//
//		Desc:		Converts to ascii. 
//
/****************************************************************************************************/

static UInt8 Asciify(UInt8 i)
{
    
    i &= 0xF;
    if (i < 10)
        return('0' + i);
    else return(('A'-10)  + i);
    
}/* end Asciify */

#define dumplen		32		// Set this to the number of bytes to dump and the rest should work out correct

#define buflen		((dumplen*2)+dumplen)+3
#define Asciistart	(dumplen*2)+3

/****************************************************************************************************/
//
//		Function:	USBLogData
//
//		Inputs:		Dir - direction
//				Count - number of bytes
//				buf - the data
//
//		Outputs:	
//
//		Desc:		Puts the data in the log. 
//
/****************************************************************************************************/

static void USBLogData(UInt8 Dir, UInt32 Count, char *buf)
{
    UInt8	wlen, i, Aspnt, Hxpnt;
    UInt8	wchr;
    char	LocBuf[buflen+1];
    
    for (i=0; i<=buflen; i++)
        {
        LocBuf[i] = 0x20;
        }
    LocBuf[i] = 0x00;
    
    if (Dir == kUSBIn)
        {
        IOLog("AJZaurusUSB:: USBLogData - Read Complete, size = %8x\n", Count);
        } 
    else 
        {
        if (Dir == kUSBOut)
            {
            IOLog("AJZaurusUSB:: USBLogData - Write, size = %8x\n", Count);
            }
        else
            {
            if (Dir == kUSBAnyDirn)
                {
                IOLog("AJZaurusUSB:: USBLogData - Other, size = %8x\n", Count);
                }
            }			
        }
    
    if (Count > dumplen)
        {
        wlen = dumplen;
        } 
    else
        {
        wlen = Count;
        }
    
    if (wlen > 0)
        {
        Aspnt = Asciistart;
        Hxpnt = 0;
        for (i=1; i<=wlen; i++)
            {
            wchr = buf[i-1];
            LocBuf[Hxpnt++] = Asciify(wchr >> 4);
            LocBuf[Hxpnt++] = Asciify(wchr);
            if ((wchr < 0x20) || (wchr > 0x7F)) 		// Non printable characters
                {
                LocBuf[Aspnt++] = 0x2E;				// Replace with a period
                }
            else
                {
                LocBuf[Aspnt++] = wchr;
                }
            }
        LocBuf[(wlen + Asciistart) + 1] = 0x00;
        IOLog("%s\n", LocBuf);
        IOSleep(Sleep_Time);					// Try and keep the log from overflowing
        } 
    else 
        {
        IOLog("AJZaurusUSB: USBLogData - No data, Count=0\n");
        }
    
}/* end USBLogData */
#endif // LOG_DATA

/* EOF */