
/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    PciDrv.h

Abstract:

    Header file for the driver modules.

Environment:

    Kernel mode

--*/


#if !defined(_PCIDRV_H_)
#define _PCIDRV_H_


//
// Let us use newly introduced (.NET DDK) safe string function to avoid
// security issues related buffer overrun.
// The advantages of the RtlStrsafe functions include:
// 1) The size of the destination buffer is always provided to the
// function to ensure that the function does not write past the end of
// the buffer.
// 2) Buffers are guaranteed to be null-terminated, even if the
// operation truncates the intended result.
//

//
// In this driver we are using a safe version vsnprintf, which is
// RtlStringCbVPrintfA. To use strsafe function on 9x, ME, and Win2K Oses, we
// have to define NTSTRSAFE_LIB before including this header file and explicitly
// link to ntstrsafe.lib. If your driver is just target for XP and above, there is
// no define NTSTRSAFE_LIB and link to the lib.
//
//#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <dontuse.h>
#include <nvme.h>

//
// InterlockedOr is not defined in Win2K header files.
// If not defined, we will define it to use intrinsic function.
//
#if !defined(InterlockedOr) && (_WIN32_WINNT==0x0500)
#define InterlockedOr _InterlockedOr
#endif

#define PCIDRV_POOL_TAG (ULONG) 'DICP'
#define PCIDRV_FDO_INSTANCE_SIGNATURE (ULONG) 'odFT'

//
// Bit Flag Macros
//

#define SET_FLAG(Flags, Bit)    ((Flags) |= (Bit))
#define CLEAR_FLAG(Flags, Bit)  ((Flags) &= ~(Bit))
#define TEST_FLAG(Flags, Bit)   (((Flags) & (Bit)) != 0)

typedef struct _GLOBALS {

    //
    // Path to the driver's Services Key in the registry
    //

    UNICODE_STRING RegistryPath;

} GLOBALS;

extern GLOBALS Globals;

//
// These are the states FDO transition to upon
// receiving a specific PnP Irp. Refer to the PnP Device States
// diagram in DDK documentation for better understanding.
//

typedef enum _DEVICE_PNP_STATE {

    NotStarted = 0,         // Not started yet
    Started,                // Device has received the START_DEVICE IRP
    StopPending,            // Device has received the QUERY_STOP IRP
    Stopped,                // Device has received the STOP_DEVICE IRP
    RemovePending,          // Device has received the QUERY_REMOVE IRP
    SurpriseRemovePending,  // Device has received the SURPRISE_REMOVE IRP
    Deleted                 // Device has received the REMOVE_DEVICE IRP

} DEVICE_PNP_STATE;

//
// This enum used in implementing the wait-lock
//
typedef enum _WAIT_REASON {
    REMOVE = 0,
    STOP = 1
} WAIT_REASON;

#define INITIALIZE_PNP_STATE(_Data_)    \
        (_Data_)->DevicePnPState =  NotStarted;\
        (_Data_)->PreviousPnPState = NotStarted;

#define SET_NEW_PNP_STATE(_Data_, _state_) \
        (_Data_)->PreviousPnPState =  (_Data_)->DevicePnPState;\
        (_Data_)->DevicePnPState = (_state_);

#define RESTORE_PREVIOUS_PNP_STATE(_Data_)   \
        (_Data_)->DevicePnPState =   (_Data_)->PreviousPnPState;\

typedef enum _QUEUE_STATE {

    HoldRequests = 0,        // Device is not started yet, temporarily
                            // stopped for resource rebalancing, or
                            // entering a sleep state.
    AllowRequests,         // Device is ready to process pending requests
                            // and take in new requests.
    FailRequests             // Fail both existing and queued up requests.

} QUEUE_STATE;

//
// General purpose workitem context used in dispatching work to
// system worker thread to be executed at PASSIVE_LEVEL.
//
typedef struct _WORKER_ITEM_CONTEXT {
    PIO_WORKITEM   WorkItem;
    PVOID          Callback; // Callback pointer
    PVOID          Argument1;
    PVOID          Argument2;
} WORKER_ITEM_CONTEXT, *PWORKER_ITEM_CONTEXT;



