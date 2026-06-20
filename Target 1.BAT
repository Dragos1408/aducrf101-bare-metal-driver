SET PATH=D:\MDK\ARM\ARMCLANG\Bin;C:\Program Files\Common Files\Oracle\Java\javapath;C:\Program Files (x86)\VMware\VMware Workstation\bin\;C:\WINDOWS\system32;C:\WINDOWS;C:\WINDOWS\System32\Wbem;C:\WINDOWS\System32\WindowsPowerShell\v1.0\;C:\WINDOWS\System32\OpenSSH\;C:\Program Files\NVIDIA Corporation\NVIDIA App\NvDLISR;C:\Program Files (x86)\NVIDIA Corporation\PhysX\Common;C:\Program Files (x86)\Microchip\MPLAB C32 Suite\bin;C:\Program Files (x86)\Windows Kits\10\Windows Performance Toolkit\;C:\Program Files\MATLAB\R2021b\runtime\win64;C:\Program Files\MATLAB\R2021b\bin;C:\msys64\mingw64\bin;C:\Program Files\dotnet\;C:\Users\drago\AppData\Local\Microsoft\WindowsApps;C:\Users\drago\AppData\Local\Python\bin;D:\Microsoft VS Code\bin
SET ARMCC6_ASMOPT=--diag_suppress=9931
SET ARMCC6_CCOPT=--diag_suppress=9931
SET ARMCC6_LINKOPT=--diag_suppress=9931
SET CPU_TYPE=ARMCM3
SET CPU_VENDOR=ARM
SET UV2_TARGET=Target 1
SET CPU_CLOCK=0x00F42400
"D:\MDK\ARM\ARMCLANG\Bin\ArmClang.exe" @".\objects\startup_aducrf101._ac"
"D:\MDK\ARM\ARMCLANG\Bin\ArmClang.exe" @".\objects\system_aducrf101.__i"
"D:\MDK\ARM\ARMCLANG\Bin\ArmClang.exe" @".\objects\main.__i"
"D:\MDK\ARM\ARMCLANG\Bin\ArmLink" --Via ".\Objects\proj_1.lnp"
