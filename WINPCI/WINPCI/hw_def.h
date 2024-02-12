#ifndef _HW_DEF_H
#define _HW_DEF_H

//-------------------------------------------------------------------------
// Bit Mask definitions
//-------------------------------------------------------------------------
#define BIT_0       0x0001
#define BIT_1       0x0002
#define BIT_30      0x40000000
#define BIT_31      0x80000000

//-------------------------------------------------------------------------
// Control/Status Registers (CSR)
//-------------------------------------------------------------------------
#pragma pack(1)

typedef struct _CTRL_REGS_IN_MEM {
    ULONG        InterruptStatus;        // register
    ULONG        InterruptEnable;        // register

    UCHAR        NotImplemented1[0x38];

    ULONG        LedState;               // register
    ULONG        DipSwitchState;         // register

    UCHAR        NotImplemented2[0x38];

    ULONG        BusMasterControlStatus; // register
    ULONG        BusMasterAccessAddress; // register
    ULONG        BusMasterAccessCount;   // register
    ULONG        BusMasterWriteData;     // register
    ULONG        BusMasterReadData;      // register
} CTRL_REGS_IN_MEM, *PCTRL_REGS_IN_MEM;

#pragma pack()

// Interrupt fields (assuming byte addressing)
#define BIT_INT_SWITCH          BIT_0            // Mask interrupts
#define BIT_INT_BUS_MASTER      BIT_31           // Mask interrupts
#define BIT_INT_ALL            (BIT_INT_SWITCH | BIT_INT_BUS_MASTER) // Mask interrupts

#define BIT_BUSMASTER_START       BIT_0  // Mask interrupts
#define BIT_BUSMASTER_BUSY        BIT_0  // Mask interrupts
#define BIT_BUSMASTER_WRITE       BIT_1  // Mask interrupts
#define BIT_BUSMASTER_TARGETABORT BIT_30  // Mask interrupts
#define BIT_BUSMASTER_MASTERABORT BIT_31  // Mask interrupts
#define BIT_BUSMASTER_START_READ   BIT_BUSMASTER_START  // Mask interrupts
#define BIT_BUSMASTER_START_WRITE  (BIT_BUSMASTER_START | BIT_BUSMASTER_WRITE)  // Mask interrupts
#define BIT_BUSMASTER_ABORT_STATUS (BIT_BUSMASTER_TARGETABORT | BIT_BUSMASTER_MASTERABORT)  // Mask interrupts


// PCI Device and vendor IDs
#define HW_PCI_DEVICE_ID               0x21F1
#define HW_PCI_VENDOR_ID               0x14A4

 // IO space length
#define HW_MAP_IOSPACE_LENGTH          sizeof(CTRL_REGS_IN_MEM)

// PCS config space including the Device Specific part of it/
#define HW_PCI_CONF_REG_LENGTH         0x40

#define HW_INTERRUPT_DISABLED(_adapter) \
   (!(_adapter->CSRAddress->InterruptEnable & BIT_INT_ALL))

#define HW_INTERRUPT_ACTIVE(_adapter) \
   (_adapter->CSRAddress->InterruptStatus & BIT_INT_ALL)

#define HW_GET_INTERRUPT_STATUS(_adapter) \
   (_adapter->CSRAddress->InterruptStatus & BIT_INT_ALL)

#define HW_ACK_INTERRUPT(_adapter, _value) { \
   _adapter->CSRAddress->InterruptStatus = (_value & BIT_INT_ALL); }


typedef struct _FDO_DATA FDO_DATA, *PFDO_DATA;

//hw_init.c
NTSTATUS
HwInitializeDeviceExtension(
    __in PFDO_DATA FdoData
    );

NTSTATUS
HwGetDeviceInformation(
    __in PFDO_DATA FdoData
);

NTSTATUS
HwAllocateDeviceResources(
    __in PFDO_DATA FdoData,
    __in PIRP Irp
    );

NTSTATUS
HwMapHWResources(
    __in PFDO_DATA FdoData,
    __in PIRP Irp
    );

NTSTATUS
HwUnmapHWResources(
    __in PFDO_DATA FdoData
    );

NTSTATUS
HwFreeDeviceResources(
    __in PFDO_DATA FdoData
    );

NTSTATUS
ReadWriteConfigSpace(
    IN PDEVICE_OBJECT DeviceObject,
    IN ULONG	      ReadOrWrite, // 0 for read 1 for write
    IN PVOID	      Buffer,
    IN ULONG	      Offset,
    IN ULONG	      Length
);

NTSTATUS
GetPCIBusInterfaceStandard(
    __in  PDEVICE_OBJECT DeviceObject,
    __out PBUS_INTERFACE_STANDARD    BusInterfaceStandard
    );

VOID
HwShutdown(
    __in  PFDO_DATA  FdoData
    );

NTSTATUS
HwSetPower(
	PFDO_DATA          FdoData ,
	DEVICE_POWER_STATE PowerState
	);


//hw_reqt.c
NTSTATUS
HwReadWriteRegister (
    __in PFDO_DATA FdoData,
    __in PIRP      Irp
	);

NTSTATUS
HwStartBusMasterWriteRead (
    __in  PFDO_DATA FdoData,
    __in  PIRP      Irp,
    __in  PMDL      Mdl
    );

#if !defined(__USE_WDK_6001__)

DRIVER_LIST_CONTROL HwInitiateScatterGatherBusmaster;

#else
VOID
HwInitiateScatterGatherBusmaster(
    __in struct _DEVICE_OBJECT  *DeviceObject,
    __in struct _IRP  *Irp,
    __in PSCATTER_GATHER_LIST  ScatterGather,
    __in PVOID  Context
    );

#endif

VOID
HwKickBusMasterReadWrite (
    __in PFDO_DATA FdoData
);

VOID
HwKickBusMaster (
    __in PFDO_DATA        FdoData,
    __in BOOLEAN          WriteToDevice,
    __in PHYSICAL_ADDRESS Address,
    __in ULONG            Length
);

//isrdpc.c
KSERVICE_ROUTINE HwInterruptHandler;
IO_DPC_ROUTINE HwDpcForIsr;


typedef
ULONG
(*PREAD_PORT)(
    __in ULONG *Register
    );

typedef
VOID
(*PWRITE_PORT)(
    __in ULONG *Register,
    __in ULONG  Value
    );


#endif


