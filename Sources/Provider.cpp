/*
 File:		Provider.cpp
 
 This module provides a packet oriented communication channel over USB
 
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
 
 This sample requires Mac OS X 10.4 and later. If built on a version prior to 
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

extern "C"
{
#include "CRC.h"
}

#include "Driver.h"

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::metaClass
//
//		Inputs:		class, superclass
//
//		Outputs:	-
//
//		Desc:		This macro defines the constructor and destructor, implements the OSMetaClass
//					allocation member function (alloc) for the class, and supplies the metaclass
//					information for the runtime typing system.
//
/****************************************************************************************************/

OSDefineMetaClassAndStructors(net_lucid_cake_driver_AJZaurusUSB, IOEthernetController);

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::configureDevice
//
//		Inputs:		numConfigs - number of configurations present
//
//		Outputs:	return Code - true (device configured), false (device not configured)
//
//		Desc:		Finds the configurations and then the appropriate interfaces etc.
//
/****************************************************************************************************/

bool net_lucid_cake_driver_AJZaurusUSB::configureDevice(UInt8 numConfigs)
{
    IOUSBFindInterfaceRequest		req;			// device request
    const IOUSBInterfaceDescriptor	*altInterfaceDesc;
    IOReturn				ior = kIOReturnSuccess;
    UInt16					numends = 0;
    UInt16					alt;
	const IOUSBConfigurationDescriptor	*cd = NULL;		// configuration descriptor
//	IOUSBInterfaceDescriptor 		*intf = NULL;		// interface descriptor
	UInt8				cval;
	UInt8				config = 0;
	UInt8				idx;
	
	IOLog("AJZaurusUSB::configureDevice %d configs\n", numConfigs);
	IOSleep(20);
#if 1
	dumpDevice(numConfigs);
	IOSleep(20);
#endif
	// Make sure we have a CDC interface to play with
	
	/*
	 we must go through all configurations
	 
	 IF Class	IF Subclass
	 2			6				found - it is the Interrupt interface of Familiar (ECM)
	 2			10				found - it is the Interrupt interface of Zaurus (MDLM)
	 10			0				data interface of Familiar - but that might be found in the RNDIS configuration as well - skip
	 2			2				Interrupt interface of RNDIS for Familiar - skip
	 255		0				found - has only Data pipe (CDC Ethernet Subclass)
	 */
	 
	for(cval=0; cval<numConfigs; cval++)
		{
		IOUSBInterface	*interface;
#if 1
		IOLog("AJZaurusUSB::configureDevice - Checking Configuration %u\n", cval);
#endif
		cd = fpDevice->GetFullConfigurationDescriptor(cval);
		if(!cd)
			{
			IOLog("AJZaurusUSB::configureDevice -   Error getting the full configuration descriptor\n");
			continue;	// try next one
			}
		config = cd->bConfigurationValue;
#if 1
		IOLog("AJZaurusUSB::configureDevice -   matching Interface descriptor found: %u\n", config);
		IOSleep(20);
#endif
		ior = fpDevice->SetConfiguration(this, config); // set this configuration as the current configuration
		if(ior != kIOReturnSuccess)
			{
			IOLog("AJZaurusUSB::configureDevice - SetConfiguration error: %d %s\n", ior, this->stringFromReturn(ior));
			return false;			
			}
		fbmAttributes = cd->bmAttributes;
#if 1
		IOLog("AJZaurusUSB::configureDevice -    bmAttributes=%08x\n", fbmAttributes);
#endif
		interface=NULL;
		req.bInterfaceClass	= kIOUSBFindInterfaceDontCare;
		req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
		req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
		req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
		while(interface=fpDevice->FindNextInterface(interface, &req))
			{ // got next interface
#if 1
			IOLog("AJZaurusUSB::configureDevice -   InterfaceClass=%d\n", interface->GetInterfaceClass());
			IOLog("AJZaurusUSB::configureDevice -   InterfaceSubClass=%d\n", interface->GetInterfaceSubClass());
			IOLog("AJZaurusUSB::configureDevice -   InterfaceProtocol=%d\n", interface->GetInterfaceProtocol());
			IOSleep(20);
#endif
			if(!interface)
				{
				IOLog("AJZaurusUSB::configureDevice -   no interface for interface descriptor\n");
				continue;
				}
			if(interface->GetInterfaceClass() == 2 && interface->GetInterfaceSubClass() == kMDLM)
				{ // found a Zaurus compatible configuration
#if 1
				IOLog("AJZaurusUSB::configureDevice -   MDLM interface found\n");
#endif
				fPadded = true;
				fChecksum = true;
				break;
				}
			if(interface->GetInterfaceClass() == 2 && interface->GetInterfaceSubClass() == kEthernetControlModel)
				{ // found a Familiar Handheld Linux compatible configuration
#if 1
				IOLog("AJZaurusUSB::configureDevice -   ECM interface found\n");
				IOSleep(20);
#endif
				fPadded = false;
				fChecksum = false;
				break;
				}
			if(interface->GetInterfaceClass() == 255 && interface->GetInterfaceSubClass() == 0)
				{ // found a special configuration
#if 1
				IOLog("AJZaurusUSB::configureDevice -   CDC Ethernet Subset found\n");
				IOSleep(20);
#endif
				fPadded = false;
				fChecksum = false;
				break;
				}
			}
		if(!interface)
			{ // we have checked all interfaces - try next configuration
			IOLog("AJZaurusUSB::configureDevice -   no matching interface for configuration %d\n", cval);
			IOSleep(20);
			continue;
			}
		fCommInterface=interface;	// we have found the Comm interface
		fCommInterfaceNumber = interface->GetInterfaceNumber();
		fInterfaceClass = interface->GetInterfaceClass();
		fInterfaceSubClass = interface->GetInterfaceSubClass();
		break;
		}

	if(cval == numConfigs)
		{
		IOLog("AJZaurusUSB::configureDevice -  no matching Interface descriptor found\n");
		return false;
		}
	
	// Save the ID's
	
	fVendorID = fpDevice->GetVendorID();
	fProductID = fpDevice->GetProductID();

#if 1
	IOLog("AJZaurusUSB::configureDevice - comm Interface=%p\n", fCommInterface);
	IOLog("AJZaurusUSB::configureDevice - vendor  id=%d (0x%04x)\n", fVendorID, fVendorID);
	IOLog("AJZaurusUSB::configureDevice - product id=%d (0x%04x)\n", fProductID, fProductID);	
	IOLog("AJZaurusUSB::configureDevice - power id=%lumA\n", 2*fpDevice->GetBusPowerAvailable());
	IOLog("AJZaurusUSB::configureDevice - max packet size for Endpoint 0=%u\n", fpDevice->GetMaxPacketSize());
	IOLog("AJZaurusUSB::configureDevice - speed=%u\n", fpDevice->GetSpeed());
	IOSleep(20);
#endif
	idx=fpDevice->GetManufacturerStringIndex();
	if(idx)
		fpDevice->GetStringDescriptor(idx, vendorString, sizeof(vendorString)-2);
	idx=fpDevice->GetProductStringIndex();
	if(idx)
		fpDevice->GetStringDescriptor(idx, modelString, sizeof(modelString)-2);
	idx=fpDevice->GetSerialNumberStringIndex();
	if(idx)
		fpDevice->GetStringDescriptor(idx, serialString, sizeof(serialString)-2);
#if 1
	IOLog("AJZaurusUSB::configureDevice - manufacturer=%s\n", vendorString);
	IOLog("AJZaurusUSB::configureDevice - product=%s\n", modelString);
	IOLog("AJZaurusUSB::configureDevice - serial number=%s idx=%u\n", serialString, idx);
	IOSleep(20);
#endif

    if(!getFunctionalDescriptors())
        {
        IOLog("AJZaurusUSB::configureDevice - getFunctionalDescriptors failed\n");
        return false;
        }
    
    if(fInterfaceSubClass != kEthernetControlModel)
        { // Zaurus (MDLM) uses a single interface for comm&data but separates endpoints
        fDataInterface = fCommInterface;	// use the same
		}
    else
        { // open the separate comm interface here if we have a ECM device
#if 1
		IOLog("AJZaurusUSB::configureDevice - comm interface %p\n", fCommInterface);
		IOLog("AJZaurusUSB::configureDevice -   ConfigValue %d\n", fCommInterface->GetConfigValue());
		IOLog("AJZaurusUSB::configureDevice -   AlternateSetting %d\n", fCommInterface->GetAlternateSetting());
		IOLog("AJZaurusUSB::configureDevice -   InterfaceClass %d\n", fCommInterface->GetInterfaceClass());
		IOLog("AJZaurusUSB::configureDevice -   InterfaceSubClass %d\n", fCommInterface->GetInterfaceSubClass());
		IOLog("AJZaurusUSB::configureDevice -   InterfaceProtocol %d\n", fCommInterface->GetInterfaceProtocol());
		IOSleep(20);
		IOLog("AJZaurusUSB::configureDevice -   InterfaceNumber %d\n", fCommInterface->GetInterfaceNumber());
		IOLog("AJZaurusUSB::configureDevice -   NumEndpoints %d\n", fCommInterface->GetNumEndpoints());
		IOLog("AJZaurusUSB::configureDevice -   BusyState %lu\n", fCommInterface->getBusyState());
		IOLog("AJZaurusUSB::configureDevice -   Inactive %d\n", fCommInterface->isInactive());
		IOLog("AJZaurusUSB::configureDevice -   Open(any) %d\n", fCommInterface->isOpen());
		IOLog("AJZaurusUSB::configureDevice -   Open(this) %d\n", fCommInterface->isOpen(this));
		IOSleep(20);
#endif
        if(!fCommInterface->open(this, kIOServiceSeize))
            {
            IOLog("AJZaurusUSB::configureDevice - open comm interface failed %p\n", fCommInterface);
			//
			// This is a Horrible Hack if some other driver has already opened our channel!!!
			//
			while(fCommInterface->isOpen())
				{
				IOLog("AJZaurusUSB::configureDevice - forced close of interfering comm interface for client %p\n", fCommInterface->getClient());
				fCommInterface->close(fCommInterface->getClient());
				}
			if(!fCommInterface->open(this, kIOServiceSeize))
				{
				IOLog("AJZaurusUSB::configureDevice - open comm interface failed again %p\n", fCommInterface);
				fCommInterface = NULL;
				return false;
				}
            }
 		req.bInterfaceClass = 10;
        req.bInterfaceSubClass = 0;
        req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
        req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
        fDataInterface = fpDevice->FindNextInterface(NULL, &req);		// find matching data interface with 2 endpoints
		if(!fDataInterface)
			{
			IOLog("AJZaurusUSB::configureDevice - Find(Next)Interface for Communications-Data failed\n");
            fCommInterface->close(this);	// close
			fCommInterface = NULL;
			return false;
			}
		}
	
	numends = fDataInterface->GetNumEndpoints();
	IOLog("AJZaurusUSB::configureDevice - %d Data Class interfaces\n", numends);
	if(numends > 1)					// There must be (at least) two bulk endpoints
		{
		//IOLog("AJZaurusUSB::configureDevice - Standard interface works\n");
		IOLog("AJZaurusUSB::configureDevice - Data Class interface found: %p\n", fDataInterface);
		if (!fDataInterface->open(this))
			{
			IOLog("AJZaurusUSB::configureDevice - open data interface failed %p\n", fDataInterface);
			numends = 0;
			}
		}
	else
		{
		IOLog("AJZaurusUSB::configureDevice - Standard interface 0 DOESN'T work\n");
		altInterfaceDesc = fDataInterface->FindNextAltInterface(NULL, &req);
		if (!altInterfaceDesc)
			IOLog("AJZaurusUSB:::configureDevice - FindNextAltInterface failed\n");
		while (altInterfaceDesc)
			{
			numends = altInterfaceDesc->bNumEndpoints;
			if (numends > 1)
				{
				if (fDataInterface->open(this))
					{
					alt = altInterfaceDesc->bAlternateSetting;
					IOLog("AJZaurusUSB::configureDevice - Data Class interface (alternate) found: %u\n", alt);
					ior = fDataInterface->SetAlternateInterface(this, alt);
					if (ior == kIOReturnSuccess)
						{
						IOLog("AJZaurusUSB::configureDevice - Alternate set\n");
						break;
						} 
					else
						{
						IOLog("AJZaurusUSB::configureDevice - SetAlternateInterface failed %d %s\n", ior, this->stringFromReturn(ior));
						numends = 0;
						}
					}
				else
					{
					IOLog("AJZaurusUSB::configureDevice - open data interface failed\n");
					numends = 0;
					}
				} 
			else
				{
				IOLog("AJZaurusUSB::configureDevice - No endpoints this alternate %p\n", altInterfaceDesc);
				}
			altInterfaceDesc = fDataInterface->FindNextAltInterface(altInterfaceDesc, &req);
			}
		}
    
    if (numends < 2)
        {
        IOLog("AJZaurusUSB::configureDevice - Finding a Data Class interface failed\n");
		if (fCommInterface != fDataInterface)
            fCommInterface->close(this);	// close only once if shared
        fCommInterface = NULL;
		fDataInterface->close(this);
		fDataInterface = NULL;
        return false;
        } 
        
	fCommInterface->retain();
	fDataInterface->retain();
	
	// Found both so now let's publish the interface
	
	if (!createNetworkInterface())
		{
		IOLog("AJZaurusUSB::configureDevice - createNetworkInterface failed\n");
		if (fCommInterface != fDataInterface)
			fCommInterface->close(this);
		fCommInterface->release();
		fCommInterface = NULL;
		fDataInterface->close(this);
		fDataInterface->release();
		fDataInterface = NULL;
		return false;
		}
        
    return true;
    
}/* end configureDevice */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::configureInterface
//
//		Inputs:		netif - the interface being configured
//
//		Outputs:	Return code - true (configured ok), false (not)
//
//		Desc:		Finish the network interface configuration
//
/****************************************************************************************************/

