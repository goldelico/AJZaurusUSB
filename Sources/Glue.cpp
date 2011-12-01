/*
 File:		Glue.cpp
 
 This module glues the Client and the Provider interface so that they work together as a Ethernet over USB driver
 It provides a packet queue and manages resources
 
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

static struct MediumTable
{
    UInt32	type;
    UInt32	speed;
}

mediumTable[] =
{
    {kIOMediumEthernetNone,												0},
    {kIOMediumEthernetAuto,												0},
    {kIOMediumEthernet10BaseT 	 | kIOMediumOptionHalfDuplex,								10},
    {kIOMediumEthernet10BaseT 	 | kIOMediumOptionFullDuplex,								10},
    {kIOMediumEthernet100BaseTX  | kIOMediumOptionHalfDuplex,								100},
    {kIOMediumEthernet100BaseTX  | kIOMediumOptionFullDuplex,								100}
};

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::commReadComplete
//
//		Inputs:		obj - me
//                  param - parameter block(the Port)
//                  rc - return code
//                  remaining - what's left
//                  (whose idea was that?)
//
//		Outputs:	None
//
//		Desc:		Interrupt pipe (Comm interface) read completion routine
//
/****************************************************************************************************/

void net_lucid_cake_driver_AJZaurusUSB::commReadComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    net_lucid_cake_driver_AJZaurusUSB	*me = (net_lucid_cake_driver_AJZaurusUSB*)obj;
    IOReturn		ior;
    UInt32		dLen;
    UInt8		notif;
    
    if (!me->fReady)
        {
        return;
        }
    if (rc == kIOReturnSuccess)	// If operation returned ok
        {
        dLen = /*COMM_BUFF_SIZE*/ me->fCommPipeMDP->getLength() - remaining;
        IOLog("AJZaurusUSB::commReadComplete - len=%lu\n", dLen);
        
        // Now look at the state stuff
        
 //       LogData(kUSBAnyDirn, dLen, me->fCommPipeBuffer);
        
        notif = me->fCommPipeBuffer[1];
        if (dLen > 7)
            {
            switch(notif)
                {
					case kNetwork_Connection:
                    me->fLinkStatus = me->fCommPipeBuffer[2];
                    IOLog("AJZaurusUSB::commReadComplete - kNetwork_Connection - link Status %d\n", me->fLinkStatus);
                    break;
					case kConnection_Speed_Change:				// In you-know-whose format
                    me->fUpSpeed = USBToHostLong(*((UInt32 *)&me->fCommPipeBuffer[8]));
                    me->fDownSpeed = USBToHostLong(*((UInt32 *)&me->fCommPipeBuffer[13]));
                    IOLog("AJZaurusUSB::commReadComplete - kConnection_Speed_Change up=%lu down=%lu\n", me->fUpSpeed, me->fDownSpeed);
                    break;
					default:
                    IOLog("AJZaurusUSB::commReadComplete - Unknown notification: %d\n", notif);
                    break;
                }
			IOSleep(20);
            }
        else
            IOLog("AJZaurusUSB::commReadComplete - Invalid notification %d\n", notif);
        } 
    else
        IOLog("AJZaurusUSB::commReadComplete - IO error: %d %s\n", rc, me->stringFromReturn(rc));
    
    // Queue the next read, only if not aborted
    
    if (rc != kIOReturnAborted)
        {
		ior = me->fCommPipe->Read(me->fCommPipeMDP,
								  5000,	// 5 seconds until timeout
								  5000,
								  me->fCommPipeMDP->getLength(),
								  &me->fCommCompletionInfo,
								  NULL);
		
        if (ior != kIOReturnSuccess)
            {
            IOLog("AJZaurusUSB::commReadComplete - Failed to queue next read: %d %s\n", ior, me->stringFromReturn(ior));
            if (ior == kIOUSBPipeStalled)
                {
                me->fCommPipe->ClearPipeStall(true);
				ior = me->fCommPipe->Read(me->fCommPipeMDP,
										  5000,	// 5 seconds until timeout
										  5000,
										  me->fCommPipeMDP->getLength(),
										  &me->fCommCompletionInfo,
										  NULL);
				
                if (ior != kIOReturnSuccess)
                    {
                    IOLog("AJZaurusUSB::commReadComplete - Failed, read dead: %d %s\n", ior, me->stringFromReturn(ior));
                    me->fCommDead = true;
                    }
                }
            }
        }
}/* end commReadComplete */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::dataReadComplete
//
//		Inputs:		obj - me
//                  param - unused
//                  rc - return code
//                  remaining - what's left
//
//		Outputs:	None
//
//		Desc:		BulkIn pipe (Data interface) read completion routine
//
/****************************************************************************************************/

