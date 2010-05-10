/*
    File:		Driver.h
	
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
        
		<0.5.1>     Dec 2006    fixed a Kernel Panic on MacBook Pro
		<0.5.0>     Nov 2006    increased stability; made work on Intel Macs
 
		<0.3.4>     Aug 2006    Source file split into modules

		....
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
        
#include <machine/limits.h>			/* UINT_MAX */
#include <libkern/OSByteOrder.h>

#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOGatedOutputQueue.h>

#include <IOKit/IOTimerEventSource.h>
#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMessage.h>

#include <sys/kpi_mbuf.h>
#include <IOKit/pwr_mgt/RootDomain.h>

#include <IOKit/usb/IOUSBBus.h>
#include <IOKit/usb/IOUSBNub.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/USB.h>
#include <IOKit/usb/IOUSBInterface.h>

#include <UserNotification/KUNCUserNotifications.h>

extern "C"
{
    #include <sys/param.h>
    #include <sys/mbuf.h>
}

#define DEBUG		1

#include "Logging.h"

#define TRANSMIT_QUEUE_SIZE     5				// How does this relate to MAX_BLOCK_SIZE?
#define WATCHDOG_TIMER_MS       1000

#define MAX_BLOCK_SIZE		PAGE_SIZE
#define COMM_BUFF_SIZE		16					// must be a multiple of Mac packet Size of endpoint (8)

#define kFiltersSupportedMask	0xefff
#define kPipeStalled		1

#define kOutBufPool		100
// try to lower this in future versions (without producing stalls)
#define kOutBuffThreshold	10

        // USB CDC Definitions (Ethernet Control Model)
		
#define kEthernetControlModel	6		
#define kMDLM 0x0a

    //	Requests

enum
{
    kSend_Encapsulated_Command		= 0,
    kGet_Encapsulated_Response		= 1,
    kSet_Ethernet_Multicast_Filter	= 0x40,
    kSet_Ethernet_PM_Packet_Filter	= 0x41,
    kGet_Ethernet_PM_Packet_Filter	= 0x42,
    kSet_Ethernet_Packet_Filter		= 0x43,
    kGet_Ethernet_Statistics		= 0x44,
    kGet_AUX_Inputs			= 4,
    kSet_AUX_Outputs			= 5,
    kSet_Temp_MAC			= 6,
    kGet_Temp_MAC			= 7,
    kSet_URB_Size			= 8,
    kSet_SOFS_To_Wait			= 9,
    kSet_Even_Packets			= 10,
    kScan				= 0xFF
};

    // Notifications

enum
{
    kNetwork_Connection			= 0,
    kResponse_Available			= 1,
    kConnection_Speed_Change		= 0x2A
};

enum
{
    CS_INTERFACE			= 0x24,
		
    Header_FunctionalDescriptor		= 0x00,
    CM_FunctionalDescriptor		= 0x01,
    Union_FunctionalDescriptor		= 0x06,
    CS_FunctionalDescriptor		= 0x07,
    Enet_Functional_Descriptor		= 0x0f,
		
    CM_ManagementData			= 0x01,
    CM_ManagementOnData			= 0x02
};

    // Stats of interest in bmEthernetStatistics (bit definitions)

enum
{
    kXMIT_OK =			0x01,		// Byte 1
    kRCV_OK =			0x02,
    kXMIT_ERROR =		0x04,
    kRCV_ERROR =		0x08,

    kRCV_CRC_ERROR =		0x02,		// Byte 3
    kRCV_ERROR_ALIGNMENT =	0x08,
    kXMIT_ONE_COLLISION =	0x10,
    kXMIT_MORE_COLLISIONS =	0x20,
    kXMIT_DEFERRED =		0x40,
    kXMIT_MAX_COLLISION =	0x80,

    kRCV_OVERRUN =		0x01,		// Byte 4
    kXMIT_TIMES_CARRIER_LOST =	0x08,
    kXMIT_LATE_COLLISIONS =	0x10
};

    // Stats request definitions
  
enum
{
    kXMIT_OK_REQ =			0x0001,
    kRCV_OK_REQ =			0x0002,
    kXMIT_ERROR_REQ =			0x0003,
    kRCV_ERROR_REQ =			0x0004,

