# Microsoft Developer Studio Project File - Name="ficl" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=ficl - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "ficl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "ficl.mak" CFG="ficl - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "ficl - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "ficl - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "ficl - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "release"
# PROP Intermediate_Dir "release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W4 /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /FD /c
# SUBTRACT CPP /Fr /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "ficl - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /Za /W4 /Gm /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "ficl - Win32 Release"
# Name "ficl - Win32 Debug"
# Begin Group "Sources"

# PROP Default_Filter "*.c"
# Begin Source File

SOURCE=.\dict.c
# End Source File
# Begin Source File

SOURCE=.\ficl.c

!IF  "$(CFG)" == "ficl - Win32 Release"

# ADD CPP /FAcs

!ELSEIF  "$(CFG)" == "ficl - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\fileaccess.c
# End Source File
# Begin Source File

SOURCE=.\float.c
# End Source File
# Begin Source File

SOURCE=.\math64.c
# End Source File
# Begin Source File

SOURCE=.\prefix.c
# End Source File
# Begin Source File

SOURCE=.\search.c
# End Source File
# Begin Source File

SOURCE=.\softcore.c
# End Source File
# Begin Source File

SOURCE=.\stack.c
# End Source File
# Begin Source File

SOURCE=.\sysdep.c
# End Source File
# Begin Source File

SOURCE=.\testmain.c
# End Source File
# Begin Source File

SOURCE=.\tools.c
# End Source File
# Begin Source File

SOURCE=.\vm.c
# End Source File
# Begin Source File

SOURCE=.\win32.c

!IF  "$(CFG)" == "ficl - Win32 Release"

!ELSEIF  "$(CFG)" == "ficl - Win32 Debug"

# ADD CPP /Ze

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\words.c
# End Source File
# End Group
# Begin Group "Headers"

# PROP Default_Filter "*.h"
# Begin Source File

SOURCE=.\ficl.h
# End Source File
# Begin Source File

SOURCE=.\math64.h
# End Source File
# Begin Source File

SOURCE=.\sysdep.h
# End Source File
# End Group
# Begin Group "softcore"

# PROP Default_Filter ".fr"
# Begin Source File

SOURCE=.\softwords\classes.fr
# End Source File
# Begin Source File

SOURCE=.\softwords\ficlclass.fr
# End Source File
# Begin Source File

SOURCE=.\softwords\ficllocal.fr
# End Source File
# Begin Source File

SOURCE=.\softwords\fileaccess.fr
# End Source File
# Begin Source File

SOURCE=.\softwords\forml.fr
# End Source File
# Begin Source File

SOURCE=.\softwords\ifbrack.fr
# End Source File
# Begin Source File

SOURCE=.\softwords\jhlocal.fr
# End Source File
# Begin Source File

SOURCE=.\softwords\marker.fr
# End Source File
# Begin Source File

SOURCE=.\softwords\oo.fr
# End Source File
# Begin Source File

SOURCE=.\softwords\prefix.fr
# End Source File
# Begin Source File

SOURCE=.\softwords\softcore.bat
# End Source File
# Begin Source File

SOURCE=.\softwords\softcore.fr
# End Source File
# Begin Source File

SOURCE=.\softwords\softcore.pl
# End Source File
# Begin Source File

SOURCE=.\softwords\string.fr
# End Source File
# End Group
# Begin Group "doc"

# PROP Default_Filter "*.html. *.txt"
# Begin Source File

SOURCE=.\doc\ficl.html
# End Source File
# Begin Source File

SOURCE=.\doc\ficl_ans.html
# End Source File
# Begin Source File

SOURCE=.\doc\ficl_debug.html
# End Source File
# Begin Source File

SOURCE=.\ficl_guts.htm
# End Source File
# Begin Source File

SOURCE=.\doc\ficl_loc.html
# End Source File
# Begin Source File

SOURCE=.\doc\ficl_oop.html
# End Source File
# Begin Source File

SOURCE=.\doc\ficl_parse.html
# End Source File
# Begin Source File

SOURCE=.\doc\ficl_rel.html
# End Source File
# Begin Source File

SOURCE=.\doc\ficlheader.js
# End Source File
# Begin Source File

SOURCE=.\doc\index.html
# End Source File
# Begin Source File

SOURCE=.\doc\oo_in_c.html
# End Source File
# Begin Source File

SOURCE=.\doc\primer.txt
# End Source File
# End Group
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# End Target
# End Project