bool net_lucid_cake_driver_AJZaurusUSB::configureInterface(IONetworkInterface *netif)
{
    IONetworkData	*nd;
    
    IOLog("AJZaurusUSB::configureInterface threadself=%p netif=%p\n", IOThreadSelf(), netif);
	IOSleep(20);
    
    if(super::configureInterface(netif) == false)
        {
        IOLog("AJZaurusUSB::configureInterface - super failed\n");
        return false;
        }
    
    // Get a pointer to the statistics structure in the interface
    
    nd = netif->getNetworkData(kIONetworkStatsKey);
    if (!nd || !(fpNetStats = (IONetworkStats *)nd->getBuffer()))
        {
        IOLog("AJZaurusUSB::configureInterface - Invalid network statistics\n");
        return false;
        }
    
    // Get the Ethernet statistics structure:
    
    nd = netif->getParameter(kIOEthernetStatsKey);
    if (!nd || !(fpEtherStats = (IOEthernetStats*)nd->getBuffer()))
        {
        IOLog("AJZaurusUSB::configureInterface - Invalid ethernet statistics\n");
        return false;
        }
    
    return true;
    
}/* end configureInterface */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::configureDevice
//
//		Inputs:		numConfigs - number of configurations present
//
//		Outputs:	return Code - true (CDC present), false (CDC not present)
//
//		Desc:		Determines if this is a CDC compliant device and then sets the configuration
//
/****************************************************************************************************/

