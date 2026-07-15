@echo off
rem Disattiva l'echo dei comandi: senza questo, ogni riga eseguita verrebbe stampata a video.
setlocal enabledelayedexpansion
rem Rende le variabili locali a questo script e abilita l'espansione ritardata (!VAR!),
rem cosi' non vengono lasciate variabili "sporche" nell'ambiente della shell chiamante.

rem ============================================================
rem  Command-line build for the Spout2_DX12 plugin.
rem  Wraps RunUAT.bat BuildPlugin, the official way to compile
rem  a standalone UE plugin without opening the Editor.
rem
rem  Usage:
rem    Build.bat [EngineDir] [OutputDir]
rem
rem  Defaults:
rem    EngineDir = C:\Program Files\Epic Games\UE_5.8
rem    OutputDir = .\Build\Spout2_DX12
rem ============================================================

rem Primo argomento passato allo script: cartella di installazione dell'Engine.
rem %~1 rimuove eventuali virgolette attorno al percorso.
set "ENGINE_DIR=%~1"
rem Se non e' stato passato nessun argomento, usa il percorso di default di UE 5.8.
if "%ENGINE_DIR%"=="" set "ENGINE_DIR=C:\Program Files\Epic Games\UE_5.8"

rem Secondo argomento: cartella dove impacchettare l'output della build.
set "OUTPUT_DIR=%~2"
rem Se non specificata, usa "Build\Spout2_DX12" relativa alla cartella dello script
rem (%~dp0 = drive+path dello script stesso).
if "%OUTPUT_DIR%"=="" set "OUTPUT_DIR=%~dp0Build\Spout2_DX12"

rem Percorso al file descrittore del plugin (.uplugin), assunto nella stessa cartella dello script.
set "PLUGIN_FILE=%~dp0Spout2_DX12.uplugin"
rem Percorso allo script ufficiale RunUAT.bat dentro l'installazione dell'Engine scelta sopra.
set "RUNUAT=%ENGINE_DIR%\Engine\Build\BatchFiles\RunUAT.bat"

rem Controllo di sicurezza: se RunUAT.bat non esiste al percorso calcolato,
rem stampa un errore con istruzioni ed esce con codice 1 (fallimento).
if not exist "%RUNUAT%" (
    echo [ERROR] RunUAT.bat not found at "%RUNUAT%".
    echo         Pass the Engine install directory as the first argument, e.g.:
    echo         Build.bat "C:\Program Files\Epic Games\UE_5.8"
    exit /b 1
)

rem Controllo di sicurezza: se il file .uplugin non esiste, stampa errore ed esce con codice 1.
if not exist "%PLUGIN_FILE%" (
    echo [ERROR] Plugin descriptor not found at "%PLUGIN_FILE%".
    exit /b 1
)

rem Riepilogo a video dei parametri usati per la build, seguito da una riga vuota.
echo Engine:  %ENGINE_DIR%
echo Plugin:  %PLUGIN_FILE%
echo Output:  %OUTPUT_DIR%
echo.

rem Chiama RunUAT.bat con il comando BuildPlugin, il tool ufficiale di Epic per
rem compilare un plugin standalone senza aprire l'Editor. "call" fa si' che
rem l'esecuzione torni a questo script invece di terminarlo.
rem Il carattere ^ a fine riga e' il simbolo di continuazione riga in batch.
call "%RUNUAT%" BuildPlugin ^
    -Plugin="%PLUGIN_FILE%" ^
    -Package="%OUTPUT_DIR%" ^
    -TargetPlatforms=Win64 ^
    -Rocket
rem   -Plugin           : percorso del file .uplugin da compilare
rem   -Package          : cartella dove impacchettare l'output
rem   -TargetPlatforms  : compila solo per Windows 64-bit
rem   -Rocket           : compila in modalita' "Installed Build" (engine binario, non da sorgente)

rem Salva subito il codice di uscita di RunUAT.bat, prima che venga sovrascritto
rem da altri comandi (es. gli "echo" successivi).
set "EXIT_CODE=%ERRORLEVEL%"
rem Se la build e' fallita (codice diverso da 0), stampa l'errore e propaga lo stesso codice.
if not "%EXIT_CODE%"=="0" (
    echo.
    echo [FAILED] BuildPlugin exited with code %EXIT_CODE%.
    exit /b %EXIT_CODE%
)

rem Se si arriva qui la build e' andata a buon fine: messaggio di successo ed uscita con codice 0.
echo.
echo [OK] Plugin built successfully. Output in "%OUTPUT_DIR%".
exit /b 0
