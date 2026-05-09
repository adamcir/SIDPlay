#include <c64.h>
#include <cbm.h>
#include <conio.h>
#include <string.h>
#include <stdint.h>

#define DEVICE 8
#define MAX_FILES 32
#define MAX_NAME 17

#define SID_BUFFER ((uint8_t *)0x4000)
#define SID_MAXSIZE 0x5000

#define PLAYMODE_AUTO 0
#define PLAYMODE_VBI_CUSTOM 1
#define PLAYMODE_CIA_CUSTOM 2
#define PLAYMODE_REJECT_CIA 3

#define KEY_RETURN 13
#define KEY_SPACE 32
#define KEY_UP 145
#define KEY_DOWN 17
#define KEY_DEL 20

/* -------------------------------------------------- */
/* Global variables */

char sid_files[MAX_FILES][MAX_NAME];
uint8_t sid_count = 0;

uint16_t sid_init;
uint16_t sid_play;
uint16_t sid_load;
uint16_t sid_size;

uint16_t sid_version;
uint16_t sid_songs;
uint16_t sid_start_song;
uint32_t sid_speed_flags;
uint8_t sid_use_cia;
uint8_t sid_is_rsid_file;

uint8_t play_mode = PLAYMODE_AUTO;
uint8_t unsafe_low_load = 0;

uint16_t custom_vbi_hz = 50;
uint16_t custom_cia_hz = 50;

uint16_t current_call;
uint8_t current_song;

/* -------------------------------------------------- */
/* ASM functions */

extern void sid_call_init(void);
extern void sid_call_play(void);
extern void sid_prepare_song(void);

/* -------------------------------------------------- */

