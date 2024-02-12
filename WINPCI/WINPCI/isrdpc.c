/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    ISRDPC.C

Abstract:

    Contains routine to handle interrupts, interrupt DPCs and WatchDogTimer DPC

Environment:

    Kernel mode

--*/

#include "precomp.h"

#if defined(EVENT_TRACING)
#include "ISRDPC.tmh"
#endif

BOOLEAN
HwInterruptHandler(
    __in PKINTERRUPT  Interupt,
    __in PVOID        ServiceContext
    )
/*++
Routine Description:

    Interrupt handler for the device.

Arguments:

    Interupt - Address of the KINTERRUPT Object for our device.
    ServiceContext - Pointer to our adapter

Return Value:

     TRUE if our device is interrupting, FALSE otherwise.

--*/
{
   //BOOLEAN   interruptRecognized = FALSE;
  //  PFDO_DATA fdoData = (PFDO_DATA)ServiceContext;
   // ULONG     intStatus;

    BOOLEAN   interruptRecognized = TRUE;
    DebugPrint(TRACE, DBG_INTERRUPT, "--> HwInterruptHandler\n");
#if 0
    do
    {
        //
        // If the adapter is in low power state, then it should not
        // recognize any interrupt
        //
        if (fdoData->DevicePowerState > PowerDeviceD0)
        {
            break;
        }
        //
        // We process the interrupt if it's not disabled and it's active
        //

        intStatus = HW_GET_INTERRUPT_STATUS(fdoData);

        if (!HW_INTERRUPT_DISABLED(fdoData) && intStatus)
        {
            interruptRecognized = TRUE;

            if(intStatus & BIT_INT_SWITCH)
            {
                LONG result;
                result = InterlockedIncrement(&fdoData->SwitchCount);
            }

            if(intStatus & BIT_INT_BUS_MASTER)
            {
                fdoData->BusMasterDone = TRUE;
            }

            //
            // Acknowledge the interrupt(s) and get the interrupt status
            //

            HW_ACK_INTERRUPT(fdoData, intStatus);

            DebugPrint(TRACE, DBG_INTERRUPT, "Requesting DPC\n");

            IoRequestDpc(fdoData->Self, NULL, fdoData);

        }
    }while (FALSE);
#endif
    DebugPrint(TRACE, DBG_INTERRUPT, "<-- HwInterruptHandler\n");

    return interruptRecognized;
}

VOID
HwDpcForIsr(    //HwInterruptHandler()�����݃��[�`������N�������DPC���[�`���B
    PKDPC            Dpc,
    PDEVICE_OBJECT   DeviceObject,
    PIRP             Irp, //Unused
    PVOID            Context
    )

/*++

Routine Description:

    DPC callback for ISR.

Arguments:

    DeviceObject - Pointer to the device object.

    Context - MiniportAdapterContext.

    Irp - Unused.

    Context - Pointer to FDO_DATA.

Return Value:

--*/
{
    //NTSTATUS     status;
   // PFDO_DATA FdoData = (PFDO_DATA) Context;
   // PIRP irp;
   // ULONG switchCount;

    DebugPrint(TRACE, DBG_DPC, "--> HwDpcForIsr\n");

#if 0
    //Handle PushSwitchNotifyIrp
    switchCount = InterlockedExchange(&FdoData->SwitchCount, 0);
    if( switchCount ) {

        irp = FdoData->PushSwitchNotifyIrp;

        if(irp) {
            if(IoSetCancelRoutine(irp, NULL)) {
                //it's not been canceled.
                FdoData->PushSwitchNotifyIrp = NULL;
                irp->IoStatus.Status = STATUS_SUCCESS;
                IoCompleteRequest(irp, IO_NO_INCREMENT);
                PciDrvIoDecrement ( FdoData );
            }
        }
    }

    //�o�X�}�X�^���������ݏ����B
    if(FdoData->BusMasterDone) {

        FdoData->BusMasterDone = FALSE;

        //Handle it !!
        irp = FdoData->ReadWriteIrp;
        if (irp) {
            //�o�X�}�X�^�̃A�{�[�g�X�e�[�^�X���m�F�B
            if(FdoData->CSRAddress->BusMasterControlStatus & BIT_BUSMASTER_ABORT_STATUS) {
                status = STATUS_IO_DEVICE_ERROR;
            }
            else {
                PSCATTER_GATHER_LIST    scatterGather = FdoData->ScatterGather;
                ULONG                   progress      = FdoData->ScatterGatherProgress;

                status = STATUS_SUCCESS;
                //�A�v���P�[�V�����֕Ԃ����[�h�^���C�g�̊����T�C�Y���X�V�B
                irp->IoStatus.Information += scatterGather->Elements[ progress ].Length;

                //�]���i���J�E���^���C���N�������g�B
                FdoData->ScatterGatherProgress++;

                //���N�G�X�g�̃L�����Z���v�������s����Ă��邩���m�F�B
                if(irp->Cancel){
                    status = STATUS_CANCELLED;
                }
                //�]�������������y�[�W�����邩���m�F�B
                else if (FdoData->ScatterGatherProgress < scatterGather->NumberOfElements) {
                    //���̕����y�[�W�̃o�X�}�X�^�]�����J�n�B
                    HwKickBusMasterReadWrite(FdoData);
                    status = STATUS_PENDING;
                }
            }

            //�S�y�[�W�̓]�������A�܂��̓G���[�����̏ꍇ�B
            if(status != STATUS_PENDING) {
                //�X�L���b�^�M���U���X�g��j���B
            	FdoData->DmaAdapterObject->DmaOperations->PutScatterGatherList(
                                            FdoData->DmaAdapterObject,
                                            FdoData->ScatterGather,
                                            FdoData->ScatterGatherWriteToDevice
                                            );

                //���[�h�^���C�g���N�G�X�g�������B
            	FdoData->ReadWriteIrp = NULL;
               	irp->IoStatus.Status = status;
            	IoCompleteRequest(irp, IO_NO_INCREMENT);
            	PciDrvIoDecrement (FdoData);
            }
        }
    }
#endif
    DebugPrint(TRACE, DBG_DPC, "<-- HwDpcForIsr\n");

}

