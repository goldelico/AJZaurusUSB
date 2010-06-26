/*
 File:		Client.cpp
 
 This module implements the Client interface, i.e. the interface to the MacOS X IP Networking stack
 
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
 Copyright 2004-2010 H. Nikolaus Schaller
 
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

#define	numStats	13
UInt16	stats[13] = {	kXMIT_OK_REQ,
    kRCV_OK_REQ,
    kXMIT_ERROR_REQ,
    kRCV_ERROR_REQ, 
    kRCV_CRC_ERROR_REQ,
    kRCV_ERROR_ALIGNMENT_REQ,
    kXMIT_ONE_COLLISION_REQ,
    kXMIT_MORE_COLLISIONS_REQ,
    kXMIT_DEFERRED_REQ,
    kXMIT_MAX_COLLISION_REQ,
    kRCV_OVERRUN_REQ,
    kXMIT_TIMES_CARRIER_LOST_REQ,
    kXMIT_LATE_COLLISIONS_REQ
};


/*
 a driver is matched and used with the following calling sequence
 
 . init()
 .   attach()
 .     probe() -> success or failure
 .   detach()
 if choosen:
 .   attach()
 .     start()
 .       open()
 .         enable()
 .             ((
 .             outputPacket()
 .             message()
 .             timeoutOccurred()
 .             [[wakeUp()]]
 .             [[putToSleep()]]
 .             terminate()
 .             ))
 .         disable()
 .       close()
 .       message()
 .       willTerminate()
 .       didTerminate()
 .     stop()
 .   detach() <- failure
 . free()
 
 A Sleep mode call sequence is
 
 .        disable()
 .          [[putToSleep()]]
 .      setPowerState(0)
 . ----- Mac sleeps -----
 .      setPowerState(1)
 .          [[resetDevice()]]
 .        enable()
 .             wakeUp()
 
 NOTES:
 * [[ abc ]] means that it is an internal method
 * (( a b c )) means and sequence can occur
 * putToSleep(), wakeUp() refer to the interface/attached device and not the Mac
 
 */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::init
//
//		Inputs:		properties - data (keys and values) used to match
//
//		Outputs:	Return code - true (init successful), false (init failed)
//
//		Desc:		Initialize the driver.
//
/****************************************************************************************************/

bool net_lucid_cake_driver_AJZaurusUSB::init(OSDictionary *properties)
{
    UInt32	i;
	
    IOLog("AJZaurusUSB(%p)::init\n", this);
	IOSleep(20);
    
    if(super::init(properties) == false)
        {
        IOLog("AJZaurusUSB::init - initialize super failed\n");
        return false;
        }
    
    // Set some defaults
    
    fMax_Block_Size = MAX_BLOCK_SIZE;
    fCurrStat = 0;
    fStatInProgress = false;
    fDataDead = false;
    fCommDead = false;
    fPacketFilter = kPACKET_TYPE_DIRECTED | kPACKET_TYPE_BROADCAST | kPACKET_TYPE_MULTICAST;
    fPadded = true;		// default to use padding (TODO: find out whether this is really necessary)
    fChecksum = true;
    
    for (i=0; i<kOutBufPool; i++)
        { // initialize output buffer reference block
			fPipeOutBuff[i].pipeOutMDP = NULL;
			fPipeOutBuff[i].pipeOutBuffer = NULL;
			fPipeOutBuff[i].m = NULL;
        }
    
    fLock = IOSimpleLockAlloc();
    return true;
    
}/* end init*/

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::attach
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Attach the driver so that we can probe.
//
/****************************************************************************************************/

#if 0	// use default method from superclass
bool net_lucid_cake_driver_AJZaurusUSB::attach(IOService *provider)
{
}
#endif

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::probe
//
//		Inputs:		provider - my provider
//
//		Outputs:	score - score
//
//		Desc:		Probe the driver, i.e. determine how good we can serve the requests.
//
/****************************************************************************************************/

