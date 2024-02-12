/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    NIC_INIT.c

Abstract:

    Contains rotuines to do resource allocation and hardware
    initialization & shutdown.

Environment:

    Kernel mode

--*/

#include "precomp.h"

#if defined(EVENT_TRACING)
#include "nic_init.tmh"
#endif

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, HwInitializeDeviceExtension)
#pragma alloc_text (PAGE, HwAllocateDeviceResources)
#pragma alloc_text (PAGE, HwMapHWResources)
#pragma alloc_text (PAGE, HwUnmapHWResources)
#pragma alloc_text (PAGE, HwGetDeviceInformation)
#pragma alloc_text (PAGE, ReadWriteConfigSpace)
#pragma alloc_text (PAGE, GetPCIBusInterfaceStandard)
#endif


NTSTATUS
HwInitializeDeviceExtension(
    __in PFDO_DATA FdoData
    )
/*++
Routine Description:


Arguments:

    FdoData     Pointer to our FdoData

Return Value:

     None

--*/
{
    NTSTATUS status;

    PAGED_CODE();

    //
    // Get the BUS_INTERFACE_STANDARD for our device so that we can
    // read & write to PCI config space at IRQL <= DISPATCH_LEVEL.
    //
    status = GetPCIBusInterfaceStandard(FdoData->Self,
                                        &FdoData->BusInterface);
    if (!NT_SUCCESS (status)){
        return status;
    }

    //
    // Initialize list heads, spinlocks, timers etc.
    //
    KeInitializeSpinLock(&FdoData->Lock);

    FdoData->SwitchCount = 0;
    FdoData->PushSwitchNotifyIrp = NULL;

    FdoData->ReadWriteIrp = NULL;
    FdoData->BusMasterDone = FALSE;

    return status;

}


NTSTATUS
HwAllocateDeviceResources(
    __in  PFDO_DATA   FdoData,
    __in      PIRP        Irp
    )
/*++
Routine Description:

    Allocates all the hw and software resources required for
    the device, enables interrupt, and initializes the device.

Arguments:

    FdoData     Pointer to our FdoData
    Irp         Pointer to start-device irp.

Return Value:

     None

--*/
{
    NTSTATUS        status;

    PAGED_CODE();

    do{

        //
        // First make sure this is our device before doing whole lot
        // of other things.
        //

       status = HwGetDeviceInformation(FdoData);
       if (!NT_SUCCESS (status)){
            return status;
        }

        status = HwMapHWResources(FdoData, Irp);
        if (!NT_SUCCESS (status)){
            DebugPrint(ERROR, DBG_INIT,"HwMapHWResources failed: 0x%x\n", status);
            break;
        }

        //
        // Enable the interrupt
        //
       // HwEnableInterrupt(FdoData);

    }while(FALSE);

    return status;

}


NTSTATUS
HwFreeDeviceResources(
    __in PFDO_DATA FdoData
    )
/*++
Routine Description:

    Free all the software resources. We shouldn't touch the hardware.

Arguments:

    FdoData     Pointer to our FdoData

Return Value:

     None

--*/
{
////    NTSTATUS    status;
////    KIRQL       oldIrql;

    DebugPrint(INFO, DBG_INIT, "-->NICFreeDeviceResources\n");


    //
    // Disconnect from the interrupt and unmap any I/O ports
    //HwGet
    HwUnmapHWResources(FdoData);

    DebugPrint(INFO, DBG_INIT, "<--NICFreeDeviceResources\n");

    return STATUS_SUCCESS;

}

NTSTATUS
HwMapHWResources(
    __in PFDO_DATA FdoData,
    __in PIRP Irp
    )
