/*
 * QEMU keysym to keycode conversion using rdesktop keymaps
 *
 * Copyright (c) 2004 Johannes Schindelin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "keymaps.h"
#include "sysemu.h"

#include "linux_keycodes.h"

static int get_keysym(const name2keysym_t *table,
		      const char *name)
{
    const name2keysym_t *p;
    for(p = table; p->name != NULL; p++) {
        if (!strcmp(p->name, name))
            return p->keysym;
    }
    return 0;
}


static void add_to_key_range(struct key_range **krp, int code) {
    struct key_range *kr;
    for (kr = *krp; kr; kr = kr->next) {
	if (code >= kr->start && code <= kr->end)
	    break;
	if (code == kr->start - 1) {
	    kr->start--;
	    break;
	}
	if (code == kr->end + 1) {
	    kr->end++;
	    break;
	}
    }
    if (kr == NULL) {
	kr = qemu_mallocz(sizeof(*kr));
        kr->start = kr->end = code;
        kr->next = *krp;
        *krp = kr;
    }
}

static kbd_layout_t *parse_keyboard_layout(const name2keysym_t *table,
					   const char *language,
					   kbd_layout_t * k)
{
    FILE *f;
    /* This file is used by both, UI and core components. There are differences
     * in the way how keymap file path is obtained for these two different
     * configurations. */
#if defined(CONFIG_STANDALONE_UI)
    char filename[2048];
#else
    char * filename;
#endif  // CONFIG_STANDALONE_UI
    char line[1024];
    int len;

#if defined(CONFIG_STANDALONE_UI)
    if (android_core_qemu_find_file(QEMU_FILE_TYPE_KEYMAP, language,
                                    filename, sizeof(filename))) {
        fprintf(stderr,
            "Could not read keymap file: '%s'\n", language);
        return NULL;
    }
#else
    filename = qemu_find_file(QEMU_FILE_TYPE_KEYMAP, language);
    if (!filename) {
        fprintf(stderr,
            "Could not read keymap file: '%s'\n", language);
        return NULL;
    }
#endif  // CONFIG_STANDALONE_UI

    if (!k)
	k = qemu_mallocz(sizeof(kbd_layout_t));
    if (!(f = fopen(filename, "r"))) {
	fprintf(stderr,
		"Could not read keymap file: '%s'\n", language);
	return NULL;
    }
#if defined(CONFIG_STANDALONE_UI)
    qemu_free(filename);
#endif  // CONFIG_STANDALONE_UI
    for(;;) {
	if (fgets(line, 1024, f) == NULL)
            break;
        len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';
        if (line[0] == '#')
	    continue;
	if (!strncmp(line, "map ", 4))
	    continue;
	if (!strncmp(line, "include ", 8)) {
	    parse_keyboard_layout(table, line + 8, k);
        } else {
	    char *end_of_keysym = line;
	    while (*end_of_keysym != 0 && *end_of_keysym != ' ')
		end_of_keysym++;
	    if (*end_of_keysym) {
		int keysym;
		*end_of_keysym = 0;
		keysym = get_keysym(table, line);
		if (keysym == 0) {
                    //		    fprintf(stderr, "Warning: unknown keysym %s\n", line);
		} else {
		    const char *rest = end_of_keysym + 1;
		    char *rest2;
		    int keycode = strtol(rest, &rest2, 0);

		    if (rest && strstr(rest, "numlock")) {
			add_to_key_range(&k->keypad_range, keycode);
			add_to_key_range(&k->numlock_range, keysym);
			//fprintf(stderr, "keypad keysym %04x keycode %d\n", keysym, keycode);
		    }

		    /* if(keycode&0x80)
		       keycode=(keycode<<8)^0x80e0; */
		    if (keysym < MAX_NORMAL_KEYCODE) {
			//fprintf(stderr,"Setting keysym %s (%d) to %d\n",line,keysym,keycode);
			k->keysym2keycode[keysym] = keycode;
		    } else {
			if (k->extra_count >= MAX_EXTRA_COUNT) {
			    fprintf(stderr,
				    "Warning: Could not assign keysym %s (0x%x) because of memory constraints.\n",
				    line, keysym);
			} else {
#if 0
			    fprintf(stderr, "Setting %d: %d,%d\n",
				    k->extra_count, keysym, keycode);
#endif
			    k->keysym2keycode_extra[k->extra_count].
				keysym = keysym;
			    k->keysym2keycode_extra[k->extra_count].
				keycode = keycode;
			    k->extra_count++;
			}
		    }
		}
	    }
	}
    }
    fclose(f);
    return k;
}


void *init_keyboard_layout(const name2keysym_t *table, const char *language)
{
    return parse_keyboard_layout(table, language, NULL);
}