//
// The device extension for the device object
//
typedef struct _FDO_DATA
{
    ULONG                   Signature; // must be PCIDRV_FDO_INSTANCE_SIGNATURE
    PDEVICE_OBJECT          Self; // a back pointer to the DeviceObject.
    PDEVICE_OBJECT          UnderlyingPDO;// The underlying PDO
    PDEVICE_OBJECT          NextLowerDriver; // The top of the device stack just
                                         // beneath this device object.
    DEVICE_PNP_STATE        DevicePnPState;   // Track the state of the device
    DEVICE_PNP_STATE        PreviousPnPState; // Remembers the previous pnp state
    UNICODE_STRING          InterfaceName; // The name returned from
                                       // IoRegisterDeviceInterface,
    QUEUE_STATE             QueueState;      // This flag is set whenever the
                                         // device needs to queue incoming
                                         // requests (when it receives a
                                         // QUERY_STOP or QUERY_REMOVE).
    LIST_ENTRY              NewRequestsQueue; // The queue where the incoming
                                          // requests are held when
                                          // QueueState is set to HoldRequest,
                                          // the device is busy or sleeping.
    KSPIN_LOCK              QueueLock;        // The spin lock that protects
                                          // the queue
    KEVENT                  RemoveEvent; // an event to sync outstandingIO to zero.
    KEVENT                  StopEvent;  // an event to sync outstandingIO to 1.
    ULONG                   OutstandingIO; // 1-biased count of reasons why
                                       // this object should stick around.
    DEVICE_CAPABILITIES     DeviceCaps;   // Copy of the device capability
                                       // Used to find S to D mappings

    // Power Management
    SYSTEM_POWER_STATE      SystemPowerState;   // Current power state of the system (S0-S5)
    DEVICE_POWER_STATE      DevicePowerState; // Current power state of the device(D0 - D3)
    PIRP                    PendingSIrp; // S-IRP is saved here before generating
                                      // correspoding D-IRP
    PVOID                   PowerCodeLockHandle;


    // Following fields are specific to the hardware
    // IDs
    UCHAR                   RevsionID;                  //PCIデバイスのリビジョンID
    USHORT                  SubVendorID;                //PCIデバイスのサブベンダID
    USHORT                  SubSystemID;                //PCIデバイスのシステムID

    // HW Resources
    BUS_INTERFACE_STANDARD  BusInterface;               //バスインターフェース情報を格納するオブジェクト
    //PHYSICAL_ADDRESS        MemPhysAddress;             //メモリ空間物理ベースアドレス
   // PCTRL_REGS_IN_MEM       CSRAddress;                 //メモリ空間（コントロールステータスレジスタ）ベースアドレス
    
    PNVME_CONTROLLER_REGISTERS   controller_regs;
    PVOID buf_va;    // DMA buffer


    //PULONG                  IoBaseAddress;              //IO空間ベースアドレス
    ULONG                   IoRange;                    //IO空間サイズ
    BOOLEAN                 MappedPorts;                //IOポートがメモリマップされたことを示すフラグ
    PREAD_PORT              ReadPort;                   //IOポートのリードアクセス用関数をセットする関数ポインタ
    PWRITE_PORT             WritePort;                  //IOポートのライトアクセス用関数をセットする関数ポインタ

    PKINTERRUPT             Interrupt;                  //割込みオブジェクト
    UCHAR                   InterruptLevel;             //割込みレベル
    ULONG                   InterruptVector;            //割込みベクタ
    ULONG                   InterruptMode;              //割込みモード
    KAFFINITY               InterruptAffinity;          //割込みアフィニティ

    // spin locks for protecting misc variables
    KSPIN_LOCK              Lock;						//アクセス排他用スピンロック

    // For handling ReadWrite
    PIRP                    ReadWriteIrp;				//ペンディング中リードライトIRPへのポインタ

      //バスマスタ転送に使用するDMAリソース。HwMapHwResources()にてアロケートされます。
    PDMA_ADAPTER            DmaAdapterObject;			//DMAアダプタオブジェクトへのポインタ
    ULONG                   AllocatedMapRegisters;		//マップレジスタ数
      //バスマスタ転送制御用。
    PSCATTER_GATHER_LIST    ScatterGather;				//スキャッタギャザリストへのポインタ
    BOOLEAN                 ScatterGatherWriteToDevice;	//方向フラグ
    ULONG                   ScatterGatherProgress;		//転送進捗カウンタ
    BOOLEAN                 BusMasterDone;				//転送完了割込みフラグ

    // For handling PushSwitch Notify
    PIRP                    PushSwitchNotifyIrp;		//ペンディング中プッシュスイッチ割込み通知IRPへのポインタ
    ULONG                   SwitchCount;				//プッシュスイッチ割込みフラグ

    // For Saving and Restoring Led State at power state transiton  
	ULONG                   LedSaved;

}  FDO_DATA, *PFDO_DATA;

