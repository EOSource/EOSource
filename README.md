### EOSource Latest Version 7.0

Please compile using latest TDM Dragon with Codeblocks with MINGW

1. Install CodeBlocks (latest codeblocks-20.03mingw-32bit-setup.exe http://www.codeblocks.org/downloads/binaries/)
2. Install TDM Dragon 32-bit (https://github.com/jmeubank/tdm-gcc/releases/download/v10.3.0-tdm-1/tdm-gcc-10.3.0.exe)
3. For Windows open Project->mingw.cbp For Linux Project->Linux.cbp
4. Press Build

Common Compile Errors
Problem: I get PTHREAD time_t error?
Solution: Add HAVE_STRUCT_TIMESPEC to your #defines in codeblocks right click project->buildoptions->#defines

---
Coming Soon
-Fireserv support
