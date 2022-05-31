### EOSource Latest Version 7.0

Please compile using TDM Dragon 4.6.1 with Codeblocks 20.03 32-Bit

1. Install Codeblocks 20.03 32-BIT ONLY
(https://eosource.net/files/codeblocks-20.03-32bit-setup.zip)
2. Install TDM Dragon 4.6.1 32-BIT ONLY
(https://eosource.net/files/tdm-gcc-4.6.1.zip)

![Installing TDM Dragon](https://eosource.net/files/tdm-setup.png)
![Installing TDM Dragon](https://eosource.net/files/tdm-setup-2.png)

3. For Windows open Project->mingw.cbp For Linux Project->Linux.cbp

![Open Project](https://eosource.net/files/project-file.png)

4. Target your TDM Dragon GCC Directory in Settings->Compiler->Toolchain Executables (normally located in C:/MINGW32)

![Open Compiler Settings](https://eosource.net/files/compiler-settings.png)
![Target TDM](https://eosource.net/files/target-tdm.png)


6. Press Build->Rebuild

![Rebuild](https://eosource.net/files/rebuild.png)

Great! I compiled EOSource.exe but now i get Errors when trying to open it!

![Rebuild](https://eosource.net/files/errors.png)

You will need two DLL files which are included in the Final Release:
1. pthreadGC2.dll
2. libmariadb.dll

You can download them here: https://eosource.net/files/release-dlls.zip
---
Coming Soon
-Fireserv support
