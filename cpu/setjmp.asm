section .text
global setjmp

; int setjmp(jmp_buf *env)
; env pointer is [esp+4]
setjmp:
    mov  eax, [esp+4]
    ; save registers into jmp_buf {eax,ebx,ecx,edx,esi,edi,ebp,esp,eip}
    xor  edx, edx
    mov  [eax+0],  edx      ; store 0 for EAX (real return value set by longjmp)
    mov  [eax+4],  ebx      ; save EBX
    mov  [eax+8],  ecx      ; save ecx
    mov  [eax+12], edx      ; save edx
    mov  [eax+16], esi      ; save esi
    mov  [eax+20], edi      ; save edi
    mov  [eax+24], ebp      ; save ebp
    lea  edx, [esp+4]
    mov  [eax+28], edx      ; save ESP (point to return addr on stack)
    mov  edx, [esp]         ; return address pushed by caller
    mov  [eax+32], edx      ; save EIP

    xor  eax, eax           ; return 0
    ret
