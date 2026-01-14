@echo off
cd ..
call pandoc -s --toc ^
  --template="C:\.pandoc\templates\easy-pandoc-templates\tex\eisvogel.tex" ^
  --highlight-style=haddock ^
  --pdf-engine=xelatex ^
  -V documentclass=report -V classoption=oneside -V classoption=openany ^
  -V title="XEngine Documentation" ^
  README.md -o ".\doc\documentation.pdf"