IOService *net_lucid_cake_driver_AJZaurusUSB::probe(IOService *provider, SInt32 *score)
{
    IOService *res = super::probe(provider, score);
    IOLog("AJZaurusUSB::probe - super::score is %ld\n", *score);	// is already 90000 (from Info.plist)
	
	// determine if we can really connect, i.e. find the right interfaces
    
	return res;    
}

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::start
//
//		Inputs:		provider - my provider
//
//		Outputs:	Return code - true (it's me), false (sorry it probably was me, but I can't configure it)
//
//		Desc:		This is called once it has beed determined I'm probably the best 
//					driver for this device.
//
/****************************************************************************************************/

bool net_lucid_cake_driver_AJZaurusUSB::start(IOService *provider)
{
    UInt8	configs;	// number of device configurations
    
    IOLog("AJZaurusUSB::start - this=%p provider=%p\n", this, provider);
	IOSleep(20);
    if(!super::start(provider))
        {
        IOLog("AJZaurusUSB::start - start super failed\n");
        return false;
        }
    
    // Get my USB device provider - the device
    
    fpDevice = OSDynamicCast(IOUSBDevice, provider);
    if(!fpDevice)
        {
        IOLog("AJZaurusUSB::start - provider invalid\n");
        stop(provider);
        return false;
        }
    
    // Let's see if we have any configurations to play with
    
    configs = fpDevice->GetNumConfigurations();
    if(configs < 1)
        {
        IOLog("AJZaurusUSB::start - no configurations\n");
        stop(provider);
        return false;
        }
    
    // Now take control of the device and configure it
    
    if(!fpDevice->open(this))
        {
        IOLog("AJZaurusUSB::start - unable to open device\n");
        stop(provider);
        return false;
        }
    
    if(!configureDevice(configs))
        {
        IOLog("AJZaurusUSB::start - configure Device failed\n");
        fpDevice->close(this);
        fpDevice = NULL;
        stop(provider);
        return false;
        }
	
    IOLog("AJZaurusUSB::start - successful\n");
#if 0	// fail for debugging
	{
	IOLog("AJZaurusUSB::start - fails for debugging purposes\n");
	fpDevice->close(this);
	fpDevice = NULL;
	stop(provider);
	return false;
	}
#endif
    return true;
    
}/* end start */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::open
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Open the driver.
//
/****************************************************************************************************/

#if 0
bool net_lucid_cake_driver_AJZaurusUSB::open()
{
}
#endif

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::enable
//
//		Inputs:		netif - the interface being enabled
//
//		Outputs:	Return code - kIOReturnSuccess or kIOReturnIOError
//
//		Desc:		Called by IOEthernetInterface client to enable the controller.
//				This method is always called while running on the default workloop
//				thread
//
/****************************************************************************************************/

