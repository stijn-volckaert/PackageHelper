/*=============================================================================
	PackageHelper Private Header

	Revision history:
		* Created by AnthraX
=============================================================================*/

#ifndef _PACKAGEHELPER_H
#define _PACKAGEHELPER_H

/*-----------------------------------------------------------------------------
	Package Definitions
-----------------------------------------------------------------------------*/
#define PACKAGE_NAME		PackageHelper_v15
#define PACKAGE_NAME_CAPS	PACKAGEHELPER_V15
#define PACKAGE_FNAME(name)	PACKAGEHELPER_V15_##name
#define PACKAGE_LIBRARY		"PackageHelper_v15.dll"
#define PACKAGE_CLASSES		"PackageHelper_v15Classes.h"

/*-----------------------------------------------------------------------------
	Windows Definitions
-----------------------------------------------------------------------------*/
#ifndef __LINUX_X86__
	#define CORE_API		DLL_IMPORT
	#define ENGINE_API		DLL_IMPORT
	#define PACKAGEHELPER_V15_API	DLL_EXPORT
#endif

/*-----------------------------------------------------------------------------
	Code Definitions
-----------------------------------------------------------------------------*/
#define __FULLVERSION

/*-----------------------------------------------------------------------------
	Includes
-----------------------------------------------------------------------------*/
#include "Engine.h"
#include "UnNet.h"
#include "UnLinker.h"
#include PACKAGE_CLASSES

#ifdef __LINUX_X86__
	#include <sys/stat.h>
	#include <errno.h>
	#include "FFileManagerLinux.h"
#endif

/*-----------------------------------------------------------------------------
	FFBufferReader - Redefined to work with ANSICHAR TArrays
-----------------------------------------------------------------------------*/
class FFBufferReader : public FArchive
{
public:
	FFBufferReader( const TArray<ANSICHAR>& InBytes )
	:	Bytes	( InBytes )
	,	Pos 	( 0 )
	{
		ArIsLoading = ArIsTrans = 1;
	}
	void Serialize( void* Data, INT Num )
	{
		check(Pos>=0);
		check(Pos+Num<=Bytes.Num());
		if( Num == 1 )
			((BYTE*)Data)[0] = Bytes(Pos);
		else
			appMemcpy( Data, &Bytes(Pos), Num );
		Pos += Num;
	}
	INT Tell()
	{
		return Pos;
	}
	INT TotalSize()
	{
		return Bytes.Num();
	}
	void Seek( INT InPos )
	{
		check(InPos>=0);
		check(InPos<=Bytes.Num());
		Pos = InPos;
	}
	UBOOL AtEnd()
	{
		return Pos>=Bytes.Num();
	}
private:
	const TArray<ANSICHAR>& Bytes;
	INT Pos;
};