void net_lucid_cake_driver_AJZaurusUSB::dumpDevice(UInt8 numConfigs)
{
    IOUSBFindInterfaceRequest		req;
    const IOUSBConfigurationDescriptor	*cd = NULL;		// configuration descriptor
    IOUSBInterfaceDescriptor 		*intf = NULL;		// interface descriptor
    IOReturn				ior = kIOReturnSuccess;
    UInt8				cval;
    UInt8				intfCount = 0;
    
    IOLog("AJZaurusUSB::dumpDevice - Enumerating %d configurations ...\n", numConfigs);
	IOSleep(20);
    
    for (cval=0; cval<numConfigs; cval++)
        {
        IOLog("AJZaurusUSB::dumpDevice - Enumerate Configuration: %u\n", cval);
		IOSleep(20);
        cd = fpDevice->GetFullConfigurationDescriptor(cval);
        if(!cd)
            IOLog("AJZaurusUSB::dumpDevice -   Enumeration failure\n");
        else 
            {
            req.bInterfaceClass	= kIOUSBFindInterfaceDontCare;
            req.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
            req.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
            req.bAlternateSetting = kIOUSBFindInterfaceDontCare;
            for(intfCount=0, intf = NULL; TRUE; intfCount++)
                {
                ior = fpDevice->FindNextInterfaceDescriptor(cd, intf, &req, &intf);
                if(ior != kIOReturnSuccess)
					{
					if(ior == kIOUSBInterfaceNotFound)
						break;	// end of list
					IOLog("AJZaurusUSB::dumpDevive -   Interface %d[%u]: can't dump %d %s\n", intfCount, cval, ior, this->stringFromReturn(ior));
					IOSleep(20);
                    continue;
					}
                IOLog("AJZaurusUSB::dumpDevive -   Interface %d[%u] found\n", intfCount, cval);
                IOLog("AJZaurusUSB::dumpDevice -     bLength=%d\n", intf->bLength);
                IOLog("AJZaurusUSB::dumpDevice -     bDescriptorType=%d\n", intf->bDescriptorType);
                IOLog("AJZaurusUSB::dumpDevice -     bInterfaceNumber=%d\n", intf->bInterfaceNumber);
                IOLog("AJZaurusUSB::dumpDevice -     bInterfaceClass=%d\n", intf->bInterfaceClass);
                IOLog("AJZaurusUSB::dumpDevice -     bInterfaceSubClass=%d\n", intf->bInterfaceSubClass);
                IOLog("AJZaurusUSB::dumpDevice -     bInterfaceProtocol=%d\n", intf->bInterfaceProtocol);
                IOLog("AJZaurusUSB::dumpDevice -     bAlternateSetting=%d\n", intf->bAlternateSetting);
                IOLog("AJZaurusUSB::dumpDevice -     bNumEndpoints=%d\n", intf->bNumEndpoints);
                IOLog("AJZaurusUSB::dumpDevice -     iInterface=%d\n", intf->iInterface);
				IOSleep(20);
               }
        }
    }
}


/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::getFunctionalDescriptors
//
//		Inputs:		
//
//		Outputs:	return - true (descriptors ok), false (somethings not right or not supported)	
//
//		Desc:		Finds all the functional descriptors for the specific interface
//
/****************************************************************************************************/