#define CLRMASK(x, mask)     ((x) &= ~(mask));
#define SETMASK(x, mask)     ((x) |=  (mask));


//
// Function prototypes
//
//
// NOTE: When you are using function types, you don't need to annotate the
// function definition to match the annotations on the function type.
// 
DRIVER_INITIALIZE DriverEntry;

DRIVER_ADD_DEVICE PciDrvAddDevice;

__drv_dispatchType(IRP_MJ_PNP)
DRIVER_DISPATCH PciDrvDispatchPnp;

__drv_dispatchType(IRP_MJ_POWER)
DRIVER_DISPATCH PciDrvDispatchPower;

__drv_dispatchType(IRP_MJ_DEVICE_CONTROL)
__drv_dispatchType(IRP_MJ_READ)
__drv_dispatchType(IRP_MJ_WRITE)
DRIVER_DISPATCH PciDrvDispatchIO;

__drv_dispatchType(IRP_MJ_CREATE)
DRIVER_DISPATCH PciDrvCreate;

__drv_dispatchType(IRP_MJ_CLOSE)
DRIVER_DISPATCH PciDrvClose;

__drv_dispatchType(IRP_MJ_CLEANUP)
DRIVER_DISPATCH PciDrvCleanup;

__drv_dispatchType(IRP_MJ_SYSTEM_CONTROL)
DRIVER_DISPATCH PciDrvSystemControl;

DRIVER_DISPATCH PciDrvSendIrpSynchronously;

DRIVER_DISPATCH PciDrvCanStopDevice;

DRIVER_DISPATCH PciDrvCanRemoveDevice;

DRIVER_DISPATCH PciDrvDispatchWaitWake;

DRIVER_UNLOAD PciDrvUnload;

DRIVER_CANCEL PciDrvCancelRoutineForReadIrp;

DRIVER_CANCEL PciDrvCancelRoutineForIoctlIrp;

DRIVER_CANCEL PciDrvCancelRoutine;

IO_COMPLETION_ROUTINE PciDrvDispatchPnpStartComplete;

IO_COMPLETION_ROUTINE PciDrvDispatchPnpComplete;

IO_COMPLETION_ROUTINE PciDrvWaitWakeIoCompletionRoutine;

IO_WORKITEM_ROUTINE PciDrvPassiveLevelClearWaitWakeEnableState;

IO_WORKITEM_ROUTINE PciDrvPassiveLevelReArmCallbackWorker;

IO_WORKITEM_ROUTINE PciDrvPowerUpDeviceCallback;

IO_WORKITEM_ROUTINE PciDrvPowerDownDeviceCallback;

IO_WORKITEM_ROUTINE PciDrvStartDeviceWorker;

NTSTATUS
PciDrvDispatchIoctl(
    __in  PFDO_DATA       FdoData,
    __in  PIRP            Irp
    );

VOID
PciDrvWithdrawIoctlIrps(
    PFDO_DATA FdoData
    );

VOID
PciDrvWithdrawReadIrps(
    PFDO_DATA FdoData
    );

VOID
PciDrvWithdrawIrps(
    PFDO_DATA FdoData
    );