IOReturn net_lucid_cake_driver_AJZaurusUSB::enable(IONetworkInterface *netif)
{
    IONetworkMedium	*medium;
    IOMediumType    	mediumType = kIOMediumEthernet10BaseT | kIOMediumOptionFullDuplex;
    
    IOLog("AJZaurusUSB::enable %p\n", netif);
	IOSleep(20);
    
    // If an interface client has previously enabled us,
    // and we know there can only be one interface client
    // for this driver, then simply return success.
	
    if (fNetifEnabled)
        {
        IOLog("AJZaurusUSB::enable - already enabled\n");
        return kIOReturnSuccess;
        }
    
    if (!fReady && !wakeUp())
        {
        IOLog("AJZaurusUSB::enable - wakeup failed\n");
        return kIOReturnIOError;
        }
    
    // Mark the controller as enabled by the interface.
    
    fNetifEnabled = true;
    
    // Assume an active link (leave this in for now - until we know better)
    // Should probably use the values returned in the Network Connection notification
    // that is if we have an interrupt pipe, otherwise default to these
    
    fLinkStatus = 1;
    
    medium = IONetworkMedium::getMediumWithType(fMediumDict, mediumType);
	
    IOLog("AJZaurusUSB::enable - medium type=%lu and medium=%p\n", mediumType, medium);
	
    setLinkStatus(kIONetworkLinkActive | kIONetworkLinkValid, medium, 10 * 1000000);	// this should switch the Network status to green
    IOLog("AJZaurusUSB::enable - LinkStatus set\n");
    
    // Start our IOOutputQueue object.
    
    fTransmitQueue->setCapacity(TRANSMIT_QUEUE_SIZE);
    IOLog("AJZaurusUSB::enable - capacity set to %d\n", TRANSMIT_QUEUE_SIZE);
    fTransmitQueue->start();
    IOLog("AJZaurusUSB::enable - transmit queue started\n");
    
    USBSetPacketFilter();
    IOLog("AJZaurusUSB::enable - packet filter applied\n");
	IOSleep(20);
    
    return kIOReturnSuccess;
    
}/* end enable */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::message
//
//		Inputs:		type - message type, provider - my provider, argument - additional parameters
//
//		Outputs:	return Code - kIOReturnSuccess
//
//		Desc:		Handles IOKit messages. 
//
/****************************************************************************************************/

