#ifndef COMMON_HEADER_H
#define COMMON_HEADER_H

//
// Basic UEFI Libraries
//
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>

//
// Boot and Runtime Services
//
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

//
// Shell Library
//
#include <Library/ShellLib.h>

//
// Protocols
//
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/BlockIo.h>
#include <Protocol/LoadedImage.h>

//
// defines
//
#define MAX_FILE_INFO_SIZE 1024
#define MAX_EFI_FILE_ARRAY_SIZE 1024

//
// function headers
//
EFI_STATUS
EFIAPI
LoadEfiImage(
    IN EFI_HANDLE ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable,
    IN CONST CHAR16 *FileName);

EFI_STATUS
EFIAPI
WaitForEvent(
    IN UINTN NumberOfEvents,
    IN EFI_EVENT *Event,
    OUT UINTN *Index);

EFI_STATUS
EFIAPI
ProcessFilesInDir(
    IN UINTN Depth,
    IN CHAR16 *DpA,
    IN EFI_HANDLE Device,
    IN EFI_FILE_HANDLE Dir,
    IN EFI_DEVICE_PATH *DirDp);

EFI_STATUS
EFIAPI PerFileFunc(
    IN CHAR16 *DpA,
    IN EFI_HANDLE Device,
    IN EFI_FILE_HANDLE Dir,
    IN EFI_DEVICE_PATH *DirDp,
    IN EFI_FILE_INFO *FileInfo,
    IN EFI_DEVICE_PATH *Dp);

#endif