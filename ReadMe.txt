FICL 3.03
April 2002

________
OVERVIEW

Ficl is a complete programming language interpreter designed to be embedded
into other systems (including firmware based ones) as a command, macro,
and development prototype language.  Ficl stands for "Forth Inspired
Command Language".

For more information, please see the "doc" directory.
For release notes, please see "doc/ficl_rel.html".

____________
INSTALLATION

Ficl builds out-of-the-box on the following platforms:
	* Linux: use "Makefile.linux".
	* RiscOS: use "Makefile.riscos".
	* Win32: use "ficl.dsw" / "ficl.dsp".
To port to other platforms, be sure to examine "sysdep.h", and
we suggest you start with the Linux makefile.  (And please--feel
free to submit your portability changes!)

____________
FICL LICENSE

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