IOReturn net_lucid_cake_driver_AJZaurusUSB::message(UInt32 type, IOService *provider, void *argument)
{
    IOReturn	ior;
    
    IOLog("AJZaurusUSB::message - type %lu/%08lx provider=%p argument=%p\n", type, type, provider, argument);
    
    if (!fpDevice)	// no USB provider
        return kIOReturnUnsupported;
	
    switch (type)
	{
		// general messages
        case kIOMessageServiceIsTerminated:
		IOLog("AJZaurusUSB::message - kIOMessageServiceIsTerminated ready=%d type=%lu\n", fReady, type);
		
		if (fReady)
			{
			if (!fTerminate)		// Check if we're already being terminated
				{ 
                    IOLog("AJZaurusUSB::message - hardcoded message!\n");
                    // NOTE! This call below depends on the hard coded path of this KEXT. Make sure
                    // that if the KEXT moves, this path is changed!
                    KUNCUserNotificationDisplayNotice(
                                                      0,		// Timeout in seconds
                                                      0,		// Flags (for later usage)
                                                      "",		// iconPath (not supported yet)
                                                      "",		// soundPath (not supported yet)
                                                      "",		// localizationPath (AJ: removed hard coded path, rather not support it)
                                                      "Unplug Header",		// the header
                                                      "Unplug Notice",		// the notice - look in Localizable.strings
                                                      "OK"); 
				}
			}
		else
			{
			if (fCommInterface)	
				{
				if (fCommInterface != fDataInterface)
					fCommInterface->close(this);
				fCommInterface->release();
				fCommInterface = NULL;	
				}
			
			if (fDataInterface)	
				{ 
					fDataInterface->close(this);	
					fDataInterface->release();
					fDataInterface = NULL;	
				}
			
			fpDevice->close(this); 	// need to close so we can get the free and stop calls, only if no sessions active (see putToSleep)
			fpDevice = NULL;
			}
		
		fTerminate = true;		// we're being terminated (unplugged)
		return kIOReturnSuccess;
        case kIOMessageServiceIsSuspended:
		IOLog("AJZaurusUSB::message - kIOMessageServiceIsSuspended\n");
		break;			
        case kIOMessageServiceIsResumed: 	
		IOLog("AJZaurusUSB::message - kIOMessageServiceIsResumed\n");
		break;			
        case kIOMessageServiceIsRequestingClose: 
		IOLog("AJZaurusUSB::message - kIOMessageServiceIsRequestingClose\n"); 
		break;
        case kIOMessageServiceIsAttemptingOpen: 	
		IOLog("AJZaurusUSB::message - kIOMessageServiceAttemptingOpen\n"); 
		break;
        case kIOMessageServiceWasClosed: 	
		IOLog("AJZaurusUSB::message - kIOMessageServiceWasClosed\n"); 
		break;
        case kIOMessageServiceBusyStateChange: 	
		IOLog("AJZaurusUSB::message - kIOMessageServiceBusyStateChange\n"); 
		break;
        case kIOMessageServicePropertyChange: 	
		IOLog("AJZaurusUSB::message - kIOMessageServicePropertyChange\n"); 
		break;
		
		// USB messages
		
        case kIOUSBMessagePortHasBeenResumed: 	
		IOLog("AJZaurusUSB::message - kIOUSBMessagePortHasBeenResumed\n");
		
		// If the reads are dead try and resurrect them
		
		if (fCommDead && fCommPipe)
			{
			ior = fCommPipe->Read(fCommPipeMDP,
								  5000,	// 5 seconds until timeout
								  5000,
								  fCommPipeMDP->getLength(),
								  &fCommCompletionInfo,
								  NULL);
			//               ior = fCommPipe->Read(fCommPipeMDP, , NULL);
			if (ior != kIOReturnSuccess)
				{
				IOLog("AJZaurusUSB::message - Failed to queue Comm pipe read: %d\n", ior);
				}
			else
				{
				fCommDead = false;
				}
			}
		// and again...
		if (fDataDead)
			{
			ior = fInPipe->Read(fPipeInMDP,
								5000,	// 5 seconds until timeout
								5000,
								fPipeInMDP->getLength(),
								&fReadCompletionInfo,
								NULL);
			if (ior != kIOReturnSuccess)
				{
				IOLog("AJZaurusUSB::message - Failed to queue Data pipe read: %d\n", ior);
				}
			else 
				{
				fDataDead = false;
				}
			}
		
		return kIOReturnSuccess;
        case kIOUSBMessageHubResumePort:
		IOLog("AJZaurusUSB::message - kIOUSBMessageHubResumePort\n");
		break;
		// power
        case kIOMessageDeviceHasPoweredOn:
		IOLog("AJZaurusUSB::message - kIOMessageDeviceHasPoweredOn\n");
		break;
        case kIOMessageSystemWillSleep:
		IOLog("AJZaurusUSB::message - kIOMessageSystemWillSleep\n");
		break;
        case kIOMessageSystemHasPoweredOn:
		IOLog("AJZaurusUSB::message - kIOMessageSystemHasPoweredOn\n");
		break;
		// any other
        default:
		IOLog("AJZaurusUSB::message - unknown message\n");
		break;
	}
	IOSleep(20);    
    return kIOReturnUnsupported;
}/* end message */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::outputPacket
//
//		Inputs:		mbuf - the packet (may be NULL after awaking from sleep)
//					param - optional parameter
//
//		Outputs:	Return code - kIOReturnOutputSuccess or kIOReturnOutputStall
//
//		Desc:		Packet transmission. The BSD mbuf needs to be formatted correctly
//					and transmitted
//
/****************************************************************************************************/

UInt32 net_lucid_cake_driver_AJZaurusUSB::outputPacket(mbuf_t pkt, void *param)
{
    UInt32	ret = kIOReturnOutputSuccess;
#if 0
    IOLog("AJZaurusUSB::outputPacket(%p)\n", pkt);
	IOSleep(20);
#endif
	if(!pkt)
        {
        IOLog("AJZaurusUSB::outputPacket(NULL)\n");
		}
	else if(!fLinkStatus)
        {
        IOLog("AJZaurusUSB::outputPacket(%p) - link is down (%d)\n", pkt, fLinkStatus);
        if(fOutputErrsOK)
            fpNetStats->outputErrors++;
        freePacket(pkt);
        } 
    else if(!USBTransmitPacket(pkt))
        { // wasn't able to transmit
			ret = kIOReturnOutputStall;
			// ret = kIOReturnOutputDropped;
			IOLog("AJZaurusUSB::outputPacket(%p) - packet dropped\n", pkt);
			// If the driver returns kIOReturnDropped, it should also put the mbuf_t back into the network stack
			// Õs common pool by invoking the superclassÕs freePacket function.   
			freePacket(pkt);
        }
    return ret;
}/* end outputPacket */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::timeoutOccurred
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Timeout handler, used for stats gathering.
//
/****************************************************************************************************/

