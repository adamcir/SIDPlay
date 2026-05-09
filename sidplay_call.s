.export _sid_call_init
.export _sid_call_play
.export _sid_prepare_song

.import _current_call
.import _current_song

_sid_prepare_song:
    ldx #$02
prep_loop:
    lda $00,x
    sta sid_zp,x
    inx
    bne prep_loop
    rts

_sid_call_init:
    lda _current_song
    sta song_tmp

    jsr setup_target

    php
    sei

    jsr save_c_zp
    jsr restore_sid_zp

    lda $01
    pha

    lda #$35
    sta $01

    lda song_tmp

call_init_target:
    jsr $ffff

    pla
    sta $01

    jsr save_sid_zp
    jsr restore_c_zp

    plp
    rts

_sid_call_play:
    jsr setup_target

    php
    sei

    jsr save_c_zp
    jsr restore_sid_zp

    lda $01
    pha

    lda #$35
    sta $01

call_play_target:
    jsr $ffff

    pla
    sta $01

    jsr save_sid_zp
    jsr restore_c_zp

    plp
    rts

setup_target:
    lda _current_call
    sta call_init_target + 1
    sta call_play_target + 1

    lda _current_call + 1
    sta call_init_target + 2
    sta call_play_target + 2

    rts

save_c_zp:
    ldx #$02
save_c_loop:
    lda $00,x
    sta c_zp,x
    inx
    bne save_c_loop
    rts

restore_c_zp:
    ldx #$02
restore_c_loop:
    lda c_zp,x
    sta $00,x
    inx
    bne restore_c_loop
    rts

save_sid_zp:
    ldx #$02
save_sid_loop:
    lda $00,x
    sta sid_zp,x
    inx
    bne save_sid_loop
    rts

restore_sid_zp:
    ldx #$02
restore_sid_loop:
    lda sid_zp,x
    sta $00,x
    inx
    bne restore_sid_loop
    rts

.bss

song_tmp:
    .res 1

c_zp:
    .res 256

sid_zp:
    .res 256