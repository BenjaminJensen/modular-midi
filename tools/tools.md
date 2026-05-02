# Tools

## Segger JLink & VS Code
Ensure the JLink root folder is in the path variable:
1. In VS Code, go to File > Preferences > Settings.
2. Click the Open Settings (JSON) icon at the top right.
3. Add (or update) the terminal.integrated.env.windows section like this:

'''
"terminal.integrated.env.windows": {
    "PATH": "C:\\Program Files\\SEGGER\\JLink_V924a;${env:PATH}"
}
'''