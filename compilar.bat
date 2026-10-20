@echo off
REM Compila no Windows (MinGW/TDM-GCC). -lgdi32 = janela do Windows; -static = o .exe roda sem instalar nada.
g++ -std=c++17 -O2 -Wall *.cpp -o simulador.exe -lgdi32 -static
if errorlevel 1 (echo FALHOU) else (echo OK: simulador.exe gerado)
