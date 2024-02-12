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

    //IRPスタックのパラメータからIOコントロールコードを取得。
    functionCode = irpStack->Parameters.DeviceIoControl.IoControlCode;

    //IRPからデータバッファを取得。
    pData = Irp->AssociatedIrp.SystemBuffer;

    //IOコントロールコードに従って、バッファサイズを取得。
    switch (functionCode)
    {
    case IOCTL_READ_LED_BY_MEM:
    case IOCTL_READ_LED_BY_IO:
    case IOCTL_READ_DIPSWITCH_BY_MEM:
    case IOCTL_READ_DIPSWITCH_BY_IO:
    case IOCTL_GET_BUS_MASTER_READ_DATA:
        //IRPスタックのパラメータからデータバッファサイズを取得。
        bufferLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;
        break;
    case IOCTL_WRITE_LED_BY_MEM:
    case IOCTL_WRITE_LED_BY_IO:
    case IOCTL_SET_BUS_MASTER_WRITE_DATA:
        //IRPスタックのパラメータからライトデータサイズを取得。
        bufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
        break;
    default:
        ASSERT(FALSE);
    break;
    }

    //パラメータを確認。
    if( (pData == NULL) || (bufferLength < sizeof(ULONG)) ){
        status = STATUS_INVALID_PARAMETER;
        return status;
    }

    //IOコントロールコードに従って、レジスタアクセスを実行。
    switch (functionCode)
    {
    case IOCTL_READ_LED_BY_MEM:
        //LED点灯出力レジスタをメモリアクセスでリード。
       // *pData = FdoData->CSRAddress->LedState;
        break;
    case IOCTL_READ_LED_BY_IO:
        //LED点灯出力レジスタをIOポートアクセスでリード。
      //  *pData = FdoData->ReadPort(FdoData->IoBaseAddress);
        break;
    case IOCTL_WRITE_LED_BY_MEM:
        //LED点灯出力レジスタをメモリアクセスでライト。
     //  FdoData->CSRAddress->LedState = *pData;
        break;
    case IOCTL_WRITE_LED_BY_IO:
        //LED点灯出力レジスタをIOポートアクセスでライト。
    //   FdoData->WritePort(FdoData->IoBaseAddress, *pData);
        break;
    case IOCTL_READ_DIPSWITCH_BY_MEM:
        //ディップスイッチ入力レジスタをメモリアクセスでリード。
        //*pData = FdoData->CSRAddress->DipSwitchState;
        break;
    case IOCTL_READ_DIPSWITCH_BY_IO:
        //ディップスイッチ入力レジスタをIOポートアクセスでリード。
      //  *pData = FdoData->ReadPort(FdoData->IoBaseAddress + 1);
        break;
    case IOCTL_SET_BUS_MASTER_WRITE_DATA:
        //バスマスタ転送ライトデータレジスタをメモリアクセスでライト。
      //  FdoData->CSRAddress->BusMasterWriteData = *pData;
        break;
    case IOCTL_GET_BUS_MASTER_READ_DATA:
        //バスマスタ転送リードデータレジスタをメモリアクセスでリード。
      //  *pData = FdoData->CSRAddress->BusMasterReadData;
        break;
    default:
        ASSERT(FALSE);
    break;
    }

    //アプリケーションへ返すリード／ライトデータサイズを設定。
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

    //リクエストのキャンセル要求が発行されていないことを確認。
    if(Irp->Cancel){
        status = STATUS_CANCELLED;
        return status;
    }

    //リード、ライトリクエストを実行中でないことを確認。
    if(FdoData->ReadWriteIrp != NULL) {
        status = STATUS_INVALID_DEVICE_REQUEST;
        ASSERTMSG("More than one Request", FALSE);
        return status;
    }

    {
        //Mdlから、転送対象バッファの物理ページ数を取得。
        ULONG pages = 0;
        pages = ADDRESS_AND_SIZE_TO_SPAN_PAGES  (
                                                    MmGetMdlVirtualAddress(Mdl),
                                                    MmGetMdlByteCount(Mdl)
                                                    );
    
        //アロケート済みマップレジスタ数以下であることを確認。
        if (pages > FdoData->AllocatedMapRegisters) {
            DebugPrint(ERROR, DBG_INIT, "Not enough map registers: Allocated %d, Required %d\n",
                                            FdoData->AllocatedMapRegisters, pages);
            status = STATUS_INSUFFICIENT_RESOURCES;
            return status;
        }
    }

    {
        //転送方向を取得。
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
    
        //リード／ライトIRPをセット。
        FdoData->ReadWriteIrp = Irp;
        //IRPをペンディング。
        IoMarkIrpPending(Irp);
    
        //バッファをフラッシュ。
        KeFlushIoBuffers(Mdl, !writeToDevice, TRUE);
    
        //DMAアダプタに登録されているGetScatterGatherList関数をコール。
        //スキャッタギャザリストが生成され、第6引数で渡した
        //HwInitiateScatterGatherBusmaster()が実行される。
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
    //GetScatterGatherListと同じコンテキスト。失敗できない。

    PFDO_DATA fdoData = (PFDO_DATA) DeviceObject->DeviceExtension;
    BOOLEAN   writeToDevice = FALSE;
    if (Context) {
        writeToDevice = TRUE;
    }

    //スキャッタギャザリストへのポインタを保存。
    fdoData->ScatterGather              = ScatterGather;
    //転送用方向を保存。
    fdoData->ScatterGatherWriteToDevice = writeToDevice;
    //転送進捗カウンタをリセット。
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

    //スキャッタギャザリストから、転送を実行する物理ページ情報を取得。
    PSCATTER_GATHER_ELEMENT elm = &scatterGather->Elements[ progress ];

    //転送方向、物理アドレス、サイズを引数に渡して、転送をキック。
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

    //バスマスタ制御レジスタをセットし、（１物理ページの）転送を開始。
    if(Length % 4) {
        ASSERTMSG("HwKickBusMaster : Length % 4", FALSE);
	}

    //Kick Bus Master write/read. On Completion, see isrdcp.c.
   // FdoData->CSRAddress->BusMasterAccessAddress = (ULONG)Address.LowPart;
   // FdoData->CSRAddress->BusMasterAccessCount = Length / 4;
  //  FdoData->CSRAddress->BusMasterControlStatus = command;
}

