
__inline VOID
HwDisableInterrupt(
    __in PFDO_DATA FdoData
    )
{
    UNREFERENCED_PARAMETER(FdoData);
   //FdoData->CSRAddress->InterruptEnable &= ~(BIT_INT_ALL);
}

//KSYNCHRONIZE_ROUTINE HwEnableInterrupt;
    
__inline 
BOOLEAN
HwEnableInterrupt(
    PVOID Context
    )
{
    UNREFERENCED_PARAMETER(Context);
    //PFDO_DATA FdoData = Context;
    //FdoData->CSRAddress->InterruptEnable |= BIT_INT_ALL;
    return TRUE;
}

__inline
BOOLEAN
IsPoMgmtSupported(
   __in PFDO_DATA FdoData
   )
{

    if ( FdoData->DeviceCaps.SystemWake != PowerSystemUnspecified &&
         FdoData->DeviceCaps.DeviceWake != PowerDeviceUnspecified )
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }

}

__inline
ULONG
HwReadPortULong (
    __in  ULONG * x
    )
{
    return READ_PORT_ULONG (x);
}
__inline
VOID
HwWritePortULong (
    __in  ULONG * x,
    __in  ULONG   y
    )
{
    WRITE_PORT_ULONG (x,y);
}

__inline
ULONG
HwReadRegisterULong (
    __in  ULONG * x
    )
{
    return READ_REGISTER_ULONG (x);
}

__inline
VOID
HwWriteRegisterULong (
    __in  ULONG * x,
    __in  ULONG   y
    )
{
    WRITE_REGISTER_ULONG (x,y);
}