bool net_lucid_cake_driver_AJZaurusUSB::getFunctionalDescriptors()
{
    IOReturn ior;
    HeaderFunctionalDescriptor *funcDesc = NULL;
    EnetFunctionalDescriptor *ENETFDesc = NULL;
    
    IOLog("AJZaurusUSB::getFunctionalDescriptors\n");
	IOSleep(20);
    
    while((funcDesc = (HeaderFunctionalDescriptor*) fCommInterface->FindNextAssociatedDescriptor((void *)funcDesc, CS_INTERFACE)))
        { // loop through all functional descriptors
        switch (funcDesc->bDescriptorSubtype)
            {
            case Header_FunctionalDescriptor:
                IOLog("AJZaurusUSB::getFunctionalDescriptors - Header Functional Descriptor - type=%d subtype=%d\n", funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype);
                break;
            case Enet_Functional_Descriptor:
                ENETFDesc = (EnetFunctionalDescriptor *) funcDesc;
                IOLog("AJZaurusUSB::getFunctionalDescriptors - Ethernet Functional Descriptor - type=%d subtype=%d\n", funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype);
                break;
            case Union_FunctionalDescriptor:
                IOLog("AJZaurusUSB::getFunctionalDescriptors - Union Functional Descriptor - type=%d subtype=%d\n", funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype);
                break;
            default:
                IOLog("AJZaurusUSB::getFunctionalDescriptors - unknown Functional Descriptor - type=%d subtype=%d\n", funcDesc->bDescriptorType, funcDesc->bDescriptorSubtype);
                break;
            }
		IOSleep(20);
        }
    
    if(ENETFDesc)
        { // The Enet Func. Desc.  must be present
        
        // Determine who is collecting the input/output network stats.
        
        fOutputPktsOK = !(ENETFDesc->bmEthernetStatistics[0] & kXMIT_OK);
        fInputPktsOK = !(ENETFDesc->bmEthernetStatistics[0] & kRCV_OK);
        fOutputErrsOK = !(ENETFDesc->bmEthernetStatistics[0] & kXMIT_ERROR);
        fInputErrsOK = !(ENETFDesc->bmEthernetStatistics[0] & kRCV_ERROR);
        
        // Save the stats (it's bit mapped)
        
        fEthernetStatistics[0] = ENETFDesc->bmEthernetStatistics[0];
        fEthernetStatistics[1] = ENETFDesc->bmEthernetStatistics[1];
        fEthernetStatistics[2] = ENETFDesc->bmEthernetStatistics[2];
        fEthernetStatistics[3] = ENETFDesc->bmEthernetStatistics[3];
        
        // Save the multicast filters (remember it's intel format)
        
        fMcFilters = USBToHostWord(*(UInt16 *)ENETFDesc->wNumberMCFilters);
        
        // Get the Ethernet address
        
        if (ENETFDesc->iMACAddress != 0)
            {	
			// check Info.plist if we should override			
			char etherbuf[6*3];	// ASCII string for Ethernet Address
			ior = fpDevice->GetStringDescriptor(ENETFDesc->iMACAddress, etherbuf, sizeof(etherbuf));
            if (ior == kIOReturnSuccess)
                { // convert string
				int i;
				IOLog("AJZaurusUSB::getFunctionalDescriptors - Ethernet string=%s\n", etherbuf);
				IOSleep(20);
				for(i=0; i<6; i++)
					{ // convert ASCII to bitmask
					int c=etherbuf[2*i];
					int byte;
					if(c >= '9') c-=('A'-10);
					byte=(c&0xf)<<4;
					c=etherbuf[2*i+1];
					if(c >= '9') c-=('A'-10);
					byte+=(c&0xf);
					fEaddr[i]=byte;
					}
#if 1	// OVERRIDE
				{ // compute a MAC address that distinguishes different devices as good as possible but is constant for each one
					UInt32 fcs=CRC32_INITFCS;
					fcs=fcs_compute32((unsigned char *)vendorString, strlen(vendorString), fcs);
					fcs=fcs_compute32((unsigned char *)modelString, strlen(modelString), fcs);
					fcs=fcs_compute32((unsigned char *)serialString, strlen(serialString), fcs);
					fEaddr[0]=0x40;
					fEaddr[1]=0x00;
					fEaddr[2]=fcs>>24;
					fEaddr[3]=fcs>>16;
					fEaddr[4]=fcs>>8;
					fEaddr[5]=fcs>>0;
				}
#endif
				IOLog("AJZaurusUSB::getFunctionalDescriptors - Ethernet address (string %d): %02x.%02x.%02x.%02x.%02x.%02x\n",
                      ENETFDesc->iMACAddress,
                      (unsigned) fEaddr[0],
                      (unsigned) fEaddr[1],
                      (unsigned) fEaddr[2],
                      (unsigned) fEaddr[3],
                      (unsigned) fEaddr[4],
                      (unsigned) fEaddr[5]                      
                      );
                //                LogData(kUSBAnyDirn, 6, fEaddr);
                
                fMax_Block_Size = USBToHostWord(*(UInt16 *)ENETFDesc->wMaxSegmentSize);
                IOLog("AJZaurusUSB::getFunctionalDescriptors - Maximum segment size %d\n", fMax_Block_Size);
				IOSleep(20);
				return true;	// all ok!
                }
			IOLog("AJZaurusUSB::getFunctionalDescriptors - Error retrieving Ethernet address\n");
            } 
        }
    
	IOLog("AJZaurusUSB::getFunctionalDescriptors -  Using defaults 'cause all else failed\n");

	fOutputPktsOK = false;
	fInputPktsOK = false;
	fOutputErrsOK = false;
	fInputErrsOK = false;
	fEthernetStatistics[0] = 0;
	fEthernetStatistics[1] = 0;
	fEthernetStatistics[2] = 0;
	fEthernetStatistics[3] = 0;
	fMcFilters = 0;				// no filters
	fMax_Block_Size = 0xea00;	// default MAX_BLOCK_SIZE;
	fEaddr[0] = 0x34;			// default Ethernet address
	fEaddr[1] = 0x30;
	fEaddr[2] = 0x30;
	fEaddr[3] = 0x30;
	fEaddr[4] = 0x30;
	fEaddr[5] = 0x00;    
    return true;
    
}/* end getFunctionalDescriptors */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::setWakeOnMagicPacket
//
//		Inputs:		active - true(wake), false(don't)
//
//		Outputs:	Return code - kIOReturnSuccess
//
//		Desc:		Set for wake on magic packet
//
/****************************************************************************************************/

IOReturn net_lucid_cake_driver_AJZaurusUSB::setWakeOnMagicPacket(bool active)
{
    IOLog("AJZaurusUSB::setWakeOnMagicPacket active:%d\n", active);
    fWOL = active;
    if (fbmAttributes & kUSBAtrRemoteWakeup)
        {
        
        // Clear the feature if wake-on-lan is not set (SetConfiguration sets the feature 
        // automatically if the device supports remote wake up)
        
        if (!active)				
            {
            IOUSBDevRequest	devreq;
            IOReturn		ior;    
            devreq.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBStandard, kUSBDevice);
            devreq.bRequest = kUSBRqClearFeature;
            devreq.wValue = kUSBFeatureDeviceRemoteWakeup;
            devreq.wIndex = 0;
            devreq.wLength = 0;
            devreq.pData = 0;
            
            ior = fpDevice->DeviceRequest(&devreq);
            if (ior == kIOReturnSuccess)
                IOLog("AJZaurusUSB::configureDevice - Clearing remote wake up feature successful: %d\n", ior);
            else
                IOLog("AJZaurusUSB::configureDevice - Clearing remote wake up feature failed: %d %s\n", ior, this->stringFromReturn(ior));
            }
        }
    else 
        IOLog("AJZaurusUSB::configureDevice - Remote wake up not supported\n");
    return kIOReturnSuccess;
    
}/* end setWakeOnMagicPacket */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::getPacketFilters
//
//		Inputs:		group - the filter group
//
//		Outputs:	Return code - kIOReturnSuccess and others
//				filters - the capability
//
//		Desc:		Set the filter capability for the driver
//
/****************************************************************************************************/

IOReturn net_lucid_cake_driver_AJZaurusUSB::getPacketFilters(const OSSymbol *group, UInt32 *filters) const
{
    IOReturn	rtn = kIOReturnSuccess;    
    IOLog("AJZaurusUSB::getPacketFilters - group=%p filters=%p\n", group, filters);
    if (group == gIOEthernetWakeOnLANFilterGroup)
        {
        if (fbmAttributes & kUSBAtrRemoteWakeup)
            *filters = kIOEthernetWakeOnMagicPacket;
        else
            *filters = 0;
        } 
    else if (group == gIONetworkFilterGroup)
        *filters = kIOPacketFilterUnicast | kIOPacketFilterBroadcast | kIOPacketFilterMulticast | kIOPacketFilterMulticastAll | kIOPacketFilterPromiscuous;
    else
        rtn = super::getPacketFilters(group, filters);  // use default
    if (rtn != kIOReturnSuccess)
        IOLog("AJZaurusUSB::getPacketFilters failed: %d\n", rtn /*, stringFromReturn(rtn) */);    
    return rtn;    
}/* end getPacketFilters */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::selectMedium
//
//		Inputs:
//
//		Outputs:
//
//		Desc:		Lets us know if someone is playing with ifconfig
//
/****************************************************************************************************/

IOReturn net_lucid_cake_driver_AJZaurusUSB::selectMedium(const IONetworkMedium *medium)
{
    IOLog("AJZaurusUSB::selectMedium\n");    
    setSelectedMedium(medium);
    return kIOReturnSuccess;
}/* end selectMedium */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::getHardwareAddress
//
//		Inputs:		
//
//		Outputs:	Return code - kIOReturnSuccess or kIOReturnError
//					ea - the address
//
//		Desc:		Get the ethernet address from the hardware (actually the descriptor)
//
/****************************************************************************************************/

