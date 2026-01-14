@echo off
setlocal

call pandoc -s --toc ^
--template="C:\.pandoc\templates\easy-pandoc-templates\html\uikit.html" ^
--highlight-style=haddock ^
--metadata title="XEngine Documentation" ^
"../README.md" -o "documentation.html"

set "FILE=documentation.html"
if not exist "%FILE%" (
  echo ERROR: No existe "%FILE%" en %CD%
  pause
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass ^
  "(Get-Content -Raw -Encoding UTF8 '%FILE%') -replace '\./docs/diagrams','./diagrams' -replace '\./docs/gameplay','./gameplay' | Set-Content -Encoding UTF8 '%FILE%'"
