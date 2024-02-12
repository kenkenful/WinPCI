#include "precomp.h"

NTSTATUS
HwReadWriteRegister (
    __in PFDO_DATA FdoData,
    __in PIRP      Irp
	)
{
    NTSTATUS status = STATUS_SUCCESS;	
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation (Irp);
    ULONG functionCode;
    PULONG pData = 0;
    ULONG bufferLength = 0;

    //IRP�X�^�b�N�̃p�����[�^����IO�R���g���[���R�[�h���擾�B
    functionCode = irpStack->Parameters.DeviceIoControl.IoControlCode;

    //IRP����f�[�^�o�b�t�@���擾�B
    pData = Irp->AssociatedIrp.SystemBuffer;

    //IO�R���g���[���R�[�h�ɏ]���āA�o�b�t�@�T�C�Y���擾�B
    switch (functionCode)
    {
    case IOCTL_READ_LED_BY_MEM:
    case IOCTL_READ_LED_BY_IO:
    case IOCTL_READ_DIPSWITCH_BY_MEM:
    case IOCTL_READ_DIPSWITCH_BY_IO:
    case IOCTL_GET_BUS_MASTER_READ_DATA:
        //IRP�X�^�b�N�̃p�����[�^����f�[�^�o�b�t�@�T�C�Y���擾�B
        bufferLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
        break;
    case IOCTL_WRITE_LED_BY_MEM:
    case IOCTL_WRITE_LED_BY_IO:
    case IOCTL_SET_BUS_MASTER_WRITE_DATA:
        //IRP�X�^�b�N�̃p�����[�^���烉�C�g�f�[�^�T�C�Y���擾�B
        bufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
        break;
    default:
        ASSERT(FALSE);
    break;
    }

    //�p�����[�^���m�F�B
    if( (pData == NULL) || (bufferLength < sizeof(ULONG)) ){
        status = STATUS_INVALID_PARAMETER;
        return status;
    }

    //IO�R���g���[���R�[�h�ɏ]���āA���W�X�^�A�N�Z�X�����s�B
    switch (functionCode)
    {
    case IOCTL_READ_LED_BY_MEM:
        //LED�_���o�̓��W�X�^���������A�N�Z�X�Ń��[�h�B
       // *pData = FdoData->CSRAddress->LedState;
        break;
    case IOCTL_READ_LED_BY_IO:
        //LED�_���o�̓��W�X�^��IO�|�[�g�A�N�Z�X�Ń��[�h�B
      //  *pData = FdoData->ReadPort(FdoData->IoBaseAddress);
        break;
    case IOCTL_WRITE_LED_BY_MEM:
        //LED�_���o�̓��W�X�^���������A�N�Z�X�Ń��C�g�B
     //  FdoData->CSRAddress->LedState = *pData;
        break;
    case IOCTL_WRITE_LED_BY_IO:
        //LED�_���o�̓��W�X�^��IO�|�[�g�A�N�Z�X�Ń��C�g�B
    //   FdoData->WritePort(FdoData->IoBaseAddress, *pData);
        break;
    case IOCTL_READ_DIPSWITCH_BY_MEM:
        //�f�B�b�v�X�C�b�`���̓��W�X�^���������A�N�Z�X�Ń��[�h�B
        //*pData = FdoData->CSRAddress->DipSwitchState;
        break;
    case IOCTL_READ_DIPSWITCH_BY_IO:
        //�f�B�b�v�X�C�b�`���̓��W�X�^��IO�|�[�g�A�N�Z�X�Ń��[�h�B
      //  *pData = FdoData->ReadPort(FdoData->IoBaseAddress + 1);
        break;
    case IOCTL_SET_BUS_MASTER_WRITE_DATA:
        //�o�X�}�X�^�]�����C�g�f�[�^���W�X�^���������A�N�Z�X�Ń��C�g�B
      //  FdoData->CSRAddress->BusMasterWriteData = *pData;
        break;
    case IOCTL_GET_BUS_MASTER_READ_DATA:
        //�o�X�}�X�^�]�����[�h�f�[�^���W�X�^���������A�N�Z�X�Ń��[�h�B
      //  *pData = FdoData->CSRAddress->BusMasterReadData;
        break;
    default:
        ASSERT(FALSE);
    break;
    }

    //�A�v���P�[�V�����֕Ԃ����[�h�^���C�g�f�[�^�T�C�Y��ݒ�B
    Irp->IoStatus.Information = sizeof(ULONG);

    return status;
}