IOReturn net_lucid_cake_driver_AJZaurusUSB::getHardwareAddress(IOEthernetAddress *ea)
{
    UInt32 i;
    IOLog("AJZaurusUSB::getHardwareAddress\n");
    for(i=0; i<6; i++)
        ea->bytes[i] = fEaddr[i];
    return kIOReturnSuccess;
    
}/* end getHardwareAddress */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::newVendorString
//
//		Inputs:		
//
//		Outputs:	Return code - the vendor string
//
//		Desc:		Identifies the hardware vendor
//
/****************************************************************************************************/

const OSString* net_lucid_cake_driver_AJZaurusUSB::newVendorString() const
{
    IOLog("AJZaurusUSB::newVendorString %s\n", vendorString);
    return OSString::withCString(vendorString);		// Maybe we should use what we got from the descriptors
}/* end newVendorString */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::newModelString
//
//		Inputs:		
//
//		Outputs:	Return code - the model string
//
//		Desc:		Identifies the hardware model
//
/****************************************************************************************************/

const OSString* net_lucid_cake_driver_AJZaurusUSB::newModelString() const
{
    IOLog("AJZaurusUSB::newModelString %s\n", modelString);
    return OSString::withCString(modelString);		// Maybe we should use the descriptors
}/* end newModelString */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::newRevisionString
//
//		Inputs:		
//
//		Outputs:	Return code - the revision string
//
//		Desc:		Identifies the hardware revision
//
/****************************************************************************************************/

const OSString* net_lucid_cake_driver_AJZaurusUSB::newRevisionString() const
{
    IOLog("AJZaurusUSB::newRevisionString %s\n", serialString);
    return OSString::withCString(serialString);
}/* end newRevisionString */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::setMulticastMode
//
//		Inputs:		active - true (set it), false (don't)
//
//		Outputs:	Return code - kIOReturnSuccess
//
//		Desc:		Sets multicast mode
//
/****************************************************************************************************/

IOReturn net_lucid_cake_driver_AJZaurusUSB::setMulticastMode(bool active)
{
    IOLog("AJZaurusUSB::setMulticastMode active=%d\n", active);
    if (active)
        {
        fPacketFilter |= kPACKET_TYPE_ALL_MULTICAST;
        }
    else
        {
        fPacketFilter &= ~kPACKET_TYPE_ALL_MULTICAST;
        }
    USBSetPacketFilter();
    return kIOReturnSuccess;
    
}/* end setMulticastMode */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::setMulticastList
//
//		Inputs:		addrs - list of addresses
//					count - number in the list
//
//		Outputs:	Return code - kIOReturnSuccess or kIOReturnIOError
//
//		Desc:		Sets multicast list
//
/****************************************************************************************************/

IOReturn net_lucid_cake_driver_AJZaurusUSB::setMulticastList(IOEthernetAddress *addrs, UInt32 count)
{
    bool	uStat;
    IOLog("AJZaurusUSB::setMulticastList addrs=%p count=%lu\n", addrs, count);
    if (count != 0)
        {
        uStat = USBSetMulticastFilter(addrs, count);
        if (!uStat)
            {
            return kIOReturnIOError;
            }
        }
    return kIOReturnSuccess;
    
}/* end setMulticastList */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::setPromiscuousMode
//
//		Inputs:		active - true (set it), false (don't)
//
//		Outputs:	Return code - kIOReturnSuccess
//
//		Desc:		Sets promiscuous mode
//
/****************************************************************************************************/

IOReturn net_lucid_cake_driver_AJZaurusUSB::setPromiscuousMode(bool active)
{
    IOLog("AJZaurusUSB::setPromiscuousMode %d\n", active);
    if (active)
        {
        fPacketFilter |= kPACKET_TYPE_PROMISCUOUS;
        }
    else
        {
        fPacketFilter &= ~kPACKET_TYPE_PROMISCUOUS;
        }
    USBSetPacketFilter();
    return kIOReturnSuccess;
}/* end setPromiscuousMode */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::USBTransmitPacket
//
//		Inputs:		packet - the packet
//
//		Outputs:	Return code - true (transmit started), false (it didn't)
//
//		Desc:		Set up and then transmit the packet
//
/****************************************************************************************************/

