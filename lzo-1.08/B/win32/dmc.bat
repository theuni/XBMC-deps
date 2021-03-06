rem /* Windows 32 bit (LIB) - Digital Mars C/C++ 8.28
rem  * a very simple make driver
rem  * Copyright (C) 1996-2002 Markus F.X.J. Oberhumer
rem  */

@if "%LZO_ECHO%"=="n" echo off

set CC=sc -mn
set CFLAGS=-Iinclude -o -w-
set MYLIB=lzo.lib

echo Compiling, please be patient...
for %%i in (src\*.c) do %CC% %CFLAGS% -c %%i
@if errorlevel 1 goto error
if exist %MYLIB% del %MYLIB%
lib %MYLIB% /b /c /n /noi @b\win32\dmc.rsp
@if errorlevel 1 goto error

%CC% %CFLAGS% -Iltest ltest\ltest.c %MYLIB%
@if errorlevel 1 goto error

%CC% %CFLAGS% examples\dict.c %MYLIB%
@if errorlevel 1 goto error
%CC% %CFLAGS% examples\lpack.c %MYLIB%
@if errorlevel 1 goto error
%CC% %CFLAGS% examples\overlap.c %MYLIB%
@if errorlevel 1 goto error
%CC% %CFLAGS% examples\precomp.c %MYLIB%
@if errorlevel 1 goto error
%CC% %CFLAGS% examples\precomp2.c %MYLIB%
@if errorlevel 1 goto error
%CC% %CFLAGS% examples\simple.c %MYLIB%
@if errorlevel 1 goto error

%CC% %CFLAGS% -Isrc tests\align.c %MYLIB%
@if errorlevel 1 goto error
%CC% %CFLAGS% -Isrc tests\chksum.c %MYLIB%
@if errorlevel 1 goto error

%CC% %CFLAGS% -Iminilzo -otestmini.exe minilzo\testmini.c minilzo\minilzo.c
@if errorlevel 1 goto error

@echo Done.
@goto end
:error
echo error!
:end
@call b\unset.bat