uint16_t be16(uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

uint16_t le16(uint8_t *p)
{
    return p[0] | ((uint16_t)p[1] << 8);
}

/* -------------------------------------------------- */

uint8_t is_psid(void)
{
    return SID_BUFFER[0] == 0x50 &&
           SID_BUFFER[1] == 0x53 &&
           SID_BUFFER[2] == 0x49 &&
           SID_BUFFER[3] == 0x44;
}

uint8_t is_rsid(void)
{
    return SID_BUFFER[0] == 0x52 &&
           SID_BUFFER[1] == 0x53 &&
           SID_BUFFER[2] == 0x49 &&
           SID_BUFFER[3] == 0x44;
}

/* -------------------------------------------------- */

uint8_t ends_with_sid(char *name)
{
    uint8_t len = strlen(name);

    if (len < 4)
        return 0;

    return (name[len - 4] == '.' &&
            (name[len - 3] == 's' || name[len - 3] == 'S') &&
            (name[len - 2] == 'i' || name[len - 2] == 'I') &&
            (name[len - 1] == 'd' || name[len - 1] == 'D'));
}

/* -------------------------------------------------- */

char *get_play_mode_name(void)
{
    if (play_mode == PLAYMODE_AUTO)
        return "AUTO";
    else if (play_mode == PLAYMODE_VBI_CUSTOM)
        return "VBI CUSTOM HZ";
    else if (play_mode == PLAYMODE_CIA_CUSTOM)
        return "CIA CUSTOM HZ";
    else if (play_mode == PLAYMODE_REJECT_CIA)
        return "REJECT CIA";

    return "UNKNOWN";
}

/* -------------------------------------------------- */

uint16_t get_effective_hz(void)
{
    if (play_mode == PLAYMODE_VBI_CUSTOM)
        return custom_vbi_hz;

    if (play_mode == PLAYMODE_CIA_CUSTOM)
        return custom_cia_hz;

    if (play_mode == PLAYMODE_AUTO)
    {
        if (sid_use_cia)
            return custom_cia_hz;
        else
            return custom_vbi_hz;
    }

    return custom_vbi_hz;
}

/* -------------------------------------------------- */

void call_init(uint16_t addr, uint8_t song)
{
    current_call = addr;
    current_song = song;
    sid_call_init();
}

void call_play(uint16_t addr)
{
    current_call = addr;
    sid_call_play();
}

/* -------------------------------------------------- */

void wait_frame(void)
{
    while (VIC.rasterline == 0)
        ;

    while (VIC.rasterline != 0)
        ;
}

/* -------------------------------------------------- */

void sid_silence(void)
{
    uint8_t i;
    uint8_t *sid = (uint8_t *)0xd400;

    for (i = 0; i < 25; i++)
        sid[i] = 0;
}

/* -------------------------------------------------- */

uint8_t menu_key(void)
{
    uint8_t key;

    key = cgetc();

    if (key == 'w' || key == 'W')
        return KEY_UP;

    if (key == 's' || key == 'S')
        return KEY_DOWN;

    return key;
}

/* -------------------------------------------------- */

void print_cursor_line(uint8_t selected, char *text)
{
    if (selected)
        cprintf("> %s\r\n", text);
    else
        cprintf("  %s\r\n", text);
}

/* -------------------------------------------------- */

uint16_t input_hz(char *title, uint16_t old_value)
{
    uint16_t value = 0;
    uint8_t digits = 0;
    uint8_t key;
    uint8_t done = 0;

    clrscr();

    cprintf("%s\r\n", title);
    cprintf("--------------------\r\n\r\n");
    cprintf("Old value: %u Hz\r\n", old_value);
    cprintf("Enter new value.\r\n");
    cprintf("Range: 1-300 Hz\r\n\r\n");
    cprintf("> ");

    while (!done)
    {
        key = cgetc();

        if (key >= '0' && key <= '9')
        {
            if (digits < 3)
            {
                value = (value * 10) + (key - '0');
                digits++;
                cputc(key);
            }
        }
        else if (key == KEY_DEL)
        {
            /*
               Jednoduchý backspace.
               Pokud by v emulátoru nefungoval, stačí zadat hodnotu znovu.
            */
            if (digits > 0)
            {
                value = value / 10;
                digits--;
                cputc(20);
            }
        }
        else if (key == KEY_RETURN)
        {
            done = 1;
        }
        else if (key == 'q' || key == 'Q')
        {
            return old_value;
        }
    }

    if (digits == 0)
        return old_value;

    if (value < 1)
        value = 1;

    if (value > 300)
        value = 300;

    return value;
}

/* -------------------------------------------------- */

void show_options_menu(void)
{
    uint8_t selected = 0;
    uint8_t done = 0;
    uint8_t key;

    while (!done)
    {
        clrscr();

        cprintf("SIDPlay options\r\n");
        cprintf("--------------------\r\n\r\n");

        if (selected == 0)
            cprintf("> VBI CUSTOM: %u HZ\r\n", custom_vbi_hz);
        else
            cprintf("  VBI CUSTOM: %u HZ\r\n", custom_vbi_hz);

        if (selected == 1)
            cprintf("> CIA CUSTOM: %u HZ\r\n", custom_cia_hz);
        else
            cprintf("  CIA CUSTOM: %u HZ\r\n", custom_cia_hz);

        print_cursor_line(selected == 2, "AUTO FROM HEADER");
        print_cursor_line(selected == 3, "REJECT CIA SONGS");

        if (selected == 4)
            cprintf("> UNSAFE LOW LOAD: %s\r\n", unsafe_low_load ? "ON" : "OFF");
        else
            cprintf("  UNSAFE LOW LOAD: %s\r\n", unsafe_low_load ? "ON" : "OFF");

        cprintf("\r\nCurrent mode:\r\n");
        cprintf("%s\r\n", get_play_mode_name());

        cprintf("\r\nVBI Hz: %u\r\n", custom_vbi_hz);
        cprintf("CIA Hz: %u\r\n", custom_cia_hz);
        cprintf("Unsafe: %s\r\n", unsafe_low_load ? "ON" : "OFF");

        cprintf("\r\nUP/DOWN or W/S\r\n");
        cprintf("RETURN = select/edit\r\n");
        cprintf("Q = back\r\n");

        key = menu_key();

        if (key == KEY_UP)
        {
            if (selected == 0)
                selected = 4;
            else
                selected--;
        }
        else if (key == KEY_DOWN)
        {
            selected++;

            if (selected > 4)
                selected = 0;
        }
        else if (key == KEY_RETURN)
        {
            if (selected == 0)
            {
                custom_vbi_hz = input_hz("VBI custom speed", custom_vbi_hz);
                play_mode = PLAYMODE_VBI_CUSTOM;
            }
            else if (selected == 1)
            {
                custom_cia_hz = input_hz("CIA custom speed", custom_cia_hz);
                play_mode = PLAYMODE_CIA_CUSTOM;
            }
            else if (selected == 2)
            {
                play_mode = PLAYMODE_AUTO;
            }
            else if (selected == 3)
            {
                play_mode = PLAYMODE_REJECT_CIA;
            }
            else if (selected == 4)
            {
                unsafe_low_load = !unsafe_low_load;
            }
        }
        else if (key == 'q' || key == 'Q')
        {
            done = 1;
        }
    }
}

/* -------------------------------------------------- */
/* Load file to SID_BUFFER */

uint16_t load_file_raw(char *name)
{
    uint16_t pos = 0;
    uint8_t c;
    uint8_t status;

    cbm_open(2, DEVICE, 2, name);
    cbm_k_chkin(2);

    while (pos < SID_MAXSIZE)
    {
        c = cbm_k_basin();
        status = cbm_k_readst();

        SID_BUFFER[pos++] = c;

        if (status != 0)
            break;
    }

    cbm_k_clrch();
    cbm_close(2);

    return pos;
}

/* -------------------------------------------------- */
/* Parse PSID/RSID header */

uint8_t parse_sid(uint16_t filesize)
{
    uint16_t data_offset;
    uint8_t *data;

    if (filesize < 0x7c)
        return 0;

    sid_is_rsid_file = is_rsid();

    if (!is_psid() && !is_rsid())
        return 0;

    sid_version = be16(&SID_BUFFER[4]);
    data_offset = be16(&SID_BUFFER[6]);

    sid_load = be16(&SID_BUFFER[8]);
    sid_init = be16(&SID_BUFFER[10]);
    sid_play = be16(&SID_BUFFER[12]);

    sid_songs = be16(&SID_BUFFER[14]);
    sid_start_song = be16(&SID_BUFFER[16]);

    sid_speed_flags =
        ((uint32_t)SID_BUFFER[18] << 24) |
        ((uint32_t)SID_BUFFER[19] << 16) |
        ((uint32_t)SID_BUFFER[20] << 8) |
        SID_BUFFER[21];

    sid_use_cia = (sid_speed_flags & 1) ? 1 : 0;

    if (play_mode == PLAYMODE_REJECT_CIA && sid_use_cia)
        return 5;

    if (data_offset >= filesize)
        return 0;

    data = SID_BUFFER + data_offset;

    if (sid_load == 0)
    {
        sid_load = le16(data);
        data += 2;
        data_offset += 2;
    }

    if (sid_init == 0)
        sid_init = sid_load;

    sid_size = filesize - data_offset;

    if (sid_is_rsid_file)
        return 2;

    if (sid_play == 0)
        return 3;

    if (!unsafe_low_load)
    {
        if (sid_load < 0x5000)
            return 4;
    }

    if ((uint32_t)sid_load + sid_size > 0xc000)
        return 4;

    memmove((void *)sid_load, data, sid_size);

    return 1;
}

/* -------------------------------------------------- */
/* Scan disk directory */

void scan_directory(void)
{
    uint8_t c;
    uint8_t status;
    uint8_t in_quote = 0;
    uint8_t pos = 0;
    char name[MAX_NAME];

    sid_count = 0;
    memset(name, 0, sizeof(name));

    cbm_open(1, DEVICE, 0, "$");
    cbm_k_chkin(1);

    while (sid_count < MAX_FILES)
    {
        c = cbm_k_basin();
        status = cbm_k_readst();

        if (status != 0)
            break;

        if (c == '"')
        {
            if (!in_quote)
            {
                in_quote = 1;
                pos = 0;
                memset(name, 0, sizeof(name));
            }
            else
            {
                in_quote = 0;
                name[pos] = 0;

                if (ends_with_sid(name))
                {
                    strcpy(sid_files[sid_count], name);
                    sid_count++;
                }
            }
        }
        else if (in_quote)
        {
            if (pos < MAX_NAME - 1)
                name[pos++] = c;
        }
    }

    cbm_k_clrch();
    cbm_close(1);
}

/* -------------------------------------------------- */

void show_parse_error(uint8_t err)
{
    if (err == 0)
        cprintf("Not valid SID file.\r\n");
    else if (err == 2)
        cprintf("RSID not supported yet.\r\n");
    else if (err == 3)
        cprintf("Play $0000 not supported.\r\n");
    else if (err == 4)
        cprintf("Unsafe load address.\r\n");
    else if (err == 5)
        cprintf("CIA timing rejected.\r\n");
    else
        cprintf("Unknown SID error.\r\n");
}

/* -------------------------------------------------- */

void play_tick_by_hz(uint16_t hz, uint16_t *acc)
{
    /*
       PAL frame = 50 Hz.

       acc += wanted Hz
       if acc >= 50, call play and subtract 50.

       Examples:
       10 Hz = 1 call per 5 frames
       20 Hz = 2 calls per 5 frames
       30 Hz = 3 calls per 5 frames
       40 Hz = 4 calls per 5 frames
       50 Hz = 1 call per frame
       60 Hz = 6 calls per 5 frames
       100 Hz = 2 calls per frame
    */

    *acc += hz;

    while (*acc >= 50)
    {
        call_play(sid_play);
        *acc -= 50;
    }
}

/* -------------------------------------------------- */

void play_sid_file(char *name)
{
    uint16_t size;
    uint16_t frames;
    uint8_t parse_result;
    uint8_t song_number;
    uint16_t play_hz;
    uint16_t acc;
    uint8_t stop;

    clrscr();
    cprintf("SIDPlay\r\n");
    cprintf("--------------------\r\n");
    cprintf("Loading: %s\r\n", name);

    size = load_file_raw(name);

    cprintf("Size: %u\r\n", size);
    cprintf("Header: %02x %02x %02x %02x\r\n",
            SID_BUFFER[0],
            SID_BUFFER[1],
            SID_BUFFER[2],
            SID_BUFFER[3]);

    parse_result = parse_sid(size);

    if (parse_result != 1)
    {
        show_parse_error(parse_result);
        cprintf("Press key...\r\n");
        cgetc();
        return;
    }

    cprintf("Type: %s\r\n", sid_is_rsid_file ? "RSID" : "PSID");
    cprintf("Version: %u\r\n", sid_version);
    cprintf("Load: $%04x\r\n", sid_load);
    cprintf("Init: $%04x\r\n", sid_init);
    cprintf("Play: $%04x\r\n", sid_play);
    cprintf("Songs: %u\r\n", sid_songs);
    cprintf("Start: %u\r\n", sid_start_song);
    cprintf("Header speed: %s\r\n", sid_use_cia ? "CIA" : "VBI");
    cprintf("Play mode: %s\r\n", get_play_mode_name());

    play_hz = get_effective_hz();

    cprintf("Effective Hz: %u\r\n", play_hz);

    cprintf("\r\nPlaying...\r\n");
    cprintf("SPACE = back to menu\r\n");
    cprintf("Q = stop\r\n");

    sid_silence();

    sid_prepare_song();

    if (sid_start_song == 0)
        song_number = 0;
    else
        song_number = sid_start_song - 1;

    call_init(sid_init, song_number);

    frames = 0;
    acc = 0;
    stop = 0;

    while (!stop)
    {
        wait_frame();

        play_tick_by_hz(play_hz, &acc);

        frames++;

        if (kbhit())
        {
            uint8_t key = cgetc();

            if (key == KEY_SPACE)
                stop = 1;

            if (key == 'q' || key == 'Q')
                stop = 1;
        }
    }

    sid_silence();
}

/* -------------------------------------------------- */

uint8_t select_song_menu(void)
{
    uint8_t selected = 0;
    uint8_t top = 0;
    uint8_t key;
    uint8_t i;
    uint8_t visible_lines = 14;

    while (1)
    {
        clrscr();

        cprintf("SIDPlay song select\r\n");
        cprintf("--------------------\r\n\r\n");

        for (i = 0; i < visible_lines; i++)
        {
            uint8_t index = top + i;

            if (index >= sid_count)
                break;

            if (index == selected)
                cprintf("> %s\r\n", sid_files[index]);
            else
                cprintf("  %s\r\n", sid_files[index]);
        }

        cprintf("\r\nUP/DOWN or W/S\r\n");
        cprintf("RETURN = play\r\n");
        cprintf("O = options\r\n");
        cprintf("Q = quit\r\n");

        key = menu_key();

        if (key == KEY_UP)
        {
            if (selected == 0)
                selected = sid_count - 1;
            else
                selected--;

            if (selected < top)
                top = selected;

            if (selected >= top + visible_lines)
            {
                if (selected >= visible_lines)
                    top = selected - visible_lines + 1;
                else
                    top = 0;
            }
        }
        else if (key == KEY_DOWN)
        {
            selected++;

            if (selected >= sid_count)
                selected = 0;

            if (selected >= top + visible_lines)
                top = selected - visible_lines + 1;

            if (selected < top)
                top = selected;
        }
        else if (key == KEY_RETURN)
        {
            return selected;
        }
        else if (key == 'o' || key == 'O')
        {
            show_options_menu();
        }
        else if (key == 'q' || key == 'Q')
        {
            return 255;
        }
    }
}

/* -------------------------------------------------- */

void main_menu_loop(void)
{
    uint8_t selected;

    while (1)
    {
        selected = select_song_menu();

        if (selected == 255)
            break;

        play_sid_file(sid_files[selected]);
    }
}

/* -------------------------------------------------- */

int main(void)
{
    uint8_t i;

    clrscr();
    cprintf("SIDPlay\r\n");
    cprintf("--------------------\r\n");
    cprintf("Scanning disk...\r\n");

    scan_directory();

    clrscr();
    cprintf("SIDPlay\r\n");
    cprintf("--------------------\r\n");
    cprintf("Found %u SID files\r\n\r\n", sid_count);

    if (sid_count == 0)
    {
        cprintf("No .SID files found.\r\n");
        cprintf("Returning to BASIC...\r\n");
        return 0;
    }

    for (i = 0; i < sid_count; i++)
    {
        cprintf("%u: %s\r\n", i + 1, sid_files[i]);
    }

    cprintf("\r\nPress key...\r\n");
    cgetc();

    main_menu_loop();

    clrscr();
    sid_silence();

    cprintf("SIDPlay finished.\r\n");
    cprintf("Returning to BASIC...\r\n");

    return 0;
}