void net_lucid_cake_driver_AJZaurusUSB::dataReadComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    net_lucid_cake_driver_AJZaurusUSB	*me = (net_lucid_cake_driver_AJZaurusUSB*)obj;
    UInt32		dLen;
    IOReturn		ior;
    
    if(!me->fReady)
        {
#if 1
        IOLog("AJZaurusUSB::dataReadComplete - not ready\n");
#endif   
        return;
        }
    if(rc == kIOReturnSuccess)	// If operation returned ok
        {
		dLen=me->fPipeInMDP->getLength() - remaining;
#if 0
        IOLog("AJZaurusUSB::dataReadComplete - len=%lu\n", dLen);
#endif   
//        LogData(kUSBIn, dlen, me->fPipeInBuffer);
        me->receivePacket(me->fPipeInBuffer, dLen);	// Move the incoming bytes up the stack
        } 
    else
        { // some error
#if 1
			if(rc == kIOUSBPipeStalled)
				{
				IOLog("AJZaurusUSB::dataReadComplete - err=kIOUSBPipeStalled\n");
				// rc = me->clearPipeStall(me->fInPipe);
				rc = me->fInPipe->ClearPipeStall(true);
				if(rc != kIOReturnSuccess)
					IOLog("AJZaurusUSB::dataReadComplete - clear pipe stall failed: %d %s\n", rc, me->fDataInterface->stringFromReturn(rc));
				}
			else if(rc == kIOUSBTransactionTimeout)
				{
				IOLog("AJZaurusUSB::dataReadComplete - Timeout err: %d %s\n", rc, me->fDataInterface->stringFromReturn(rc));
				rc = me->fInPipe->ClearPipeStall(true);
				if(rc != kIOReturnSuccess)
					IOLog("AJZaurusUSB::dataReadComplete - clear pipe stall failed: %d %s\n", rc, me->fDataInterface->stringFromReturn(rc));
				}
			else
				IOLog("AJZaurusUSB::dataReadComplete - IO err: %d %s\n", rc, me->fDataInterface->stringFromReturn(rc));
#endif
        }
	
    // Queue the next read
	
	ior = me->fInPipe->Read(me->fPipeInMDP,
							10000,	// 10 seconds until timeout
							10000,
							me->fPipeInMDP->getLength(),
							&me->fReadCompletionInfo,
							NULL);
	
    if(ior != kIOReturnSuccess)
        {
        IOLog("AJZaurusUSB::dataReadComplete - Failed to queue read: %d %s\n", ior, me->fDataInterface->stringFromReturn(rc));
        if(ior == kIOUSBPipeStalled)
            {
			//			IOLog("AJZaurusUSB::dataReadComplete - err=kIOUSBPipeStalled\n");
            me->fInPipe->ClearPipeStall(true);
			ior = me->fInPipe->Read(me->fPipeInMDP,
									10000,	// 10 seconds until timeout
									10000,
									me->fPipeInMDP->getLength(),
									&me->fReadCompletionInfo,
									NULL);
			
            if(ior != kIOReturnSuccess)
                {
                IOLog("AJZaurusUSB::dataReadComplete - Failed again, read dead: %d %s\n", ior, me->fDataInterface->stringFromReturn(rc));
                me->fDataDead = true;
                }
            }
        }
	
} /* end dataReadComplete */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::dataWriteComplete
//
//		Inputs:		obj - me
//				param - pool index
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		BulkOut pipe (Data interface) write completion routine
//
/****************************************************************************************************/

