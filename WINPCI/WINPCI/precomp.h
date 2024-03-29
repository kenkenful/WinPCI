//
//
// Disable warnings that prevent our driver from compiling with /W4 MSC_WARNING_LEVEL
//

// Disable warning C4214: nonstandard extension used : bit field types other than int
// Disable warning C4201: nonstandard extension used : nameless struct/union
// Disable warning C4115: named type definition in parentheses
//
#pragma warning(disable:4214 4201 4115 )

//#define __USE_WDK_6001__

#include <ntddk.h> 


typedef unsigned int        UINT;
typedef unsigned int        *PUINT;

#include <initguid.h> // required for GUID definitions
#include <wdmguid.h> // required for WMILIB_CONTEXT
#include <wmistr.h>
#include <wmilib.h>
#include <ntintsafe.h>

#include "public.h"   // Stuff that should be exposed to app goes here.
#include "trace.h" // required for tracing support
#include "hw_def.h" // all the NIC specific definitions go here
#include "pcidrv.h"  // Has all the generic function prototypes and FDO_DATA definition
#include "macros.h"  // Hardware specific macros are defined here.
#include <dontuse.h>

// Disable warning C4127: conditional expression is constant
// Disable warning C4057; X differs in indirection to slightly different base types from Y 
// Disable warning C4100: unreferenced formal parameter
#pragma warning(disable:4127 4057 4100)


