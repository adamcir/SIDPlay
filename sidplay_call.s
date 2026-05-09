.export _sid_call_init
.export _sid_call_play
.export _sid_prepare_song

.import _current_call
.import _current_song

_sid_prepare_song:
    rts

_sid_call_init:
    lda _current_song
    sta song_tmp

    jsr setup_target

    php
    sei

    lda $01
    pha

    ; $35 = RAM v $A000-$BFFF a $E000-$FFFF,
    ; I/O v $D000-$DFFF zůstane viditelné.
    lda #$35
    sta $01

    lda song_tmp

call_init_target:
    jsr $ffff

    pla
    sta $01

    plp
    rts

_sid_call_play:
    jsr setup_target

    php
    sei

    lda $01
    pha

    lda #$35
    sta $01

call_play_target:
    jsr $ffff

    pla
    sta $01

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

.bss

song_tmp:
    .res 1