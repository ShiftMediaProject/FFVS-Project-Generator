@ECHO OFF

SET UPSTREAMURL=https://github.com/ShiftMediaProject
SET DEPENDENCIES=( ^
bzip2, ^
fdk-aac, ^
fontconfig, ^
freetype2, ^
fribidi, ^
game-music-emu, ^
gnutls, ^
lame, ^
libass, ^
libbluray, ^
libcdio, ^
libcdio-paranoia, ^
libiconv, ^
libilbc, ^
libgcrypt, ^
liblzma, ^
libssh, ^
libxml2, ^
libvpx, ^
mfx_dispatch, ^
modplug, ^
opus, ^
sdl, ^
soxr, ^
speex, ^
theora, ^
vorbis, ^
x264, ^
x265, ^
xvid, ^
zlib ^
)
SET PGOPTIONS=--enable-gpl --enable-version3 --enable-nonfree --enable-bzlib --enable-iconv --enable-lzma --enable-sdl2 --enable-zlib --enable-libmp3lame --enable-libvorbis --enable-libspeex --enable-libopus --enable-libilbc --enable-libtheora --enable-libx264 --enable-libx265 --enable-libxvid --enable-libvpx --enable-libgme --enable-libmodplug --enable-libsoxr --enable-libfreetype --enable-fontconfig --enable-libfribidi --enable-libass --enable-libxml2 --enable-gnutls --disable-schannel --enable-gcrypt --enable-libssh --enable-libcdio --enable-libbluray --enable-opengl --enable-libmfx --enable-ffnvcodec --enable-cuda --enable-amf --enable-libfdk-aac

REM Store current directory and ensure working directory is the location of current .bat
SET CURRDIR=%CD%
cd %~dp0

REM Initialise error check value
SET ERROR=0

REM Check if executable can be located
IF NOT EXIST "project_generate.exe" (
    ECHO "Error: FFVS Project Generator executable file not found."
    IF EXIST "../.git" (
        ECHO "Please build the executable using the supplied project before continuing."
    )
    GOTO exitOnError
)

REM Check if FFmpeg directory can be located
SET SEARCHPATHS=(./, ../, ./ffmpeg/, ../ffmpeg/, ../../ffmpeg/, ../../../, ../../)
SET FFMPEGPATH=
FOR %%I IN %SEARCHPATHS% DO (
    IF EXIST "%%I/ffmpeg.h" (
        SET FFMPEGPATH=%%I
    )
    IF EXIST "%%I/fftools/ffmpeg.h" (
        SET FFMPEGPATH=%%I
    )
)
IF "%FFMPEGPATH%"=="" (
    ECHO Error: Failed finding FFmpeg source directory
    GOTO exitOnError
) ELSE (
    ECHO Located FFmpeg source directory at "%FFMPEGPATH%"
    ECHO.
)

REM Copy across the batch file used to auto get required dependencies
CALL :makeGetDeps || GOTO exit

REM Get/Update any used dependency libraries
SET USERPROMPT=N
SET /P USERPROMPT=Do you want to download/update the required dependency projects (Y/N)?
IF /I "%USERPROMPT%"=="Y" (
    ECHO.
    CALL :getDeps || GOTO exit
    ECHO Ensure that any dependency projects have been built using the supplied project within the dependencies ./SMP folder before continuing.
    ECHO Warning: Some used dependencies require a manual download. Consult the readme for instructions to install the following needed components:
    ECHO    OpenGL, CUDA
    PAUSE
)

REM Run the executable
ECHO Running project generator...
project_generate.exe %PGOPTIONS%
GOTO exit

:makeGetDeps
ECHO Creating project_get_dependencies.bat...
FOR %%I IN %DEPENDENCIES% DO SET LASTDEP=%%I
MKDIR "%FFMPEGPATH%/SMP" >NUL 2>&1
(
    ECHO @ECHO OFF
    ECHO SETLOCAL EnableDelayedExpansion
    ECHO.
    ECHO SET UPSTREAMURL=%UPSTREAMURL%
    ECHO SET DEPENDENCIES=( ^^
    FOR %%I IN %DEPENDENCIES% DO (
        IF "%%I"=="%LASTDEP%" (
            ECHO %%I ^^
        ) ELSE (
            ECHO %%I, ^^
        )
    )
    type smp_project_get_dependencies
) > "%FFMPEGPATH%/SMP/project_get_dependencies.bat"
ECHO.
EXIT /B %ERRORLEVEL%

:getDeps
REM Add current repo to list of already passed dependencies
ECHO Getting and updating any required dependency libs...
cd "%FFMPEGPATH%/SMP"
CALL project_get_dependencies.bat "ffmpeg" || EXIT /B 1
cd %~dp0
ECHO.
EXIT /B %ERRORLEVEL%

:exitOnError
SET ERROR=1

:exit
REM Check if this was launched from an existing terminal or directly from .bat
REM  If launched by executing the .bat then pause on completion
cd %CURRDIR%
ECHO %CMDCMDLINE% | FINDSTR /L %COMSPEC% >NUL 2>&1
IF %ERRORLEVEL% == 0 IF "%~1"=="" PAUSE
EXIT /B %ERROR%