/*++
Routine Description:

    Gets the HW resources assigned by the bus driver from the start-irp
    and maps it to system address space. Initializes the DMA adapter
    and sets up the ISR.

    Three base address registers are supported by the 8255x:
    1) CSR Memory Mapped Base Address Register (BAR 0 at offset 10)
    2) CSR I/O Mapped Base Address Register (BAR 1 at offset 14)
    3) Flash Memory Mapped Base Address Register (BAR 2 at offset 18)

    The 8255x requires one BAR for I/O mapping and one BAR for memory
    mapping of these registers anywhere within the 32-bit memory address space.
    The driver determines which BAR (I/O or Memory) is used to access the
    Control/Status Registers.

    Just for illustration, this driver maps both memory and I/O registers and
    shows how to use READ_PORT_xxx or READ_REGISTER_xxx functions to perform
    I/O in a platform independent basis. On some platforms, the I/O registers
    can get mapped in to memory space and your driver should be able to handle
    this transparently.

    One BAR is also required to map the accesses to an optional Flash memory.
    The 82557 implements this register regardless of the presence or absence
    of a Flash chip on the adapter. The 82558 and 82559 only implement this
    register if a bit is set in the EEPROM. The size of the space requested
    by this register is 1Mbyte, and it is always mapped anywhere in the 32-bit
    memory address space.
    Note: Although the 82558 only supports up to 64 Kbytes of Flash memory
    and the 82559 only supports 128 Kbytes of Flash memory, 1 Mbyte of
    address space is still requested. Software should not access Flash
    addresses above 64 Kbytes for the 82558 or 128 Kbytes for the 82559
    because Flash accesses above the limits are aliased to lower addresses.

Arguments:

    FdoData     Pointer to our FdoData
    Irp         Pointer to start-device irp.

Return Value:

     None

--*/
{
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resourceTrans;
    PCM_PARTIAL_RESOURCE_LIST       partialResourceListTranslated;
    PIO_STACK_LOCATION              stack;
    ULONG                           i;
    NTSTATUS                        status = STATUS_SUCCESS;
    BOOLEAN bResPort = FALSE, bResInterrupt = FALSE, bResMemory = FALSE;
    ULONG                           numberOfBARs = 0;

    DebugPrint(TRACE, DBG_INIT, "--> HwMapHWResources\n");


    stack = IoGetCurrentIrpStackLocation (Irp);

    PAGED_CODE();

    if (NULL == stack->Parameters.StartDevice.AllocatedResourcesTranslated) {
        status = STATUS_DEVICE_CONFIGURATION_ERROR;
        goto End;
    }

    //
    // Parameters.StartDevice.AllocatedResourcesTranslated points
    // to a CM_RESOURCE_LIST describing the hardware resources that
    // the PnP Manager assigned to the device. This list contains
    // the resources in translated form. Use the translated resources
    // to connect the interrupt vector, map I/O space, and map memory.
    //

    partialResourceListTranslated = &stack->Parameters.StartDevice.\
                      AllocatedResourcesTranslated->List[0].PartialResourceList;

    resourceTrans = &partialResourceListTranslated->PartialDescriptors[0];

    DebugPrint(LOUD, DBG_INIT,  "partialResourceListTranslated->Count: (%d)\n",  partialResourceListTranslated->Count);

    for (i = 0; i < partialResourceListTranslated->Count; i++, resourceTrans++) {

        switch (resourceTrans->Type) {

        case CmResourceTypePort:
            //
            // We will increment the BAR count only for valid resources. We will
            // not count the private device types added by the PCI bus driver.
            //
            numberOfBARs++;

            DebugPrint(LOUD, DBG_INIT,
                "I/O mapped CSR: (%x) Length: (%d)\n",
                resourceTrans->u.Port.Start.LowPart,
                resourceTrans->u.Port.Length);

            //
            // Since we know the resources are listed in the same order the as
            // BARs in the config space, this should be the second one.
            //
            if(numberOfBARs != 2) {
                DebugPrint(ERROR, DBG_INIT, "I/O mapped CSR is not in the right order\n");
                status = STATUS_DEVICE_CONFIGURATION_ERROR;
                goto End;
            }

            //
            // The port is in memory space on this machine.
            // We shuld use READ_PORT_Xxx, and WRITE_PORT_Xxx routines
            // to read or write to the port.
            //

           // FdoData->IoBaseAddress = ULongToPtr(resourceTrans->u.Port.Start.LowPart);
            //FdoData->IoRange = resourceTrans->u.Port.Length;
            //
            // Since all our accesses are ULONG wide, we will create an accessor
            // table just for these two functions.
            //
            FdoData->ReadPort = HwReadPortULong;
            FdoData->WritePort = HwWritePortULong;

            bResPort = TRUE;
            FdoData->MappedPorts = FALSE;
            break;

        case CmResourceTypeMemory:

            numberOfBARs++;

            if(numberOfBARs == 1) {
                DebugPrint(LOUD, DBG_INIT, "Memory mapped CSR:(%x:%x) Length:(%d)\n",
                                        resourceTrans->u.Memory.Start.LowPart,
                                        resourceTrans->u.Memory.Start.HighPart,
                                        resourceTrans->u.Memory.Length);
                //FdoData->MemPhysAddress = resourceTrans->u.Memory.Start;

                // nvme のController registerをmapする。
                FdoData->controller_regs = MmMapIoSpace(
                                               resourceTrans->u.Memory.Start,
                                                     sizeof(NVME_CONTROLLER_REGISTERS),
                                                    MmNonCached);

              if(FdoData->controller_regs == NULL) {
                  DebugPrint(ERROR, DBG_INIT, "MmMapIoSpace failed\n");
                  status = STATUS_INSUFFICIENT_RESOURCES;
                  goto End;
              }

                DebugPrint(LOUD, DBG_INIT, "nvme controller regs=%p\n", FdoData->controller_regs);

                bResMemory = TRUE;

            } else if(numberOfBARs == 2){

                DebugPrint(LOUD, DBG_INIT,
                    "I/O mapped CSR in Memory Space: (%x) Length: (%d)\n",
                    resourceTrans->u.Memory.Start.LowPart,
                    resourceTrans->u.Memory.Length);
                //
                // The port is in memory space on this machine.
                // We should call MmMapIoSpace to map the physical to virtual
                // address, and also use the READ/WRITE_REGISTER_xxx function
                // to read or write to the port.
                //


                // 今回は使用しない。
             //   FdoData->IoBaseAddress = MmMapIoSpace(
              //                                  resourceTrans->u.Memory.Start,
               //                                 resourceTrans->u.Memory.Length,
               //                                 MmNonCached);
               // if(FdoData->IoBaseAddress == NULL) {
              //         DebugPrint(ERROR, DBG_INIT, "MmMapIoSpace failed\n");
              //         status = STATUS_INSUFFICIENT_RESOURCES;
              //         goto End;
              //  }

                FdoData->ReadPort = HwReadRegisterULong;
                FdoData->WritePort = HwWriteRegisterULong;
                FdoData->MappedPorts = TRUE;
                bResPort = TRUE;

            } else {
                DebugPrint(ERROR, DBG_INIT,
                            "Memory Resources are not in the right order\n");
                status = STATUS_DEVICE_CONFIGURATION_ERROR;
                goto End;
            }

            break;

        case CmResourceTypeInterrupt:

            ASSERT(!bResInterrupt);

            bResInterrupt = TRUE;
            //
            // Save all the interrupt specific information in the device
            // extension because we will need it to disconnect and connect the
            // interrupt later on during power suspend and resume.
            //
            FdoData->InterruptLevel = (UCHAR)resourceTrans->u.Interrupt.Level;
            FdoData->InterruptVector = resourceTrans->u.Interrupt.Vector;
            FdoData->InterruptAffinity = resourceTrans->u.Interrupt.Affinity;

            if (resourceTrans->Flags & CM_RESOURCE_INTERRUPT_LATCHED) {

                FdoData->InterruptMode = Latched;
                DebugPrint(LOUD, DBG_INIT, "Latched \n");
            } else {

                FdoData->InterruptMode = LevelSensitive;
                DebugPrint(LOUD, DBG_INIT, "LevelSensitive \n");

            }

            //
            // Because this is a PCI device, we KNOW it must be
            // a LevelSensitive Interrupt.
            //

            //ASSERT(FdoData->InterruptMode == LevelSensitive);

            DebugPrint(LOUD, DBG_INIT,
                "Interrupt level: 0x%0x, Vector: 0x%0x, Affinity: 0x%x\n",
                FdoData->InterruptLevel,
                FdoData->InterruptVector,
                (UINT)FdoData->InterruptAffinity); // casting is done to keep WPP happy

            if (resourceTrans->Flags & CM_RESOURCE_INTERRUPT_MESSAGE) {
                DebugPrint(LOUD, DBG_INIT,
                    "Message Interrupt level: 0x%0x, Vector: 0x%0x, Affinity: 0x%x\n",
                    (UCHAR)resourceTrans->u.MessageInterrupt.Translated.Level,
                    resourceTrans->u.MessageInterrupt.Translated.Vector,
                    (UINT)resourceTrans->u.MessageInterrupt.Translated.Affinity); // casting is done to keep WPP happy

            }

            break;

        default:
            //
            // This could be device-private type added by the PCI bus driver. We
            // shouldn't filter this or change the information contained in it.
            //
            DebugPrint(LOUD, DBG_INIT, "Unhandled resource type (0x%x)\n",
                                        resourceTrans->Type);
            break;

        }
    }

    //
    // Make sure we got all the 3 resources to work with.
    //
    if (!( bResInterrupt && bResMemory)) {
        DebugPrint(LOUD, DBG_INIT, "Failure bResInterrupt && bResMemory\n");

        status = STATUS_DEVICE_CONFIGURATION_ERROR;
        goto End;
    }

    //
    // Disable interrupts here which is as soon as possible
    //

   // 今回は使用しない。
   // HwDisableInterrupt(FdoData);

    IoInitializeDpcRequest(FdoData->Self, HwDpcForIsr);

  
   //Register the interrupt
   
   status = IoConnectInterrupt(&FdoData->Interrupt,
                             HwInterruptHandler,
                         FdoData,                   // ISR Context
                         NULL,
                         FdoData->InterruptVector,
                         FdoData->InterruptLevel,
                         FdoData->InterruptLevel,
                         FdoData->InterruptMode,
                         TRUE, // shared interrupt
                         FdoData->InterruptAffinity,
                         FALSE);
    if (status != STATUS_SUCCESS)
    {
        DebugPrint(ERROR, DBG_INIT, "IoConnectInterrupt failed %x\n", status);
        goto End;
    }
    else {
        DebugPrint(TRACE, DBG_INIT, "IoConnectInterrupt Success\n");
    }

    PHYSICAL_ADDRESS phyaddr, addrmask;

    FdoData->buf_va = NULL;
    addrmask.LowPart = 0xffffffff;
    addrmask.HighPart = 0;

    if ((FdoData->buf_va = MmAllocateContiguousMemory(4096, addrmask)) == NULL) {
        DebugPrint(ERROR, DBG_INIT, "MmAllocateContiguousMemory failed\n");
    }
    else {
        phyaddr = MmGetPhysicalAddress(FdoData->buf_va);
        DebugPrint(TRACE, DBG_INIT, "phyaddr.LowPart = 0x%x\n", phyaddr.LowPart);

    }



    //バスマスタ転送の為のDmaAdapterObjectを生成。
    //バスマスタ転送は、（ソフトウェア的な）スキャッタギャザー転送で行う。

#if 0  // 今回は使わない
    {
        DEVICE_DESCRIPTION              deviceDescription;
        ULONG MapRegisters = 0;
    
        RtlZeroMemory(&deviceDescription, sizeof(DEVICE_DESCRIPTION));
        deviceDescription.Version           = DEVICE_DESCRIPTION_VERSION;
        deviceDescription.Master            = TRUE;
        deviceDescription.ScatterGather     = TRUE;
        //deviceDescription.DemandMode              //not used for bus master
        //deviceDescription.AutoInitialize          //not used for bus master
        deviceDescription.Dma32BitAddresses = TRUE;
        //deviceDescription.IgnoreCount             //not used when Version is DEVICE_DESCRIPTION_VERSION
        deviceDescription.Dma64BitAddresses = FALSE;
        //deviceDescription.BusNumber               //not used by WDM drivers
        //deviceDescription.DmaChannel              //not usded
        deviceDescription.InterfaceType     = PCIBus;
        //deviceDescription.DmaWidth                //for system dma
        //deviceDescription.DmaSpeed                //for system dma
        deviceDescription.MaximumLength     = 0xffffffff;
        //deviceDescription.DmaPort                 //obsolete
    
        FdoData->DmaAdapterObject = IoGetDmaAdapter(
                                            FdoData->UnderlyingPDO,
                                            &deviceDescription,
                                            &MapRegisters);
    
        if (FdoData->DmaAdapterObject == NULL) {
            DebugPrint(ERROR, DBG_INIT, "IoGetDmaAdapter failed\n");
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto End;
        }
    
    	FdoData->AllocatedMapRegisters = MapRegisters;
    }
#endif

End:
    //
    // If we have jumped here due to any kind of mapping or resource allocation
    // failure, we should clean up. Since we know that if we fail Start-Device,
    // the system is going to send Remove-Device request, we will defer the
    // job of cleaning to NICUnmapHWResources which is called in the Remove path.
    //
    if (status != STATUS_SUCCESS)
    {
       // HwUnmapHWResources(FdoData);
    }

    DebugPrint(TRACE, DBG_INIT, "<-- HwMapHWResources\n");


    return status;

}

