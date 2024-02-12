/*++
    Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

//
// Define an Interface Guid for toaster device class.
// This GUID is used to register (IoRegisterDeviceInterface)
// an instance of an interface so that user application
// can control the toaster device.
//

DEFINE_GUID(GUID_DEVINTERFACE_PCIDRV, 
	0x1996623c, 0x2705, 0x4ced, 0xb6, 0x94, 0x46, 0xd9, 0xbb, 0x3a, 0xf7, 0x87);


#ifndef __PUBLIC_H
#define __PUBLIC_H

//IO�R���g���[���R�[�h��`�B
#define FILE_DEVICE_PCI         FILE_DEVICE_UNKNOWN

//LED�ADipSwitch�A�N�Z�X�i�������j
#define IOCTL_READ_LED_BY_MEM     \
    CTL_CODE (FILE_DEVICE_PCI, 0x0 , METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_WRITE_LED_BY_MEM     \
    CTL_CODE (FILE_DEVICE_PCI, 0x1 , METHOD_BUFFERED, FILE_WRITE_ACCESS)

#define IOCTL_READ_DIPSWITCH_BY_MEM     \
    CTL_CODE (FILE_DEVICE_PCI, 0x2 , METHOD_BUFFERED, FILE_READ_ACCESS)

//LED�ADipSwitch�A�N�Z�X�iIO�j
#define IOCTL_READ_LED_BY_IO     \
    CTL_CODE (FILE_DEVICE_PCI, 0x3 , METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_WRITE_LED_BY_IO     \
    CTL_CODE (FILE_DEVICE_PCI, 0x4 , METHOD_BUFFERED, FILE_WRITE_ACCESS)

#define IOCTL_READ_DIPSWITCH_BY_IO     \
    CTL_CODE (FILE_DEVICE_PCI, 0x5 , METHOD_BUFFERED, FILE_READ_ACCESS)

//�v�b�V���X�C�b�`�����ݒʒm
#define IOCTL_WAIT_NOTIFY_SWITCH_PUSHED     \
    CTL_CODE (FILE_DEVICE_PCI, 0x6 , METHOD_BUFFERED, FILE_ANY_ACCESS)

//�o�X�}�X�^�]���̃f�[�^���W�X�^�A�N�Z�X�i�e�X�g�p�j
#define IOCTL_SET_BUS_MASTER_WRITE_DATA     \
    CTL_CODE (FILE_DEVICE_PCI, 0x7 , METHOD_BUFFERED, FILE_WRITE_ACCESS)

#define IOCTL_GET_BUS_MASTER_READ_DATA     \
    CTL_CODE (FILE_DEVICE_PCI, 0x8 , METHOD_BUFFERED, FILE_READ_ACCESS)

#endif

