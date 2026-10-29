@echo off
chcp 65001 >nul
echo ============================================================================
echo   INSTALADOR DO KERNEL C++ SUPERQUADRA v2.0.0 PARA ARCHICAD 29
echo ============================================================================
echo.

set "DEST_DIR=C:\Program Files\Graphisoft\Archicad 29\Add-Ons\Extras"
set "APX_LOCAL=%~dp0SuperquadraAutomation.apx"
set "APX_BUILD=%~dp0devkit_tools\Build\SuperquadraAutomation\29\INT\RelWithDebInfo\SuperquadraAutomation.apx"

if exist "%APX_BUILD%" (
    set "APX_SOURCE=%APX_BUILD%"
) else if exist "%APX_LOCAL%" (
    set "APX_SOURCE=%APX_LOCAL%"
) else (
    echo [ERRO] Arquivo SuperquadraAutomation.apx nao encontrado!
    pause
    exit /b 1
)

if not exist "%DEST_DIR%" (
    echo Criando pasta de destino: "%DEST_DIR%"
    mkdir "%DEST_DIR%"
)

echo Copiando extensao C++ compilada (v2.0.0) para o Archicad 29...
copy /Y "%APX_SOURCE%" "%DEST_DIR%\SuperquadraAutomation.apx"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCESSO] Extensao SuperquadraAutomation.apx v2.0.0 instalada com sucesso em:
    echo   "%DEST_DIR%\SuperquadraAutomation.apx"
    echo.
    echo IMPORTANTE: Reinicie o Archicad 29 para carregar os 15 comandos do Kernel v2.0.0!
) else (
    echo.
    echo [ERRO] Falha ao copiar o arquivo (Permissao Negada).
    echo Por favor, clique com o botao direito em "instalar_extensao.bat"
    echo e selecione "Executar como Administrador".
)

echo.
pause
