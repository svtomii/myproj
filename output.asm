; === Extended Python Compiler ===
.model small
.stack 200h

.data
    _t1 DW 0
    _nl  DB 13,10,'$'
    _buf DB 12 DUP('$')

.code

_read_int PROC
    push bx
    push cx
    push dx
    xor bx, bx
    xor cx, cx
_ri_next:
    mov ah, 01h
    int 21h
    cmp al, 13
    je _ri_done
    cmp al, 10
    je _ri_done
    cmp al, '-'
    jne _ri_trydigit
    mov cx, 1
    jmp _ri_next
_ri_trydigit:
    sub al, '0'
    jb _ri_next
    cmp al, 9
    ja _ri_next
    xor ah, ah
    push ax
    mov ax, bx
    mov bx, 10
    imul bx
    pop bx
    add ax, bx
    mov bx, ax
    jmp _ri_next
_ri_done:
    test cx, cx
    jz _ri_pos
    neg bx
_ri_pos:
    mov ax, bx
    pop dx
    pop cx
    pop bx
    ret
_read_int ENDP

_print_int PROC
    push bx
    push cx
    push dx
    push si
    lea si, _buf
    test ax, ax
    jns _wi_pos
    mov byte ptr [si], '-'
    inc si
    neg ax
_wi_pos:
    xor cx, cx
_wi_extr:
    xor dx, dx
    mov bx, 10
    div bx
    push dx
    inc cx
    test ax, ax
    jnz _wi_extr
_wi_fill:
    pop dx
    add dl, '0'
    mov [si], dl
    inc si
    loop _wi_fill
    mov byte ptr [si], '$'
    lea dx, _buf
    mov ah, 09h
    int 21h
    pop si
    pop dx
    pop cx
    pop bx
    ret
_print_int ENDP

_print_nl PROC
    lea dx, _nl
    mov ah, 09h
    int 21h
    ret
_print_nl ENDP

main PROC
    mov ax, @data
    mov ds, ax

    mov ax, 10
    mov a, ax
    mov ax, 20
    mov b, ax
    mov ax, a
    mov bx, b
    add ax, bx
    mov _t1, ax
    mov ax, _t1
    mov c, ax
    mov ax, c
    call _print_int
    call _print_nl

    mov ax, 4C00h
    int 21h
main ENDP

END main
