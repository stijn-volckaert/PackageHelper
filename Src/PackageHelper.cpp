/*=============================================================================
	PackageHelper Native Actor

	Revision history:
		* Created by AnthraX
=============================================================================*/

/*-----------------------------------------------------------------------------
	Includes
-----------------------------------------------------------------------------*/
#include "PackageHelper_Private.h"

/*-----------------------------------------------------------------------------
	Implementation Definitions - Works on linux!
-----------------------------------------------------------------------------*/
IMPLEMENT_PACKAGE(PackageHelper_v13);
IMPLEMENT_CLASS(APHActor);
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) DLL_EXPORT FName PACKAGEHELPER_V13_FNAME(name)=FName(TEXT(#name));
#define AUTOGENERATE_FUNCTION(cls,num,func) IMPLEMENT_FUNCTION(cls,num,func)
#include "PackageHelper_v13Classes.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

/*-----------------------------------------------------------------------------
	Definitions
-----------------------------------------------------------------------------*/
#define Min(a,b)	(a > b) ? b : a

/*-----------------------------------------------------------------------------
	Type Definitions
-----------------------------------------------------------------------------*/
typedef unsigned char	BYTE;
typedef BYTE*			PBYTE;
typedef char			CHAR;
typedef CHAR*			PSTR;

/*-----------------------------------------------------------------------------
	Global Variables
-----------------------------------------------------------------------------*/
TArray<FString> * realServerActors;
TArray<FString> * realServerPackages;
FFileManager*	PHFileManager = GFileManager;

/*-----------------------------------------------------------------------------
	MD5Arc - Calculates the MD5 hash of an FArchive
-----------------------------------------------------------------------------*/
FString MD5Arc(FArchive* FileArchive, DWORD* FileSize)
{
	guard(MD5Arc);
	FMD5Context context;
	BYTE*		szHash			= new BYTE[16];
	BYTE*		pBuffer			= new BYTE[512];
	FString		Result			= TEXT("");
	DWORD		dwBytesToHash	= 0;

    *FileSize = 0;
	appMD5Init(&context);

	while (!FileArchive->AtEnd())
	{
		dwBytesToHash = Min(512, FileArchive->TotalSize() - FileArchive->Tell());
		FileArchive->Serialize(pBuffer, dwBytesToHash);
		*FileSize += dwBytesToHash;
		appMD5Update(&context, pBuffer, dwBytesToHash);
	}
	appMD5Final(szHash, &context);
	FileArchive->Seek(0);

	for (int i = 0; i < 16; ++i)
		Result += FString::Printf(TEXT("%02X"), szHash[i]);

	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	DecryptArc - Decrypt an FArchive using the given key
-----------------------------------------------------------------------------*/
BYTE* DecryptArc(FArchive* FileArc, PBYTE EncKey, INT EncKeyLength)
{
	guard(DecryptArc);
	INT*	rc4Key		= NULL;
	INT		tmpChar		= 0;
	INT		Pos			= 0;
	INT		i			= 0;
	INT		j			= 0;
	BYTE*	FileBuffer	= new BYTE[FileArc->TotalSize()];
	INT*	IntBuffer	= NULL;
	BYTE*	Result		= NULL;

	guard(ReadDefs);
	Pos = FileArc->Tell();
	FileArc->Seek(0);
	FileArc->Serialize(FileBuffer, FileArc->TotalSize());
	FileArc->Seek(Pos);
	unguard;

	// Check buffer
	for (i = 0; i < FileArc->TotalSize(); ++i)
	{
		if (FileBuffer[i] < 48 || FileBuffer[i] > 57)
		{
			GLog->Logf(TEXT("Definition File is corrupt."));
			delete[] FileBuffer;
			return NULL;
		}
	}

	if (FileArc->TotalSize() % 3)
	{
		GLog->Logf(TEXT("Definition File is corrupt."));
		delete[] FileBuffer;
		return NULL;
	}

	// Convert FileBuffer to IntBuffer
	IntBuffer = new INT[FileArc->TotalSize()/3];
	for (i = 0; i < FileArc->TotalSize()/3; ++i)
		IntBuffer[i] = 100*(FileBuffer[i*3]-48) + 10*(FileBuffer[i*3+1]-48) + (FileBuffer[i*3+2]-48);
	delete[] FileBuffer;

	// Key init
	rc4Key = new INT[256];
	for (i = 0; i < 256; ++i) rc4Key[i] = i;

	// Key swap
	for (i = 0; i < 256; ++i)
	{
		j			= (j + rc4Key[i] + EncKey[i % (EncKeyLength-1)]) % 256;
		tmpChar		= rc4Key[i];
		rc4Key[i]	= rc4Key[j];
		rc4Key[j]	= tmpChar;
	}

	// Encrypt
	i = 0;
	j = 0;
	Result = new BYTE[FileArc->TotalSize()/3];
	for (INT y = 0; y < FileArc->TotalSize()/3; ++y)
	{
		i = (i + 1) % 256;
		j = (j + rc4Key[i]) % 256;

		// key swap
		tmpChar		= rc4Key[i];
		rc4Key[i]	= rc4Key[j];
		rc4Key[j]	= tmpChar;

		// Calc char
		Result[y] = IntBuffer[y]^rc4Key[(rc4Key[i]+rc4Key[j]) % 256];
	}

	delete[] IntBuffer;
	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	FindDir - Find the path associated with this file extension
-----------------------------------------------------------------------------*/
const FString FindDir(const FString Filename)
{
	// try to find the file
#ifdef __LINUX_X86__
	if (PHFileManager == GFileManager)
		PHFileManager = new PHFileManagerLinux;
#endif
	
	TArray<FString> FoundFiles = PHFileManager->FindFiles(*Filename, 1, 0);

	if (FoundFiles.Num() > 0)	
		return FoundFiles(0).Left(FoundFiles(0).Caps().InStr(Filename.Caps()));	

	guard(FindDir);
	FString ext		= Filename.Mid(Filename.InStr(TEXT(".")));
	FString sys		= TEXT("");
	FString pathext = TEXT("");

	for (INT i=0; i < GSys->Paths.Num(); i++)
	{
		pathext = GSys->Paths(i).Mid(GSys->Paths(i).InStr(TEXT("*")) + 1).Caps();

		if (pathext == ext.Caps())
			return GSys->Paths(i).Left(GSys->Paths(i).InStr(TEXT("*")));
		else if (pathext == TEXT(".U"))
			sys = GSys->Paths(i).Left(GSys->Paths(i).InStr(TEXT("*")));
	}

	// Defaulted to system path
	return sys;
	unguard;
}

/*-----------------------------------------------------------------------------
	execGetPackageInfo - Retrieves the ServerActors/ServerPackages lists
	from Engine.GameEngine and stores them in the ServerActors/ServerPackages
	string arrays in TargetActor (if they exist!)
-----------------------------------------------------------------------------*/
void APHActor::execGetPackageInfo(FFrame &Stack, RESULT_DECL)
{
	guard(APHActor::execGetPackageInfo);
	P_GET_ACTOR(TActor);
	P_FINISH;

	// References which MUST be resolved first
	UGameEngine* gameEngine = NULL;
	UStrProperty* targetServerPackages = NULL;
	UStrProperty* targetServerActors = NULL;

	// Check the targetactor reference
	if (TActor == NULL)
	{
		GLog->Logf(TEXT("ERROR: TargetActor could not be found. Call PHActor.Touch(YourActor) first"));
		*(UBOOL*)Result = false;
		return;
	}

	// Find GameEngine
	for (TObjectIterator<UGameEngine> GameEngineIt; GameEngineIt; ++GameEngineIt)
	{
		gameEngine = (UGameEngine*)*GameEngineIt;
		break;
	}

	// Cancel if GameEngine is null or TargetActor is null
	if (!gameEngine)
	{
		GLog->Logf(TEXT("ERROR: Couldn't find the GameEngine. Shouldn't happen!"));
		*(UBOOL*)Result=false;
		return;
	}

	// Find the StrProperties
	for (TFieldIterator<UStrProperty> It(TActor->GetClass()); It; ++It)
	{
		if (appStricmp(*It->GetFName(),TEXT("ServerPackages")) == 0)
		{
			targetServerPackages = (UStrProperty*)*It;
		}
		else if (appStricmp(*It->GetFName(),TEXT("ServerActors")) == 0)
		{
			targetServerActors = (UStrProperty*)*It;
		}
	}

	// Cancel if the arrays weren't found
	if (!targetServerPackages || !targetServerActors)
	{
		GLog->Logf(TEXT("ERROR: Couldn't find the ServerActors/ServerPackages arrays in %s"),TActor->GetFullName());
		*(UBOOL*)Result=false;
		return;
	}

	// Get references to the ServerActors/ServerPackages arrays (v451 broke v432 UnGame.h header compatibility!!! => need relocation)
	int reloc = 0;
#ifdef __FULLVERSION
	Level->EngineVersion == TEXT("451") ? reloc = 4 : reloc = 0;
#endif
	realServerActors = (TArray<FString>*)((BYTE*)(&(gameEngine->ServerActors))+reloc);
	realServerPackages = (TArray<FString>*)((BYTE*)(&(gameEngine->ServerPackages))+reloc);

	// Copy ServerActors to the TargetActor array
	INT i;
	for (i = 0; i < targetServerActors->ArrayDim; ++i)
	{
		FString ServerActor = TEXT("");
		if (realServerActors->IsValidIndex(i))
			ServerActor = FString((*realServerActors)(i));
		targetServerActors->ImportText(*ServerActor,(BYTE*)TActor + targetServerActors->Offset + i*targetServerActors->ElementSize, 0);
	}

	// Copy ServerPackages to the gameEngine array
	for (i = 0; i < targetServerPackages->ArrayDim; ++i)
	{
		FString ServerPackage = TEXT("");
		if (realServerPackages->IsValidIndex(i))
			ServerPackage = FString((*realServerPackages)(i));
		targetServerPackages->ImportText(*ServerPackage,(BYTE*)TActor + targetServerPackages->Offset + i*targetServerPackages->ElementSize, 0);
	}

	*(UBOOL*)Result = true;

	unguard;
}

/*-----------------------------------------------------------------------------
	execSetPackageInfo - Retrieves the ServerActors/ServerPackages lists
	from the TargetActor arrays and stores them in the Engine.GameEngine arrays
-----------------------------------------------------------------------------*/
void APHActor::execSetPackageInfo(FFrame &Stack, RESULT_DECL)
{
	guard(APHActor::execSetPackageInfo);
	P_GET_ACTOR(TActor);
	P_FINISH;

	// References which MUST be resolved first
	UGameEngine* gameEngine = NULL;
	UStrProperty* targetServerPackages = NULL;
	UStrProperty* targetServerActors = NULL;

	// Check the targetactor reference
	if (TActor == NULL)
	{
		GLog->Logf(TEXT("ERROR: TargetActor could not be found. Call PHActor.Touch(YourActor) first"));
		*(UBOOL*)Result = false;
		return;
	}

	// Find GameEngine
	for (TObjectIterator<UGameEngine> GameEngineIt; GameEngineIt; ++GameEngineIt)
	{
		gameEngine = (UGameEngine*)*GameEngineIt;
		break;
	}

	// Cancel if GameEngine is null or TargetActor is null
	if (!gameEngine)
	{
		GLog->Logf(TEXT("ERROR: Couldn't find the GameEngine. Shouldn't happen!"));
		*(UBOOL*)Result=false;
		return;
	}

	// Find the StrProperties
	for (TFieldIterator<UStrProperty> It(TActor->GetClass()); It; ++It)
	{
		if (appStricmp(*It->GetFName(),TEXT("ServerPackages")) == 0)
		{
			targetServerPackages = (UStrProperty*)*It;
		}
		else if (appStricmp(*It->GetFName(),TEXT("ServerActors")) == 0)
		{
			targetServerActors = (UStrProperty*)*It;
		}
	}

	// Cancel if the arrays weren't found
	if (!targetServerPackages || !targetServerActors)
	{
		GLog->Logf(TEXT("ERROR: Couldn't find the ServerActors/ServerPackages arrays in %s"),TActor->GetFullName());
		*(UBOOL*)Result=false;
		return;
	}

	// Get references to the ServerActors/ServerPackages arrays (v451 broke v432 UnGame.h header compatibility!!! => need relocation)
	int reloc = 0;
#ifdef __FULLVERSION
	Level->EngineVersion == TEXT("451") ? reloc = 4 : reloc = 0;
#endif

	realServerActors = (TArray<FString>*)((BYTE*)(&(gameEngine->ServerActors))+reloc);
	realServerPackages = (TArray<FString>*)((BYTE*)(&(gameEngine->ServerPackages))+reloc);

	// Wipe the current ServerActors/ServerPackages Arrays in GameEngine
	realServerActors->Empty(0);
	realServerPackages->Empty(0);

	// Copy ServerActors to the gameEngine array
	int i;
	for (i = 0; i < targetServerActors->ArrayDim; ++i)
	{
		TCHAR * serverActor = new TCHAR[500];
		if (targetServerActors->ExportText(i,serverActor,(BYTE*)TActor,(BYTE*)TActor,0))
		{
			if (serverActor[0] != 0)
				new(*realServerActors) FString(serverActor);
		}
		delete serverActor;
	}

	// Copy ServerPackages to the gameEngine array
	for (i = 0; i < targetServerPackages->ArrayDim; ++i)
	{
		TCHAR * serverPackage = new TCHAR[500];
		if (targetServerPackages->ExportText(i,serverPackage,(BYTE*)TActor,(BYTE*)TActor,0))
		{
			if (serverPackage[0] != 0)
				new(*realServerPackages) FString(serverPackage);
		}
		delete serverPackage;
	}

	*(UBOOL*)Result = true;

	unguard;
}

/*-----------------------------------------------------------------------------
	execSavePackageInfo - Saves the Engine.GameEngine config
-----------------------------------------------------------------------------*/
void APHActor::execSavePackageInfo(FFrame &Stack, RESULT_DECL)
{
	guard(APHActor::execSavePackageInfo);
	P_FINISH;

	// Find the GameEngine
	UGameEngine* gameEngine = NULL;
	for (TObjectIterator<UGameEngine> It; It; ++It)
	{
		gameEngine = (UGameEngine*)*It;
		break;
	}

	// Return if needed
	if (!gameEngine)
	{
		GLog->Logf(TEXT("ERROR: Couldn't find the GameEngine. Shouldn't happen!"));
		*(UBOOL*)Result=false;
		return;
	}

	gameEngine->SaveConfig();
	*(UBOOL*)Result=true;

	unguard;
}

/*-----------------------------------------------------------------------------
	GetFileMD5 - Calculate the MD5 Hash of the given file
-----------------------------------------------------------------------------*/
FString GetMD5Hash(TLazyArray<BYTE>* Data)
{
	guard(GetMD5Hash);
	FMD5Context context;
	DWORD		dwBytesRead		= 0;
	DWORD		dwBytesTotal	= 0;
	PBYTE       szFileBuffer	= new BYTE[512];
	PBYTE       szFileHash		= new BYTE[16];
	FString     Result			= TEXT("");

	appMD5Init(&context);
	Data->Load();

	for (TLazyArray<BYTE>::TIterator It(*Data); It; ++It)
	{
		szFileBuffer[dwBytesRead++] = *It;
		if (dwBytesRead == 512)
		{
			appMD5Update(&context, szFileBuffer, dwBytesRead);
			dwBytesTotal += dwBytesRead;
			dwBytesRead = 0;
		}
	}
	if (dwBytesRead)
	{
		appMD5Update(&context, szFileBuffer, dwBytesRead);
		dwBytesTotal += dwBytesRead;
	}
	appMD5Final(szFileHash, &context);

	for(int i = 0; i < 16; ++i)
		Result += FString::Printf(TEXT("%02X"),szFileHash[i]);
	Data->Unload();

	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	execGetLoaderMD5 - Extracts a DLL from a loader package and calculates the
	MD5 for that dll
-----------------------------------------------------------------------------*/
void APHActor::execGetLoaderMD5(FFrame &Stack, RESULT_DECL)
{
	guard(APHActor::execGetLoaderMD5);
	P_GET_STR(LoaderName);
	P_FINISH;

	// Remove extension
	if (LoaderName.Right(2).Caps() == TEXT(".U"))
		LoaderName = LoaderName.Left(LoaderName.Len()-2);

	// Find or load package
	UObject* Package = LoadPackage(NULL, *LoaderName, LOAD_None);

	// Check for fail
	if (!Package)
	{
		GLog->Logf(TEXT("ERROR: Couldn't find package %s"), *LoaderName);
		*(FString*)Result = TEXT("ERROR");
		return;
	}

	// Load the DllClass (Engine.Music)
	UClass*		DllClass	= FindObjectChecked<UClass>(ANY_PACKAGE, TEXT("Music"));
	FString		Hashes		= TEXT("");

	// Look for music objects in this loader
	for (TObjectIterator<UObject> It; It; ++It)
	{
		if (It->IsIn(Package) && It->IsA(DllClass))
		{
			UMusic* DllData = CastChecked<UMusic>(*It);
			if (DllData)
				Hashes += FString::Printf(TEXT("%s:%s:::"), It->GetName(), *GetMD5Hash(&DllData->Data));		
		}
	}

	if (Hashes != TEXT(""))
	{
		*(FString*)Result = Hashes;
		return;
	}

	// Ugh! No dlls in this loader
	GLog->Logf(TEXT("ERROR: Couldn't find a dll in package %s"), *LoaderName);
	*(FString*)Result = TEXT("ERROR");

	unguard;
}

/*-----------------------------------------------------------------------------
	PHGetLinker - Get a ULinkerLoad object for the specified package
-----------------------------------------------------------------------------*/
ULinkerLoad* PHGetLinker(FString& PackageName)
{
	// UPackages don't have linkers but the objects contained within might have a linker
	for (TObjectIterator<UPackage> PackageIterator; 
		PackageIterator;
		++PackageIterator)
	{
		FString Name = PackageIterator->GetPathName();
		Name = Name.Caps();

		if (PackageName == Name)
		{
			for (TObjectIterator<UObject> It; It; ++It)
			{
				if (It->IsIn(*PackageIterator) && It->GetLinker())
				{
					return It->GetLinker();
				}
			}
		}
	}

	// No linker found => try to load
	UObject::BeginLoad();
	UPackage* InPackage = UObject::CreatePackage(NULL, *PackageName);
	ULinkerLoad* Linker = UObject::GetPackageLinker(InPackage, NULL, LOAD_None, NULL, NULL);
	UObject::EndLoad();

	return Linker;
}

/*-----------------------------------------------------------------------------
	execGetFileInfo - Calculates Filesize and MD5 hash for the given file
-----------------------------------------------------------------------------*/
void APHActor::execGetFileInfo(FFrame &Stack, RESULT_DECL)
{
	guard(APHActor::execGetFileInfo);
	P_GET_STR(FileName);
	P_FINISH;

	ULinkerLoad*	Linker		= NULL;
	FString			FullName	= TEXT("");
	FString			BaseName	= TEXT("");

	if (FileName.InStr(TEXT(".")) != -1)
		BaseName = FileName.Left(FileName.InStr(TEXT("."))).Caps();
	else
		BaseName = FileName.Caps();

#ifdef __LINUX_X86__
	if (PHFileManager == GFileManager)
		PHFileManager = new PHFileManagerLinux;
#endif

	// Try locating the packagelinker first and read from that
	// Find the UPackage
	Linker = PHGetLinker(BaseName);

	if (Linker)
		FullName = Linker->Filename;

	if (FullName.Len() == 0)
	{
		// Get file path
		FString	FilePath = FindDir(FileName);
		FullName = FilePath + FileName;
	}

	FArchive* FileArc = PHFileManager->CreateFileReader(*FullName);

	// File opened!
	if (FileArc)
	{
		DWORD	dwFileSize;
		FString	MD5 = MD5Arc(FileArc, &dwFileSize);
		*(FString*)Result = FString::Printf(TEXT("%s:::%d:::%s"), *MD5, dwFileSize, *FullName);
		FileArc->Close();
	}
	else
	{
		*(FString*)Result = TEXT("NOT FOUND");
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	execMoveDefsFile - Renames the old file to the new file
-----------------------------------------------------------------------------*/
void APHActor::execMoveDefsFile(FFrame &Stack, RESULT_DECL)
{
	guard(APHActor::execMoveDefsFile);
	P_GET_STR(OldName);
	P_GET_STR(NewName);
	P_FINISH;

	UBOOL	Success		= 0;

	if (OldName.Len() < 6 || NewName.Len() < 6
		|| OldName.Right(5).Caps() != TEXT(".DEFS")
		|| NewName.Right(5).Caps() != TEXT(".DEFS"))
	{
		GLog->Logf(TEXT("Illegal FileNames: %s - %s"), *OldName, *NewName);
		*(UBOOL*)Result = 0;
		return;
	}

	FString Path = FindDir(OldName);

	// Add Paths
	OldName = Path + OldName;
	NewName = Path + NewName;

	// Check if OldFile exists
	if (PHFileManager->FileSize(*OldName) <= 0)
	{
		GLog->Logf(TEXT("File Not Found: %s"), *OldName);
		*(UBOOL*)Result = 0;
		return;
	}

	// Check if NewFile already exists
	if (PHFileManager->FileSize(*NewName) > 0)
	{
		GLog->Logf(TEXT("File already exists: %s - Deleting..."), *NewName);
		Success = PHFileManager->Delete(*NewName, 0, 1);
		if (!Success)
		{
			GLog->Logf(TEXT("Could not delete file: %s"), *NewName);
			*(UBOOL*)Result = 0;
			return;
		}
	}

	Success = PHFileManager->Move(*NewName, *OldName, 1, 1);
	if (!Success)
		GLog->Logf(TEXT("Could not move file: %s -> %s"), *OldName, *NewName);
	//else
	//	GLog->Logf(TEXT("Moved file: %s -> %s"), *OldName, *NewName);
	*(UBOOL*)Result = Success;
	unguard;
}

/*-----------------------------------------------------------------------------
	execFileExists - Checks if a file exists
-----------------------------------------------------------------------------*/
void APHActor::execFileExists(FFrame &Stack, RESULT_DECL)
{
	guard(APHActor::execFileExists);
	P_GET_STR(FileName);
	P_FINISH;

#ifdef __LINUX_X86__
	if (PHFileManager == GFileManager)
		PHFileManager = new PHFileManagerLinux;
#endif

	FString Path = FindDir(FileName);
	FileName     = Path + FileName;

	if (PHFileManager->FileSize(*FileName) > 0)
		*(UBOOL*)Result = 1;
	else
		*(UBOOL*)Result = 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	execLoadDefsFile - Decrypts the FArchive, copies it into an FBufferReader
	and constructs a linker with the FBufferReader
-----------------------------------------------------------------------------*/
void APHActor::execLoadDefsFile(FFrame &Stack, RESULT_DECL)
{
	guard(APHActor::execLoadDefsFile);
	P_GET_STR(FileName);
	P_FINISH;

#ifdef __LINUX_X86__
	if (PHFileManager == GFileManager)
		PHFileManager = new PHFileManagerLinux;
#endif

	if (FileName.Len() < 6 || FileName.Right(5).Caps() != TEXT(".DEFS"))
	{
		GLog->Logf(TEXT("Illegal Definitions File: %s"), *FileName);
		*(FString*)Result = TEXT("ERROR");
		return;
	}

	FString OrigName  = FileName;
	FString	Path      = FindDir(FileName);
	FString TempName  = TEXT("");
	FString ClassName = TEXT("");
	FileName          = Path + FileName;
	FArchive* FileArc = NULL;
	BYTE* DecryptBuf  = NULL;
	BYTE EncKey[]     = {1, 3, 3, 7, 7, 4, 4, 1};
	INT iBufSize	  = 0;

	if (PHFileManager->FileSize(*FileName) <= 0)
	{
		GLog->Logf(TEXT("Definition file does not exist: %s"), *FileName);
		*(FString*)Result = TEXT("ERROR");
		return;
	}
	else
	{
		FileArc = PHFileManager->CreateFileReader(*FileName);

		if (!FileArc)
		{
			GLog->Logf(TEXT("Couldn't read definition file: %s"), *FileName);
			*(FString*)Result = TEXT("ERROR");
			return;
		}

		DecryptBuf = DecryptArc(FileArc, EncKey, 8);
		iBufSize = FileArc->TotalSize() / 3;

		if (DecryptBuf)
		{
			FileArc->Close();
			TempName  = FileName.Left(FileName.Caps().InStr(TEXT(".DEFS"))) + TEXT("_temp.u");
			ClassName = OrigName.Left(OrigName.Len() - 5) + TEXT("_temp.ACEDefs");
			FileArc   = PHFileManager->CreateFileWriter(*TempName);

			if (!FileArc)
			{
				GLog->Logf(TEXT("Couldn't load definitions file: %s"), *FileName);
				*(FString*)Result = TEXT("ERROR");
				return;
			}

			FileArc->Serialize(DecryptBuf, iBufSize);
			FileArc->Close();

			UObject* DefsClass = StaticLoadObject( UClass::StaticClass(), NULL, *ClassName, *TempName, LOAD_NoWarn | LOAD_Quiet, NULL);
			if (DefsClass)
				*(FString*)Result = DefsClass->GetPathName();
			else
				*(FString*)Result = TEXT("ERROR");
		}
		else
		{
			GLog->Logf(TEXT("Definition file is corrupt: %s"), *FileName);
			*(FString*)Result = TEXT("ERROR");
			return;
		}
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	execOpenBinaryLog
-----------------------------------------------------------------------------*/
void APHActor::execOpenBinaryLog(FFrame &Stack, RESULT_DECL)
{
	guard(APHActor::execOpenBinaryLog);
	P_GET_STR(FileName);
	P_FINISH;

	FString TempName = FileName + TEXT(".temp");
	FinalName = FileName;
	BinaryArc = (INT)PHFileManager->CreateFileWriter( *TempName );

	if (BinaryArc)
		*(UBOOL*)Result = 1;
	else
		*(UBOOL*)Result = 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	execCloseBinaryLog
-----------------------------------------------------------------------------*/
void APHActor::execCloseBinaryLog(FFrame &Stack, RESULT_DECL)
{
	guard(APHActor::execCloseBinaryLog);
	P_FINISH;

	UBOOL bSuccess = 0;

	if (BinaryArc)
	{
		((FArchive*)BinaryArc)->Flush();
		((FArchive*)BinaryArc)->Close();
		FString TempName = FinalName + TEXT(".temp");
		//GLog->Logf(TEXT("PackageHelper: Closing Binary Log"));
		//GLog->Logf(TEXT("PackageHelper: Old Name: %s"), *TempName);
		//GLog->Logf(TEXT("PackageHelper: New Name: %s"), *FinalName);
		bSuccess         = PHFileManager->Move( *FinalName, *TempName, 1, 1 );
		//GLog->Logf(TEXT("PackageHelper: Success: %d"), bSuccess);
	}

	*(UBOOL*)Result = bSuccess;
	unguard;
}

/*-----------------------------------------------------------------------------
	execLogBinary
-----------------------------------------------------------------------------*/
void APHActor::execLogBinary(FFrame &Stack, RESULT_DECL)
{
	guard(APHActor::execLogBinary);
	P_GET_STR(LogString);
	P_FINISH;

	if (BinaryArc)
	{
		FString SingleChar = TEXT("");
		CHAR   Temp;
		for (INT i = 0; i < LogString.Len() / 3; ++i)
		{
			SingleChar = LogString.Mid(i*3).Left(3);
			Temp       = appAtoi( *SingleChar );
			*((FArchive*)BinaryArc) << Temp;
		}

		*(UBOOL*)Result = 1;
	}
	else
	{
		*(UBOOL*)Result = 0;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	execIsInPackageMap
-----------------------------------------------------------------------------*/
void APHActor::execIsInPackageMap(FFrame& Stack, RESULT_DECL)
{
	guard(APHActor::execIsInPackageMap);
	P_GET_STR(PackageName);
	P_FINISH;

	if (XLevel && XLevel->NetDriver && XLevel->NetDriver->MasterMap)
	{
		UBOOL Found = 0;
		for (INT i = 0; i < XLevel->NetDriver->MasterMap->List.Num(); ++i)
		{
			FPackageInfo& Pkg = XLevel->NetDriver->MasterMap->List(i);
			//GLog->Logf(TEXT("Pkg: %s"), *Pkg.Linker->Filename);
			if (Pkg.Linker && Pkg.Linker->Filename.Caps().InStr(*PackageName.Caps()) != -1)
			{
				Found = 1;
				*(UBOOL*)Result = 1;
				break;
			}
		}

		if (!Found)
			*(UBOOL*)Result = 0;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	execHasEmbeddedCode
-----------------------------------------------------------------------------*/
void APHActor::execHasEmbeddedCode(FFrame& Stack, RESULT_DECL)
{
	guard(APHActor::execHasEmbeddedCode);
	P_GET_STR(MapName);
	P_FINISH;

	UPackage* MapPackage = NULL;
	ULinkerLoad* Linker  = NULL;

	for (TObjectIterator<UPackage> It; It; ++It)
	{
		if (!appStricmp(*MapName,It->GetName()))
		{
			MapPackage = *It;
			break;
		}
	}

	//if (MapPackage) GLog->Logf(TEXT("MapPackage: %s 0x%08X"), MapPackage->GetFullName(), MapPackage->GetLinker());
	if (MapPackage && !MapPackage->GetLinker())
	{
		UObject::BeginLoad();
		UPackage* InPackage = UObject::CreatePackage(NULL, MapPackage->GetName());
		Linker = UObject::GetPackageLinker(InPackage, NULL, LOAD_None, NULL, NULL);
		UObject::EndLoad();
	}
	else if (MapPackage && MapPackage->GetLinker())
	{
		Linker = MapPackage->GetLinker();
	}

	INT FunctionIndex = -1;
	if (MapPackage && Linker)
	{
		//GLog->Logf(TEXT("Found Linker"));
		for (INT i = 0; i < Linker->NameMap.Num(); ++i)
		{
			//GLog->Logf(TEXT("Name: %s"), *Linker->NameMap(i));
			if (!appStrcmp(*(Linker->NameMap(i)), TEXT("Function")))
			{
				FunctionIndex = i;
				break;
			}
		}
	}

	if (FunctionIndex > 1)
	{
		*(UBOOL*)Result = 1;
	}
	else
	{
		*(UBOOL*)Result = 0;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	execFindImports
-----------------------------------------------------------------------------*/
void APHActor::execFindImports(FFrame& Stack, RESULT_DECL)
{
	guard(APHActor::execFindImports);
	P_GET_STR(FullImport);
	P_FINISH;

	// Parse the imported function. The format should be 
	// <Some Package>.<Some Class>.<Some Function>
	FString ImportedPackage, ImportedClass, ImportedFunction;
	if (FullImport.InStr(TEXT(".")) != 0)
	{
		ImportedPackage = FullImport.Left(FullImport.InStr(TEXT(".")));
		FullImport = FullImport.Mid(FullImport.InStr(TEXT(".")) + 1);

		if (FullImport.InStr(TEXT(".")) != 0)
		{
			ImportedClass = FullImport.Left(FullImport.InStr(TEXT(".")));
			ImportedFunction = FullImport.Mid(FullImport.InStr(TEXT(".")) + 1);
		}
	}

	if (ImportedPackage.Len() == 0 ||
		ImportedClass.Len() == 0 ||
		ImportedFunction.Len() == 0)
	{
		*(FString*)Result = TEXT("ERROR");
		return;
	}

	FString Packages;

	// Look only at packages in the packagemap
	if (XLevel && 
		XLevel->NetDriver && 
		XLevel->NetDriver->MasterMap)
	{
		ULinkerLoad* Linker = NULL;
		for (INT i = 0; i < XLevel->NetDriver->MasterMap->List.Num(); ++i)
		{
			FPackageInfo& Pkg = XLevel->NetDriver->MasterMap->List(i);

			// these should all have linkers...
			if (Pkg.Linker)
			{
				for (INT j = 0; j < Pkg.Linker->ImportMap.Num(); ++j)
				{
					FObjectImport& Import = Pkg.Linker->ImportMap(j);

					// Check if the function matches
					if (appStricmp(*Import.ObjectName, *ImportedFunction) ||
						appStricmp(*Import.ClassName, TEXT("Function")) ||
						appStricmp(*Import.ClassPackage, TEXT("Core")))
						continue;

					// Skip if the function is not imported from another package
					if ((INT)Import.PackageIndex >= 0)
						continue;
					
					FObjectImport& ClassImport = Pkg.Linker->ImportMap(-(INT)Import.PackageIndex - 1);

					// Check if the class matches
					if (appStricmp(*ClassImport.ObjectName, *ImportedClass) ||
						appStricmp(*ClassImport.ClassName, TEXT("Class")) ||
						appStricmp(*ClassImport.ClassPackage, TEXT("Core")))
						continue;

					// Skip if the class was not imported from another package
					if ((INT)ClassImport.PackageIndex >= 0)
						continue;

					FObjectImport& PackageImport = Pkg.Linker->ImportMap(-(INT)ClassImport.PackageIndex - 1);

					// Check if the package matches
					if (appStricmp(*PackageImport.ObjectName, *ImportedPackage) ||
						appStricmp(*PackageImport.ClassName, TEXT("Package")) ||
						appStricmp(*PackageImport.ClassPackage, TEXT("Core")))
						continue;

					// Normalize the package name and add it to the list
					FString BaseName;
					if (Pkg.Linker->Filename.InStr(TEXT("/"), true))
						BaseName = Pkg.Linker->Filename.Mid(Pkg.Linker->Filename.InStr(TEXT("/"), true) + 1);
					if (BaseName.InStr(TEXT("\\"), true))
						BaseName = BaseName.Mid(BaseName.InStr(TEXT("\\"), true) + 1);
					Packages += BaseName + TEXT(";");

					/*
					GLog->Logf(TEXT("Found Import - In Package: %s - Import Info: %s.%s.%s"),
						*Pkg.Linker->Filename,
						*PackageImport.ObjectName,
						*ClassImport.ObjectName,
						*Import.ObjectName);
					*/
				}
			}
		}
	}

	*(FString*)Result = Packages;
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
