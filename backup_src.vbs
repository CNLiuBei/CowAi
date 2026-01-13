Set objShell = CreateObject("WScript.Shell")
Set objFSO = CreateObject("Scripting.FileSystemObject")
strScriptPath = objFSO.GetParentFolderName(WScript.ScriptFullName)
objShell.CurrentDirectory = strScriptPath
strPyFile = strScriptPath & "\backup_src.py"
strCommand = "python " & Chr(34) & strPyFile & Chr(34)
objShell.Run strCommand, 1, True
WScript.Echo "Done!"
