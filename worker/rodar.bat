@echo off
rem sobe o worker de extracao. fica em loop olhando o supabase. ctrl+c pra parar.
python "%~dp0worker.py" %*