void net_lucid_cake_driver_AJZaurusUSB::dataWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    net_lucid_cake_driver_AJZaurusUSB	*me = (net_lucid_cake_driver_AJZaurusUSB *)obj;
    //UInt32		pktLen = 0;
    UInt32		poolIndx;
    SInt32		dataCount = 0;
    bool		stalled = FALSE;
#if 0
    IOLog("AJZaurusUSB::dataWriteComplete\n");
#endif
    if (!me->fReady)
		{
		IOLog("AJZaurusUSB::dataWriteComplete - not ready\n");
        return;
		}
    poolIndx = (UInt32)param;
    
    if (rc == kIOReturnSuccess)						// If operation returned ok
        {
#if 0
        IOLog("AJZaurusUSB::dataWriteComplete - pool index=%lu\n", poolIndx);
#endif
        IOSimpleLockLock(me->fLock);
        --(me->fDataCount);
        dataCount = me->fDataCount;
        stalled = me->fOutputStalled;
        if(stalled)
            IOLog("AJZaurusUSB::dataWriteComplete - was stalled\n");
        me->fOutputStalled = false; // no longer...
		me->fPipeOutBuff[poolIndx].m = NULL;    // free up buffer
        IOSimpleLockUnlock(me->fLock);
        } 
    else 
        {
        IOLog("AJZaurusUSB::dataWriteComplete - IO err %d\n", rc);
        // FIXME: what do we do with the buffer?
        if (rc != kIOReturnAborted)
            {
			
            rc = me->fOutPipe->ClearPipeStall(true);
            if (rc != kIOReturnSuccess)
                {
                IOLog("AJZaurusUSB::dataWriteComplete - clear pipe stall failed (trying to continue): %d\n", rc);
                }
            }
        }
    if (stalled && dataCount == 0) 
        {
        IOLog("AJZaurusUSB:dataWriteComplete - trying to revive a stalled queue\n");
        me->fTransmitQueue->service(IOBasicOutputQueue::kServiceAsync);	// revive a stalled transmit queue
        }
    
    return;
    
}/* end dataWriteComplete */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::merWriteComplete
//
//		Inputs:		obj - me
//				param - parameter block (may or may not be present depending on request) 
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		Management element request write completion routine
//
/****************************************************************************************************/

void net_lucid_cake_driver_AJZaurusUSB::merWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    IOUSBDevRequest	*MER = (IOUSBDevRequest*)param;
    UInt16		dataLen;
    
    if (MER)
        {
        if (rc == kIOReturnSuccess)
			{
#if 0
            IOLog("AJZaurusUSB::merWriteComplete (request=%d, remaining=%lu)\n", MER->bRequest, remaining);
#endif
			}
        else 
            IOLog("AJZaurusUSB::merWriteComplete (%d) - io err: %d %s\n", MER->bRequest, rc, me->stringFromReturn(rc));
        dataLen = MER->wLength;
#if 0
        IOLog("AJZaurusUSB::merWriteComplete - data length %d\n", dataLen);
#endif
        if(dataLen != 0 && (MER->pData))
            IOFree(MER->pData, dataLen);
        IOFree(MER, sizeof(IOUSBDevRequest));
        
        }
    else
        {
        if (rc == kIOReturnSuccess)
			{
#if 0
            IOLog("AJZaurusUSB::merWriteComplete (request=unknown, remaining=%lu)\n", remaining);
#endif
			}
        else 
            IOLog("AJZaurusUSB::merWriteComplete (unknown) - io err: %d %s\n", rc, me->stringFromReturn(rc));
        }	
}/* end merWriteComplete */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::statsWriteComplete
//
//		Inputs:		obj - me
//				param - parameter block 
//				rc - return code
//				remaining - what's left
//
//		Outputs:	None
//
//		Desc:		Ethernet statistics request write completion routine
//
/****************************************************************************************************/