NTSTATUS
HwUnmapHWResources(
    __in PFDO_DATA FdoData
    )
/*++
Routine Description:

    Disconnect the interrupt and unmap all the memory and I/O resources.

Arguments:

    FdoData     Pointer to our FdoData

Return Value:

     None

--*/
{
   // PDMA_ADAPTER    DmaAdapterObject = FdoData->DmaAdapterObject;
   // UNREFERENCED_PARAMETER(DmaAdapterObject);
   // PAGED_CODE();

    DebugPrint(TRACE, DBG_INIT, "--> HwUnmapHWResources\n");
    IoDisconnectInterrupt(FdoData->Interrupt);
    FdoData->Interrupt = NULL;


    if (FdoData->controller_regs)
    {
        MmUnmapIoSpace(FdoData->controller_regs, sizeof(NVME_CONTROLLER_REGISTERS));
        FdoData->controller_regs = NULL;
    }

    if (FdoData->buf_va != NULL) {
        MmFreeContiguousMemory(FdoData->buf_va);
    }
    


#if 0
    //
    // Free hardware resources
    //
	IoDisconnectInterrupt(FdoData->Interrupt);
	FdoData->Interrupt = NULL;

    if (FdoData->CSRAddress)
    {
        MmUnmapIoSpace(FdoData->CSRAddress, HW_MAP_IOSPACE_LENGTH);
        FdoData->CSRAddress = NULL;
    }

    if(FdoData->MappedPorts){
        MmUnmapIoSpace(FdoData->IoBaseAddress, FdoData->IoRange);
        FdoData->IoBaseAddress = NULL;
    }

    if(DmaAdapterObject) {
        DmaAdapterObject->DmaOperations->PutDmaAdapter(DmaAdapterObject);
        FdoData->DmaAdapterObject = NULL;
    }
#endif
    DebugPrint(TRACE, DBG_INIT, "<-- HwUnmapHWResources\n");


    return STATUS_SUCCESS;

}


