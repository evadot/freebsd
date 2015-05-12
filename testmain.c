/*
** stub main for testing FICL under Win32
** $Id: testmain.c,v 1.11 2001-10-28 10:59:19-08 jsadler Exp jsadler $
*/
/*
** Copyright (c) 1997-2001 John Sadler (john_sadler@alum.mit.edu)
** All rights reserved.
**
** Get the latest Ficl release at http://ficl.sourceforge.net
**
** I am interested in hearing from anyone who uses ficl. If you have
** a problem, a success story, a defect, an enhancement request, or
** if you would like to contribute to the ficl release, please
** contact me by email at the address above.
**
** L I C E N S E  and  D I S C L A I M E R
** 
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
** OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
** HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
** SUCH DAMAGE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#if defined (_WIN32)
#include <direct.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifdef linux
#include <unistd.h>
#endif

#include "ficl.h"

/*
** Ficl interface to _getcwd (Win32)
** Prints the current working directory using the VM's 
** textOut method...
*/
static void ficlGetCWD(FICL_VM *pVM)
{
    char *cp;

#if defined (_WIN32)
    cp = _getcwd(NULL, 80);
#else
   cp = getcwd(NULL, 80);
#endif
    vmTextOut(pVM, cp, 1);
    free(cp);
    return;
}

/*
** Ficl interface to _chdir (Win32)
** Gets a newline (or NULL) delimited string from the input
** and feeds it to the Win32 chdir function...
** Example:
**    cd c:\tmp
*/
static void ficlChDir(FICL_VM *pVM)
{
    FICL_STRING *pFS = (FICL_STRING *)pVM->pad;
    vmGetString(pVM, pFS, '\n');
    if (pFS->count > 0)
    {
       int err = chdir(pFS->text);
       if (err)
        {
            vmTextOut(pVM, "Error: path not found", 1);
            vmThrow(pVM, VM_QUIT);
        }
    }
    else
    {
        vmTextOut(pVM, "Warning (chdir): nothing happened", 1);
    }
    return;
}

/*
** Ficl interface to system (ANSI)
** Gets a newline (or NULL) delimited string from the input
** and feeds it to the Win32 system function...
** Example:
**    system del *.*
**    \ ouch!
*/
static void ficlSystem(FICL_VM *pVM)
{
    FICL_STRING *pFS = (FICL_STRING *)pVM->pad;

    vmGetString(pVM, pFS, '\n');
    if (pFS->count > 0)
    {
        int err = system(pFS->text);
        if (err)
        {
            sprintf(pVM->pad, "System call returned %d", err);
            vmTextOut(pVM, pVM->pad, 1);
            vmThrow(pVM, VM_QUIT);
        }
    }
    else
    {
        vmTextOut(pVM, "Warning (system): nothing happened", 1);
    }
    return;
}

/*
** Ficl add-in to load a text file and execute it...
** Cheesy, but illustrative.
** Line oriented... filename is newline (or NULL) delimited.
** Example:
**    load test.ficl
*/
#define nLINEBUF 256
static void ficlLoad(FICL_VM *pVM)
{
    char    cp[nLINEBUF];
    char    filename[nLINEBUF];
    FICL_STRING *pFilename = (FICL_STRING *)filename;
    int     nLine = 0;
    FILE   *fp;
    int     result;
    CELL    id;
    struct stat buf;


    vmGetString(pVM, pFilename, '\n');

    if (pFilename->count <= 0)
    {
        vmTextOut(pVM, "Warning (load): nothing happened", 1);
        return;
    }

    /*
    ** get the file's size and make sure it exists 
    */
    result = stat( pFilename->text, &buf );

    if (result != 0)
    {
        vmTextOut(pVM, "Unable to stat file: ", 0);
        vmTextOut(pVM, pFilename->text, 1);
        vmThrow(pVM, VM_QUIT);
    }

    fp = fopen(pFilename->text, "r");
    if (!fp)
    {
        vmTextOut(pVM, "Unable to open file ", 0);
        vmTextOut(pVM, pFilename->text, 1);
        vmThrow(pVM, VM_QUIT);
    }

    id = pVM->sourceID;
    pVM->sourceID.p = (void *)fp;

    /* feed each line to ficlExec */
    while (fgets(cp, nLINEBUF, fp))
    {
        int len = strlen(cp) - 1;

        nLine++;
        if (len <= 0)
            continue;

        if (cp[len] == '\n')
            cp[len] = '\0';

        result = ficlExec(pVM, cp);
        /* handle "bye" in loaded files. --lch */
        switch (result)
        {
            case VM_OUTOFTEXT:
            case VM_USEREXIT:
                break;

            default:
                pVM->sourceID = id;
                fclose(fp);
                vmThrowErr(pVM, "Error loading file <%s> line %d", pFilename->text, nLine);
                break; 
        }
    }
    /*
    ** Pass an empty line with SOURCE-ID == -1 to flush
    ** any pending REFILLs (as required by FILE wordset)
    */
    pVM->sourceID.i = -1;
    ficlExec(pVM, "");

    pVM->sourceID = id;
    fclose(fp);

    /* handle "bye" in loaded files. --lch */
    if (result == VM_USEREXIT)
        vmThrow(pVM, VM_USEREXIT);
    return;
}