    kRCV_CRC_ERROR_REQ =		0x0012,
    kRCV_ERROR_ALIGNMENT_REQ =		0x0014,
    kXMIT_ONE_COLLISION_REQ =		0x0015,
    kXMIT_MORE_COLLISIONS_REQ =		0x0016,
    kXMIT_DEFERRED_REQ =		0x0017,
    kXMIT_MAX_COLLISION_REQ =		0x0018,

    kRCV_OVERRUN_REQ =			0x0019,
    kXMIT_TIMES_CARRIER_LOST_REQ =	0x001c,
    kXMIT_LATE_COLLISIONS_REQ =		0x001d
};

enum
{
    kCDCPowerOffState	= 0,
    kCDCPowerOnState	= 1,
    kNumCDCStates	= 2
};

    // Packet Filter definitions
  
enum
{
    kPACKET_TYPE_PROMISCUOUS =		0x0001,
    kPACKET_TYPE_ALL_MULTICAST =	0x0002,
    kPACKET_TYPE_DIRECTED =		0x0004,
    kPACKET_TYPE_BROADCAST =		0x0008,
    kPACKET_TYPE_MULTICAST =		0x0010
};

typedef struct 
{
    UInt8	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
} HeaderFunctionalDescriptor;

typedef struct 
{
    UInt8	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
    UInt8 	bmCapabilities;
    UInt8 	bDataInterface;
} CMFunctionalDescriptor;
	
typedef struct
{
    UInt8 	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
    UInt8 	iMACAddress;
    UInt8 	bmEthernetStatistics[4];
    UInt8 	wMaxSegmentSize[2];
    UInt8 	wNumberMCFilters[2];
    UInt8 	bNumberPowerFilters;
} EnetFunctionalDescriptor;

typedef struct 
{
    UInt8	bFunctionLength;
    UInt8 	bDescriptorType;
    UInt8 	bDescriptorSubtype;
    UInt8 	bMasterInterface;
    UInt8	bSlaveInterface[];
} UnionFunctionalDescriptor;

typedef struct 
{
    IOBufferMemoryDescriptor	*pipeOutMDP;
    UInt8						*pipeOutBuffer;
    mbuf_t						m;
	IOUSBCompletion				writeCompletionInfo;
} pipeOutBuffers;

#define super IOEthernetController

class net_lucid_cake_driver_AJZaurusUSB : public IOEthernetController
{
    OSDeclareDefaultStructors(net_lucid_cake_driver_AJZaurusUSB);	// Constructor & Destructor stuff

private:
    bool			fTerminate;					// Are we being terminated (ie the device was unplugged)
    UInt8			fbmAttributes;				// Device attributes
    UInt16			fVendorID;
    UInt16			fProductID;
	char			vendorString[50];
	char			modelString[50];
	char			serialString[50];
        
    IOEthernetInterface		*fNetworkInterface;	// the interface of which we are the Client
    IOBasicOutputQueue		*fTransmitQueue;

    IOWorkLoop				*fWorkLoop;
    IONetworkStats			*fpNetStats;
    IOEthernetStats			*fpEtherStats;
    IOTimerEventSource		*fTimerSource;
    
    OSDictionary			*fMediumDict;

    bool					fReady;
    bool					fNetifEnabled;
    bool					fWOL;
    bool					fDataDead;
    bool					fCommDead;
    UInt8					fLinkStatus;
    UInt32					fUpSpeed;
    UInt32					fDownSpeed;
    UInt16					fPacketFilter;
     
    IOUSBInterface		*fCommInterface;
    IOUSBInterface		*fDataInterface;
    
    IOUSBPipe			*fInPipe;
    IOUSBPipe			*fOutPipe;
    IOUSBPipe			*fCommPipe;
    
    IOBufferMemoryDescriptor	*fCommPipeMDP;
    IOBufferMemoryDescriptor	*fPipeInMDP;
    //IOBufferMemoryDescriptor	*fPipeOutMDP;

    UInt8			*fCommPipeBuffer;
    UInt8			*fPipeInBuffer;
    pipeOutBuffers	fPipeOutBuff[kOutBufPool];
    
    UInt8			fCommInterfaceNumber;
    UInt8			fDataInterfaceNumber;
    UInt32			fCount;
    UInt32			fOutPacketSize;

