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
#define KEY_UP 145
#define KEY_DOWN 17
#define KEY_LEFT 157
#define KEY_RIGHT 29
#define KEY_DEL 20

char sid_files[MAX_FILES][MAX_NAME];
uint8_t sid_count = 0;

uint8_t song_play_mode[MAX_FILES];
uint16_t song_vbi_hz[MAX_FILES];
uint16_t song_cia_hz[MAX_FILES];
uint8_t song_unsafe_low_load[MAX_FILES];

uint8_t default_play_mode = PLAYMODE_AUTO;
uint16_t default_vbi_hz = 50;
uint16_t default_cia_hz = 50;
uint8_t default_unsafe_low_load = 0;

uint8_t play_mode = PLAYMODE_AUTO;
uint8_t unsafe_low_load = 0;
uint16_t custom_vbi_hz = 50;
uint16_t custom_cia_hz = 50;

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

char sid_title[33];
char sid_author[33];

uint16_t current_call;
uint8_t current_song;

extern void sid_call_init(void);
extern void sid_call_play(void);
extern void sid_prepare_song(void);

void setup_screen(void)
{
    bordercolor(COLOR_BLACK);
    bgcolor(COLOR_BLACK);
    textcolor(COLOR_WHITE);
    clrscr();
}

void print_sidplay_logo(char *info)
{
    textcolor(COLOR_LIGHTGREEN);
    cprintf("SID");

    textcolor(COLOR_ORANGE);
    cprintf("Play");

    textcolor(COLOR_WHITE);

    if (info != 0 && info[0] != 0)
        cprintf(" %s\r\n", info);
    else
        cprintf("\r\n");

    cprintf("----------------------------------------\r\n");
}

void print_help(void)
{
    cprintf("\r\n");
    cprintf("UP/DOWN move\r\n");
    cprintf("RET/RIGHT enter\r\n");
    cprintf("Q/LEFT back\r\n");
}

void print_main_help(void)
{
    cprintf("\r\n");
    cprintf("UP/DOWN move\r\n");
    cprintf("RET/RIGHT enter\r\n");
    cprintf("Q exit program\r\n");
}

void print_cursor_line(uint8_t selected, char *text)
{
    if (selected)
    {
        textcolor(COLOR_YELLOW);
        cprintf("> %s\r\n", text);
        textcolor(COLOR_WHITE);
    }
    else
    {
        cprintf("  %s\r\n", text);
    }
}

uint8_t menu_key(void)
{
    uint8_t key;

    key = cgetc();

    if (key == 'w' || key == 'W')
        return KEY_UP;

    if (key == 's' || key == 'S')
        return KEY_DOWN;

    if (key == 'a' || key == 'A')
        return KEY_LEFT;

    if (key == 'd' || key == 'D')
        return KEY_RIGHT;

    return key;
}

uint8_t is_back_key(uint8_t key)
{
    return key == 'q' || key == 'Q' || key == KEY_LEFT;
}

uint8_t is_program_exit_key(uint8_t key)
{
    return key == 'q' || key == 'Q';
}

void wait_for_back(void)
{
    uint8_t key;

    while (1)
    {
        key = menu_key();

        if (is_back_key(key))
            return;
    }
}

