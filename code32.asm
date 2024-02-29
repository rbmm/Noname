.686

.MODEL FLAT

.code

; long __fastcall Code64Call(void *,void *,void *,void *,unsigned long)

?Code64Call@@YIJPAX000K@Z proc
	xchg edi,[esp+4]
	xchg esi,[esp+8]
	xchg ebp,[esp+12]
	jmp @2
	ALIGN 16
@3:
INCLUDE <sc64.asm>
@2:
	push 33h
	call @1
	;++++++++ x64 +++++++++
	call @3
	retf
	;-------- x64 ---------
@1:
	call fword ptr [esp]
	pop ecx
	pop ecx
	mov edi,[esp+4]
	mov esi,[esp+8]
	mov ebp,[esp+12]
	ret 12
?Code64Call@@YIJPAX000K@Z endp

end