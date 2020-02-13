/*=============================================================================
	PackageHelper Native Actor

	Revision history:
		* Created by AnthraX
=============================================================================*/

/*-----------------------------------------------------------------------------
	Includes
-----------------------------------------------------------------------------*/
// ReSharper disable CppZeroConstantCanBeReplacedWithNullptr
// ReSharper disable CppClangTidyCppcoreguidelinesMacroUsage
// ReSharper disable CppUseAuto
// ReSharper disable CppClangTidyHicppUseAuto
// ReSharper disable CppClangTidyModernizeLoopConvert
// ReSharper disable CppClangTidyModernizeUseAuto
// ReSharper disable CppClangTidyHicppMemberInit
// ReSharper disable CppMemberFunctionMayBeConst
// ReSharper disable CppMemberFunctionMayBeStatic
#include "PackageHelper_Private.h"

/*-----------------------------------------------------------------------------
	Implementation Definitions - Works on linux!
-----------------------------------------------------------------------------*/
IMPLEMENT_PACKAGE(PackageHelper_v15);
IMPLEMENT_CLASS(APHActor);
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) DLL_EXPORT FName PACKAGEHELPER_V15_FNAME(name)=FName(TEXT(#name));
#define AUTOGENERATE_FUNCTION(cls,num,func) IMPLEMENT_FUNCTION(cls,num,func)
#include "PackageHelper_v15Classes.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

/*-----------------------------------------------------------------------------
	Definitions
-----------------------------------------------------------------------------*/
#define Min(a,b)	((a) > (b)) ? (b) : (a)

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
FFileManager*	PHFileManager = GFileManager;
TArray<FArchive*> BinaryArcs;

/*-----------------------------------------------------------------------------
	MD5Arc - Calculates the MD5 hash of an FArchive
-----------------------------------------------------------------------------*/
FString MD5Arc(FArchive* FileArchive, DWORD* FileSize)
{
	guard(MD5Arc);
	FMD5Context Context;
	BYTE		Hash[16];
	BYTE		Buffer[512];
	FString		Result		= TEXT("");
	DWORD		BytesToHash	= 0;

    *FileSize = 0;
	appMD5Init(&Context);

	while (!FileArchive->AtEnd())
	{
		BytesToHash = Min(512, FileArchive->TotalSize() - FileArchive->Tell());
		FileArchive->Serialize(Buffer, BytesToHash);
		*FileSize += BytesToHash;
		appMD5Update(&Context, Buffer, BytesToHash);
	}
	appMD5Final(Hash, &Context);
	FileArchive->Seek(0);

	for (INT i = 0; i < 16; ++i)
		Result += FString::Printf(TEXT("%02X"), Hash[i]);

	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	DecryptArc - Decrypt an FArchive using the given key
-----------------------------------------------------------------------------*/
BYTE* DecryptArc(FArchive* FileArc, const PBYTE EncKey, INT EncKeyLength)
{
	guard(DecryptArc);
	INT	TmpChar, i, j = 0;
	BYTE* FileBuffer = new BYTE[FileArc->TotalSize()];

	guard(ReadDefs);
	const INT Pos = FileArc->Tell();
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
	INT* IntBuffer = new INT[FileArc->TotalSize() / 3];
	for (i = 0; i < FileArc->TotalSize()/3; ++i)
		IntBuffer[i] = 100*(FileBuffer[i*3]-48) + 10*(FileBuffer[i*3+1]-48) + (FileBuffer[i*3+2]-48);	

	// Key init
	INT* Rc4Key = new INT[256];
	for (i = 0; i < 256; ++i) 
		Rc4Key[i] = i;

	// Key swap
	for (i = 0; i < 256; ++i)
	{
		j			= (j + Rc4Key[i] + EncKey[i % (EncKeyLength-1)]) % 256;
		TmpChar		= Rc4Key[i];
		Rc4Key[i]	= Rc4Key[j];
		Rc4Key[j]	= TmpChar;
	}

	// Encrypt
	i = 0;
	j = 0;
	BYTE* Result = new BYTE[FileArc->TotalSize() / 3];
	for (INT y = 0; y < FileArc->TotalSize()/3; ++y)
	{
		i = (i + 1) % 256;
		j = (j + Rc4Key[i]) % 256;

		// key swap
		TmpChar		= Rc4Key[i];
		Rc4Key[i]	= Rc4Key[j];
		Rc4Key[j]	= TmpChar;

		// Calc char
		Result[y] = IntBuffer[y]^Rc4Key[(Rc4Key[i]+Rc4Key[j]) % 256];
	}

	delete[] FileBuffer;
	delete[] Rc4Key;
	delete[] IntBuffer;
	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	FindDir - Find the path associated with this file extension
-----------------------------------------------------------------------------*/
FString FindDir(const FString& Filename)
{
	// try to find the file
	TArray<FString> FoundFiles = PHFileManager->FindFiles(*Filename, 1, 0);

	if (FoundFiles.Num() > 0)	
		return FoundFiles(0).Left(FoundFiles(0).Caps().InStr(Filename.Caps()));	

	guard(FindDir);
	const FString	Ext		= Filename.Mid(Filename.InStr(TEXT(".")));
	FString			Sys		= TEXT("");
	FString			PathExt = TEXT("");

	for (INT i=0; i < GSys->Paths.Num(); i++)
	{
		PathExt = GSys->Paths(i).Mid(GSys->Paths(i).InStr(TEXT("*")) + 1);

		if (PathExt == Ext)
			return GSys->Paths(i).Left(GSys->Paths(i).InStr(TEXT("*")));
		if (PathExt == TEXT(".U"))
			Sys = GSys->Paths(i).Left(GSys->Paths(i).InStr(TEXT("*")));
	}

	// Defaulted to system path
	return Sys;
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

	// Check the targetactor reference
	if (!TActor)
	{
		GLog->Logf(TEXT("ERROR: TargetActor could not be found. Call PHActor.Touch(YourActor) first"));
		*static_cast<UBOOL*>(Result) = 0;
		return;
	}

	if (!XLevel	|| !XLevel->Engine || !Cast<UGameEngine>(XLevel->Engine))
	{
		GLog->Logf(TEXT("ERROR: Couldn't find the GameEngine. Shouldn't happen!"));
		*static_cast<UBOOL*>(Result) = 0;
		return;
	}

	UGameEngine*	GameEngine						= Cast<UGameEngine>(XLevel->Engine);
	UArrayProperty* GameEngineServerPackagesProp	= reinterpret_cast<UArrayProperty*>(GameEngine->FindObjectField(TEXT("ServerPackages")));
	UArrayProperty* GameEngineServerActorsProp		= reinterpret_cast<UArrayProperty*>(GameEngine->FindObjectField(TEXT("ServerActors")));
	UStrProperty*	TargetServerPackagesProp		= reinterpret_cast<UStrProperty*>(TActor->FindObjectField(TEXT("ServerPackages")));
	UStrProperty*	TargetServerActorsProp			= reinterpret_cast<UStrProperty*>(TActor->FindObjectField(TEXT("ServerActors")));
	
	// Cancel if the arrays weren't found
	if (!TargetServerPackagesProp || !TargetServerActorsProp)
	{
		GLog->Logf(TEXT("ERROR: Couldn't find the ServerActors/ServerPackages arrays in %s"), *FObjectFullName(TActor));
		*static_cast<UBOOL*>(Result)=0;
		return;
	}

	if (!GameEngineServerPackagesProp || !GameEngineServerActorsProp)
	{
		GLog->Logf(TEXT("ERROR: Couldn't find the ServerActors/ServerPackages arrays in %s"), *FObjectFullName(GameEngine));
		*static_cast<UBOOL*>(Result) = 0;
		return;
	}

	TArray<FString>*	GameEngineServerPackages	= reinterpret_cast<TArray<FString>*>(reinterpret_cast<INT_PTR>(GameEngine) + GameEngineServerPackagesProp->Offset);
	TArray<FString>*	GameEngineServerActors		= reinterpret_cast<TArray<FString>*>(reinterpret_cast<INT_PTR>(GameEngine) + GameEngineServerActorsProp->Offset);
	FString*			TargetServerPackages		= reinterpret_cast<FString*>(reinterpret_cast<INT_PTR>(TActor) + TargetServerPackagesProp->Offset);
	FString*			TargetServerActors			= reinterpret_cast<FString*>(reinterpret_cast<INT_PTR>(TActor) + TargetServerActorsProp->Offset);
	
	for (INT i = 0; i < TargetServerPackagesProp->ArrayDim; ++i)
	{
		if (GameEngineServerPackages->IsValidIndex(i))
			TargetServerPackages[i] = GameEngineServerPackages->operator()(i);
		else
			TargetServerPackages[i] = TEXT("");
	}

	for (INT i = 0; i < TargetServerActorsProp->ArrayDim; ++i)
	{
		if (GameEngineServerActors->IsValidIndex(i))
			TargetServerActors[i] = GameEngineServerActors->operator()(i);
		else
			TargetServerActors[i] = TEXT("");
	}

	*static_cast<UBOOL*>(Result) = 1;

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

	// Check the targetactor reference
	if (!TActor)
	{
		GLog->Logf(TEXT("ERROR: TargetActor could not be found. Call PHActor.Touch(YourActor) first"));
		*static_cast<UBOOL*>(Result) = 0;
		return;
	}

	if (!XLevel	|| !XLevel->Engine || !Cast<UGameEngine>(XLevel->Engine))
	{
		GLog->Logf(TEXT("ERROR: Couldn't find the GameEngine. Shouldn't happen!"));
		*static_cast<UBOOL*>(Result) = 0;
		return;
	}

	UGameEngine*	GameEngine						= Cast<UGameEngine>(XLevel->Engine);
	UArrayProperty* GameEngineServerPackagesProp	= reinterpret_cast<UArrayProperty*>(GameEngine->FindObjectField(TEXT("ServerPackages")));
	UArrayProperty* GameEngineServerActorsProp		= reinterpret_cast<UArrayProperty*>(GameEngine->FindObjectField(TEXT("ServerActors")));
	UStrProperty*	TargetServerPackagesProp		= reinterpret_cast<UStrProperty*>(TActor->FindObjectField(TEXT("ServerPackages")));
	UStrProperty*	TargetServerActorsProp			= reinterpret_cast<UStrProperty*>(TActor->FindObjectField(TEXT("ServerActors")));
	
	// Cancel if the arrays weren't found
	if (!TargetServerPackagesProp || !TargetServerActorsProp)
	{
		GLog->Logf(TEXT("ERROR: Couldn't find the ServerActors/ServerPackages arrays in %s"), *FObjectFullName(TActor));
		*static_cast<UBOOL*>(Result)=0;
		return;
	}

	if (!GameEngineServerPackagesProp || !GameEngineServerActorsProp)
	{
		GLog->Logf(TEXT("ERROR: Couldn't find the ServerActors/ServerPackages arrays in %s"), *FObjectFullName(GameEngine));
		*static_cast<UBOOL*>(Result) = 0;
		return;
	}

	TArray<FString>*	GameEngineServerPackages	= reinterpret_cast<TArray<FString>*>(reinterpret_cast<INT_PTR>(GameEngine) + GameEngineServerPackagesProp->Offset);
	TArray<FString>*	GameEngineServerActors		= reinterpret_cast<TArray<FString>*>(reinterpret_cast<INT_PTR>(GameEngine) + GameEngineServerActorsProp->Offset);
	FString*			TargetServerPackages		= reinterpret_cast<FString*>(reinterpret_cast<INT_PTR>(TActor) + TargetServerPackagesProp->Offset);
	FString*			TargetServerActors			= reinterpret_cast<FString*>(reinterpret_cast<INT_PTR>(TActor) + TargetServerActorsProp->Offset);

	GameEngineServerPackages->Empty(0);
	GameEngineServerActors->Empty(0);
	for (INT i = 0; i < TargetServerPackagesProp->ArrayDim; ++i)
	{
		if (TargetServerPackages[i].Len() > 0)
			new(*GameEngineServerPackages) FString(TargetServerPackages[i]);
	}

	for (INT i = 0; i < TargetServerActorsProp->ArrayDim; ++i)
	{
		if (TargetServerActors[i].Len() > 0)
			new(*GameEngineServerActors) FString(TargetServerActors[i]);
	}

	*static_cast<UBOOL*>(Result) = 1;
	
	unguard;
}

/*-----------------------------------------------------------------------------
	execSavePackageInfo - Saves the Engine.GameEngine config
-----------------------------------------------------------------------------*/
void APHActor::execSavePackageInfo(FFrame &Stack, RESULT_DECL)
{
	guard(APHActor::execSavePackageInfo);
	P_FINISH;

	if (XLevel && XLevel->Engine && Cast<UGameEngine>(XLevel->Engine))
	{
		Cast<UGameEngine>(XLevel->Engine)->SaveConfig();
		*static_cast<UBOOL*>(Result) = 1;
	}
	else
	{
		*static_cast<UBOOL*>(Result) = 0;
	}	

	unguard;
}

/*-----------------------------------------------------------------------------
	GetFileMD5 - Calculate the MD5 Hash of the given file
-----------------------------------------------------------------------------*/
FString GetMD5Hash(TLazyArray<BYTE>* Data)
{
	guard(GetMD5Hash);
	FMD5Context Context;
	BYTE		FileBuffer[512];
	BYTE		FileHash[16];
	DWORD		BytesRead		= 0;
	FString     Result			= TEXT("");

	appMD5Init(&Context);
	Data->Load();

	for (TLazyArray<BYTE>::TIterator It(*Data); It; ++It)
	{
		FileBuffer[BytesRead++] = *It;
		if (BytesRead == 512)
		{
			appMD5Update(&Context, FileBuffer, BytesRead);
			BytesRead = 0;
		}
	}
	if (BytesRead)
		appMD5Update(&Context, FileBuffer, BytesRead);
	appMD5Final(FileHash, &Context);

	for (INT i = 0; i < 16; ++i)
		Result += FString::Printf(TEXT("%02X"), FileHash[i]);
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
	if (LoaderName.Right(2) == TEXT(".U"))
		LoaderName = LoaderName.Left(LoaderName.Len()-2);

	// Find or load package
	UObject* Package = LoadPackage(NULL, *LoaderName, LOAD_None);

	// Check for fail
	if (!Package)
	{
		GLog->Logf(TEXT("ERROR: Couldn't find package %s"), *LoaderName);
		*static_cast<FString*>(Result) = TEXT("ERROR");
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
		*static_cast<FString*>(Result) = Hashes;
		return;
	}

	// Ugh! No dlls in this loader
	GLog->Logf(TEXT("ERROR: Couldn't find a dll in package %s"), *LoaderName);
	*static_cast<FString*>(Result) = TEXT("ERROR");

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

	FString	FullName = TEXT("");
	FString	BaseName;

	if (FileName.InStr(TEXT(".")) != -1)
		BaseName = FileName.Left(FileName.InStr(TEXT("."))).Caps();
	else
		BaseName = FileName.Caps();

	// Try locating the packagelinker first and read from that
	// Find the UPackage
	ULinkerLoad* Linker = PHGetLinker(BaseName);

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
		DWORD FileSize;
		const FString	MD5 = MD5Arc(FileArc, &FileSize);
		*static_cast<FString*>(Result) = FString::Printf(TEXT("%s:::%d:::%s"), *MD5, FileSize, *FullName);
		FileArc->Close();
	}
	else
	{
		*static_cast<FString*>(Result) = TEXT("NOT FOUND");
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

	if (OldName.Len() < 6 
		|| NewName.Len() < 6
		|| OldName.Right(5) != TEXT(".DEFS")
		|| NewName.Right(5) != TEXT(".DEFS"))
	{
		GLog->Logf(TEXT("Illegal FileNames: %s - %s"), *OldName, *NewName);
		*static_cast<UBOOL*>(Result) = 0;
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
		*static_cast<UBOOL*>(Result) = 0;
		return;
	}

	// Check if NewFile already exists
	if (PHFileManager->FileSize(*NewName) > 0)
	{
		GLog->Logf(TEXT("File already exists: %s - Deleting..."), *NewName);
		const UBOOL Success = PHFileManager->Delete(*NewName, 0, 1);
		if (!Success)
		{
			GLog->Logf(TEXT("Could not delete file: %s"), *NewName);
			*static_cast<UBOOL*>(Result) = 0;
			return;
		}
	}

	const UBOOL Success = PHFileManager->Move(*NewName, *OldName, 1, 1);
	if (!Success)
		GLog->Logf(TEXT("Could not move file: %s -> %s"), *OldName, *NewName);

	*static_cast<UBOOL*>(Result) = Success;
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

	FString Path = FindDir(FileName);
	FileName     = Path + FileName;

	*static_cast<UBOOL*>(Result) = (PHFileManager->FileSize(*FileName) > 0) ? 1 : 0;	
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

	if (FileName.Len() < 6 || FileName.Right(5) != TEXT(".DEFS"))
	{
		GLog->Logf(TEXT("Illegal Definitions File: %s"), *FileName);
		*static_cast<FString*>(Result) = TEXT("ERROR");
		return;
	}

	const FString	OrigName	= FileName;
	FString			Path		= FindDir(FileName);
	BYTE			EncKey[]    = {1, 3, 3, 7, 7, 4, 4, 1};

	FileName = Path + FileName;
	if (PHFileManager->FileSize(*FileName) <= 0)
	{
		GLog->Logf(TEXT("Definition file does not exist: %s"), *FileName);
		*static_cast<FString*>(Result) = TEXT("ERROR");
		return;
	}
	
	FArchive* FileArc = PHFileManager->CreateFileReader(*FileName);

	if (!FileArc)
	{
		GLog->Logf(TEXT("Couldn't read definition file: %s"), *FileName);
		*static_cast<FString*>(Result) = TEXT("ERROR");
		return;
	}

	BYTE* DecryptBuf  = DecryptArc(FileArc, EncKey, 8);
	const INT BufSize = FileArc->TotalSize() / 3;

	if (DecryptBuf)
	{
		FileArc->Close();
		const FString TempName	= FileName.Left(FileName.Caps().InStr(TEXT(".DEFS"))) + TEXT("_temp.u");
		const FString ClassName = OrigName.Left(OrigName.Len() - 5) + TEXT("_temp.ACEDefs");
		FileArc   = PHFileManager->CreateFileWriter(*TempName);

		if (!FileArc)
		{
			GLog->Logf(TEXT("Couldn't load definitions file: %s"), *FileName);
			*static_cast<FString*>(Result) = TEXT("ERROR");
			return;
		}

		FileArc->Serialize(DecryptBuf, BufSize);
		FileArc->Close();

		UObject* DefsClass = StaticLoadObject( UClass::StaticClass(), NULL, *ClassName, *TempName, LOAD_NoWarn | LOAD_Quiet, NULL);
		if (DefsClass)
			*static_cast<FString*>(Result) = DefsClass->GetPathName();
		else
			*static_cast<FString*>(Result) = TEXT("ERROR");
	}
	else
	{
		GLog->Logf(TEXT("Definition file is corrupt: %s"), *FileName);
		*static_cast<FString*>(Result) = TEXT("ERROR");
		return;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	execCloseBinaryLog
-----------------------------------------------------------------------------*/
static UBOOL CloseBinaryLog(INT Index, FString& FinalName)
{
	UBOOL Success = 0;

	FArchive* BinaryArc = BinaryArcs.IsValidIndex(Index) ?
		BinaryArcs(Index) :
		nullptr;

	if (BinaryArc)
	{
		BinaryArc->Flush();
		BinaryArc->Close();
		const FString TempName = FinalName + TEXT(".temp");
		Success = PHFileManager->Move(*FinalName, *TempName, 1, 1);
	}

	return Success;
}

void APHActor::execCloseBinaryLog(FFrame& Stack, RESULT_DECL)
{
	guard(APHActor::execCloseBinaryLog);
	P_FINISH;

	const UBOOL Success = BinaryArcOpened && CloseBinaryLog(BinaryArcIndex, FinalName);

	if (Success)
		BinaryArcOpened = 0;

	*static_cast<UBOOL*>(Result) = Success;
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

	if (BinaryArcOpened)
	{
		if (CloseBinaryLog(BinaryArcIndex, FinalName))
			BinaryArcOpened = 0;
	}

	const FString TempName = FileName + TEXT(".temp");
	FinalName = FileName;
	FArchive* BinaryArc = PHFileManager->CreateFileWriter( *TempName );

	// shove this into the BinaryArcs array
	if (BinaryArc)
	{
		for (INT i = 0; i < BinaryArcs.Num(); ++i)
		{
			if (BinaryArcs(i) == nullptr)
			{
				BinaryArcs(i) = BinaryArc;
				BinaryArcIndex = i;
				BinaryArcOpened = 1;
				break;
			}
		}

		if (!BinaryArcOpened)
		{
			BinaryArcIndex = BinaryArcs.AddItem(BinaryArc);
			BinaryArcOpened = (BinaryArcIndex == -1) ? 0 : 1;
		}
	}

	*static_cast<UBOOL*>(Result) = (BinaryArc) ? 1 : 0;
	
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

	FArchive* BinaryArc = (BinaryArcOpened && BinaryArcs.IsValidIndex(BinaryArcIndex)) ?
		BinaryArcs(BinaryArcIndex) :
		nullptr;

	if (BinaryArc)
	{
		FString SingleChar = TEXT("");
		for (INT i = 0; i < LogString.Len() / 3; ++i)
		{
			SingleChar = LogString.Mid(i*3).Left(3);
			CHAR Temp  = appAtoi(*SingleChar);
			*static_cast<FArchive*>(BinaryArc) << Temp;
		}

		*static_cast<UBOOL*>(Result) = 1;
	}
	else
	{
		*static_cast<UBOOL*>(Result) = 0;
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
				*static_cast<UBOOL*>(Result) = 1;
				break;
			}
		}

		if (!Found)
			*static_cast<UBOOL*>(Result) = 0;
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

	*static_cast<UBOOL*>(Result) = (FunctionIndex > 1) ? 1 : 0;
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
		*static_cast<FString*>(Result) = TEXT("ERROR");
		return;
	}

	FString Packages;

	// Look only at packages in the packagemap
	if (XLevel && XLevel->NetDriver && XLevel->NetDriver->MasterMap)
	{
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
					if (static_cast<INT>(Import.PackageIndex) >= 0)
						continue;
					
					FObjectImport& ClassImport = Pkg.Linker->ImportMap(-static_cast<INT>(Import.PackageIndex) - 1);

					// Check if the class matches
					if (appStricmp(*ClassImport.ObjectName, *ImportedClass) ||
						appStricmp(*ClassImport.ClassName, TEXT("Class")) ||
						appStricmp(*ClassImport.ClassPackage, TEXT("Core")))
						continue;

					// Skip if the class was not imported from another package
					if (static_cast<INT>(ClassImport.PackageIndex) >= 0)
						continue;

					FObjectImport& PackageImport = Pkg.Linker->ImportMap(-static_cast<INT>(ClassImport.PackageIndex) - 1);

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

	*static_cast<FString*>(Result) = Packages;
	unguard;
}

/*-----------------------------------------------------------------------------
	execFindNativeCalls
-----------------------------------------------------------------------------*/
static BYTE ReadExpr(TArray<BYTE>& ScriptBuffer, INT& Pos, TArray<INT>& Natives, INT& FoundNative)
{
	const BYTE Opcode = ScriptBuffer(Pos++);

	//GLog->Logf(TEXT("PackageHelper: Read expr %d (%02x / %s) - Pos %d (%02x)"), Opcode, Opcode, (Opcode >= 0 && Opcode < 0x60) ? appFromAnsi(OpcodeNames[Opcode]) : TEXT("Native call"), Pos-1, Pos-1);

	if (Opcode >= EX_MinConversion && Opcode < EX_MaxConversion)
	{		
		ReadExpr(ScriptBuffer, Pos, Natives, FoundNative);
	}
	else if (Opcode >= EX_FirstNative)
	{
		const INT NativeNum = Opcode;
		for (INT i = 0; i < Natives.Num(); ++i)
		{
			if (NativeNum == Natives(i))
			{
				//GLog->Logf(TEXT("PackageHelper: Found call to native func %d (%02x) at pos %d"), NativeNum, NativeNum, Pos);
				FoundNative = NativeNum;
				break;
			}
		}
		while (ReadExpr(ScriptBuffer, Pos, Natives, FoundNative) != EX_EndFunctionParms)
			;
	}
	else if (Opcode >= EX_ExtendedNative)
	{
		const INT NativeNum = (Opcode - EX_ExtendedNative) * 256 + ScriptBuffer(Pos++);
		for (INT i = 0; i < Natives.Num(); ++i)
		{
			if (NativeNum == Natives(i))
			{
				//GLog->Logf(TEXT("PackageHelper: Found call to native func %d (%02x) at pos %d"), NativeNum, NativeNum, Pos);
				FoundNative = NativeNum;
				break;
			}
		}
		while (ReadExpr(ScriptBuffer, Pos, Natives, FoundNative) != EX_EndFunctionParms)
			;
	}
	else
	{
		switch (Opcode)
		{
			// ==========================================================================
			// Opcodes followed by one or more expressions
			// ==========================================================================
			case EX_New:
			{
				ReadExpr(ScriptBuffer, Pos, Natives, FoundNative);
				ReadExpr(ScriptBuffer, Pos, Natives, FoundNative);
				// Intentional fallthrough. EX_New is followed by 4 expressions
			}
			case EX_Let:
			case EX_LetBool:
			case EX_ArrayElement:
			case EX_DynArrayElement:
			{
				ReadExpr(ScriptBuffer, Pos, Natives, FoundNative);
				// Intentional fallthrough. EX_Let, EX_LetBool, EX_ArrayElement, EX_DynArrayElement are followed by 2 expressions
			}
			case EX_EatString:
			case EX_Return:
			case EX_GotoLabel:
			{
				ReadExpr(ScriptBuffer, Pos, Natives, FoundNative);
				break;
			}

			// ==========================================================================
			// Single Opcodes
			// ==========================================================================
			case EX_BoolVariable:
			case EX_Nothing:
			case EX_EndFunctionParms:
			case EX_IntZero:
			case EX_IntOne:
			case EX_True:
			case EX_False:
			case EX_NoObject:
			case EX_Self:
			case EX_IteratorPop:
			case EX_Stop:
			case EX_IteratorNext:
			{
				break;
			}

			// ==========================================================================
			// Opcodes followed by UObject references or fixed-length constants
			// ==========================================================================
			case EX_ByteConst:
			case EX_IntConstByte:
			{
				Pos++;
				break;
			}
			case EX_Jump:
			{
				Pos += 2;
				break;
			}
			case EX_LocalVariable:
			case EX_InstanceVariable:
			case EX_DefaultVariable:
			case EX_NativeParm:
			case EX_IntConst:
			case EX_FloatConst:
			case EX_ObjectConst:
			case EX_NameConst:			
			{
				// UProperty reference (for EX_LocalVariable, EX_InstanceVariable,
				// EX_DefaultVariable, EX_NativeParm) or 32-bit constant. Always 4 bytes
				// even on 64-bit clients
				Pos += 4; 
				break;
			}
			case EX_RotationConst:
			case EX_VectorConst:
			{
				Pos += 12; // three 32-bit constants
				break;
			}

			// ==========================================================================
			// Opcodes followed by variable-length constants
			// ==========================================================================
			case EX_StringConst:
			{
				while (ScriptBuffer(Pos++))
					;
				break;
			}
			case EX_UnicodeStringConst:
			{
				while (*reinterpret_cast<_WORD*>(&ScriptBuffer(Pos)))
					Pos += 2;
				break;
			}
			case EX_LabelTable:
			{
				while (true)
				{
					const FLabelEntry* E = reinterpret_cast<FLabelEntry*>(&ScriptBuffer(Pos));
					Pos += 8;
					if (E->Name == NAME_None)
						break;
				}
				break;
			}
			
			// ==========================================================================
			// Opcodes followed by a combination of exprs and fixed-length constants
			// ==========================================================================
			case EX_JumpIfNot:
			case EX_Assert:
			case EX_Skip:
			{
				Pos += 2;
				ReadExpr(ScriptBuffer, Pos, Natives, FoundNative);
				break;
			}
			case EX_MetaCast:
			case EX_DynamicCast:
			case EX_StructMember:
			{
				Pos += 4; // UClass reference
				ReadExpr(ScriptBuffer, Pos, Natives, FoundNative);
				break;
			}
			case EX_FinalFunction:
			case EX_VirtualFunction:
			case EX_GlobalFunction:						
			{
				Pos += 4; // UStruct reference (for EX_FinalFunction, EX_StructMember) or FName (for EX_VirtualFunction and EX_GlobalFunction). Always 4 bytes even on 64-bit clients
				while (ReadExpr(ScriptBuffer, Pos, Natives, FoundNative) != EX_EndFunctionParms)
					;
				break;
			}
			case EX_StructCmpEq:
			case EX_StructCmpNe:
			{
				Pos += 4;
				ReadExpr(ScriptBuffer, Pos, Natives, FoundNative);
				ReadExpr(ScriptBuffer, Pos, Natives, FoundNative);
				break;
			}
			case EX_ClassContext:
			case EX_Context:
			{
				ReadExpr(ScriptBuffer, Pos, Natives, FoundNative);
				Pos += 3;
				ReadExpr(ScriptBuffer, Pos, Natives, FoundNative);
				break;
			}			
			case EX_Iterator:
			{
				ReadExpr(ScriptBuffer, Pos, Natives, FoundNative);
				Pos += 2;
				break;
			}
			case EX_Switch:
			{
				Pos++;
				ReadExpr(ScriptBuffer, Pos, Natives, FoundNative);
				break;
			}
			case EX_Case:
			{
				const _WORD Offset = *reinterpret_cast<_WORD*>(&ScriptBuffer(Pos));
				Pos += 2;
				if (Offset != 0xffff)
					ReadExpr(ScriptBuffer, Pos, Natives, FoundNative);
				break;
			}

			// ==========================================================================
			// 
			// ==========================================================================
			default:
			{
				GLog->Logf(TEXT("PackageHelper: Found unrecognized Opcode %02x at pos %d"), Opcode, Pos);
			}
		}
	}

	return Opcode;
}

static INT ScanScriptBuffer(TArray<BYTE>& ScriptBuffer, TArray<INT> Natives)
{
	INT Pos = 0;
	INT FoundNative = 0;

	while (Pos < ScriptBuffer.Num() - 1 && !FoundNative)
		ReadExpr(ScriptBuffer, Pos, Natives, FoundNative);
	
	return FoundNative;
}

void APHActor::execFindNativeCalls(FFrame& Stack, RESULT_DECL)
{
	P_GET_STR(NativeNums);
	P_GET_STR(Exclusions);
	P_FINISH;

	TArray<INT> Natives;
	TArray<FString> ExcludePackages;

	// Parse the list of iNatives we're scanning for
	NativeNums += TEXT(";");	
	INT Tmp = NativeNums.InStr(TEXT(";"));
	while (Tmp != -1)
	{
		FString NativeNum = NativeNums.Left(Tmp);
		NativeNums = NativeNums.Mid(Tmp + 1);
		Tmp = NativeNums.InStr(TEXT(";"));

		if (NativeNum.Len() > 0)		
			Natives.AddItem(appAtoi(*NativeNum));
	}

	// Parse the list of UPackages we want to exclude from the search
	Exclusions += TEXT(";");
	Tmp = Exclusions.InStr(TEXT(";"));
	while (Tmp != -1)
	{
		FString Package = Exclusions.Left(Tmp);
		Exclusions = Exclusions.Mid(Tmp + 1);		

		// Chop off the extension
		Tmp = Package.InStr(TEXT("."), 1);
		if (Tmp != -1)
			Package = Package.Left(Tmp);

		if (Package.Len() > 0)
			ExcludePackages.AddItem(Package);

		Tmp = Exclusions.InStr(TEXT(";"));
	}

	if (Natives.Num() == 0)
	{
		GLog->Logf(TEXT("PackageHelper: No natives requested"));
		*static_cast<FString*>(Result) = TEXT("");
		return;
	}	

	// Look only at packages in the packagemap
	FString OutPackages;
	if (XLevel && XLevel->NetDriver && XLevel->NetDriver->MasterMap)
	{		
		for (INT i = 0; i < XLevel->NetDriver->MasterMap->List.Num(); ++i)
		{
			FPackageInfo& Pkg = XLevel->NetDriver->MasterMap->List(i);

			// these should all have linkers...
			if (Pkg.Linker && Pkg.Parent)
			{
				// Do not scan if excluded
				UBOOL Excluded = FALSE;
				UBOOL FoundDangerousNative = FALSE;
				for (INT j = 0; j < ExcludePackages.Num(); ++j)
				{
					if (ExcludePackages(j) == Pkg.Parent->GetName())
					{
						Excluded = TRUE;
						break;
					}
				}

				if (Excluded)
				{
					//GLog->Logf(TEXT("PackageHelper: Package is excluded from the search: %s"), Pkg.Parent->GetFullName());
					continue;
				}

				UObject::BeginLoad();
				Pkg.Linker->LoadAllObjects();
				UObject::EndLoad();

				// Serialize all classes in this package
				for (TObjectIterator<UStruct> It; It; ++It)
				{
					if (It->IsIn(Pkg.Parent) && It->Script.Num() > 0)
					{
						//GLog->Logf(TEXT("Scanning struct: %s - Scriptbuffer size: %d"), It->GetFullName(), It->Script.Num());
						const INT FoundNative = ScanScriptBuffer(It->Script, Natives);
						if (FoundNative)
						{
							GLog->Logf(TEXT("PackageHelper: Found a dangerous call to native function %d in: %s"), FoundNative, *FObjectFullName(*It));
							FoundDangerousNative = TRUE;
							break;
						}
					}
				}

				// Normalize the package name and add it to the list
				if (FoundDangerousNative)
				{
					FString BaseName;
					if (Pkg.Linker->Filename.InStr(TEXT("/"), true))
						BaseName = Pkg.Linker->Filename.Mid(Pkg.Linker->Filename.InStr(TEXT("/"), true) + 1);
					if (BaseName.InStr(TEXT("\\"), true))
						BaseName = BaseName.Mid(BaseName.InStr(TEXT("\\"), true) + 1);
					OutPackages += BaseName + TEXT(";");
				}
			}
		}
	}

	*static_cast<FString*>(Result) = OutPackages;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
