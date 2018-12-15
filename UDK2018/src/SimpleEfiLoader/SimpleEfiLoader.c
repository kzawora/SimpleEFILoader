#include "CommonHeader.h"
typedef struct
{
	CHAR16 FileName[512];
	CHAR16 FilePath[512];
	CHAR16 Volume[128];
	UINT64 FileSize;
} EfiFile;

EfiFile EfiFileArray[MAX_EFI_FILE_ARRAY_SIZE];
UINT32 EfiFileArrayIndex = 0;

EFI_STATUS
EFIAPI
UefiMain(
	IN EFI_HANDLE ImageHandle,
	IN EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_HANDLE AgentHandle;
	EFI_STATUS Status;
	UINTN NumHandles;
	EFI_HANDLE *Handles;
	UINTN Index;
	VOID *Context;

	AgentHandle = ImageHandle;

	// clear console out
	ShellInitialize();

	gST->ConOut->ClearScreen(gST->ConOut);
	Print(L"==== SIMPLE EFI LOADER ====\n");

	// gets all handles with simple file system installed
	Status = gBS->LocateHandleBuffer(
		ByProtocol,
		&gEfiSimpleFileSystemProtocolGuid,
		NULL,
		&NumHandles,
		&Handles);
	if (EFI_ERROR(Status))
	{
		return Status;
	}

	// loop through all handles we just got
	for (Index = 0; Index < NumHandles; Index++)
	{
		EFI_FILE_HANDLE Root;
		EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
		EFI_DEVICE_PATH *Dp;

		// get simple file system protocol instance
		// from current handle
		Status = gBS->OpenProtocol(
			Handles[Index],
			&gEfiSimpleFileSystemProtocolGuid,
			&Fs,
			NULL,
			AgentHandle,
			EFI_OPEN_PROTOCOL_GET_PROTOCOL);
		if (EFI_ERROR(Status))
		{
			DEBUG((EFI_D_ERROR, "Missing EFI_SIMPLE_FILE_SYSTEM_PROTOCOL on handle.\n"));
			continue;
		}

		// get device path instance from current handle
		Status = gBS->OpenProtocol(
			Handles[Index],
			&gEfiDevicePathProtocolGuid,
			&Dp,
			NULL,
			AgentHandle,
			EFI_OPEN_PROTOCOL_GET_PROTOCOL);
		if (EFI_ERROR(Status))
		{
			DEBUG((EFI_D_ERROR, "Missing EFI_DEVICE_PATH_PROTOCOL on handle.\n"));
			continue;
		}

		// open root dir from current simple file system
		Status = Fs->OpenVolume(Fs, &Root);
		if (EFI_ERROR(Status))
		{
			DEBUG((EFI_D_ERROR, "Unable to open volume.\n"));
			continue;
		}

		// recursively process files in root dir
		Context = NULL;
		CHAR16 DevicePathArray[1024];
		*DevicePathArray = 0;
		StrCat(DevicePathArray, L"\\");

		Status = ProcessFilesInDir(0, DevicePathArray, Handles[Index], Root, Dp);

		Root->Close(Root);
		if (EFI_ERROR(Status))
		{
			DEBUG((EFI_D_ERROR, "ProcessFilesInDir error. Continuing with next volume...\n"));
			continue;
		}
	}

	EFI_EVENT TimerEvent;
	EFI_EVENT WaitList[2];
	EFI_INPUT_KEY Key;
	UINT32 Selected = 0;
	BOOLEAN Init = 1;
	Index = 0;

	do
	{
		Status = gBS->CreateEvent(EVT_TIMER, 0, NULL, NULL, &TimerEvent);

		//
		// Refresh immediately if it's the first iteration, otherwise set timer for 10 secs.
		//
		if (Init == 1)
		{
			Status = gBS->SetTimer(TimerEvent, TimerRelative, 1);
			Init = 0;
		}
		else
			Status = gBS->SetTimer(TimerEvent, TimerRelative, 1000000000);
		//
		// Wait for the keystroke event or the timer
		//
		WaitList[0] = gST->ConIn->WaitForKey;
		WaitList[1] = TimerEvent;

		Status = gBS->WaitForEvent(2, WaitList, &Index);
		//
		// Check for the timer expiration
		//
		if (!EFI_ERROR(Status) && Index == 1)
		{
			Status = EFI_TIMEOUT;
		}
		gBS->CloseEvent(TimerEvent);
		gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);

		// print
		gST->ConOut->ClearScreen(gST->ConOut);

		ShellPrintEx(-1, -1, L"%H==%E== %BSI%VMP%HLE%E E%BFI%V L%HOA%EDE%BR %V==%H==\n");
		ShellPrintEx(-1, -1, L"%V%d %NEFI FILES FOUND!\n\n", EfiFileArrayIndex);
		CHAR16 PressedKey = Key.UnicodeChar;
		UINT32 PressedNumber = (UINT32)(PressedKey)-48;
		if (PressedNumber >= 0 && PressedNumber < EfiFileArrayIndex)
			Selected = PressedNumber;
		if (Key.ScanCode == SCAN_UP)
			Selected = Selected == 0 ? EfiFileArrayIndex - 1 : (Selected - 1) % EfiFileArrayIndex;
		if (Key.ScanCode == SCAN_DOWN)
			Selected = (Selected + 1) % EfiFileArrayIndex;
		for (UINT32 Tmp = 0; Tmp < EfiFileArrayIndex; Tmp++)
		{
			if (Selected == Tmp)
			{
				ShellPrintEx(-1, -1, L"%EBoot option %d: %s\n", Tmp, EfiFileArray[Tmp].FileName);
				ShellPrintEx(-1, -1, L"    %HSize: %V%ld bytes\n", EfiFileArray[Tmp].FileSize);
				ShellPrintEx(-1, -1, L"    %HPath: %s\n", EfiFileArray[Tmp].FilePath);
				ShellPrintEx(-1, -1, L"    %HVolume: %s\n", EfiFileArray[Tmp].Volume);
			}
			else
			{
				ShellPrintEx(-1, -1, L"Boot option %d: %s\n", Tmp, EfiFileArray[Tmp].FileName);
			}
		}

	} while (Status == EFI_TIMEOUT || Key.UnicodeChar != CHAR_CARRIAGE_RETURN);
	ShellPrintEx(-1, -1, L"\n%H[INFO] Booting option %d...\n\n", Selected);

	//	LoadEfiImage(ImageHandle, SystemTable, EfiFileArray[Selected].FileName);
	LoadEfiImage(ImageHandle, SystemTable, EfiFileArray[Selected].FilePath);

	ShellPrintEx(-1, -1, L"\n%V[SUCCESS] UEFI Application finished successfully.\n");

	return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ProcessFilesInDir(
	IN UINTN Depth,
	IN CHAR16 *DpA,
	IN EFI_HANDLE Device,
	IN EFI_FILE_HANDLE Dir,
	IN EFI_DEVICE_PATH *DirDp)
{
	EFI_STATUS Status;
	EFI_FILE_INFO *FileInfo;
	CHAR16 *FileName;
	UINTN FileInfoSize;
	EFI_DEVICE_PATH *Dp;

	// big enough to hold EFI_FILE_INFO struct and
	// the whole file path
	FileInfo = AllocatePool(MAX_FILE_INFO_SIZE);
	if (FileInfo == NULL)
	{
		return EFI_OUT_OF_RESOURCES;
	}

	for (;;)
	{
		// get the next file's info. there's an internal position
		// that gets incremented when you read from a directory
		// so that subsequent reads gets the next file's info
		FileInfoSize = MAX_FILE_INFO_SIZE;
		Status = Dir->Read(Dir, &FileInfoSize, (VOID *)FileInfo);
		if (EFI_ERROR(Status) || FileInfoSize == 0)
		{ // this is how we	eventually exit this function when we run out of files
			if (Status == EFI_BUFFER_TOO_SMALL)
			{
				Print(L"EFI_FILE_INFO > MAX_FILE_INFO_SIZE. Increase the size\n");
			}
			FreePool(FileInfo);
			return Status;
		}

		FileName = FileInfo->FileName;
		CHAR16 NewDpA[1024];
		StrCpy(NewDpA, DpA);
		if (Depth != 0)
		{
			StrCat(NewDpA, ConvertDevicePathToText(DirDp, FALSE, FALSE));
			StrCat((CHAR16 *)NewDpA, L"\\");
		}

		// skip files named . or ..
		if (StrCmp(FileName, L".") == 0 || StrCmp(FileName, L"..") == 0)
		{
			continue;
		}

		// so we have absolute device path to child file/dir
		Dp = FileDevicePath(DirDp, FileName);
		if (Dp == NULL)
		{
			FreePool(FileInfo);
			return EFI_OUT_OF_RESOURCES;
		}

		// Do whatever processing on the file
		PerFileFunc(NewDpA, Device, Dir, DirDp, FileInfo, Dp);

		if (FileInfo->Attribute & EFI_FILE_DIRECTORY)
		{
			//
			// recurse
			//

			EFI_FILE_HANDLE NewDir;

			Status = Dir->Open(Dir, &NewDir, FileName, EFI_FILE_MODE_READ, 0);
			if (Status != EFI_SUCCESS)
			{
				FreePool(FileInfo);
				FreePool(Dp);
				return Status;
			}
			NewDir->SetPosition(NewDir, 0);
			Status = ProcessFilesInDir(Depth + 1, NewDpA, Device, NewDir, Dp);
			Dir->Close(NewDir);
			if (Status != EFI_SUCCESS)
			{
				FreePool(FileInfo);
				FreePool(Dp);
				return Status;
			}
		}

		FreePool(Dp);
	}
}