NTSTATUS
PciDrvReturnResources (
    __in PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
PciDrvQueueRequest    (
    __in PFDO_DATA FdoData,
    __in PIRP Irp
    );


VOID
PciDrvProcessQueuedRequests    (
    __in PFDO_DATA FdoData
    );


NTSTATUS
PciDrvStartDevice (
    __in PFDO_DATA     FdoData,
    __in PIRP             Irp
    );


VOID
PciDrvCancelQueuedReadWriteIrps(
    PFDO_DATA FdoData
    );

VOID
PciDrvCancelQueuedIoctlIrps(
    __in PFDO_DATA FdoData
    );

NTSTATUS
PciDrvQueueIoctlIrp(
    __in  PFDO_DATA               FdoData,
    __in  PIRP                    Irp
    );

NTSTATUS
PciDrvGetDeviceCapabilities(
    __in  PDEVICE_OBJECT          DeviceObject,
    __in  PDEVICE_CAPABILITIES    DeviceCapabilities
    );

BOOLEAN
PciDrvReadRegistryValue(
    __in  PFDO_DATA   FdoData,
    __in  PWSTR       Name,
    __out PULONG      Value
    );

BOOLEAN
PciDrvWriteRegistryValue(
    __in PFDO_DATA  FdoData,
    __in PWSTR      Name,
    __in ULONG      Value
    );


NTSTATUS
PciDrvRead (
    __in  PFDO_DATA       FdoData,
    __in  PIRP            Irp
    );


NTSTATUS
PciDrvWrite(
    __in  PFDO_DATA       FdoData,
    __in  PIRP            Irp
);

NTSTATUS
PciDrvReadWrite (
    __in  PFDO_DATA FdoData,
    __in PIRP       Irp
    );

LONG
PciDrvIoIncrement    (
    __in PFDO_DATA   FdoData
    );


LONG
PciDrvIoDecrement    (
    __in PFDO_DATA   FdoData
    );

VOID
PciDrvReleaseAndWait(
    __in  PFDO_DATA   FdoData,
    __in  ULONG           ReleaseCount,
    __in  WAIT_REASON     Reason
    );

ULONG
PciDrvGetOutStandingIoCount(
    __in PFDO_DATA FdoData
    );

PCHAR
PnPMinorFunctionString (
    UCHAR MinorFunction
);

NTSTATUS
PciDrvForwardAndForget(
    __in PFDO_DATA     FdoData,
    __in PIRP          Irp
    );

#if !defined(__USE_WDK_6001__)

WMI_SET_DATABLOCK_CALLBACK PciDrvSetWmiDataBlock;

WMI_QUERY_DATABLOCK_CALLBACK PciDrvQueryWmiDataBlock;

WMI_QUERY_REGINFO_CALLBACK PciDrvQueryWmiRegInfo;

#else

NTSTATUS
PciDrvSetWmiDataBlock (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG InstanceIndex,
    IN ULONG BufferSize,
    IN PUCHAR Buffer
    );

NTSTATUS
PciDrvQueryWmiDataBlock (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN ULONG GuidIndex,
    IN ULONG InstanceIndex,
    IN ULONG InstanceCount,
    IN OUT PULONG InstanceLengthArray,
    IN ULONG BufferAvail,
    OUT PUCHAR Buffer
    );

NTSTATUS
PciDrvQueryWmiRegInfo (
    IN PDEVICE_OBJECT DeviceObject,
    OUT PULONG RegFlags,
    OUT PUNICODE_STRING InstanceName,
    OUT PUNICODE_STRING *RegistryPath,
    OUT PUNICODE_STRING MofResourceName,
    OUT PDEVICE_OBJECT *Pdo
    );

#endif

NTSTATUS
PciDrvWmiRegistration(
    __in PFDO_DATA               FdoData
);

NTSTATUS
PciDrvWmiDeRegistration(
    __in PFDO_DATA               FdoData
);

NTSTATUS
PciDrvSetWaitWakeEnableState(
    __in PFDO_DATA FdoData,
    __in BOOLEAN WakeState
    );

BOOLEAN
PciDrvGetWaitWakeEnableState(
    __in PFDO_DATA   FdoData
    );

VOID
PciDrvAdjustCapabilities(
    __in PDEVICE_CAPABILITIES DeviceCapabilities
    );


BOOLEAN
PciDrvCanWakeUpDevice(
    __in PFDO_DATA FdoData,
    __in SYSTEM_POWER_STATE PowerState
    );

BOOLEAN
PciDrvArmForWake(
    __in  PFDO_DATA   FdoData,
    __in  BOOLEAN     DeviceStateChange
    );

VOID
PciDrvDisarmWake(
    __in  PFDO_DATA   FdoData,
    __in  BOOLEAN     DeviceStateChange
    );

#if !defined(__USE_WDK_6001__)

REQUEST_POWER_COMPLETE PciDrvWaitWakePoCompletionRoutine;

#else

VOID
PciDrvWaitWakePoCompletionRoutine (
    __in PDEVICE_OBJECT DeviceObject,
    __in UCHAR MinorFunction,
    __in POWER_STATE PowerState,
    __in_opt PVOID Context,
    __in PIO_STATUS_BLOCK IoStatus
    );

#endif

NTSTATUS
PciDrvQueuePassiveLevelCallback(
    __in PFDO_DATA    FdoData,
    __in PIO_WORKITEM_ROUTINE CallbackFunction,
    __in_opt PVOID    Context1,
    __in_opt PVOID    Context2
    );

VOID
PciDrvRegisterForIdleDetection(
    __in PFDO_DATA   FdoData,
    __in BOOLEAN      DeviceStateChange
    );

VOID
PciDrvDeregisterIdleDetection(
    __in PFDO_DATA   FdoData,
    __in BOOLEAN      DeviceStateChange
    );

NTSTATUS
PciDrvSetPowerSaveEnableState(
    __in PFDO_DATA FdoData,
    __in BOOLEAN State
    );

BOOLEAN
PciDrvGetPowerSaveEnableState(
    __in PFDO_DATA   FdoData
    );

NTSTATUS
PciDrvPowerUpDevice(
    __in PFDO_DATA FdoData,
    __in BOOLEAN   Wait
    );

VOID
PciDrvSetIdleTimer(
    __in PFDO_DATA FdoData
    );

#define PciDrvSetDeviceBusy(FdoData) \
                            FdoData->IsDeviceIdle = FALSE;

#if !defined(__USE_WDK_6001__)

KDEFERRED_ROUTINE PciDrvIdleDetectionTimerDpc;

#else

VOID
PciDrvIdleDetectionTimerDpc (
    IN struct _KDPC *Dpc,
    IN PVOID DeferredContext,
    IN PVOID SystemArgument1,
    IN PVOID SystemArgument2
    );

#endif

VOID
PciDrvReStartIdleDetectionTimer(
    __in PFDO_DATA FdoData
    );


VOID
PciDrvCancelIdleDetectionTimer(
    __in PFDO_DATA FdoData
    );

#if !defined(__USE_WDK_6001__)

CALLBACK_FUNCTION PciDrvPowerStateCallback;

#else

VOID
PciDrvPowerStateCallback (
    __in_opt PVOID CallbackContext,
    __in_opt PVOID Argument1,
    __in_opt PVOID Argument2
    );

#endif

VOID
PciDrvRegisterPowerStateNotification(
    __in PFDO_DATA   FdoData
    );

VOID
PciDrvUnregisterPowerStateNotification(
    __in PFDO_DATA   FdoData
    );

#if defined(WIN2K)

NTKERNELAPI
VOID
ExFreePoolWithTag(
    __in PVOID P,
    __in ULONG Tag
    );
//
// This value should be returned from completion routines to continue
// completing the IRP upwards. Otherwise, STATUS_MORE_PROCESSING_REQUIRED
// should be returned.
//
#define STATUS_CONTINUE_COMPLETION      STATUS_SUCCESS


#endif

#endif  // _PCIDRV_H_