/*
** Dump a tab delimited file that summarizes the contents of the
** dictionary hash table by hashcode...
*/
static void spewHash(FICL_VM *pVM)
{
    FICL_HASH *pHash = vmGetDict(pVM)->pForthWords;
    FICL_WORD *pFW;
    FILE *pOut;
    unsigned i;
    unsigned nHash = pHash->size;

    if (!vmGetWordToPad(pVM))
        vmThrow(pVM, VM_OUTOFTEXT);

    pOut = fopen(pVM->pad, "w");
    if (!pOut)
    {
        vmTextOut(pVM, "unable to open file", 1);
        return;
    }

    for (i=0; i < nHash; i++)
    {
        int n = 0;

        pFW = pHash->table[i];
        while (pFW)
        {
            n++;
            pFW = pFW->link;
        }

        fprintf(pOut, "%d\t%d", i, n);

        pFW = pHash->table[i];
        while (pFW)
        {
            fprintf(pOut, "\t%s", pFW->name);
            pFW = pFW->link;
        }

        fprintf(pOut, "\n");
    }

    fclose(pOut);
    return;
}

static void ficlBreak(FICL_VM *pVM)
{
    pVM->state = pVM->state;
    return;
}

static void ficlClock(FICL_VM *pVM)
{
    clock_t now = clock();
    stackPushUNS(pVM->pStack, (FICL_UNS)now);
    return;
}

static void clocksPerSec(FICL_VM *pVM)
{
    stackPushUNS(pVM->pStack, CLOCKS_PER_SEC);
    return;
}


void buildTestInterface(FICL_SYSTEM *pSys)
{
    ficlBuild(pSys, "break",    ficlBreak,    FW_DEFAULT);
    ficlBuild(pSys, "clock",    ficlClock,    FW_DEFAULT);
    ficlBuild(pSys, "cd",       ficlChDir,    FW_DEFAULT);
    ficlBuild(pSys, "load",     ficlLoad,     FW_DEFAULT);
    ficlBuild(pSys, "pwd",      ficlGetCWD,   FW_DEFAULT);
    ficlBuild(pSys, "system",   ficlSystem,   FW_DEFAULT);
    ficlBuild(pSys, "spewhash", spewHash,     FW_DEFAULT);
    ficlBuild(pSys, "clocks/sec", 
                                clocksPerSec, FW_DEFAULT);

    return;
}


#if !defined (_WINDOWS)
#define nINBUF 256

#if !defined (_WIN32)
#define __try
#define __except(i) if (0)
#endif

int main(int argc, char **argv)
{
    int ret = 0;
    char in[nINBUF];
    FICL_VM *pVM;
	FICL_SYSTEM *pSys;

    pSys = ficlInitSystem(10000);
    buildTestInterface(pSys);
    pVM = ficlNewVM(pSys);

    ret = ficlEvaluate(pVM, ".ver .( " __DATE__ " ) cr quit");

    /*
    ** load file from cmd line...
    */
    if (argc  > 1)
    {
        sprintf(in, ".( loading %s ) cr load %s\n cr", argv[1], argv[1]);
        __try
        {
            ret = ficlEvaluate(pVM, in);
        }
        __except(1)
        {
            vmTextOut(pVM, "exception -- cleaning up", 1);
            vmReset(pVM);
        }
    }

    while (ret != VM_USEREXIT)
    {
        fgets(in, nINBUF, stdin);
        __try
        {
            ret = ficlExec(pVM, in);
        }
        __except(1)
        {
            vmTextOut(pVM, "exception -- cleaning up", 1);
            vmReset(pVM);
        }
    }

    ficlTermSystem(pSys);
    return 0;
}

#endif