NTSTATUS
HwGetDeviceInformation(
    __in PFDO_DATA FdoData
)
/*++
Routine Description:

    This function reads the PCI config space and make sure that it's our
    device and stores the device IDs and power information in the device
    extension. Should be done in the StartDevice.

Arguments:

    FdoData     Pointer to our FdoData

Return Value:

     None

--*/
{
    DebugPrint(TRACE, DBG_INIT, "---> HwGetDeviceInformation\n");

    PCI_COMMON_CONFIG pci_config;
    NTSTATUS status = ReadWriteConfigSpace(FdoData-> UnderlyingPDO, 0, &pci_config, 0, sizeof(PCI_COMMON_CONFIG));
    if (NT_SUCCESS(status))
    {
        DbgPrint("======================PCI_COMMON_CONFIG Begin=====================\n");
        DbgPrint("VendorID:%x\n", pci_config.VendorID);
        DbgPrint("DeviceID:%x\n", pci_config.DeviceID);
        DbgPrint("CapabilitiesPtr: %x\n", pci_config.u.type0.CapabilitiesPtr);
    }
    else {
        DbgPrint("Failure ReadWriteconfig\n");
    }

    if (pci_config.VendorID != HW_PCI_VENDOR_ID ||
        pci_config.DeviceID != HW_PCI_DEVICE_ID)
    {
        DebugPrint(ERROR, DBG_INIT,
            "VendorID/DeviceID don't match - %x/%x\n",
            pci_config.VendorID, pci_config.DeviceID);
        return STATUS_DEVICE_DOES_NOT_EXIST;

    }

    FdoData->RevsionID = pci_config.RevisionID;
    FdoData->SubVendorID = pci_config.u.type0.SubVendorID;
    FdoData->SubSystemID = pci_config.u.type0.SubSystemID;

    DebugPrint(TRACE, DBG_INIT, "<-- HwGetDeviceInformation\n");


#if 0

    NTSTATUS            status = STATUS_SUCCESS;
    typedef __declspec(align(MEMORY_ALLOCATION_ALIGNMENT)) CommonConfigCharBuf;
    CommonConfigCharBuf buffer[HW_PCI_CONF_REG_LENGTH ];
    PPCI_COMMON_CONFIG  pPciConfig = (PPCI_COMMON_CONFIG) buffer;
    ULONG               bytesRead =0;

    DebugPrint(TRACE, DBG_INIT, "---> HwGetDeviceInformation\n");

    PAGED_CODE();

    bytesRead = FdoData->BusInterface.GetBusData(
                        FdoData->BusInterface.Context,
                         PCI_WHICHSPACE_CONFIG, //READ
                         buffer,
                         FIELD_OFFSET(PCI_COMMON_CONFIG, VendorID),
                         HW_PCI_CONF_REG_LENGTH);

    if (bytesRead != HW_PCI_CONF_REG_LENGTH) {
        DebugPrint(ERROR, DBG_INIT,
                        "GetBusData (HW_PCI_CONF_REG_LENGTH) failed =%d\n",
                         bytesRead);
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    //
    // Is this our device?
    //

    if (pPciConfig->VendorID != HW_PCI_VENDOR_ID ||
        pPciConfig->DeviceID != HW_PCI_DEVICE_ID)
    {
        DebugPrint(ERROR, DBG_INIT,
                        "VendorID/DeviceID don't match - %x/%x\n",
                        pPciConfig->VendorID, pPciConfig->DeviceID);
        //return STATUS_DEVICE_DOES_NOT_EXIST;

    }

    //
    // save info from config space
    //
    FdoData->RevsionID = pPciConfig->RevisionID;
    FdoData->SubVendorID = pPciConfig->u.type0.SubVendorID;
    FdoData->SubSystemID = pPciConfig->u.type0.SubSystemID;

    DebugPrint(TRACE, DBG_INIT, "<-- HwGetDeviceInformation\n");

#endif
    return status;
}


VOID
HwShutdown(
    __in  PFDO_DATA     FdoData)
/*++

Routine Description:

    Shutdown the device

Arguments:

    FdoData -  Pointer to our adapter

Return Value:

    None

--*/
{
    DebugPrint(INFO, DBG_INIT, "---> HwShutdown\n");

  //  if(FdoData->CSRAddress) {
        //
        // Disable interrupt and issue a full reset
        //
       // HwDisableInterrupt(FdoData);

		//FdoData->CSRAddress->LedState = 0;

    //}
    DebugPrint(INFO, DBG_INIT, "<--- HwShutdown\n");
}




NTSTATUS
ReadWriteConfigSpace(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG	      ReadOrWrite, // 0 for read 1 for write
    IN PVOID	      Buffer,
    IN ULONG	      Offset,
    IN ULONG	      Length
)
{
    KEVENT event;
    NTSTATUS status;
    PIRP irp;
    IO_STATUS_BLOCK ioStatusBlock;
    PIO_STACK_LOCATION irpStack;
    PDEVICE_OBJECT targetObject;

    PAGED_CODE();

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    targetObject = IoGetAttachedDeviceReference(DeviceObject);

    irp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP,
        targetObject,
        NULL,
        0,
        NULL,
        &event,
        &ioStatusBlock);

    if (irp == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto End;
    }

    irpStack = IoGetNextIrpStackLocation(irp);

    if (ReadOrWrite == 0) {
        irpStack->MinorFunction = IRP_MN_READ_CONFIG;
    }
    else {
        irpStack->MinorFunction = IRP_MN_WRITE_CONFIG;
    }

    irpStack->Parameters.ReadWriteConfig.WhichSpace = PCI_WHICHSPACE_CONFIG;
    irpStack->Parameters.ReadWriteConfig.Buffer = Buffer;
    irpStack->Parameters.ReadWriteConfig.Offset = Offset;
    irpStack->Parameters.ReadWriteConfig.Length = Length;

    // 
    // Initialize the status to error in case the bus driver does not 
    // set it correctly.
    // 

    irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

    status = IoCallDriver(targetObject, irp);

    if (status == STATUS_PENDING) {

        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = ioStatusBlock.Status;
    }

End:
    // 
    // Done with reference
    // 
    ObDereferenceObject(targetObject);

    return status;

}


