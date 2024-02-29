how write shell code in windows ?
probably most do this direct on asm. however possible write it on c/c++ (with general and minimal asm code). 
simply need good understand how compiler and linker work
what is shellcode by fact ? code with no relocs and no imports
let show this on example. 

assume we have 32-bit process on x64 system ( wow64 process)
and we want enumerate modules from 64-bit process. not trivial task from 32-bit
we need enter to Heaven's Gate for this and execute here shellcode, which do task.

we need several steps.

1. write normal c++ code for solution.
   for concrete task we can do next:
  - a) create section
  - b) open target process
  - c) map section in target process
  - d) create new thread in target process with RtlExitUserThread entry point (yes!)
  - e) queue APC to this thread with start address at LdrQueryProcessModuleInformation (we need 3 params for this api)
  - f) wait thread exit
  - g) umap section from target process
  - h) close target process
  - i) map section in own process - if all ok - we got RTL_PROCESS_MODULES here
  - j) use RTL_PROCESS_MODULES data
  - k) unmap section
  - l) close section
  
the d/e steps require 64 bit code. Let's put them in a separate function (for greater convenience, I also included there c/f/g):

```cpp
NTSTATUS DoRemoteQuery(HANDLE hSection, HANDLE hProcess);
```

place this code in separate file ( in my case [x64.cpp](https://github.com/rbmm/Noname/blob/main/x64.cpp)  )

after all work in x64 mode, let go next

2. begin transorm to shell code

use pragma code_seg for section names

```cpp
#pragma code_seg(".text$mn$cpp")
  ...
NTSTATUS DoRemoteQuery(HANDLE hSection, HANDLE hProcess) 
{
  ...
}
#pragma code_seg(".text$mn$end")

HRESULT X64Entry()ASM_FUNCTION;// this function will be implemented in asm

SIZE_T SizeOfShellCode()
{
	return (((PBYTE)SizeOfShellCode - (PBYTE)X64Entry) + 7) >> 3;
}

#pragma code_seg()

add asm file ( code64.asm - https://github.com/rbmm/Noname/blob/main/code64.asm)
```

with 

```asm
_TEXT$asm SEGMENT ALIGN(16)

?X64Entry@@YAJXZ proc
...
?X64Entry@@YAJXZ endp
...
_TEXT$asm ENDS
```

the `_TEXT$asm` section name will be mapped to `.text$mn$asm`

mandatory (!!)` add `/cbstring` and `/Oi` compiler options to x64.cpp
the `/cbstring put string literals to section "near" code section
our c/c++ code will be in .text$mn$cpp
as result string will be located in .text$mn$cpp$s  
/Oi allow use intrinsic functions, like strcmp, which we need

add

```cpp
#ifdef _WIN64
#define DECLSPEC_IMPORT
#endif
```

usually func declared with DECLSPEC_IMPORT macro
when it expanded to `__declspec(dllimport)` - compiler will be generate indirect call [__imp_func]
but this is bad for shellcode , we need relative call func

so we will be have next section layout:

```
.text$mn
---- shell begin -----
.text$mn$asm
.text$mn$cpp
.text$mn$cpp$s
---- shell end ----
.text$mn$end 
```

and with `SIZE_T SizeOfShellCode()` we can easy locate size and begin ( `X64Entry` function in `.text$mn$asm` ) of shell code

we need "implement" all ntdll api which we will be call in gate by self
really we need only implement it delay-load, what is not so hard - look in code64.asm

ensure that after such transormations - all code still work ok

3. save shell code as asm file ( for ml[64] ) - during run x64 version

```cpp
#ifdef _WIN64
	PrepareCode(L"sc64.asm", (PULONG64)X64Entry, SizeOfShellCode());
#endif
```

simply convert code to array of DQ xxxx lines ( look for [util.cpp](https://github.com/rbmm/Noname/blob/main/util.cpp) )

i save it as [sc64.asm](https://github.com/rbmm/Noname/blob/main/sc64.asm)

4. now we can switch to 32 bit build

we need remove x64.cpp and code64.asm from compilation here
and add [code32.asm](https://github.com/rbmm/Noname/blob/main/code32.asm) for enter Heaven's Gate - here we include generated [sc64.asm](https://github.com/rbmm/Noname/blob/main/sc64.asm)

we not need here `#define DECLSPEC_IMPORT` of course
the direct call to DoRemoteQuery we replace to:

```cpp
#ifdef _WIN64
			status = DoRemoteQuery(hSection, hProcess);
#else
			status = Code64Call(hSection, hProcess, 0, 0, iDoRemoteQuery);
#endif
```

and this is all !
if no any mistakes in process - we got working 32bit binary, which enumerate modules from 64 bit process ( shell/explorer in concrete example)
by enter x64 mode and execute shell code here