NTSTATUS
HwStartBusMasterWriteRead (
    __in PFDO_DATA FdoData,
    __in PIRP      Irp,
    __in PMDL      Mdl
    )
{
    NTSTATUS     status;
    PIO_STACK_LOCATION irpStack;

    //���N�G�X�g�̃L�����Z���v�������s����Ă��Ȃ����Ƃ��m�F�B
    if(Irp->Cancel){
        status = STATUS_CANCELLED;
        return status;
    }

    //���[�h�A���C�g���N�G�X�g�����s���łȂ����Ƃ��m�F�B
    if(FdoData->ReadWriteIrp != NULL) {
        status = STATUS_INVALID_DEVICE_REQUEST;
        ASSERTMSG("More than one Request", FALSE);
        return status;
    }

    {
        //Mdl����A�]���Ώۃo�b�t�@�̕����y�[�W�����擾�B
        ULONG pages = 0;
        pages = ADDRESS_AND_SIZE_TO_SPAN_PAGES  (
                                                    MmGetMdlVirtualAddress(Mdl),
                                                    MmGetMdlByteCount(Mdl)
                                                    );
    
        //�A���P�[�g�ς݃}�b�v���W�X�^���ȉ��ł��邱�Ƃ��m�F�B
        if (pages > FdoData->AllocatedMapRegisters) {
            DebugPrint(ERROR, DBG_INIT, "Not enough map registers: Allocated %d, Required %d\n",
                                            FdoData->AllocatedMapRegisters, pages);
            status = STATUS_INSUFFICIENT_RESOURCES;
            return status;
        }
    }

    {
        //�]���������擾�B
        BOOLEAN writeToDevice = FALSE;

        irpStack = IoGetCurrentIrpStackLocation(Irp);
        if (irpStack->MajorFunction == IRP_MJ_READ) {
            writeToDevice = FALSE;
        }
        else if (irpStack->MajorFunction == IRP_MJ_WRITE) {
            writeToDevice = TRUE;
        }
        else {
            ASSERT(FALSE);
        }
    
        //���[�h�^���C�gIRP���Z�b�g�B
        FdoData->ReadWriteIrp = Irp;
        //IRP���y���f�B���O�B
        IoMarkIrpPending(Irp);
    
        //�o�b�t�@���t���b�V���B
        KeFlushIoBuffers(Mdl, !writeToDevice, TRUE);
    
        //DMA�A�_�v�^�ɓo�^����Ă���GetScatterGatherList�֐����R�[���B
        //�X�L���b�^�M���U���X�g����������A��6�����œn����
        //HwInitiateScatterGatherBusmaster()�����s�����B
#pragma warning(disable:4306)
    	status = FdoData->DmaAdapterObject->DmaOperations->GetScatterGatherList (
                                            FdoData->DmaAdapterObject,
                                            FdoData->Self,
                                            Mdl,
                                            MmGetMdlVirtualAddress(Mdl),
                                            MmGetMdlByteCount(Mdl),
                                            HwInitiateScatterGatherBusmaster,
                                            (PVOID)writeToDevice,
                                            writeToDevice
                                            );
#pragma warning(default:4306)
    }

    if( !NT_SUCCESS(status)) {
        FdoData->ReadWriteIrp = NULL;
        status = STATUS_INSUFFICIENT_RESOURCES;
    }
    else {
        status = STATUS_PENDING;
    }

    return status;
}


VOID
HwInitiateScatterGatherBusmaster(
    __in struct _DEVICE_OBJECT  *DeviceObject,
    __in struct _IRP  *Irp, //not used
    __in PSCATTER_GATHER_LIST  ScatterGather,
    __in PVOID  Context
    )
{
    //GetScatterGatherList�Ɠ����R���e�L�X�g�B���s�ł��Ȃ��B

    PFDO_DATA fdoData = (PFDO_DATA) DeviceObject->DeviceExtension;
    BOOLEAN   writeToDevice = FALSE;
    if (Context) {
        writeToDevice = TRUE;
    }

    //�X�L���b�^�M���U���X�g�ւ̃|�C���^��ۑ��B
    fdoData->ScatterGather              = ScatterGather;
    //�]���p������ۑ��B
    fdoData->ScatterGatherWriteToDevice = writeToDevice;
    //�]���i���J�E���^�����Z�b�g�B
    fdoData->ScatterGatherProgress      = 0;

    HwKickBusMasterReadWrite(fdoData);

}

VOID
HwKickBusMasterReadWrite (
    __in PFDO_DATA FdoData
)
{
    PSCATTER_GATHER_LIST    scatterGather = FdoData->ScatterGather;
    ULONG                   progress = FdoData->ScatterGatherProgress;

    //�X�L���b�^�M���U���X�g����A�]�������s���镨���y�[�W�����擾�B
    PSCATTER_GATHER_ELEMENT elm = &scatterGather->Elements[ progress ];

    //�]�������A�����A�h���X�A�T�C�Y�������ɓn���āA�]�����L�b�N�B
    HwKickBusMaster( FdoData, FdoData->ScatterGatherWriteToDevice, elm->Address, elm->Length );
}

VOID
HwKickBusMaster (
    __in PFDO_DATA        FdoData,
    __in BOOLEAN          WriteToDevice,
    __in PHYSICAL_ADDRESS Address,
    __in ULONG            Length
)
{
    ULONG command;
    if (WriteToDevice) {
        command = BIT_BUSMASTER_START_READ;
    }
    else {
        command = BIT_BUSMASTER_START_WRITE;
    }

    //�o�X�}�X�^���䃌�W�X�^���Z�b�g���A�i�P�����y�[�W�́j�]�����J�n�B
    if(Length % 4) {
        ASSERTMSG("HwKickBusMaster : Length % 4", FALSE);
	}

    //Kick Bus Master write/read. On Completion, see isrdcp.c.
   // FdoData->CSRAddress->BusMasterAccessAddress = (ULONG)Address.LowPart;
   // FdoData->CSRAddress->BusMasterAccessCount = Length / 4;
  //  FdoData->CSRAddress->BusMasterControlStatus = command;
}