	bool			fPadded;
	bool			fChecksum;
	UInt8			fInterfaceClass;		// interface class
	UInt8			fInterfaceSubClass;
	IOSimpleLock*	fLock;
	SInt32			fDataCount;
	bool			fOutputStalled;
    
    UInt8			fEaddr[6];				// ethernet address
    UInt16			fMax_Block_Size;
    UInt16			fMcFilters;
    UInt8 			fEthernetStatistics[4];
    
    UInt16			fCurrStat;
    UInt32			fStatValue;
	UInt8			fPowerState;
    bool			fStatInProgress;
    bool			fInputPktsOK;
    bool			fInputErrsOK;
    bool			fOutputPktsOK;
    bool			fOutputErrsOK;

    IOUSBCompletion		fCommCompletionInfo;
    IOUSBCompletion		fReadCompletionInfo;
    //IOUSBCompletion		fWriteCompletionInfo;
    IOUSBCompletion		fMERCompletionInfo;
    IOUSBCompletion		fStatsCompletionInfo;

	// callbacks
	
    static void			commReadComplete(void *obj, void *param, IOReturn ior, UInt32 remaining);
    static void			dataReadComplete(void *obj, void *param, IOReturn ior, UInt32 remaining);
    static void			dataWriteComplete(void *obj, void *param, IOReturn ior, UInt32 remaining);
    static void			merWriteComplete(void *obj, void *param, IOReturn ior, UInt32 remaining);
    static void			statsWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining);
    
    // internal CDC Driver instance Methods
	
    bool			wakeUp(void);
    void			putToSleep(void);
    bool			createMediumTables(void);
    bool 			allocateResources(void);
    void			releaseResources(void);
    bool 			configureDevice(UInt8 numConfigs);
    void			dumpDevice(UInt8 numConfigs);
    bool			getFunctionalDescriptors(void);
    bool			createNetworkInterface(void);
    bool			USBTransmitPacket(mbuf_t packet);
    bool			USBSetMulticastFilter(IOEthernetAddress *addrs, UInt32 count);
    bool			USBSetPacketFilter(void);
    IOReturn		clearPipeStall(IOUSBPipe *thePipe);
	void			resetDevice(void);
    void			receivePacket(UInt8 *packet, UInt32 size);
    static void 	timerFired(OSObject *owner, IOTimerEventSource *sender);
    void			timeoutOccurred(IOTimerEventSource *timer);

public:

    IOUSBDevice				*fpDevice;		// our provider (USB device)

        // IOKit methods
        
    virtual bool			init(OSDictionary *properties = 0);
	virtual	IOService		*probe(IOService *provider, SInt32 *score);
    virtual bool			start(IOService *provider);
    virtual IOReturn		message(UInt32 type, IOService *provider, void *argument = 0);
    virtual void			free(void);
    virtual void			stop(IOService *provider);

        // IOEthernetController methods

    virtual IOReturn		enable(IONetworkInterface *netif);
    UInt32					outputPacket(mbuf_t pkt, void *param);
    virtual IOReturn		disable(IONetworkInterface *netif);
    virtual IOReturn		setWakeOnMagicPacket(bool active);
    virtual IOReturn		getPacketFilters(const OSSymbol	*group, UInt32 *filters ) const;
    virtual IOReturn		selectMedium(const IONetworkMedium *medium);
    virtual IOReturn		getHardwareAddress(IOEthernetAddress *addr);
    virtual IOReturn		setMulticastMode(IOEnetMulticastMode mode);
    virtual IOReturn		setMulticastList(IOEthernetAddress *addrs, UInt32 count);
    virtual IOReturn		setPromiscuousMode(IOEnetPromiscuousMode mode);
    virtual IOOutputQueue	*createOutputQueue(void);
    virtual const OSString	*newVendorString(void) const;
    virtual const OSString	*newModelString(void) const;
    virtual const OSString	*newRevisionString(void) const;
    virtual bool			configureInterface(IONetworkInterface *netif);
	virtual bool			createWorkLoop();
	virtual IOWorkLoop*		getWorkLoop() const;
	
		// Power Manager
	
	bool					initForPM(IOService *provider);
	unsigned long			initialPowerStateForDomainState(IOPMPowerFlags flags);
	IOReturn				setPowerState(unsigned long, IOService *);
	
}; /* end class net_lucid_cake_driver_AJZaurusUSB */