void net_lucid_cake_driver_AJZaurusUSB::statsWriteComplete(void *obj, void *param, IOReturn rc, UInt32 remaining)
{
    net_lucid_cake_driver_AJZaurusUSB	*me = (net_lucid_cake_driver_AJZaurusUSB *)obj;
    IOUSBDevRequest	*STREQ = (IOUSBDevRequest*)param;
    UInt16		currStat;
    
    if (STREQ)
        {
        if (rc == kIOReturnSuccess && me->fReady)
            {
            IOLog("AJZaurusUSB::statsWriteComplete - request=%d remaining=%lu\n", STREQ->bRequest, remaining);
            currStat = STREQ->wValue;
            switch(currStat)
                {
					case kXMIT_OK_REQ:
                    me->fpNetStats->outputPackets = USBToHostLong(me->fStatValue);
                    break;
					case kRCV_OK_REQ:
                    me->fpNetStats->inputPackets = USBToHostLong(me->fStatValue);
                    break;
					case kXMIT_ERROR_REQ:
                    me->fpNetStats->outputErrors = USBToHostLong(me->fStatValue);
                    break;
					case kRCV_ERROR_REQ:
                    me->fpNetStats->inputErrors = USBToHostLong(me->fStatValue);
                    break;
					case kRCV_CRC_ERROR_REQ:
                    me->fpEtherStats->dot3StatsEntry.fcsErrors = USBToHostLong(me->fStatValue); 
                    break;
					case kRCV_ERROR_ALIGNMENT_REQ:
                    me->fpEtherStats->dot3StatsEntry.alignmentErrors = USBToHostLong(me->fStatValue);
                    break;
					case kXMIT_ONE_COLLISION_REQ:
                    me->fpEtherStats->dot3StatsEntry.singleCollisionFrames = USBToHostLong(me->fStatValue);
                    break;
					case kXMIT_MORE_COLLISIONS_REQ:
                    me->fpEtherStats->dot3StatsEntry.multipleCollisionFrames = USBToHostLong(me->fStatValue);
                    break;
					case kXMIT_DEFERRED_REQ:
                    me->fpEtherStats->dot3StatsEntry.deferredTransmissions = USBToHostLong(me->fStatValue);
                    break;
					case kXMIT_MAX_COLLISION_REQ:
                    me->fpNetStats->collisions = USBToHostLong(me->fStatValue);
                    break;
					case kRCV_OVERRUN_REQ:
                    me->fpEtherStats->dot3StatsEntry.frameTooLongs = USBToHostLong(me->fStatValue);
                    break;
					case kXMIT_TIMES_CARRIER_LOST_REQ:
                    me->fpEtherStats->dot3StatsEntry.carrierSenseErrors = USBToHostLong(me->fStatValue);
                    break;
					case kXMIT_LATE_COLLISIONS_REQ:
                    me->fpEtherStats->dot3StatsEntry.lateCollisions = USBToHostLong(me->fStatValue);
                    break;
					default:
                    IOLog("AJZaurusUSB::statsWriteComplete - Invalid stats code (currstat=%d): %d\n", currStat, rc);
                    break;
                }
            
            }
        else
            IOLog("AJZaurusUSB::statsWriteComplete - request %d io err:%d %s\n", STREQ->bRequest, rc, me->stringFromReturn(rc));
        IOFree(STREQ, sizeof(IOUSBDevRequest));
        }
    else
        {
        if(rc == kIOReturnSuccess)
            IOLog("AJZaurusUSB::statsWriteComplete (request unknown) - remaining=%lu\n", remaining);
        else
            IOLog("AJZaurusUSB::statsWriteComplete (request unknown) - io err: %d %s\n", rc, me->stringFromReturn(rc));
        }
    
    me->fStatValue = 0;
    me->fStatInProgress = false;
    return;
    
}/* end statsWriteComplete */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::timerFired
//
//		Inputs:		owner and sender
//
//		Outputs:	
//
//		Desc:		Static member function called when a timer event fires.
//					Forwards this call to the timeOutOccurred method
//
/****************************************************************************************************/

void net_lucid_cake_driver_AJZaurusUSB::timerFired(OSObject *owner, IOTimerEventSource *sender)
{
    
    // IOLog("com_apple_driver_dts_USBCDCEthernet::timerFired\n");
    
    if (owner)
        {
        net_lucid_cake_driver_AJZaurusUSB* target = OSDynamicCast(net_lucid_cake_driver_AJZaurusUSB, owner);
        
        if (target)
            {
            target->timeoutOccurred(sender);
            }
        }
    
}/* end timerFired */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::createNetworkInterface
//
//		Inputs:		
//
//		Outputs:	return Code - true (created and initialilzed ok), false (it failed)
//
//		Desc:		Creates and initializes the network interface
//
/****************************************************************************************************/