EFI_STATUS
EFIAPI PerFileFunc(
	IN CHAR16 *DpA,
	IN EFI_HANDLE Device,
	IN EFI_FILE_HANDLE Dir,
	IN EFI_DEVICE_PATH *DirDp,
	IN EFI_FILE_INFO *FileInfo,
	IN EFI_DEVICE_PATH *Dp)
{
	CHAR16 *name = FileInfo->FileName;
	UINT32 nameLen;
	for (nameLen = 0; name[nameLen] != 0; nameLen++)
		;

	// GOD PLEASE STOP THIS TRAINWRECK
	if (
		(name[nameLen - 1] == (CHAR16)'i' && name[nameLen - 2] == (CHAR16)'f' && name[nameLen - 3] == (CHAR16)'e' && name[nameLen - 4] == (CHAR16)'.') ||
		(name[nameLen - 1] == (CHAR16)'I' && name[nameLen - 2] == (CHAR16)'F' && name[nameLen - 3] == (CHAR16)'E' && name[nameLen - 4] == (CHAR16)'.'))
	{

		StrCpy(EfiFileArray[EfiFileArrayIndex].FileName, FileInfo->FileName);
		StrCpy(EfiFileArray[EfiFileArrayIndex].FilePath, DpA);
		StrCat(EfiFileArray[EfiFileArrayIndex].FilePath, FileInfo->FileName);
		EfiFileArray[EfiFileArrayIndex].FileSize = FileInfo->FileSize;
		StrCpy(EfiFileArray[EfiFileArrayIndex].Volume, ConvertDevicePathToText(FileDevicePath(Device, L""), TRUE, TRUE));
		/*
		used earlier on for debugging
		Print(L"FOUND: %s\n", FileInfo->FileName);
		ShellPrintEx(-1, -1, L"%HDevicePath = %s\n", EfiFileArray[EfiFileArrayIndex].FilePath);
		*/
		EfiFileArrayIndex++;
	}

	return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
LoadEfiImage(
	IN EFI_HANDLE ImageHandle,
	IN EFI_SYSTEM_TABLE *SystemTable,
	IN CONST CHAR16 *FileName)
{
	UINTN NumHandles;
	UINTN Index;
	EFI_HANDLE *SFS_Handles;
	EFI_HANDLE AppImageHandle = NULL;
	EFI_STATUS Status = EFI_SUCCESS;
	EFI_BLOCK_IO_PROTOCOL *BlkIo;
	EFI_DEVICE_PATH_PROTOCOL *FilePath;
	EFI_LOADED_IMAGE_PROTOCOL *ImageInfo;
	UINTN ExitDataSize;

	Status = gBS->LocateHandleBuffer(
		ByProtocol,
		&gEfiSimpleFileSystemProtocolGuid,
		NULL,
		&NumHandles,
		&SFS_Handles);

	if (Status != EFI_SUCCESS)
	{
		ShellPrintEx(-1, -1, L"%E[ERROR] Could not find handles - %r\n", Status);
		return Status;
	}

	for (Index = 0; Index < NumHandles; Index++)
	{
		Status = gBS->OpenProtocol(
			SFS_Handles[Index],
			&gEfiSimpleFileSystemProtocolGuid,
			(VOID **)&BlkIo,
			ImageHandle,
			NULL,
			EFI_OPEN_PROTOCOL_GET_PROTOCOL);

		if (Status != EFI_SUCCESS)
		{
			Print(L"Protocol is not supported - %r\n", Status);
			return Status;
		}

		FilePath = FileDevicePath(SFS_Handles[Index], FileName);
		ShellPrintEx(-1, -1, L"%H[INFO] DevicePath Pointer - %r\n", FilePath);
		ShellPrintEx(-1, -1, L"%H[INFO] DevicePath = %s\n", ConvertDevicePathToText(FilePath, TRUE, TRUE));

		Status = gBS->LoadImage(
			FALSE,
			ImageHandle,
			FilePath,
			(VOID *)NULL,
			0,
			&AppImageHandle);

		if (Status != EFI_SUCCESS)
		{
			ShellPrintEx(-1, -1, L"%E[ERROR] Could not load the image - %r\n", Status);
			continue;
		}

		ShellPrintEx(-1, -1, L"%V[SUCCESS] Loaded the image successfully!\n");
		Status = gBS->OpenProtocol(
			AppImageHandle,
			&gEfiLoadedImageProtocolGuid,
			(VOID **)&ImageInfo,
			ImageHandle,
			(VOID *)NULL,
			EFI_OPEN_PROTOCOL_GET_PROTOCOL);

		ShellPrintEx(-1, -1, L"%V[SUCCESS] ImageInfo opened\n");

		if (!EFI_ERROR(Status))
		{
			ShellPrintEx(-1, -1, L"%H[INFO] ImageSize = %d\n", ImageInfo->ImageSize);
		}

		ShellPrintEx(-1, -1, L"%V[SUCCESS] Image is starting!\n");
		Status = gBS->StartImage(AppImageHandle, &ExitDataSize, (CHAR16 **)NULL);
		if (Status != EFI_SUCCESS)
		{
			ShellPrintEx(-1, -1, L"%E[ERROR] Could not start the image - %r %x\n", Status, Status);
			ShellPrintEx(-1, -1, L"%E[ERROR] Exit data size: %d\n", ExitDataSize);
			continue;
		}
		return Status;
	}

	return Status;
}