void net_lucid_cake_driver_AJZaurusUSB::timeoutOccurred(IOTimerEventSource *timer)
{
    UInt16		currStat;
    IOReturn		rc;
    IOUSBDevRequest	*STREQ;
    bool		statOk = false;
    
	//    IOLog("AJZaurusUSB::timeoutOccurred\n");
    
    if (fReady)
        fTransmitQueue->service(IOBasicOutputQueue::kServiceAsync); // AJ: revive a stalled queue (according to Apple's documentation, this call doesn't do any harm, even if the queue wasn't stalled).
    else
        IOLog("AJZaurusUSB::timeoutOccurred - Spurious\n");    
    
    if ((fEthernetStatistics[0]|fEthernetStatistics[1]|fEthernetStatistics[2]|fEthernetStatistics[3]) == 0)
        { // no bit is set
			//       IOLog("AJZaurusUSB::timeoutOccurred - No Ethernet statistics defined\n");
			fTimerSource->setTimeoutMS(WATCHDOG_TIMER_MS);	// AJ: set up the watchdog again
			return;
        }
    
    
    // Only get statistics it if it's not already in progress
    
    if (!fStatInProgress)
        {
        
        // Check if the stat we're currently interested in is supported
        
        currStat = stats[fCurrStat++];
        if (fCurrStat >= numStats)
            fCurrStat = 0;
        switch(currStat)
            {
				case kXMIT_OK_REQ:
                if (fEthernetStatistics[0] & kXMIT_OK)
                    statOk = true;
                break;
				case kRCV_OK_REQ:
                if (fEthernetStatistics[0] & kRCV_OK)
                    statOk = true;
                break;
				case kXMIT_ERROR_REQ:
                if (fEthernetStatistics[0] & kXMIT_ERROR_REQ)
                    statOk = true;
                break;
				case kRCV_ERROR_REQ:
                if (fEthernetStatistics[0] & kRCV_ERROR_REQ)
                    statOk = true;
                break;
				case kRCV_CRC_ERROR_REQ:
                if (fEthernetStatistics[2] & kRCV_CRC_ERROR)
                    statOk = true;
                break;
				case kRCV_ERROR_ALIGNMENT_REQ:
                if (fEthernetStatistics[2] & kRCV_ERROR_ALIGNMENT)
                    statOk = true;
                break;
				case kXMIT_ONE_COLLISION_REQ:
                if (fEthernetStatistics[2] & kXMIT_ONE_COLLISION)
                    statOk = true;
                break;
				case kXMIT_MORE_COLLISIONS_REQ:
                if (fEthernetStatistics[2] & kXMIT_MORE_COLLISIONS)
                    statOk = true;
                break;
				case kXMIT_DEFERRED_REQ:
                if (fEthernetStatistics[2] & kXMIT_DEFERRED)
                    statOk = true;
                break;
				case kXMIT_MAX_COLLISION_REQ:
                if (fEthernetStatistics[2] & kXMIT_MAX_COLLISION)
                    statOk = true;
                break;
				case kRCV_OVERRUN_REQ:
                if (fEthernetStatistics[3] & kRCV_OVERRUN)
                    statOk = true;
                break;
				case kXMIT_TIMES_CARRIER_LOST_REQ:
                if (fEthernetStatistics[3] & kXMIT_TIMES_CARRIER_LOST)
                    statOk = true;
                break;
				case kXMIT_LATE_COLLISIONS_REQ:
                if (fEthernetStatistics[3] & kXMIT_LATE_COLLISIONS)
                    statOk = true;
                break;
				default:
                break;
            }
        }
    
    if (statOk)
        {
        STREQ = (IOUSBDevRequest*)IOMalloc(sizeof(IOUSBDevRequest));
        if (!STREQ)
            IOLog("AJZaurusUSB::timeoutOccurred - allocate STREQ failed\n");
        else
            {
            bzero(STREQ, sizeof(IOUSBDevRequest));
            
            // Now build the Statistics Request
            
            STREQ->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
            STREQ->bRequest = kGet_Ethernet_Statistics;
            STREQ->wValue = currStat;
            STREQ->wIndex = fCommInterfaceNumber;
            STREQ->wLength = 4;
            STREQ->pData = &fStatValue;
            
            fStatsCompletionInfo.parameter = STREQ;
            
            rc = fpDevice->DeviceRequest(STREQ, &fStatsCompletionInfo);
            if (rc != kIOReturnSuccess)
                {
                IOLog("AJZaurusUSB::timeoutOccurred - Error issueing DeviceRequest for %d: %d\n", STREQ->bRequest, rc);
                IOFree(STREQ, sizeof(IOUSBDevRequest));
                }
            else
                fStatInProgress = true;
            }
        }
    
	//    IOLog("AJZaurusUSB::timeoutOccurred - Ethernet statistics done\n");
    
    // Restart the watchdog timer
    
    fTimerSource->setTimeoutMS(WATCHDOG_TIMER_MS);
    
}/* end timeoutOccurred */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::wakeUp
//
//		Inputs:		
//
//		Outputs:	Return Code - true(we're awake), false(failed)
//
//		Desc:		Resumes the device it it was suspended and then gets all the data
//				structures sorted out and all the pipes ready.
//
/****************************************************************************************************/