bool net_lucid_cake_driver_AJZaurusUSB::USBTransmitPacket(mbuf_t packet)
{
    UInt32		numbufs = 0;		// number of mbufs for this packet
    mbuf_t		m;				// current mbuf
    UInt32		total_pkt_length = 0;
    UInt32		new_pkt_length;
    UInt32		checksum_length;
    UInt32		fcs;
    UInt32		pad;
    UInt32		rTotal = 0;
    IOReturn	ior = kIOReturnSuccess;
    UInt32		poolIndx;
    UInt16		tryCount = 0;
    // Count the number of mbufs in this packet
    
    m = packet;
    while (m)
        {
        total_pkt_length += mbuf_len(m);
        numbufs++;
        m = mbuf_next(m);
        }
    
    // AJ: Calculate size including CRC and padding
    // -->
    checksum_length = fChecksum?4:0;
    if (fPadded)
        { // we need to pad so that after appending the CRC we have a multiple of packetsize less one
        new_pkt_length = fOutPacketSize * ( ((total_pkt_length + checksum_length) / fOutPacketSize) + 1) - 1;
        }
    else
        {
        if (fChecksum)  // require a minimum of one full packet
            new_pkt_length = MAX(fOutPacketSize, total_pkt_length + checksum_length);
        else
            new_pkt_length = total_pkt_length;
        }
    // <--
#if 0
    IOLog("AJZaurusUSB::USBTransmitPacket %p padded:%d checksum:%d length:%lu mbufs:%lu\n", packet, fPadded, fChecksum, new_pkt_length, numbufs);
#endif
    if (new_pkt_length+1 > fMax_Block_Size)
        {
        IOLog("AJZaurusUSB::USBTransmitPacket - Bad packet size\n");	// Note for now and revisit later
        if (fOutputErrsOK)
            fpNetStats->outputErrors++;
        // OSIncrementAtomic(&fSyncCount);
        return false;
        }

    // Find a free ouput buffer in the pool

    IOSimpleLockLock(fLock);
    while(TRUE)
        { // try to get a buffer
        for(poolIndx=0; poolIndx<kOutBufPool; poolIndx++)
			{
            if(fPipeOutBuff[poolIndx].m == NULL)
                break;  // got one
			}
        if(poolIndx<kOutBufPool)
            break;  // break while loop
        tryCount++;
        if(tryCount > kOutBuffThreshold)
            { // waited too long
            IOLog("AJZaurusUSB::USBTransmitPacket - Exceeded output buffer wait threshold - output stalled\n");
            if(fOutputErrsOK)
                fpNetStats->outputErrors++;
            IOSimpleLockUnlock(fLock);
            fOutputStalled = true;
            return false;
            }
        IOLog("AJZaurusUSB::USBTransmitPacket - Waiting %d-th time for output buffer\n", tryCount);
        IOSleep(1);	// sleep 1 second
        }
    fPipeOutBuff[poolIndx].m = packet;
    ++fDataCount;
    if(fDataCount > kOutBufPool-10)
        IOLog("AJZaurusUSB::USBTransmitPacket - Warning %ld of %d output buffers in use!\n", fDataCount, kOutBufPool);
    if(fOutputStalled)
        IOLog("AJZaurusUSB::USBTransmitPacket - Output no longer stalled\n");
    fOutputStalled = false;
    IOSimpleLockUnlock(fLock);
    
    // Start filling in the send buffer
    // AJ: modified to do CRC calculation and padding
    // -->
    fcs = CRC32_INITFCS;
    m = packet;							// start with the first mbuf of the packet
    rTotal = 0;							// running total				
    do
        {  
            if (mbuf_len(m) == 0)					// Ignore zero length mbufs
                continue;
            fcs = fcs_memcpy32(&(fPipeOutBuff[poolIndx].pipeOutBuffer[rTotal]), (unsigned char*) mbuf_data(m), mbuf_len(m), fcs);
            //bcopy(mtod(m, unsigned char *), &fPipeOutBuff[poolIndx].pipeOutBuffer[rTotal], mbuf_len(m));
            rTotal += mbuf_len(m);
            
        } while ((m = mbuf_next(m)) != 0);
    if ((pad = new_pkt_length - rTotal - checksum_length) > 0)
        {
        // pad to required length less four (CRC), copy fcs and append pad byte if required
        fcs = fcs_pad32(&(fPipeOutBuff[poolIndx].pipeOutBuffer[rTotal]), pad, fcs);
        rTotal += pad;
        }
    fcs = ~fcs;
    if (fChecksum)
        {
        fPipeOutBuff[poolIndx].pipeOutBuffer[rTotal++] = fcs&0xff;
        fPipeOutBuff[poolIndx].pipeOutBuffer[rTotal++] = (fcs>>8)&0xff;
        fPipeOutBuff[poolIndx].pipeOutBuffer[rTotal++] = (fcs>>16)&0xff;
        fPipeOutBuff[poolIndx].pipeOutBuffer[rTotal++] = (fcs>>24)&0xff;
        }
    
    // append a byte if required, we over-allocated by one to allow for this
    if (fPadded)
        {
        if (!(rTotal % fOutPacketSize))
            {
            fPipeOutBuff[poolIndx].pipeOutBuffer[rTotal++] = 0;
            }
        }
#if 0
    IOLog("AJZaurusUSB::USBTransmitPacket - length=%lu checksum=%08lx\n", rTotal, fcs);
#endif
    // <--
    
//    LogData(kUSBOut, rTotal, fPipeOutBuff[poolIndx].pipeOutBuffer);
    
    //fPipeOutBuff[poolIndx].m = packet;
    fPipeOutBuff[poolIndx].writeCompletionInfo.parameter = (void *)poolIndx;
    fPipeOutBuff[poolIndx].pipeOutMDP->setLength(rTotal);
    ior = fOutPipe->Write(fPipeOutBuff[poolIndx].pipeOutMDP, 
						  5000,
						  5000,
						  fPipeOutBuff[poolIndx].pipeOutMDP->getLength(),
						  &(fPipeOutBuff[poolIndx].writeCompletionInfo));
    if (ior != kIOReturnSuccess)
        {
        //OSIncrementAtomic(&fSyncCount);
        IOLog("AJZaurusUSB::USBTransmitPacket - Write failed: ior=%d %s\n", ior, this->stringFromReturn(ior));
        if (ior == kIOUSBPipeStalled)
            {
            IOLog("AJZaurusUSB::USBTransmitPacket - Pipe stalled\n");
            //OSDecrementAtomic(&fSyncCount);
            fOutPipe->ClearPipeStall(true);  // reset and try again
            ior = fOutPipe->Write(fPipeOutBuff[poolIndx].pipeOutMDP, 
								  5000,
								  5000,
								  fPipeOutBuff[poolIndx].pipeOutMDP->getLength(),
								  &(fPipeOutBuff[poolIndx].writeCompletionInfo));
            if (ior != kIOReturnSuccess)
                {
                //OSIncrementAtomic(&fSyncCount);
                IOLog("AJZaurusUSB::USBTransmitPacket - Write really failed: %d %s\n", ior, this->stringFromReturn(ior));
                if(fOutputErrsOK)
                    fpNetStats->outputErrors++;
                IOSimpleLockLock(fLock);
                --fDataCount;
                fPipeOutBuff[poolIndx].m = NULL;
                IOSimpleLockUnlock(fLock);
                return false;
                }
            }
        else
            { // other error
            IOSimpleLockLock(fLock);
            --fDataCount;
            fPipeOutBuff[poolIndx].m = NULL;
            IOSimpleLockUnlock(fLock);
            return false;
            }
        }
    
    freePacket(packet);
    if (fOutputPktsOK)
        fpNetStats->outputPackets++;
    
    return true;
    
}/* end USBTransmitPacket */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::USBSetMulticastFilter
//
//		Inputs:		addrs - the list of addresses
//				count - How many
//
//		Outputs:	
//
//		Desc:		Set up and send SetMulticastFilter Management Element Request(MER).
//
/****************************************************************************************************/

bool net_lucid_cake_driver_AJZaurusUSB::USBSetMulticastFilter(IOEthernetAddress *addrs, UInt32 count)
{
    IOReturn		rc;
    IOUSBDevRequest	*MER;
    UInt8		*eaddrs;
    UInt32		eaddLen;
    UInt32		i,j,rnum;
    
    IOLog("AJZaurusUSB::USBSetMulticastFilter - filters=%d count=%lu\n", fMcFilters, count);
    
    if (count > (UInt32)(fMcFilters & kFiltersSupportedMask))
        {
        IOLog("AJZaurusUSB::USBSetMulticastFilter - No multicast filters supported\n");
        return false;
        }
    
    MER = (IOUSBDevRequest*)IOMalloc(sizeof(IOUSBDevRequest));
    if (!MER)
        {
        IOLog("AJZaurusUSB::USBSetMulticastFilter - allocate MER failed\n");
        return false;
        }
    bzero(MER, sizeof(IOUSBDevRequest));
    
    eaddLen = count * kIOEthernetAddressSize;
    eaddrs = (UInt8 *)IOMalloc(eaddLen);
    if (!eaddrs)
        {
        IOLog("AJZaurusUSB::USBSetMulticastFilter - allocate address buffer failed\n");
        return false;
        }
    bzero(eaddrs, eaddLen); 
    
    // Build the filter address buffer
    
    rnum = 0;
    for (i=0; i<count; i++)
        {
        if (rnum > eaddLen)				// Just in case
            {
            break;
            }
        for (j=0; j<kIOEthernetAddressSize; j++)
            {
            eaddrs[rnum++] = addrs->bytes[j];
            }
        }
    
    // Now build the Management Element Request
    
    MER->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    MER->bRequest = kSet_Ethernet_Multicast_Filter;
    MER->wValue = count;
    MER->wIndex = fCommInterfaceNumber;
    MER->wLength = eaddLen;
    MER->pData = eaddrs;
    
    fMERCompletionInfo.parameter = MER;
    
    rc = fpDevice->DeviceRequest(MER, &fMERCompletionInfo);
    if (rc != kIOReturnSuccess)
        {
        IOLog("AJZaurusUSB::USBSetMulticastFilter - Error issueing DeviceRequest %d: %d %s\n", MER->bRequest, rc, this->stringFromReturn(rc));
        IOFree(MER->pData, eaddLen);
        IOFree(MER, sizeof(IOUSBDevRequest));
        return false;
        }
    
    return true;
    
}/* end USBSetMulticastFilter */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::USBSetPacketFilter
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Set up and send SetEthernetPackettFilters Management Element Request(MER).
//
/****************************************************************************************************/

