section .text
global longjmp

; void longjmp(jmp_buf *env, int val)
; env = [esp+4], val = [esp+8]
longjmp:
    mov  ecx, [esp+4]       ; env pointer -> keep in ECX
    mov  edx, [esp+8]       ; val
    test edx, edx
    jnz  .haveval
    mov  edx, 1             ; if val==0, return 1
.haveval:
    mov  eax, edx           ; return value in EAX
    ; restore callee-saved and context registers from env
    mov  ebx, [ecx+4]
    mov  edx, [ecx+12]
    mov  esi, [ecx+16]
    mov  edi, [ecx+20]
    mov  ebp, [ecx+24]
    mov  esp, [ecx+28]
    jmp  dword [ecx+32]     ; jump to saved EIP