bool net_lucid_cake_driver_AJZaurusUSB::wakeUp()
{
    IOReturn 	rtn = kIOReturnSuccess;
    
    IOLog("AJZaurusUSB::wakeUp\n");
 	IOSleep(20);
	
    fReady = false;
    
    if (fTimerSource)
        fTimerSource->cancelTimeout();	// cancel any running timer
    
    setLinkStatus(0, 0);				// Initialize the link state
	
    if (!allocateResources()) 
        {
        return false;
        }
    
    // Read the comm interrupt pipe for status:
    
    fCommCompletionInfo.target = this;
    fCommCompletionInfo.action = commReadComplete;
    fCommCompletionInfo.parameter = NULL;
    
	//    if(fCommPipe)
	//      {
	//rtn = fCommPipe->Read(fCommPipeMDP, &fCommCompletionInfo, NULL);
	rtn = kIOReturnSuccess;
    //    }
    if(rtn == kIOReturnSuccess)
        {
        // Read the data-in bulk pipe:
        
        fReadCompletionInfo.target = this;
        fReadCompletionInfo.action = dataReadComplete;
        fReadCompletionInfo.parameter = NULL;
		
		rtn = fInPipe->Read(fPipeInMDP,
							5000,	// 5 seconds until timeout
							5000,
							fPipeInMDP->getLength(),
							&fReadCompletionInfo,
							NULL);
		
        if (rtn == kIOReturnSuccess)
            {
            // Set up the data-out bulk pipe:
            
			for (int i=0; i<kOutBufPool; i++)
                {
                fPipeOutBuff[i].writeCompletionInfo.target = this;
                fPipeOutBuff[i].writeCompletionInfo.action = dataWriteComplete;
                fPipeOutBuff[i].writeCompletionInfo.parameter = NULL;				// for now, filled in with the mbuf address when sent
                }
            
            // Set up the management element request completion routine:
            
            fMERCompletionInfo.target = this;
            fMERCompletionInfo.action = merWriteComplete;
            fMERCompletionInfo.parameter = NULL;				// for now, filled in with parm block when allocated
            
            // Set up the statistics request completion routine:
            
            fStatsCompletionInfo.target = this;
            fStatsCompletionInfo.action = statsWriteComplete;
            fStatsCompletionInfo.parameter = NULL;				// for now, filled in with parm block when allocated
            }
        }
    
    if (rtn != kIOReturnSuccess)
        {
        
        // We failed for some reason
        
        IOLog("AJZaurusUSB::wakeUp - Setting up the pipes failed\n" );
        releaseResources();
        return false;
        } 
    else
		{
        if (!fMediumDict && !createMediumTables())
			{
			IOLog("AJZaurusUSB::wakeUp - createMediumTables failed\n" );
			releaseResources();
			return false;
			}
		
        fTimerSource->setTimeoutMS(WATCHDOG_TIMER_MS);
        fReady = true;
        }
    
    return true;
    
}/* end wakeUp */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::putToSleep
//
//		Inputs:		
//
//		Outputs:	Return Code - true(we're asleep), false(failed)
//
//		Desc:		Do clean up and suspend the device.
//
/****************************************************************************************************/