bool net_lucid_cake_driver_AJZaurusUSB::USBSetPacketFilter()
{
    IOReturn		rc;
    IOUSBDevRequest	*MER;
    
    IOLog("AJZaurusUSB::USBSetPacketFilter %d\n", fPacketFilter);
    IOSleep(20);
	
    MER = (IOUSBDevRequest*)IOMalloc(sizeof(IOUSBDevRequest));
    if (!MER)
        {
        IOLog("AJZaurusUSB::USBSetPacketFilter - allocate MER failed\n");
        return false;
        }
    bzero(MER, sizeof(IOUSBDevRequest));
    
    // Now build the Management Element Request
    
    MER->bmRequestType = USBmakebmRequestType(kUSBOut, kUSBClass, kUSBInterface);
    MER->bRequest = kSet_Ethernet_Packet_Filter;
    MER->wValue = fPacketFilter;
    MER->wIndex = fCommInterfaceNumber;
    MER->wLength = 0;
    MER->pData = NULL;
    
    fMERCompletionInfo.parameter = MER;
    
    rc = fpDevice->DeviceRequest(MER, &fMERCompletionInfo);
    if (rc != kIOReturnSuccess)
        {
        IOLog("AJZaurusUSB::USBSetPacketFilter - DeviceRequest error for %d: %d %s\n", MER->bRequest, rc, this->stringFromReturn(rc));
        if (rc == kIOUSBPipeStalled)
            {
            
            // Clear the stall and try it once more
            
            fpDevice->GetPipeZero()->ClearPipeStall(true);
            rc = fpDevice->DeviceRequest(MER, &fMERCompletionInfo);
            if (rc != kIOReturnSuccess)
                {
                IOLog("AJZaurusUSB::USBSetPacketFilter - DeviceRequest for %d, error a second time: %d %s\n", MER->bRequest, rc, this->stringFromReturn(rc));
                IOFree(MER, sizeof(IOUSBDevRequest));
                return false;
                }
            }
        }
    
    return true;
    
}/* end USBSetPacketFilter */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::clearPipeStall
//
//		Inputs:		thePipe - the pipe
//
//		Outputs:	
//
//		Desc:		Clear a stall on the specified pipe. All outstanding I/O
//				is returned as aborted.
//
/****************************************************************************************************/

IOReturn net_lucid_cake_driver_AJZaurusUSB::clearPipeStall(IOUSBPipe *thePipe)
{
    UInt8	pipeStatus;
    IOReturn 	rtn = kIOReturnSuccess;
    
    IOLog("AJZaurusUSB::clearPipeStall %p\n", thePipe);
    
    pipeStatus = thePipe->GetPipeStatus();
    if (pipeStatus == kPipeStalled)
        {
        rtn = thePipe->ClearPipeStall(true);
        if (rtn == kIOReturnSuccess)
            IOLog("AJZaurusUSB::clearPipeStall - Successful\n");
        else 
            IOLog("AJZaurusUSB::clearPipeStall - Failed: %d %s\n", rtn, this->stringFromReturn(rtn));
        }
    else
        IOLog("AJZaurusUSB::clearPipeStall - Pipe not stalled: status=%d\n", pipeStatus);
    
    // rewritten to prevent nasty issues when doing synchronous stuff here
    //rtn = thePipe->ClearPipeStall(true);
    
    return rtn;
    
}/* end clearPipeStall */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::resetDevice
//
//		Inputs:	
//
//		Outputs:	
//
//		Desc:		Check to see if we need to reset the device on wake from sleep.
//
/****************************************************************************************************/

void net_lucid_cake_driver_AJZaurusUSB::resetDevice(void)
{
    USBStatus	status;
	
    IOLog("AJZaurusUSB::resetDevice\n");
    
	if(!fpDevice)
		return;

	if(fpDevice->GetDeviceStatus(&status) != kIOReturnSuccess)	// try to get device status
		{
        IOLog("AJZaurusUSB::resetDevice - could not get device status; needs to reset and reenumerate\n");
		fpDevice->ResetDevice();
		if(!fpDevice)
			IOLog("AJZaurusUSB::resetDevice - fpDevice became NULL by ResetDevice!\n");
		else
			fpDevice->ReEnumerateDevice(0);
		}

}/* end resetDevice */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::receivePacket
//
//		Inputs:		packet - the packet
//					size - Number of bytes in the packet
//
//		Outputs:	
//
//		Desc:		Build the mbufs and then send to the network stack.
//
/****************************************************************************************************/

void net_lucid_cake_driver_AJZaurusUSB::receivePacket(UInt8 *packet, UInt32 size)
{
    mbuf_t		m;
    UInt32		submit;
    UInt32		fcs;
#if 0
    IOLog("AJZaurusUSB::receivePacket size=%lu\n", size);
#endif
    if (size > fMax_Block_Size)
        {
        IOLog("AJZaurusUSB::receivePacket - Packet size error, packet dropped (len=%lu, expected %d)\n", size, fMax_Block_Size);
        if (fInputErrsOK)
            fpNetStats->inputErrors++;
        return;
        }
    
    // check CRC
    
    if (fChecksum)
        {
        if ((size % fOutPacketSize) == 1)
            {
            
            // check fcs across length minus one bytes
            if ((fcs = fcs_compute32(packet, size - 1, CRC32_INITFCS)) == CRC32_GOODFCS)
                {
                // success, trim extra byte and fall through
                --size;
                }
            // failed, check additional byte
            else if ((fcs = fcs_compute32(packet+size - 1, 1, fcs)) != CRC32_GOODFCS)
                {
                // failed
                IOLog("AJZaurusUSB::receivePacket - CRC failed on extra byte; packet (size=%lu) dropped: %08lx\n", size, fcs);
                if (fInputErrsOK)
                    fpNetStats->inputErrors++;
                return;
                }
            // success fall through, possibly with corrected length
            }
        // normal check across full frame
        else if ((fcs = fcs_compute32(packet, size, CRC32_INITFCS)) != CRC32_GOODFCS)
            {
            IOLog("AJZaurusUSB::receivePacket - CRC failed; packet (size=%lu) dropped: %08lx\n", size, fcs);
            if (fInputErrsOK)
                fpNetStats->inputErrors++;
            return;
            }
        
        // trim fcs
        size -= 4;
        }
    
    // push the packet up the TCP/IP stack
    m = allocatePacket(size);
    if (m)
        {
        bcopy(packet, (unsigned char*) mbuf_data(m), size);
        submit = fNetworkInterface->inputPacket(m, size);
#if 0
        IOLog("AJZaurusUSB::receivePacket - %lu Packets submitted to IP layer\n", submit);
#endif
        if (fInputPktsOK)
            fpNetStats->inputPackets++;
        } 
    else
        {
        IOLog("AJZaurusUSB::receivePacket - Buffer allocation failed, packet dropped\n");
        if (fInputErrsOK)
            fpNetStats->inputErrors++;
        }
    
}/* end receivePacket */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::createWorkLoop
//
//		Inputs:
//
//		Outputs:	return Code - true if the work loop could be created, false otherwise.
//
//		Desc:		Creates a work loop for the driver. 
//
/****************************************************************************************************/