int keysym2scancode(void *kbd_layout, int keysym)
{
	/* + Modified by James */
#if 0
    kbd_layout_t *k = kbd_layout;
    if (keysym < MAX_NORMAL_KEYCODE) {
	if (k->keysym2keycode[keysym] == 0)
	    fprintf(stderr, "Warning: no scancode found for keysym %d\n",
		    keysym);
	return k->keysym2keycode[keysym];
    } else {
	int i;
#ifdef XK_ISO_Left_Tab
	if (keysym == XK_ISO_Left_Tab)
	    keysym = XK_Tab;
#endif
	for (i = 0; i < k->extra_count; i++)
	    if (k->keysym2keycode_extra[i].keysym == keysym)
		return k->keysym2keycode_extra[i].keycode;
    }

    return 0;
#else
    int scancode = 0;
    int code = (int)keysym;

    if (code >= '0' && code <= '9') {
    	scancode = (code & 0xF) - 1;

    	if (scancode < 0)
    		scancode += 10;

    	scancode += KEY_1;
    } else if (code >= 0xFF50 && code <= 0xFF58) {
    	static const unsigned short int map[] = {
    		KEY_HOME, KEY_LEFT, KEY_UP, KEY_RIGHT,
    		KEY_DOWN, KEY_SOFT1, KEY_SOFT2, KEY_END,
    		0,
    	};

    	scancode = map[code & 0xF];
    } else if (code >= 0xFFE1 && code <= 0xFFEE) {
    	static const unsigned short int map[] = {
    		KEY_LEFTSHIFT, KEY_RIGHTSHIFT, KEY_COMPOSE, KEY_COMPOSE,
    		KEY_CAPSLOCK, KEY_LEFTSHIFT, KEY_LEFTMETA, KEY_RIGHTMETA,
    		KEY_LEFTALT, KEY_RIGHTALT, 0, 0,
    		0, 0
    	};

    	scancode = map[code & 0xF];
    } else if ((code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z')) {
    	static const unsigned short int map[] = {
    		KEY_A, KEY_B, KEY_C, KEY_D, KEY_E,
    		KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    		KEY_K, KEY_L, KEY_M, KEY_N, KEY_O,
    		KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    		KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y,
    		KEY_Z
    	};

    	scancode = map[(code & 0x5F) - 'A'];
    } else if (code >= 0xFFBE && code <= 0xFFD5) {
    	static const unsigned short int map[] = {
    			KEY_F1, KEY_F2, KEY_F3, KEY_F4,
    			KEY_F5, KEY_F6, KEY_F7, KEY_F8,
    			KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    			KEY_F13, KEY_F14, KEY_F15, KEY_F16,
    			KEY_F17, KEY_F18, KEY_F19, KEY_F20,
    			KEY_F21, KEY_F22, KEY_F23, KEY_F24
    	};

    	scancode = map[code - 0xFFBE];
    } else if (code >= 0xFF08 && code <= 0xFF0D) {
    	static const unsigned short int map[] = {
    			KEY_BACKSPACE, KEY_TAB, KEY_LINEFEED, KEY_CLEAR,
    			0, KEY_ENTER
    	};

    	scancode = map[code - 0xFF08];
    } else if (code >= 0xFF13 && code <= 0xFF15) {
    	static const unsigned short int map[] = {
    			KEY_PAUSE, KEY_SCROLLLOCK, KEY_SYSRQ,
    	};

    	scancode = map[code - 0xFF13];
    } else {
    	switch(code) {
    		case 0x0003: scancode = KEY_CENTER; break;
    		case 0x0020: scancode = KEY_SPACE; break;
    		case 0x0023: scancode = KEY_SHARP; break;
    		case 0x0033: scancode = KEY_SHARP; break;
    		case 0x002C: scancode = KEY_COMMA; break;
    		case 0x003C: scancode = KEY_COMMA; break;
    		case 0x002E: scancode = KEY_DOT; break;
    		case 0x003E: scancode = KEY_DOT; break;
    		case 0x002F: scancode = KEY_SLASH; break;
    		case 0x003F: scancode = KEY_SLASH; break;
    		case 0x0032: scancode = KEY_EMAIL; break;
    		case 0x0040: scancode = KEY_EMAIL; break;
    		case 0xFF1B: scancode = KEY_BACK; break;
    		case 0xFFFF: scancode = KEY_DELETE; break;
    		case 0x002A: scancode = KEY_STAR; break;
		case 0xFFAB: scancode = KEY_VOLUMEUP; break;
		case 0xFFAD: scancode = KEY_VOLUMEDOWN; break;
    	}
    }

    return scancode;
#endif
    /* - End of modification */
}

int keycode_is_keypad(void *kbd_layout, int keycode)
{
    kbd_layout_t *k = kbd_layout;
    struct key_range *kr;

    for (kr = k->keypad_range; kr; kr = kr->next)
        if (keycode >= kr->start && keycode <= kr->end)
            return 1;
    return 0;
}

int keysym_is_numlock(void *kbd_layout, int keysym)
{
    kbd_layout_t *k = kbd_layout;
    struct key_range *kr;

    for (kr = k->numlock_range; kr; kr = kr->next)
        if (keysym >= kr->start && keysym <= kr->end)
            return 1;
    return 0;
}