#ifdef __LINUX_X86__
// File manager.
class PHArchiveFileReader : public FArchive
{
public:
	PHArchiveFileReader( FILE* InFile, FOutputDevice* InError, INT InSize )
	:	File			( InFile )
	,	Error			( InError )
	,	Size			( InSize )
	,	Pos				( 0 )
	,	BufferBase		( 0 )
	,	BufferCount		( 0 )
	{
		guard(PHArchiveFileReader::PHArchiveFileReader);
		fseek( File, 0, SEEK_SET );
		ArIsLoading = ArIsPersistent = 1;
		unguard;
	}
	~PHArchiveFileReader()
	{
		guard(PHArchiveFileReader::~PHArchiveFileReader);
		if( File )
			Close();
		unguard;
	}
	void Precache( INT HintCount )
	{
		guardSlow(PHArchiveFileReader::Precache);
		checkSlow(Pos==BufferBase+BufferCount);
		BufferBase = Pos;
		BufferCount = Min( Min( HintCount, (INT)(ARRAY_COUNT(Buffer) - (Pos&(ARRAY_COUNT(Buffer)-1))) ), Size-Pos );
		if( fread( Buffer, BufferCount, 1, File )!=1 && BufferCount!=0 )
		{
			ArIsError = 1;
			Error->Logf( TEXT("fread failed: BufferCount=%i Error=%i"), BufferCount, ferror(File) );
			return;
		}
		unguardSlow;
	}
	void Seek( INT InPos )
	{
		guard(PHArchiveFileReader::Seek);
		check(InPos>=0);
		check(InPos<=Size);
		if( fseek(File,InPos,SEEK_SET) )
		{
			ArIsError = 1;
			Error->Logf( TEXT("seek Failed %i/%i: %i %i"), InPos, Size, Pos, ferror(File) );
		}
		Pos         = InPos;
		BufferBase  = Pos;
		BufferCount = 0;
		unguard;
	}
	INT Tell()
	{
		return Pos;
	}
	INT TotalSize()
	{
		return Size;
	}
	UBOOL Close()
	{
		guardSlow(PHArchiveFileReader::Close);
		if( File )
			fclose( File );
		File = NULL;
		return !ArIsError;
		unguardSlow;
	}
	void Serialize( void* V, INT Length )
	{
		guardSlow(PHArchiveFileReader::Serialize);
		while( Length>0 )
		{
			INT Copy = Min( Length, BufferBase+BufferCount-Pos );
			if( Copy==0 )
			{
				if( Length >= (INT)ARRAY_COUNT(Buffer) )
				{
					if( fread( V, Length, 1, File )!=1 )
					{
						ArIsError = 1;
						Error->Logf( TEXT("fread failed: Length=%i Error=%i"), Length, ferror(File) );
					}
					Pos += Length;
					BufferBase += Length;
					return;
				}
				Precache( MAXINT );
				Copy = Min( Length, BufferBase+BufferCount-Pos );
				if( Copy<=0 )
				{
					ArIsError = 1;
					Error->Logf( TEXT("ReadFile beyond EOF %i+%i/%i"), Pos, Length, Size );
				}
				if( ArIsError )
					return;
			}
			appMemcpy( V, Buffer+Pos-BufferBase, Copy );
			Pos       += Copy;
			Length    -= Copy;
			V          = (BYTE*)V + Copy;
		}
		unguardSlow;
	}
	UBOOL AtEnd()
	{
		INT Pos = Tell();
		return Pos!=INDEX_NONE && Pos>=TotalSize();
	}
protected:
	FILE*			File;
	FOutputDevice*	Error;
	INT				Size;
	INT				Pos;
	INT				BufferBase;
	INT				BufferCount;
	BYTE			Buffer[1024];
};

class PHArchiveFileWriter : public FArchive
{
public:
	PHArchiveFileWriter( FILE* InFile, FOutputDevice* InError )
	:	File		(InFile)
	,	Error		( InError )
	,	Pos			(0)
	,	BufferCount	(0)
	{
		ArIsSaving = ArIsPersistent = 1;
	}
	~PHArchiveFileWriter()
	{
		guard(PHArchiveFileWriter::~PHArchiveFileWriter);
		if( File )
			Close();
		File = NULL;
		unguard;
	}
	void Seek( INT InPos )
	{
		Flush();
		if( fseek(File,InPos,SEEK_SET) )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("SeekFailed",TEXT("Core")) );
		}
		Pos = InPos;
	}
	INT Tell()
	{
		return Pos;
	}
	UBOOL Close()
	{
		guardSlow(PHArchiveFileWriter::Close);
		Flush();
		if( File && fclose( File ) )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("WriteFailed",TEXT("Core")) );
		}
		File = NULL;
		return !ArIsError;
		unguardSlow;
	}
	void Serialize( void* V, INT Length )
	{
		Pos += Length;
		INT Copy;
		while( Length > (Copy=ARRAY_COUNT(Buffer)-BufferCount) )
		{
			appMemcpy( Buffer+BufferCount, V, Copy );
			BufferCount += Copy;
			Length      -= Copy;
			V            = (BYTE*)V + Copy;
			Flush();
		}
		if( Length )
		{
			appMemcpy( Buffer+BufferCount, V, Length );
			BufferCount += Length;
		}
	}
	void Flush()
	{
		if( BufferCount && fwrite( Buffer, BufferCount, 1, File )!=1 )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("WriteFailed",TEXT("Core")) );
		}
		BufferCount=0;
	}
