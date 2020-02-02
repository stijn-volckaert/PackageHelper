// =============================================================================
// PackageHelper v1.5 - (c) 2009, 2020 Stijn "AnthraX" Volckaert
// =============================================================================
// PackageHelper provides native utility functions to deal with UPackages and
// package installation on Unreal Engine 1 servers. An example of how to use
// PackageHelper can be found here:
// https://github.com/stijn-volckaert/ACE_AutoConfig
// =============================================================================
class PHActor extends Actor
    native;

// =============================================================================
// Variables
// =============================================================================

//
// Reference to the actor that contains the ServerActors and ServerPackages 
// Arrays. To set this reference, simply call PackageHelper.Touch(MyActor).
// PackageHelper expects the TargetActor to declare the ServerActors and 
// ServerPackages variables as arrays of 255 strings.
//
var Actor TargetActor;

//
// Index into an internal C++ TArray<FArchive*>
//
var const int BinaryArcIndex;
var const bool BinaryArcOpened;

//
// Internal filename
//
var const string FinalName;

// =============================================================================
// native functions
// =============================================================================

//
// Gets the ServerPackages/ServerActors lists from the active UGameEngine and 
// copies them into the TargetActor's ServerPackages/ServerActors arrays.
//
native function bool   GetPackageInfo(Actor TActor);

//
// Reads the TargetActor's ServerPackages/ServerActors arrays and stores them
// into the active UGameEngine's ServerPackages/ServerActors lists
// 
native function bool   SetPackageInfo(Actor TActor);

//
// Saves the UGameEngine configuration to the ini file
//
native function bool   SavePackageInfo();

//
// Calclates the MD5 hash of a loader dll in an NPLoader package
//
native function string GetLoaderMD5(string Loader);

//
// Returns the MD5 hash and filesize of the specified file in 
// "MD5Hash:::FileSize" format
//
native function string GetFileInfo(string FileName);

//
// Moves an ACE <=v0.8 definitions file
//
native function bool   MoveDefsFile(string OldName, string NewName);

//
// Decrypts an ACE <=v0.8 definitions file and returns the path name
// of the decrypted class.
//
native function string LoadDefsFile(string FileName);

//
// Checks if the specified file exists
//
native function bool   FileExists(string FileName);

//
// Opens the specified file in binary mode
//
native function bool   OpenBinaryLog(string FileName);

//
// Writes to the currently open binary file. The output buffer is encoded
// as a %03d string
// 
native function bool   LogBinary(string LogString);

//
// Closes the currently open binary file
//
native function bool   CloseBinaryLog();

//
// Checks if the specified UPackage is the Master packagemap of the Level's 
// NetDriver.
//
native function bool   IsInPackageMap(string Package);

//
// Checks if there is any UScript code in the specified package 
//
native function bool   HasEmbeddedCode(string Mapname);

//
// Find all packages that import the specified function
// Return is in "Package1;Package2;" format
//
native function string FindImports(string ImportedFunction);

//
// Find all packages containing calls to the specified numbered natives
// Does not scan the excluded packages
// NativeNums is in "123;468;124;" format
// ExcludePackages is in "Package1;Package2;" format
// Return is in "Package1;Package2;" format
//
native function string FindNativeCalls(string NativeNums, string ExcludePackages);

// =============================================================================
// Touch ~ Used to set the reference to the targetactor
// =============================================================================
function Touch(Actor TActor)
{
    TargetActor = TActor;
}

// =============================================================================
// GetItemName ~
// =============================================================================
function string GetItemName(string Message)
{
    local bool bResult;
    local string OldName, NewName, Tmp;

    Message = CAPS(Message);

    switch(Message)
    {
        // Retrieve ServerActors/ServerPackages list from UGameEngine and
        // store them into the ServerActors/ServerPackages array in TargetActor
        case "GETPACKAGEINFO":
            bResult = GetPackageInfo(TargetActor);
            break;
        // Retrieve ServerActors/ServerPackages list from the array in TargetActor
        // and store them in UGameEngine's list
        case "SETPACKAGEINFO":
            bResult = SetPackageInfo(TargetActor);
            break;
        // Save the list
        case "SAVEPACKAGEINFO":
            bResult = SavePackageInfo();
            break;
        // Get the MD5 hash for a dll inside a loader package
        default:
            if (Left(Message,12) == "GETLOADERMD5")
                return GetLoaderMD5(Mid(Message,13));
            else if (Left(Message,11) == "GETFILEINFO")
                return GetFileInfo(Mid(Message,12));
            else if (Left(Message,12) == "MOVEDEFSFILE")
            {
                OldName = Mid(Message,13);
                if (InStr(OldName, " ") != -1)
                {
                    NewName = Mid(OldName, InStr(OldName, " ")+1);
                    OldName = Left(OldName, InStr(OldName, " "));
                }
                bResult = MoveDefsFile(OldName, NewName);
            }
            else if (Left(Message,12) == "LOADDEFSFILE")
                return LoadDefsFile(Mid(Message,13));
            else if (Left(Message,10) == "FILEEXISTS")
                bResult = FileExists(Mid(Message,11));
            else if (Left(Message,13) == "OPENBINARYLOG")
                bResult = OpenBinaryLog(Mid(Message,14));
            else if (Left(Message,9) == "LOGBINARY")
                bResult = LogBinary(Mid(Message,10));
            else if (Left(Message,14) == "CLOSEBINARYLOG")
                bResult = CloseBinaryLog();
            else if (Left(Message,7) == "ISINMAP")
                bResult = IsInPackageMap(Mid(Message,8));
            else if (Left(Message, 15) == "HASEMBEDDEDCODE")
                bResult = HasEmbeddedCode(Mid(Message, 16));
            else if (Left(Message, 11) == "FINDIMPORTS")
                return FindImports(Mid(Message, 12));
            else if (Left(Message, 15) == "FINDNATIVECALLS")
            {
                Tmp = Mid(Message, 16);
                if (InStr(Tmp, " ") != -1)
                    return FindNativeCalls(Left(Tmp, InStr(Tmp, " ")), Mid(Tmp, InStr(Tmp, " ") + 1));
                else
                    return FindNativeCalls(Tmp, "");
            }
            break;
    }

    // Multilangual server support. return string(bResult); would return "VRAI" on a
    // french server...
    if (bResult)
        return "TRUE";
    return "FALSE";
}

// =============================================================================
// defaultproperties
// =============================================================================
defaultproperties
{
    bHidden=True
}