void net_lucid_cake_driver_AJZaurusUSB::putToSleep()
{
    //IOReturn	ior;
    
    IOLog("AJZaurusUSB::putToSleep\n");
    
    fReady = false;
    
    if (fTimerSource)
        fTimerSource->cancelTimeout();
	
    setLinkStatus(0, 0);
    
    // Release all resources
    
    releaseResources();
    
    //if (!fTerminate)
    //{
    //    if (fbmAttributes & kUSBAtrBusPowered)
    //    {
    //        ior = fpDevice->SuspendDevice(true);         // Suspend the device again (if supported and not unplugged)
    //        if (ior)
    //        {
    //            ALERT(0, ior, 'rPSD', "net_lucid_cake_driver_AJZaurusUSB::releasePort - SuspendDevice error");
    //        }
    //    }
    //}
    // AJ: The above is commented out because it's an endless source of
    // trouble (at least with the Zaurus, probably also with other
    // PDAs).
    // Not trying to suspend/wake up the Zaurus at all seems to be just
    // fine (it won't suspend anyway upon SuspendDevice(true)).
    
    if(fTerminate && (!fReady))		// if it's the result of a terminate and no interfaces enabled we also need to close the device
        {
		IOLog("AJZaurusUSB::putToSleep - close device\n");
        fpDevice->close(this);
        fpDevice = NULL;
        }
    
}/* end putToSleep */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::disable
//
//		Inputs:		netif - the interface being disabled
//
//		Outputs:	Return code - kIOReturnSuccess
//
//		Desc:		Called by IOEthernetInterface client to disable the controller.
//				This method is always called while running on the default workloop
//				thread
//
/****************************************************************************************************/

IOReturn net_lucid_cake_driver_AJZaurusUSB::disable(IONetworkInterface *netif)
{
    
    IOLog("AJZaurusUSB::disable\n");
    
    // Disable our IOOutputQueue object. This will prevent the
    // outputPacket() method from being called
    
	if(fTransmitQueue)
		{
		fTransmitQueue->stop();
		fTransmitQueue->setCapacity(0);
		fTransmitQueue->flush();	// Flush all packets currently in the output queue
		}
	
    putToSleep();
    
    fNetifEnabled = false;
    fReady = false;
    
    return kIOReturnSuccess;
    
}/* end disable */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::close
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Close the driver.
//
/****************************************************************************************************/

#if 0
bool net_lucid_cake_driver_AJZaurusUSB::close()
{
}
#endif

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::stop
//
//		Inputs:		provider - my provider
//
//		Outputs:	None
//
//		Desc:		Stops
//
/****************************************************************************************************/

