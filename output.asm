; === Сгенерировано компилятором Python-subset → IBM PC ASM ===
.model small
.stack 200h

.data
    _t1 DW 0
    _t2 DW 0
    x DW 0
    y DW 0
    _nl  DB 13,10,'$'
    _buf DB 12 DUP('$')

.code

; Читает строку с консоли, возвращает знаковое 16-bit целое в AX
_read_int PROC
    push bx
    push cx
    push dx
    xor  bx, bx          ; bx = накопитель результата
    xor  cx, cx          ; cx = флаг отрицательности
_ri_next:
    mov  ah, 01h
    int  21h              ; читаем символ с эхо → AL
    cmp  al, 13           ; Enter?
    je   _ri_done
    cmp  al, 10
    je   _ri_done
    cmp  al, '-'
    jne  _ri_trydigit
    mov  cx, 1
    jmp  _ri_next
_ri_trydigit:
    sub  al, '0'
    jb   _ri_next         ; символ < '0' → игнорировать
    cmp  al, 9
    ja   _ri_next         ; символ > '9' → игнорировать
    xor  ah, ah           ; AX = цифра 0..9
    push ax               ; сохранить цифру
    mov  ax, bx           ; AX = текущий накопитель
    mov  bx, 10
    imul bx               ; DX:AX = накопитель * 10
    pop  bx               ; BX = цифра
    add  ax, bx           ; AX = накопитель*10 + цифра
    mov  bx, ax           ; обновить накопитель
    jmp  _ri_next
_ri_done:
    test cx, cx
    jz   _ri_pos
    neg  bx               ; отрицательное число
_ri_pos:
    mov  ax, bx
    pop  dx
    pop  cx
    pop  bx
    ret
_read_int ENDP

; Выводит знаковое 16-bit целое из AX на консоль
_write_int PROC
    push bx
    push cx
    push dx
    push si
    lea  si, _buf
    test ax, ax
    jns  _wi_pos
    mov  byte ptr [si], '-'
    inc  si
    neg  ax
_wi_pos:
    xor  cx, cx           ; счётчик цифр
_wi_extr:
    xor  dx, dx
    mov  bx, 10
    div  bx               ; AX = ax/10, DX = ax%10
    push dx               ; запомнить цифру
    inc  cx
    test ax, ax
    jnz  _wi_extr
_wi_fill:
    pop  dx
    add  dl, '0'
    mov  [si], dl
    inc  si
    loop _wi_fill
    mov  byte ptr [si], '$'
    lea  dx, _buf
    mov  ah, 09h
    int  21h              ; вывод строки DS:DX до '$'
    pop  si
    pop  dx
    pop  cx
    pop  bx
    ret
_write_int ENDP

_write_nl PROC
    lea  dx, _nl
    mov  ah, 09h
    int  21h
    ret
_write_nl ENDP

main PROC
    mov ax, @data
    mov ds, ax

    mov ax, 5
    mov x, ax
    mov ax, 10
    mov y, ax
L1:
    mov ax, x
    cmp ax, y
    jl _ct1T
    mov ax, 0
    jmp _ct1D
_ct1T:
    mov ax, 1
_ct1D:
    mov _t1, ax
    mov ax, _t1
    cmp ax, 0
    je  L2
    ; --- write(x) ---
    mov ax, x
    call _write_int
    call _write_nl
    mov ax, x
    mov bx, 1
    add ax, bx
    mov _t2, ax
    mov ax, _t2
    mov x, ax
    jmp L1
L2:
    ; --- write(y) ---
    mov ax, y
    call _write_int
    call _write_nl

    mov ax, 4C00h
    int 21h
main ENDP

END main
