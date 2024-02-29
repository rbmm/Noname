
; long __cdecl DoRemoteQuery(void *,void *)
extern ?DoRemoteQuery@@YAJPEAX0@Z : PROC

; void *__cdecl GetFuncAddress(const char *)
extern ?GetFuncAddress@@YAPEAXPEBD@Z : PROC

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; .text$mn$asm 

_TEXT$asm SEGMENT ALIGN(16)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; must be at section begin !!

?X64Entry@@YAJXZ proc

	mov rax,gs:[10h]
	xchg rsp,rax		; set 64-bit stack
	push rax			; save 32-bit stack
	sub rsp,28h
  
	mov ecx,ecx
	mov edx,edx
	mov r8d,edi
	mov r9d,esi
	
	test ebp,ebp
	jz @@0
	mov eax,0C0000003h
	jmp @@1
@@0:
	call ?DoRemoteQuery@@YAJPEAX0@Z
@@1:  
	add rsp,28h
	pop rsp				; restore 32-bit stack
	ret
	
?X64Entry@@YAJXZ endp

common_imp_call proc private
	push r9
	push r8
	push rdx
	push rcx
	sub rsp,28h
	mov rcx,rax
	call ?GetFuncAddress@@YAPEAXPEBD@Z
	add rsp,28h
	pop rcx
	pop rdx
	pop r8
	pop r9
	jmp rax
common_imp_call endp

NtApi MACRO name
name proc
	lea rax,@@1
	jmp common_imp_call
@@1: 
	DB '&name',0
name endp
ENDM

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; ++ Import

NtApi LdrGetProcedureAddress
NtApi RtlCreateUserThread
NtApi ZwMapViewOfSection
NtApi ZwUnmapViewOfSection
NtApi NtQueueApcThread
NtApi NtResumeThread
NtApi NtTerminateThread
NtApi NtWaitForSingleObject
NtApi NtClose

;; -- Import
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

_TEXT$asm ENDS

end