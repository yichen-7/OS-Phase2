#!/bin/bash
rm -rf .vscode
mkdir .vscode
echo "
{
  // Use IntelliSense to learn about possible attributes.
  // Hover to view descriptions of existing attributes.
  // For more information, visit: https://go.microsoft.com/fwlink/?linkid=830387
  \"version\": \"0.2.0\",
  \"configurations\": [
    {
      \"type\": \"lldb\",
      \"request\": \"launch\",
      \"name\": \"Debug\",
      \"program\": \"\${workspaceFolder}/\${fileBasenameNoExtension}\",
      \"args\": [],
      \"cwd\": \"\${workspaceFolder}\",
      \"preLaunchTask\": \"C/C++: gcc build active file\",
    }
  ]
}
" > .vscode/launch.json

echo "
{
  \"tasks\": [
    {
      \"type\": \"cppbuild\",
      \"label\": \"C/C++: gcc build active file\",
      \"command\": \"/usr/bin/gcc\",
      \"args\": [
        \"-g\",
        \"\${file}\",
        \"-o\",
        \"\${fileDirname}/\${fileBasenameNoExtension}\"
      ],
      \"options\": {
        \"cwd\": \"/usr/bin\"
      },
      \"problemMatcher\": [
        \"\$gcc\"
      ],
      \"group\": {
        \"kind\": \"build\",
        \"isDefault\": true
      },
      \"detail\": \"Generated task by Debugger\"
    }
  ],
  \"version\": \"2.0.0\"
}
" > .vscode/tasks.json