/*
 File:		CRC.h
 
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

#ifndef INCLUDE_CRC_H
#define INCLUDE_CRC_H

#include <machine/limits.h>			/* UINT_MAX */

// needs both to declare UInt32

#include <libkern/OSByteOrder.h>
#include <IOKit/IOLib.h>

// AJ: begin CRC code (ported from Linux driver usbdnet.c written by
// Stuart Lynne <sl@lineo.com> and Tom Rushworth <tbr@lineo.com>
// with some algorithms adopted from usbnet.c by
// David Brownell <dbrownell@users.sourceforge.net>
// -->

extern UInt32 crc32_table[256];

#define CRC32_INITFCS     0xffffffff  // Initial FCS value 
#define CRC32_GOODFCS     0xdebb20e3  // Good final FCS value 

#define CRC32_FCS(fcs, c) (((fcs) >> 8) ^ crc32_table[((fcs) ^ (c)) & 0xff])

/* fcs_memcpy32 - memcpy and calculate fcs
* Perform a memcpy and calculate fcs using ppp 32bit CRC algorithm.
*/ 
static inline UInt32 fcs_memcpy32(unsigned char *dp, unsigned char *sp, int len, UInt32 fcs)
{   
    for (;len-- > 0; fcs = CRC32_FCS(fcs, *dp++ = *sp++));
    return fcs;
}

/* fcs_pad32 - pad and calculate fcs
* Pad and calculate fcs using ppp 32bit CRC algorithm.
*/
static inline UInt32 fcs_pad32(unsigned char *dp, int len, UInt32 fcs)
{
    for (;len-- > 0; fcs = CRC32_FCS(fcs, *dp++ = '\0'));
    return fcs;
}

/* fcs_compute32 - calculate fcs
* Perform calculate fcs using ppp 32bit CRC algorithm.
*/
static inline UInt32 fcs_compute32(unsigned char *sp, int len, UInt32 fcs)
{
    for (;len-- > 0; fcs = CRC32_FCS(fcs, *sp++));
    return fcs;
}
// <--

#endif INCLUDE_CRC_H
/* EOF */