bool net_lucid_cake_driver_AJZaurusUSB::createNetworkInterface()
{
    
    IOLog("AJZaurusUSB::createNetworkInterface\n");
    
    // Allocate memory for buffers etc
    
    fTransmitQueue = (IOBasicOutputQueue*)getOutputQueue();
    if (!fTransmitQueue) 
        {
        IOLog("AJZaurusUSB::createNetworkInterface - Output queue initialization failed\n");
        return false;
        }
    fTransmitQueue->retain();
    
    // Get a reference to the IOWorkLoop in our superclass
    
    fWorkLoop = getWorkLoop();
    
    // Allocate Timer event source
    
    fTimerSource = IOTimerEventSource::timerEventSource(this, timerFired);
    if (fTimerSource == NULL)
        {
        IOLog("AJZaurusUSB::createNetworkInterface - Allocate Timer event source failed\n");
        return false;
        }
    
    if (fWorkLoop->addEventSource(fTimerSource) != kIOReturnSuccess)
        {
        IOLog("AJZaurusUSB::start - Add Timer event source failed\n");        
        return false;
        }
    
    // Attach an IOEthernetInterface client
    
    IOLog("AJZaurusUSB::createNetworkInterface - attaching and registering interface\n");
    
    if (!attachInterface((IONetworkInterface **)&fNetworkInterface, true))
        {	
			IOLog("AJZaurusUSB::createNetworkInterface - attachInterface failed\n");      
			return false;
        }
    
    // Ready to service interface requests
    
    //fNetworkInterface->registerService();
    // AJ: this is already achieved by attachInterface
    // with a second parameter of true.
    
    IOLog("AJZaurusUSB::createNetworkInterface - Exiting, successful\n");
    
    return true;
    
}/* end createNetworkInterface */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::createOutputQueue
//
//		Inputs:		
//
//		Outputs:	Return code - the output queue
//
//		Desc:		Creates the output queue
//
/****************************************************************************************************/

IOOutputQueue* net_lucid_cake_driver_AJZaurusUSB::createOutputQueue()
{
    IOLog("AJZaurusUSB::createOutputQueue\n");
    return IOBasicOutputQueue::withTarget(this, TRANSMIT_QUEUE_SIZE);
}/* end createOutputQueue */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::createMediumTables
//
//		Inputs:		
//
//		Outputs:	Return code - true (tables created), false (not created)
//
//		Desc:		Creates the medium tables
//
/****************************************************************************************************/

bool net_lucid_cake_driver_AJZaurusUSB::createMediumTables()
{
    IONetworkMedium	*medium;
    UInt64		maxSpeed;
    UInt32		i;
    IOLog("AJZaurusUSB::createMediumTables\n");
    maxSpeed = 100;
    fMediumDict = OSDictionary::withCapacity(sizeof(mediumTable) / sizeof(mediumTable[0]));
    if (fMediumDict == 0)
        {
        IOLog("AJZaurusUSB::createMediumTables - create dict. failed\n" );
        return false;
        }
    for (i = 0; i < sizeof(mediumTable) / sizeof(mediumTable[0]); i++ )
        {
        medium = IONetworkMedium::medium(mediumTable[i].type, mediumTable[i].speed);
        if (medium)
            {
			if(medium->getSpeed() <= maxSpeed)
				IONetworkMedium::addMedium(fMediumDict, medium);
            medium->release();
            }
        }
    if (publishMediumDictionary(fMediumDict) != true)
        {
        IOLog("AJZaurusUSB::createMediumTables - publish dict. failed\n" );
        return false;
        }
    medium = IONetworkMedium::getMediumWithType(fMediumDict, kIOMediumEthernetAuto);
    setCurrentMedium(medium);
    return true;
    
}/* end createMediumTables */

/* EOF */