bool net_lucid_cake_driver_AJZaurusUSB::createWorkLoop()
{
    fWorkLoop = IOWorkLoop::workLoop();
    IOLog("AJZaurusUSB::createWorkLoop - thread=%p workLoop=%p\n", fWorkLoop->getThread(), fWorkLoop);
    return (fWorkLoop != 0);
}/* end createWorkLoop */


/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::getWorkLoop
//
//		Inputs:
//
//		Outputs:	return Code - the work loop.
//
//		Desc:		Returns the driver's work loop. 
//
/****************************************************************************************************/

IOWorkLoop* net_lucid_cake_driver_AJZaurusUSB::getWorkLoop() const
{
#if 1
    IOLog("AJZaurusUSB::getWorkLoop (%p)\n", fWorkLoop);
#endif
    return fWorkLoop;
}/* end getWorkLoop */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::allocateResources
//
//		Inputs:		
//
//		Outputs:	return code - true (allocate was successful), false (it failed)
//
//		Desc:		Finishes up the rest of the configuration and gets all the endpoints open etc.
//
/****************************************************************************************************/

bool net_lucid_cake_driver_AJZaurusUSB::allocateResources()
{
    IOUSBFindEndpointRequest	epReq;		// endPoint request struct on stack
    UInt32			i;
    
#if 1
    IOLog("AJZaurusUSB::allocateResources\n");
#endif
    
    // Open all the end points
    
    epReq.type = kUSBBulk;
    epReq.direction = kUSBIn;
    epReq.maxPacketSize	= 0;
    epReq.interval = 0;
    if(!fDataInterface)
        {
        IOLog("AJZaurusUSB::allocateResources - no data interface\n");
        return false;
        }
    fInPipe = fDataInterface->FindNextPipe(0, &epReq);
    if(!fInPipe)
        {
        IOLog("AJZaurusUSB::allocateResources - no bulk input pipe\n");
        return false;
        }
#if 1
    IOLog("AJZaurusUSB::allocateResources - bulk input pipe - myPacketSize=%u interval=%u pipe=%p\n", epReq.maxPacketSize, epReq.interval, fInPipe);
#endif
    epReq.direction = kUSBOut;
    fOutPipe = fDataInterface->FindNextPipe(0, &epReq);
    if(!fOutPipe)
        {
        IOLog("AJZaurusUSB::allocateResources - no bulk output pipe\n");
        return false;
        }
    fOutPacketSize = epReq.maxPacketSize;
#if 1
    IOLog("AJZaurusUSB::allocateResources - bulk output pipe - myPacketSize=%u interval=%u pipe=%p\n", epReq.maxPacketSize, epReq.interval, fOutPipe);
#endif
    // Interrupt pipe - Comm Interface
    
    epReq.type = kUSBInterrupt;
    epReq.direction = kUSBIn;
    fCommPipe = fCommInterface->FindNextPipe(0, &epReq);
    if(!fCommPipe)
        {
        IOLog("AJZaurusUSB::allocateResources - no interrupt pipe\n");
        fCommPipeMDP = NULL;
        fCommPipeBuffer = NULL;
        fLinkStatus = 1;					// Mark it active cause we'll never get told
        }
	else
		{ // there is a comm pipe
#if 0
		IOLog("AJZaurusUSB::allocateResources - comm pipe - myPacketSize=%u interval=%u pipe=%p\n", epReq.maxPacketSize, epReq.interval, fCommPipe);
#endif
		// Allocate Memory Descriptor Pointer with memory for the Comm pipe:
        
		fCommPipeMDP = IOBufferMemoryDescriptor::withCapacity(COMM_BUFF_SIZE, kIODirectionIn);
		if (!fCommPipeMDP)
			return false;
        
		fCommPipeMDP->setLength(COMM_BUFF_SIZE);
		fCommPipeBuffer = (UInt8*)fCommPipeMDP->getBytesNoCopy();
#if 0
		IOLog("AJZaurusUSB::allocateResources - comm buffer %p\n", fCommPipeBuffer);
#endif
		}

    // Allocate Memory Descriptor Pointer with memory for the data-in bulk pipe:
    
	fMax_Block_Size = 64*((fMax_Block_Size+(64-1))/64);	// 64 is the Max Block Size we should read from the endpoint descriptor

    fPipeInMDP = IOBufferMemoryDescriptor::withCapacity(fMax_Block_Size, kIODirectionIn);
    if (!fPipeInMDP)
        return false;
    
    fPipeInMDP->setLength(fMax_Block_Size);
    fPipeInBuffer = (UInt8*)fPipeInMDP->getBytesNoCopy();
#if 1
    IOLog("AJZaurusUSB::allocateResources - input buffer %p[%lu]\n", fPipeInBuffer, fPipeInMDP->getLength());
#endif
    // Allocate Memory Descriptor Pointers with memory for the data-out bulk pipe pool
    
    for (i=0; i<kOutBufPool; i++)
        {
        fPipeOutBuff[i].pipeOutMDP = IOBufferMemoryDescriptor::withCapacity(fMax_Block_Size, kIODirectionOut);
        if (!fPipeOutBuff[i].pipeOutMDP)
            {
            IOLog("AJZaurusUSB::allocateResources - Allocate output descriptor failed\n");
            return false;
            }
        
        fPipeOutBuff[i].pipeOutMDP->setLength(fMax_Block_Size);
        fPipeOutBuff[i].pipeOutBuffer = (UInt8*)fPipeOutBuff[i].pipeOutMDP->getBytesNoCopy();
#if 0
        IOLog("AJZaurusUSB::allocateResources - mdp=%p output buffer=%p[%u]\n", fPipeOutBuff[i].pipeOutMDP, fPipeOutBuff[i].pipeOutBuffer, fPipeOutBuff[i].pipeOutBuffer->getLength());
#endif
        }
#if 1
    IOLog("AJZaurusUSB::allocateResources - done\n");
#endif  
    return true;
    
}/* end allocateResources */

/****************************************************************************************************/
//
//		Method:		net_lucid_cake_driver_AJZaurusUSB::releaseResources
//
//		Inputs:		
//
//		Outputs:	
//
//		Desc:		Frees up the resources allocated in allocateResources
//
/****************************************************************************************************/

void net_lucid_cake_driver_AJZaurusUSB::releaseResources()
{
    UInt32	i;
    IOLog("AJZaurusUSB::releaseResources\n");
    for (i=0; i<kOutBufPool; i++)
        {
        if (fPipeOutBuff[i].pipeOutMDP)	
            { 
            fPipeOutBuff[i].pipeOutMDP->release();	
            fPipeOutBuff[i].pipeOutMDP = NULL;
            }
        }
    if (fPipeInMDP)
        { 
        fPipeInMDP->release();	
        fPipeInMDP = NULL; 
        }
    if (fCommPipeMDP)	
        { 
        fCommPipeMDP->release();	
        fCommPipeMDP = NULL; 
        }
    
}/* end releaseResources */

/* EOF */