uint16_t be16(uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

uint16_t le16(uint8_t *p)
{
    return p[0] | ((uint16_t)p[1] << 8);
}

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

void copy_sid_text(uint16_t offset, char *dest)
{
    uint8_t i;
    uint8_t c;

    for (i = 0; i < 32; i++)
    {
        c = SID_BUFFER[offset + i];

        if (c == 0)
            break;

        dest[i] = c;
    }

    dest[i] = 0;
}

char *get_play_mode_name_by_value(uint8_t mode)
{
    if (mode == PLAYMODE_AUTO)
        return "AUTO";
    else if (mode == PLAYMODE_VBI_CUSTOM)
        return "VBI CUSTOM HZ";
    else if (mode == PLAYMODE_CIA_CUSTOM)
        return "CIA CUSTOM HZ";
    else if (mode == PLAYMODE_REJECT_CIA)
        return "REJECT CIA";

    return "UNKNOWN";
}

char *get_play_mode_name(void)
{
    return get_play_mode_name_by_value(play_mode);
}

uint8_t next_play_mode(uint8_t mode)
{
    if (mode == PLAYMODE_AUTO)
        return PLAYMODE_VBI_CUSTOM;
    else if (mode == PLAYMODE_VBI_CUSTOM)
        return PLAYMODE_CIA_CUSTOM;
    else if (mode == PLAYMODE_CIA_CUSTOM)
        return PLAYMODE_REJECT_CIA;

    return PLAYMODE_AUTO;
}

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

void apply_song_settings(uint8_t index)
{
    play_mode = song_play_mode[index];
    custom_vbi_hz = song_vbi_hz[index];
    custom_cia_hz = song_cia_hz[index];
    unsafe_low_load = song_unsafe_low_load[index];
}

void init_song_settings(void)
{
    uint8_t i;

    for (i = 0; i < MAX_FILES; i++)
    {
        song_play_mode[i] = default_play_mode;
        song_vbi_hz[i] = default_vbi_hz;
        song_cia_hz[i] = default_cia_hz;
        song_unsafe_low_load[i] = default_unsafe_low_load;
    }
}

void apply_defaults_to_all_songs(void)
{
    uint8_t i;

    for (i = 0; i < sid_count; i++)
    {
        song_play_mode[i] = default_play_mode;
        song_vbi_hz[i] = default_vbi_hz;
        song_cia_hz[i] = default_cia_hz;
        song_unsafe_low_load[i] = default_unsafe_low_load;
    }
}

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

void wait_frame(void)
{
    while (VIC.rasterline == 0)
        ;

    while (VIC.rasterline != 0)
        ;
}

void sid_silence(void)
{
    uint8_t i;
    uint8_t *sid = (uint8_t *)0xd400;

    sid[4] = 0;
    sid[11] = 0;
    sid[18] = 0;

    for (i = 0; i < 29; i++)
        sid[i] = 0;
}

void show_authors(void)
{
    setup_screen();

    print_sidplay_logo("authors");

    cprintf("Program:\r\n");
    cprintf("Adam (Adava) Cir / adamcir\r\n\r\n");

    cprintf("Year:\r\n");
    cprintf("2026\r\n\r\n");

    cprintf("Project:\r\n");
    cprintf("SIDPlay for Commodore 64\r\n\r\n");

    cprintf("Supported format:\r\n");
    cprintf("Simple PSID init/play tunes\r\n\r\n");

    cprintf("Limitations:\r\n");
    cprintf("No full RSID support\r\n");
    cprintf("No Play $0000 IRQ tunes\r\n");
    cprintf("No perfect CIA timer yet\r\n\r\n");

    cprintf("Q/LEFT = back\r\n");
    wait_for_back();
}

uint16_t input_hz(char *title, uint16_t old_value)
{
    uint16_t value = 0;
    uint8_t digits = 0;
    uint8_t key;
    uint8_t done = 0;

    setup_screen();

    print_sidplay_logo(title);

    cprintf("Old value: %u Hz\r\n", old_value);
    cprintf("Enter new value.\r\n");
    cprintf("Range: 1-300 Hz\r\n\r\n");
    cprintf("> ");

    while (!done)
    {
        key = menu_key();

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
            if (digits > 0)
            {
                value = value / 10;
                digits--;
                cputc(50);
            }
        }
        else if (key == KEY_RETURN || key == KEY_RIGHT)
        {
            done = 1;
        }
        else if (is_back_key(key))
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

void show_global_settings_menu(void)
{
    uint8_t selected = 0;
    uint8_t done = 0;
    uint8_t key;

    while (!done)
    {
        setup_screen();

        print_sidplay_logo("settings");

        if (selected == 0)
        {
            textcolor(COLOR_YELLOW);
            cprintf("> DEFAULT MODE: %s\r\n",
                    get_play_mode_name_by_value(default_play_mode));
            textcolor(COLOR_WHITE);
        }
        else
        {
            cprintf("  DEFAULT MODE: %s\r\n",
                    get_play_mode_name_by_value(default_play_mode));
        }

        if (selected == 1)
        {
            textcolor(COLOR_YELLOW);
            cprintf("> DEFAULT VBI HZ: %u\r\n", default_vbi_hz);
            textcolor(COLOR_WHITE);
        }
        else
        {
            cprintf("  DEFAULT VBI HZ: %u\r\n", default_vbi_hz);
        }

        if (selected == 2)
        {
            textcolor(COLOR_YELLOW);
            cprintf("> DEFAULT CIA HZ: %u\r\n", default_cia_hz);
            textcolor(COLOR_WHITE);
        }
        else
        {
            cprintf("  DEFAULT CIA HZ: %u\r\n", default_cia_hz);
        }

        if (selected == 3)
        {
            textcolor(COLOR_YELLOW);
            cprintf("> DEFAULT UNSAFE: %s\r\n",
                    default_unsafe_low_load ? "ON" : "OFF");
            textcolor(COLOR_WHITE);
        }
        else
        {
            cprintf("  DEFAULT UNSAFE: %s\r\n",
                    default_unsafe_low_load ? "ON" : "OFF");
        }

        print_cursor_line(selected == 4, "APPLY DEFAULTS TO ALL SONGS");
        print_cursor_line(selected == 5, "BACK");

        print_help();

        key = menu_key();

        if (key == KEY_UP)
        {
            if (selected == 0)
                selected = 5;
            else
                selected--;
        }
        else if (key == KEY_DOWN)
        {
            selected++;

            if (selected > 5)
                selected = 0;
        }
        else if (key == KEY_RETURN || key == KEY_RIGHT)
        {
            if (selected == 0)
                default_play_mode = next_play_mode(default_play_mode);
            else if (selected == 1)
                default_vbi_hz = input_hz("default VBI speed", default_vbi_hz);
            else if (selected == 2)
                default_cia_hz = input_hz("default CIA speed", default_cia_hz);
            else if (selected == 3)
                default_unsafe_low_load = !default_unsafe_low_load;
            else if (selected == 4)
                apply_defaults_to_all_songs();
            else if (selected == 5)
                done = 1;
        }
        else if (is_back_key(key))
        {
            done = 1;
        }
    }
}

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

    copy_sid_text(22, sid_title);
    copy_sid_text(54, sid_author);

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

void show_parse_error(uint8_t err)
{
    textcolor(COLOR_LIGHTRED);

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

    textcolor(COLOR_WHITE);
}

void play_tick_by_hz(uint16_t hz, uint16_t *acc)
{
    *acc += hz;

    while (*acc >= 50)
    {
        call_play(sid_play);
        *acc -= 50;
    }
}

void play_sid_file(uint8_t index)
{
    char *name;
    uint16_t size;
    uint16_t frames;
    uint8_t parse_result;
    uint8_t song_number;
    uint16_t play_hz;
    uint16_t acc;
    uint8_t stop;

    name = sid_files[index];

    apply_song_settings(index);

    setup_screen();

    print_sidplay_logo("player");

    cprintf("Loading: %s\r\n", name);

    size = load_file_raw(name);

    parse_result = parse_sid(size);

    if (parse_result != 1)
    {
        cprintf("Size: %u\r\n", size);
        cprintf("Header: %02x %02x %02x %02x\r\n",
                SID_BUFFER[0],
                SID_BUFFER[1],
                SID_BUFFER[2],
                SID_BUFFER[3]);

        show_parse_error(parse_result);
        cprintf("Q/LEFT = back\r\n");
        wait_for_back();
        return;
    }

    cprintf("Title: %s\r\n", sid_title);
    cprintf("Author: %s\r\n", sid_author);
    cprintf("File: %s\r\n", name);
    cprintf("Load: $%04x\r\n", sid_load);
    cprintf("Init: $%04x\r\n", sid_init);
    cprintf("Play: $%04x\r\n", sid_play);
    cprintf("Songs: %u\r\n", sid_songs);
    cprintf("Start: %u\r\n", sid_start_song);
    cprintf("Speed: %s\r\n", sid_use_cia ? "CIA" : "VBI");
    cprintf("Mode: %s\r\n", get_play_mode_name());

    play_hz = get_effective_hz();

    textcolor(COLOR_YELLOW);
    cprintf("Hz: %u\r\n", play_hz);
    textcolor(COLOR_WHITE);

    cprintf("\r\nPlaying...\r\n");
    cprintf("Q/LEFT = back\r\n");

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
            uint8_t key = menu_key();

            if (is_back_key(key))
                stop = 1;
        }
    }

    sid_silence();
}