NTSTATUS
GetPCIBusInterfaceStandard(
    __in  PDEVICE_OBJECT DeviceObject,
    __out PBUS_INTERFACE_STANDARD    BusInterfaceStandard
    )
/*++
Routine Description:

    This routine gets the bus interface standard information from the PDO.

Arguments:
    DeviceObject - Device object to query for this information.
    BusInterface - Supplies a pointer to the retrieved information.

Return Value:

    NT status.

--*/
{
    KEVENT event;
    NTSTATUS status;
    PIRP irp;
    IO_STATUS_BLOCK ioStatusBlock;
    PIO_STACK_LOCATION irpStack;
    PDEVICE_OBJECT targetObject;

    DebugPrint(TRACE, DBG_INIT, "GetPciBusInterfaceStandard entered.\n");

    PAGED_CODE();

    KeInitializeEvent( &event, NotificationEvent, FALSE );

    targetObject = IoGetAttachedDeviceReference( DeviceObject );

    irp = IoBuildSynchronousFsdRequest( IRP_MJ_PNP,
                                        targetObject,
                                        NULL,
                                        0,
                                        NULL,
                                        &event,
                                        &ioStatusBlock );
    if (irp == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto End;
    }

    irpStack = IoGetNextIrpStackLocation( irp );
    irpStack->MinorFunction = IRP_MN_QUERY_INTERFACE;
    irpStack->Parameters.QueryInterface.InterfaceType =
                        (LPGUID) &GUID_BUS_INTERFACE_STANDARD ;
    irpStack->Parameters.QueryInterface.Size = sizeof(BUS_INTERFACE_STANDARD);
    irpStack->Parameters.QueryInterface.Version = 1;
    irpStack->Parameters.QueryInterface.Interface =
                                        (PINTERFACE)BusInterfaceStandard;

    irpStack->Parameters.QueryInterface.InterfaceSpecificData = NULL;

    //
    // Initialize the status to error in case the bus driver does not
    // set it correctly.
    irp->IoStatus.Status = STATUS_NOT_SUPPORTED ;

    status = IoCallDriver( targetObject, irp );
    if (status == STATUS_PENDING) {

        status = KeWaitForSingleObject( &event, Executive, KernelMode, FALSE, NULL);
        ASSERT(NT_SUCCESS(status));
        status = ioStatusBlock.Status;

    }

End:
    // Done with reference
    ObDereferenceObject( targetObject );
    return status;

}


NTSTATUS
HwSetPower(
	PFDO_DATA          FdoData ,
	DEVICE_POWER_STATE oldPowerState
	)
/*++
Routine Description:

	This routine is called when the FdoData receives a SetPower
	request. It redirects the call to an appropriate routine to
	Set the New PowerState

Arguments:

	FdoData                 Pointer to the FdoData structure
	PowerState              OldPowerState

Return Value:

	NTSTATUS Code

--*/
{
	NTSTATUS      status = STATUS_SUCCESS;
    DEVICE_POWER_STATE newPowerState = FdoData->DevicePowerState;
    
#if 1
		if( PowerDeviceD3 == newPowerState ) {
			//稼働状態→完全にオフ。
			//LEDを保存して消灯
		//	FdoData->LedSaved = FdoData->CSRAddress->LedState;
		//	FdoData->CSRAddress->LedState = 0;
		}
		else if( PowerDeviceD3 == oldPowerState ) {
			//完全にオフ→稼働状態。
			//LEDを復旧
		//	FdoData->CSRAddress->LedState = FdoData->LedSaved;
		}
#endif

	return status;
}