protected:
	FILE*			File;
	FOutputDevice*	Error;
	INT				Pos;
	INT				BufferCount;
	BYTE			Buffer[4096];
};

void FixFilename( const TCHAR* Filename )
{
	TCHAR* Cur;

	for( Cur = (TCHAR*)Filename; *Cur != '\0'; Cur++ )
		if( *Cur == '\\' )
			*Cur = '/';
}

class PHFileManagerLinux : public FFileManager
{
public:
	FArchive* CreateFileReader( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		guard(PHFileManagerLinux::CreateFileReader);
		FixFilename( Filename );
		FILE* File = fopen(ANSI_TO_TCHAR(Filename), TCHAR_TO_ANSI(TEXT("rb")));
		if( !File )
		{
			GLog->Logf(TEXT("PackageHelper: File not found: %s - Trying case-insensitive search..."), Filename);
			TArray<FString> Result = FindFiles(Filename, 0, 0);
			if (Result.Num() > 0)
			{			    
				FString FullResult(Filename);
				// FindFiles only returns the basename...
				FullResult = FullResult.Left(FullResult.Len() - strlen(*Result(0))) + *Result(0);
				GLog->Logf(TEXT("PackageHelper: Found: %s"), *FullResult);				
			    File = fopen(TCHAR_TO_ANSI(*FullResult), TCHAR_TO_ANSI(TEXT("rb")));
			}
			if ( !File )
			{
			    if( Flags & FILEREAD_NoFail )
                    appErrorf(TEXT("Failed to read file: %s"),Filename);
                return NULL;
			}
		}
		fseek( File, 0, SEEK_END );
		return new(TEXT("LinuxFileReader"))PHArchiveFileReader(File,Error,ftell(File));
		unguard;
	}
	FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		guard(PHFileManagerLinux::CreateFileWriter);
		if( Flags & FILEWRITE_EvenIfReadOnly )
			chmod(TCHAR_TO_ANSI(Filename), __S_IREAD | __S_IWRITE);
		if( (Flags & FILEWRITE_NoReplaceExisting) && FileSize(Filename)>=0 )
			return NULL;
		const TCHAR* Mode = (Flags & FILEWRITE_Append) ? TEXT("ab") : TEXT("wb");
		FixFilename( Filename );
		FILE* File = fopen(ANSI_TO_TCHAR(Filename), TCHAR_TO_ANSI(Mode));
		if( !File )
		{
			if( Flags & FILEWRITE_NoFail )
				appErrorf( TEXT("Failed to write: %s"), Filename );
			return NULL;
		}
		if( Flags & FILEWRITE_Unbuffered )
			setvbuf( File, 0, _IONBF, 0 );
		return new(TEXT("LinuxFileWriter"))PHArchiveFileWriter(File,Error);
		unguard;
	}
	UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )
	{
		guard(PHFileManagerLinux::Delete);
		FixFilename( Filename );
		if( EvenReadOnly )
			chmod(ANSI_TO_TCHAR(Filename), __S_IREAD | __S_IWRITE);
		return unlink(ANSI_TO_TCHAR(Filename))==0 || (errno==ENOENT && !RequireExists);
		unguard;
	}
	SQWORD GetGlobalTime( const TCHAR* Filename )
	{
		guard(PHFileManagerLinux::GetGlobalTime);

		return 0;

		unguard;
	}
	UBOOL SetGlobalTime( const TCHAR* Filename )
	{
		guard(PHFileManagerLinux::SetGlobalTime);

		return 0;

		unguard;
	}
	UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 )
	{
		guard(PHFileManagerLinux::MakeDirectory);
		FixFilename( Path );
		if( Tree )
		{
			check(Tree);
			INT SlashCount=0, CreateCount=0;
			for( TCHAR Full[256]=TEXT(""), *Ptr=Full; ; *Ptr++=*Path++ )
			{
				if( *Path==PATH_SEPARATOR[0] || *Path==0 )
				{
					if( SlashCount++>0 && !IsDrive(Full) )
					{
						*Ptr = 0;
						if( !MakeDirectory( Full, 0 ) )
							return 0;
						CreateCount++;
					}
				}
				if( *Path==0 )
					break;
			}
			return CreateCount!=0;
		}
		return mkdir(ANSI_TO_TCHAR(Path), __S_IREAD && __S_IWRITE && __S_IEXEC)==0 || errno==EEXIST;
		unguard;
	}
	UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 )
	{
		guard(PHFileManagerLinux::DeleteDirectory);
		FixFilename( Path );
		if( Tree )
		{
			check(Tree);
				INT i;
			if( !appStrlen(Path) )
				return 0;
			FString Spec = FString(Path) * TEXT("*");
			TArray<FString> List = FindFiles( *Spec, 1, 0 );
			for( i=0; i<List.Num(); i++ )
				if( !Delete(*(FString(Path) * List(i)),1,1) )
					return 0;
			List = FindFiles( *Spec, 0, 1 );
			for( i=0; i<List.Num(); i++ )
				if( !DeleteDirectory(*(FString(Path) * List(i)),1,1) )
					return 0;
			return DeleteDirectory( Path, RequireExists, 0 );
		}
		return rmdir(TCHAR_TO_ANSI(Path))==0 || (errno==ENOENT && !RequireExists);
		unguard;
	}
	TArray<FString> FindFiles( const TCHAR* Filename, UBOOL Files, UBOOL Directories )
	{
		guard(PHFileManagerLinux::FindFiles);
		TArray<FString> Result;

		DIR *Dirp;
		struct dirent* Direntp;
		char Path[256];
		char File[256];
		char *Filestart;
		char *Cur;
		UBOOL Match;

		// Initialize Path to Filename.
		appStrcpy( Path, Filename );

		// Convert MS "\" to Unix "/".
		for( Cur = Path; *Cur != '\0'; Cur++ )
			if( *Cur == '\\' )
				*Cur = '/';

		// Separate path and filename.
		Filestart = Path;
		for( Cur = Path; *Cur != '\0'; Cur++ )
			if( *Cur == '/' )
				Filestart = Cur + 1;

		// Store filename and remove it from Path.
		appStrcpy( File, Filestart );
		*Filestart = '\0';

		// Check for empty path.
		if (appStrlen( Path ) == 0)
			appSprintf( Path, "./" );

        //GLog->Logf(TEXT("Search Directory: %s"), ANSI_TO_TCHAR(Path));

		// Open directory, get first entry.
		Dirp = opendir( Path );
		if (Dirp == NULL)
				return Result;
		Direntp = readdir( Dirp );

		// Check each entry.
		while( Direntp != NULL )
		{
			Match = false;

			//GLog->Logf(TEXT("Found File: %s"), ANSI_TO_TCHAR(Direntp->d_name));

			if( appStrcmp( File, "*" ) == 0 )
			{
				// Any filename.
				Match = true;
			}
			else if( appStrcmp( File, "*.*" ) == 0 )
			{
				// Any filename with a '.'.
				if( appStrchr( Direntp->d_name, '.' ) != NULL )
					Match = true;
			}
			else if( File[0] == '*' )
			{
				// "*.ext" filename.
				if( appStrstr( Direntp->d_name, (File + 1) ) != NULL )
					Match = true;
			}
			else if( File[appStrlen( File ) - 1] == '*' )
			{
				// "name.*" filename.
				if( appStrncmp( Direntp->d_name, File, appStrlen( File ) - 1 ) ==
					0 )
					Match = true;
			}
			else if( appStrstr( File, "*" ) != NULL )
			{
				// single str.*.str match.
				TCHAR* star = appStrstr( File, "*" );
				INT filelen = appStrlen( File );
				INT starlen = appStrlen( star );
				INT starpos = filelen - (starlen - 1);
				TCHAR prefix[256];
				appStrncpy( prefix, File, starpos );
				star++;
				if( appStrncmp( Direntp->d_name, prefix, starpos - 1 ) == 0 )
				{
					// part before * matches
					TCHAR* postfix = Direntp->d_name + (appStrlen(Direntp->d_name) - starlen) + 1;
					if ( appStrcmp( postfix, star ) == 0 )
						Match = true;
				}
			}
			else
			{
				// Literal filename.
				if( appStricmp( Direntp->d_name, File ) == 0 )
					Match = true;
			}

			// Does this entry match the Filename?
			if( Match )
			{
				// Yes, add the file name to Result.
				new(Result)FString(Direntp->d_name);
			}

			// Get next entry.
			Direntp = readdir( Dirp );
		}

		// Close directory.
		closedir( Dirp );

		return Result;
		unguard;
	}
	UBOOL SetDefaultDirectory( const TCHAR* Filename )
	{
		guard(PHFileManagerLinux::SetDefaultDirectory);
		FixFilename(Filename);
		return chdir(TCHAR_TO_ANSI(Filename))==0;
		unguard;
	}
	FString GetDefaultDirectory()
	{
		guard(PHFileManagerLinux::GetDefaultDirectory);
		{
			ANSICHAR Buffer[1024]="";
			getcwd( Buffer, ARRAY_COUNT(Buffer) );
			return appFromAnsi( Buffer );
		}
		unguard;
	}
	INT FileSize( const TCHAR* Filename )
	{
		// Create a generic file reader, get its size, and return it.
		guard(PHFileManagerLinux::FileSize);
		FArchive* Ar = CreateFileReader( Filename, 0, GError );
		if( !Ar )
			return -1;
		INT Result = Ar->TotalSize();
		delete Ar;
		return Result;
		unguard;
	}
	UBOOL Copy( const TCHAR* DestFile, const TCHAR* SrcFile, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes, void (*Progress)(FLOAT Fraction) )
	{
		// Direct file copier.
		guard(FFileManagerGeneric::Copy);
		UBOOL Success = 0;
		if( Progress )
			Progress( 0.0f );
		FArchive* Src = CreateFileReader( SrcFile, 0, GError );
		if( Src )
		{
			INT Size = Src->TotalSize();
			FArchive* Dest = CreateFileWriter( DestFile, (ReplaceExisting?0:FILEWRITE_NoReplaceExisting) | (EvenIfReadOnly?FILEWRITE_EvenIfReadOnly:0), GError );
			if( Dest )
			{
				BYTE Buffer[4096];
				for( INT Total=0; Total<Size; Total+=sizeof(Buffer) )
				{
					INT Count = Min( Size-Total, (INT)sizeof(Buffer) );
					Src->Serialize( Buffer, Count );
					if( Src->IsError() )
						break;
					Dest->Serialize( Buffer, Count );
					if( Dest->IsError() )
						break;
					if( Progress )
						Progress( (FLOAT)Total / Size );
				}
				Success = Dest->Close();
				delete Dest;
				if( !Success )
					Delete( DestFile );
			}
			Success = Success && Src->Close();
			delete Src;
		}
		if( Progress )
			Progress( 1.0 );
		return Success;
		unguard;
	}
	UBOOL Move( const TCHAR* Dest, const TCHAR* Src, UBOOL ReplaceExisting=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 )
	{
		// Move file manually.
		guard(FFileManagerGeneric::Move);
		if( !Copy(Dest,Src,ReplaceExisting,EvenIfReadOnly,Attributes,NULL) )
			return 0;
		Delete( Src, 1, 1 );
		return 1;
		unguard;
	}
private:
	UBOOL IsDrive( const TCHAR* Path )
	{
		// Does Path refer to a drive letter or BNC path?
		guard(FFileManagerGeneric::IsDrive);
		if( appStricmp(Path,TEXT(""))==0 )
			return 1;
		else if( appToUpper(Path[0])!=appToLower(Path[0]) && Path[1]==':' && Path[2]==0 )
			return 1;
		else if( appStricmp(Path,TEXT("\\"))==0 )
			return 1;
		else if( appStricmp(Path,TEXT("\\\\"))==0 )
			return 1;
		else if( Path[0]=='\\' && Path[1]=='\\' && !appStrchr(Path+2,'\\') )
			return 1;
		else if( Path[0]=='\\' && Path[1]=='\\' && appStrchr(Path+2,'\\') && !appStrchr(appStrchr(Path+2,'\\')+1,'\\') )
			return 1;
		else
			return 0;
		unguard;
	}
};
#endif

#endif