uint8_t song_settings_menu(uint8_t index)
{
    uint8_t selected = 0;
    uint8_t key;

    while (1)
    {
        setup_screen();

        print_sidplay_logo("song setup");

        cprintf("Song:\r\n");
        cprintf("%s\r\n\r\n", sid_files[index]);

        print_cursor_line(selected == 0, "PLAY");

        if (selected == 1)
        {
            textcolor(COLOR_YELLOW);
            cprintf("> MODE: %s\r\n",
                    get_play_mode_name_by_value(song_play_mode[index]));
            textcolor(COLOR_WHITE);
        }
        else
        {
            cprintf("  MODE: %s\r\n",
                    get_play_mode_name_by_value(song_play_mode[index]));
        }

        if (selected == 2)
        {
            textcolor(COLOR_YELLOW);
            cprintf("> VBI HZ: %u\r\n", song_vbi_hz[index]);
            textcolor(COLOR_WHITE);
        }
        else
        {
            cprintf("  VBI HZ: %u\r\n", song_vbi_hz[index]);
        }

        if (selected == 3)
        {
            textcolor(COLOR_YELLOW);
            cprintf("> CIA HZ: %u\r\n", song_cia_hz[index]);
            textcolor(COLOR_WHITE);
        }
        else
        {
            cprintf("  CIA HZ: %u\r\n", song_cia_hz[index]);
        }

        if (selected == 4)
        {
            textcolor(COLOR_YELLOW);
            cprintf("> UNSAFE LOW LOAD: %s\r\n",
                    song_unsafe_low_load[index] ? "ON" : "OFF");
            textcolor(COLOR_WHITE);
        }
        else
        {
            cprintf("  UNSAFE LOW LOAD: %s\r\n",
                    song_unsafe_low_load[index] ? "ON" : "OFF");
        }

        print_cursor_line(selected == 5, "BACK");

        print_help();

        key = menu_key();

        if (key == KEY_UP)
        {
            if (selected == 0)
                selected = 5;
            else
                selected--;
        }
        else if (key == KEY_DOWN)
        {
            selected++;

            if (selected > 5)
                selected = 0;
        }
        else if (key == KEY_RETURN || key == KEY_RIGHT)
        {
            if (selected == 0)
                return 1;
            else if (selected == 1)
                song_play_mode[index] = next_play_mode(song_play_mode[index]);
            else if (selected == 2)
                song_vbi_hz[index] = input_hz("song VBI speed", song_vbi_hz[index]);
            else if (selected == 3)
                song_cia_hz[index] = input_hz("song CIA speed", song_cia_hz[index]);
            else if (selected == 4)
                song_unsafe_low_load[index] = !song_unsafe_low_load[index];
            else if (selected == 5)
                return 0;
        }
        else if (is_back_key(key))
        {
            return 0;
        }
    }
}