void net_lucid_cake_driver_AJZaurusUSB::stop(IOService *provider)
{
    IOLog("AJZaurusUSB::stop\n");
 	IOSleep(20);
	
    if (fNetworkInterface)
        {
        detachInterface(fNetworkInterface);
        fNetworkInterface->release();
        fNetworkInterface = NULL;
        }
	
    if (fCommInterface)	
        {
		if (fCommInterface != fDataInterface)
            fCommInterface->close(this);
        fCommInterface->release();
        fCommInterface = NULL;	
        }
    
    if (fDataInterface)	
        { 
			fDataInterface->close(this);	
			fDataInterface->release();
			fDataInterface = NULL;	
        }
    
    if (fpDevice)
        {
        fpDevice->close(this);
        fpDevice = NULL;
        }
    
    if (fMediumDict)
        {
        fMediumDict->release();
        fMediumDict = NULL;
        }
    
    if (fTransmitQueue)
        {
        fTransmitQueue->release();
        fTransmitQueue = NULL;
        }
	
    super::stop(provider);
    
    return;
    
}/* end stop */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::detach
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Detach the driver.
//
/****************************************************************************************************/

#if 0	// default method from superclass
bool net_lucid_cake_driver_AJZaurusUSB::detach(IOService *provider)
{
}
#endif

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::free
//
//		Inputs:		None
//
//		Outputs:	None
//
//		Desc:		Clean up and free the log 
//
/****************************************************************************************************/

void net_lucid_cake_driver_AJZaurusUSB::free()
{
    
    IOLog("AJZaurusUSB::free\n");
	
    super::free();
    IOSimpleLockFree(fLock);
    return;
    
}/* end free */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::registerWithPolicyMaker
//
//		Inputs:		provider - my provider
//
//		Outputs:	return code - true(initialized), false(failed)
//
//		Desc:		Add ourselves to the power management tree so we can do
//					the right thing on sleep/wakeup. 
//
//		see:		http://developer.apple.com/mac/library/documentation/DeviceDrivers/Conceptual/IOKitFundamentals/PowerMgmt/PowerMgmt.html#//apple_ref/doc/uid/TP0000020-BABCCBIJ
//					and section "Network / Power Management" of http://developer.apple.com/mac/library/documentation/DeviceDrivers/Conceptual/IOKitFundamentals/Families_Ref/Families_Ref.html#//apple_ref/doc/uid/TP0000021-BABFBCFG
//
/****************************************************************************************************/

IOReturn net_lucid_cake_driver_AJZaurusUSB::registerWithPolicyMaker( IOService * policyMaker )
{
    IOReturn ioreturn;
	
	static IOPMPowerState gOurPowerStates[kNumCDCStates] =
	{
    {kIOPMPowerStateVersion1,0,0,0,0,0,0,0,0,0,0,0},
    {kIOPMPowerStateVersion1,kIOPMDeviceUsable,kIOPMPowerOn,kIOPMPowerOn,0,0,0,0,0,0,0,0}
	};
	
	IOLog("AJZaurusUSB::registerWithPolicyMaker\n");
	
    fPowerState = kCDCPowerOnState;				// init our power state to be 'on'
	ioreturn = policyMaker->registerPowerDriver( this, gOurPowerStates, kNumCDCStates);
    return ioreturn;
    
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
    IOLog("AJZaurusUSB::setPowerState - power %lu\n", whatState);
	
    if(!(whatState == kCDCPowerOffState || whatState == kCDCPowerOnState))
		return IOPMNoSuchState;
	
	if(whatState == fPowerState)
		return IOPMAckImplied;	// unchanged
	fPowerState=whatState;
	if(fPowerState == kCDCPowerOnState)
		{
		IOLog("AJZaurusUSB::setPowerState - power on\n");
		// do whatever we need to do
		
		resetDevice();
		}
	else
		{
		IOLog("AJZaurusUSB::setPowerState - power off\n");		
		}
	return IOPMNoErr;
}

/* EOF */