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
#if !defined(__LINUX_X86__) && !defined(__LINUX_ARM__)
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

#if defined(__LINUX_X86__) || defined(__LINUX_ARM__)
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

#endif