uint8_t select_song_menu(void)
{
    uint8_t selected = 0;
    uint8_t top = 0;
    uint8_t key;
    uint8_t i;
    uint8_t visible_lines = 14;

    while (1)
    {
        setup_screen();

        print_sidplay_logo("play song");

        for (i = 0; i < visible_lines; i++)
        {
            uint8_t index = top + i;

            if (index >= sid_count)
                break;

            if (index == selected)
            {
                textcolor(COLOR_YELLOW);
                cprintf("> %s\r\n", sid_files[index]);
                textcolor(COLOR_WHITE);
            }
            else
            {
                cprintf("  %s\r\n", sid_files[index]);
            }
        }

        print_help();

        key = menu_key();

        if (key == KEY_UP)
        {
            if (selected == 0)
                selected = sid_count - 1;
            else
                selected--;

            if (selected < top)
                top = selected;
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
        else if (key == KEY_RETURN || key == KEY_RIGHT)
        {
            return selected;
        }
        else if (is_back_key(key))
        {
            return 255;
        }
    }
}

void play_song_section(void)
{
    uint8_t selected;
    uint8_t should_play;

    while (1)
    {
        selected = select_song_menu();

        if (selected == 255)
            return;

        should_play = song_settings_menu(selected);

        if (should_play)
            play_sid_file(selected);
    }
}

void main_menu(void)
{
    uint8_t selected = 0;
    uint8_t key;
    uint8_t done = 0;

    while (!done)
    {
        setup_screen();

        print_sidplay_logo("main menu");

        print_cursor_line(selected == 0, "PLAY SONG");
        print_cursor_line(selected == 1, "SETTINGS");
        print_cursor_line(selected == 2, "AUTHORS");

        print_main_help();

        key = menu_key();

        if (key == KEY_UP)
        {
            if (selected == 0)
                selected = 2;
            else
                selected--;
        }
        else if (key == KEY_DOWN)
        {
            selected++;

            if (selected > 2)
                selected = 0;
        }
        else if (key == KEY_RETURN || key == KEY_RIGHT)
        {
            if (selected == 0)
                play_song_section();
            else if (selected == 1)
                show_global_settings_menu();
            else if (selected == 2)
                show_authors();
        }
        else if (is_program_exit_key(key))
        {
            done = 1;
        }
    }
}

int main(void)
{
    setup_screen();

    print_sidplay_logo("boot");
    cprintf("Scanning disk...\r\n");

    scan_directory();

    init_song_settings();

    if (sid_count == 0)
    {
        setup_screen();
        print_sidplay_logo("disk");
        cprintf("No .SID files found.\r\n");
        cprintf("Returning to BASIC...\r\n");
        return 0;
    }

    main_menu();

    setup_screen();
    sid_silence();

    print_sidplay_logo("exit");

    cprintf("SIDPlay finished.\r\n");
    cprintf("Returning to BASIC...\r\n");

    return 0;
}