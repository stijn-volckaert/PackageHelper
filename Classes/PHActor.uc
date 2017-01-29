// =============================================================================
// PackageHelper v1.3 - (c) 2009 AnthraX
// =============================================================================
// The purpose of this mod is to provide reliable access to the serveractors and
// serverpackages list without an extra dependency.
// This means that:
// * Unlike the ConsoleCommand("get Engine.GameEngine ServerPackages") command,
// this method will NOT crash the server
// * This package does NOT need to be in the EditPackages list of the mod that
// uses PackageHelper's services. In fact, I strongly advise against it. I will
// provide example code for anyone who wishes to use this package.
// =============================================================================
class PHActor extends Actor
    native;

// =============================================================================
// Variables
// =============================================================================
var Actor TargetActor;                                               // Reference to the actor that contains the ServerActors and ServerPackages Arrays.
var const int BinaryArc;                                             // Internal FArchive ptr
var const string FinalName;                                          // Internal Filename

// =============================================================================
// native functions
// =============================================================================
native function bool   GetPackageInfo(Actor TActor);                 // Gets the ServerPackages/ServerActors list from UGameEngine and stores them in TargetActor
native function bool   SetPackageInfo(Actor TActor);                 // Gets the ServerPackages/ServerActors list from TargetActor and stores them in UGameEngine
native function bool   SavePackageInfo();                            // Saves the UGameEngine configuration
native function string GetLoaderMD5(string Loader);                  // Get the MD5 of a dll inside a loader package
native function string GetFileInfo(string FileName);                 // Return: MD5:::FileSize
native function bool   MoveDefsFile(string OldName, string NewName); // Move a definition file
native function string LoadDefsFile(string FileName);                // Return: ObjectName
native function bool   FileExists(string FileName);                  // Check if a file exists
native function bool   OpenBinaryLog(string FileName);               // Open a binary log file
native function bool   LogBinary(string LogString);                  // Logstring is in %03d format
native function bool   CloseBinaryLog();                             // Close binary log
native function bool   IsInPackageMap(string Package);               // Meh..
native function bool   HasEmbeddedCode(string Mapname);              // Maps with embedded code have to be checked
native function string FindImports(string ImportedFunction);         // Find all packages that import the specified function

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
    local string OldName, NewName;

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
