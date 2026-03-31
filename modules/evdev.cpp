module;

#include <stdint.h>
#include <string_view>
#include <span>

export module evdev;

import format;

using namespace std;

export enum class evdev_key_code : uint16_t {
    KEY_RESERVED = 0,
    KEY_ESC = 1,
    KEY_1 = 2,
    KEY_2 = 3,
    KEY_3 = 4,
    KEY_4 = 5,
    KEY_5 = 6,
    KEY_6 = 7,
    KEY_7 = 8,
    KEY_8 = 9,
    KEY_9 = 10,
    KEY_0 = 11,
    KEY_MINUS = 12,
    KEY_EQUAL = 13,
    KEY_BACKSPACE = 14,
    KEY_TAB = 15,
    KEY_Q = 16,
    KEY_W = 17,
    KEY_E = 18,
    KEY_R = 19,
    KEY_T = 20,
    KEY_Y = 21,
    KEY_U = 22,
    KEY_I = 23,
    KEY_O = 24,
    KEY_P = 25,
    KEY_LEFTBRACE = 26,
    KEY_RIGHTBRACE = 27,
    KEY_ENTER = 28,
    KEY_LEFTCTRL = 29,
    KEY_A = 30,
    KEY_S = 31,
    KEY_D = 32,
    KEY_F = 33,
    KEY_G = 34,
    KEY_H = 35,
    KEY_J = 36,
    KEY_K = 37,
    KEY_L = 38,
    KEY_SEMICOLON = 39,
    KEY_APOSTROPHE = 40,
    KEY_GRAVE = 41,
    KEY_LEFTSHIFT = 42,
    KEY_BACKSLASH = 43,
    KEY_Z = 44,
    KEY_X = 45,
    KEY_C = 46,
    KEY_V = 47,
    KEY_B = 48,
    KEY_N = 49,
    KEY_M = 50,
    KEY_COMMA = 51,
    KEY_DOT = 52,
    KEY_SLASH = 53,
    KEY_RIGHTSHIFT = 54,
    KEY_KPASTERISK = 55,
    KEY_LEFTALT = 56,
    KEY_SPACE = 57,
    KEY_CAPSLOCK = 58,
    KEY_F1 = 59,
    KEY_F2 = 60,
    KEY_F3 = 61,
    KEY_F4 = 62,
    KEY_F5 = 63,
    KEY_F6 = 64,
    KEY_F7 = 65,
    KEY_F8 = 66,
    KEY_F9 = 67,
    KEY_F10 = 68,
    KEY_NUMLOCK = 69,
    KEY_SCROLLLOCK = 70,
    KEY_KP7 = 71,
    KEY_KP8 = 72,
    KEY_KP9 = 73,
    KEY_KPMINUS = 74,
    KEY_KP4 = 75,
    KEY_KP5 = 76,
    KEY_KP6 = 77,
    KEY_KPPLUS = 78,
    KEY_KP1 = 79,
    KEY_KP2 = 80,
    KEY_KP3 = 81,
    KEY_KP0 = 82,
    KEY_KPDOT = 83,
    KEY_ZENKAKUHANKAKU = 85,
    KEY_102ND = 86,
    KEY_F11 = 87,
    KEY_F12 = 88,
    KEY_RO = 89,
    KEY_KATAKANA = 90,
    KEY_HIRAGANA = 91,
    KEY_HENKAN = 92,
    KEY_KATAKANAHIRAGANA = 93,
    KEY_MUHENKAN = 94,
    KEY_KPJPCOMMA = 95,
    KEY_KPENTER = 96,
    KEY_RIGHTCTRL = 97,
    KEY_KPSLASH = 98,
    KEY_SYSRQ = 99,
    KEY_RIGHTALT = 100,
    KEY_LINEFEED = 101,
    KEY_HOME = 102,
    KEY_UP = 103,
    KEY_PAGEUP = 104,
    KEY_LEFT = 105,
    KEY_RIGHT = 106,
    KEY_END = 107,
    KEY_DOWN = 108,
    KEY_PAGEDOWN = 109,
    KEY_INSERT = 110,
    KEY_DELETE = 111,
    KEY_MACRO = 112,
    KEY_MUTE = 113,
    KEY_VOLUMEDOWN = 114,
    KEY_VOLUMEUP = 115,
    KEY_POWER = 116,
    KEY_KPEQUAL = 117,
    KEY_KPPLUSMINUS = 118,
    KEY_PAUSE = 119,
    KEY_SCALE = 120,
    KEY_KPCOMMA = 121,
    KEY_HANGEUL = 122,
    KEY_HANJA = 123,
    KEY_YEN = 124,
    KEY_LEFTMETA = 125,
    KEY_RIGHTMETA = 126,
    KEY_COMPOSE = 127,
    KEY_STOP = 128,
    KEY_AGAIN = 129,
    KEY_PROPS = 130,
    KEY_UNDO = 131,
    KEY_FRONT = 132,
    KEY_COPY = 133,
    KEY_OPEN = 134,
    KEY_PASTE = 135,
    KEY_FIND = 136,
    KEY_CUT = 137,
    KEY_HELP = 138,
    KEY_MENU = 139,
    KEY_CALC = 140,
    KEY_SETUP = 141,
    KEY_SLEEP = 142,
    KEY_WAKEUP = 143,
    KEY_FILE = 144,
    KEY_SENDFILE = 145,
    KEY_DELETEFILE = 146,
    KEY_XFER = 147,
    KEY_PROG1 = 148,
    KEY_PROG2 = 149,
    KEY_WWW = 150,
    KEY_MSDOS = 151,
    KEY_COFFEE = 152,
    KEY_ROTATE_DISPLAY = 153,
    KEY_CYCLEWINDOWS = 154,
    KEY_MAIL = 155,
    KEY_BOOKMARKS = 156,
    KEY_COMPUTER = 157,
    KEY_BACK = 158,
    KEY_FORWARD = 159,
    KEY_CLOSECD = 160,
    KEY_EJECTCD = 161,
    KEY_EJECTCLOSECD = 162,
    KEY_NEXTSONG = 163,
    KEY_PLAYPAUSE = 164,
    KEY_PREVIOUSSONG = 165,
    KEY_STOPCD = 166,
    KEY_RECORD = 167,
    KEY_REWIND = 168,
    KEY_PHONE = 169,
    KEY_ISO = 170,
    KEY_CONFIG = 171,
    KEY_HOMEPAGE = 172,
    KEY_REFRESH = 173,
    KEY_EXIT = 174,
    KEY_MOVE = 175,
    KEY_EDIT = 176,
    KEY_SCROLLUP = 177,
    KEY_SCROLLDOWN = 178,
    KEY_KPLEFTPAREN = 179,
    KEY_KPRIGHTPAREN = 180,
    KEY_NEW = 181,
    KEY_REDO = 182,
    KEY_F13 = 183,
    KEY_F14 = 184,
    KEY_F15 = 185,
    KEY_F16 = 186,
    KEY_F17 = 187,
    KEY_F18 = 188,
    KEY_F19 = 189,
    KEY_F20 = 190,
    KEY_F21 = 191,
    KEY_F22 = 192,
    KEY_F23 = 193,
    KEY_F24 = 194,
    KEY_PLAYCD = 200,
    KEY_PAUSECD = 201,
    KEY_PROG3 = 202,
    KEY_PROG4 = 203,
    KEY_ALL_APPLICATIONS = 204,
    KEY_SUSPEND = 205,
    KEY_CLOSE = 206,
    KEY_PLAY = 207,
    KEY_FASTFORWARD = 208,
    KEY_BASSBOOST = 209,
    KEY_PRINT = 210,
    KEY_HP = 211,
    KEY_CAMERA = 212,
    KEY_SOUND = 213,
    KEY_QUESTION = 214,
    KEY_EMAIL = 215,
    KEY_CHAT = 216,
    KEY_SEARCH = 217,
    KEY_CONNECT = 218,
    KEY_FINANCE = 219,
    KEY_SPORT = 220,
    KEY_SHOP = 221,
    KEY_ALTERASE = 222,
    KEY_CANCEL = 223,
    KEY_BRIGHTNESSDOWN = 224,
    KEY_BRIGHTNESSUP = 225,
    KEY_MEDIA = 226,
    KEY_SWITCHVIDEOMODE = 227,
    KEY_KBDILLUMTOGGLE = 228,
    KEY_KBDILLUMDOWN = 229,
    KEY_KBDILLUMUP = 230,
    KEY_SEND = 231,
    KEY_REPLY = 232,
    KEY_FORWARDMAIL = 233,
    KEY_SAVE = 234,
    KEY_DOCUMENTS = 235,
    KEY_BATTERY = 236,
    KEY_BLUETOOTH = 237,
    KEY_WLAN = 238,
    KEY_UWB = 239,
    KEY_UNKNOWN = 240,
    KEY_VIDEO_NEXT = 241,
    KEY_VIDEO_PREV = 242,
    KEY_BRIGHTNESS_CYCLE = 243,
    KEY_BRIGHTNESS_AUTO = 244,
    KEY_DISPLAY_OFF = 245,
    KEY_WWAN = 246,
    KEY_RFKILL = 247,
    KEY_MICMUTE = 248,
    BTN_MISC = 0x100,
    BTN_0 = 0x100,
    BTN_1 = 0x101,
    BTN_2 = 0x102,
    BTN_3 = 0x103,
    BTN_4 = 0x104,
    BTN_5 = 0x105,
    BTN_6 = 0x106,
    BTN_7 = 0x107,
    BTN_8 = 0x108,
    BTN_9 = 0x109,
    BTN_MOUSE = 0x110,
    BTN_LEFT = 0x110,
    BTN_RIGHT = 0x111,
    BTN_MIDDLE = 0x112,
    BTN_SIDE = 0x113,
    BTN_EXTRA = 0x114,
    BTN_FORWARD = 0x115,
    BTN_BACK = 0x116,
    BTN_TASK = 0x117,
    BTN_JOYSTICK = 0x120,
    BTN_TRIGGER = 0x120,
    BTN_THUMB = 0x121,
    BTN_THUMB2 = 0x122,
    BTN_TOP = 0x123,
    BTN_TOP2 = 0x124,
    BTN_PINKIE = 0x125,
    BTN_BASE = 0x126,
    BTN_BASE2 = 0x127,
    BTN_BASE3 = 0x128,
    BTN_BASE4 = 0x129,
    BTN_BASE5 = 0x12a,
    BTN_BASE6 = 0x12b,
    BTN_DEAD = 0x12f,
    BTN_GAMEPAD = 0x130,
    BTN_SOUTH = 0x130,
    BTN_EAST = 0x131,
    BTN_C = 0x132,
    BTN_NORTH = 0x133,
    BTN_WEST = 0x134,
    BTN_Z = 0x135,
    BTN_TL = 0x136,
    BTN_TR = 0x137,
    BTN_TL2 = 0x138,
    BTN_TR2 = 0x139,
    BTN_SELECT = 0x13a,
    BTN_START = 0x13b,
    BTN_MODE = 0x13c,
    BTN_THUMBL = 0x13d,
    BTN_THUMBR = 0x13e,
    BTN_DIGI = 0x140,
    BTN_TOOL_PEN = 0x140,
    BTN_TOOL_RUBBER = 0x141,
    BTN_TOOL_BRUSH = 0x142,
    BTN_TOOL_PENCIL = 0x143,
    BTN_TOOL_AIRBRUSH = 0x144,
    BTN_TOOL_FINGER = 0x145,
    BTN_TOOL_MOUSE = 0x146,
    BTN_TOOL_LENS = 0x147,
    BTN_TOOL_QUINTTAP = 0x148,
    BTN_STYLUS3 = 0x149,
    BTN_TOUCH = 0x14a,
    BTN_STYLUS = 0x14b,
    BTN_STYLUS2 = 0x14c,
    BTN_TOOL_DOUBLETAP = 0x14d,
    BTN_TOOL_TRIPLETAP = 0x14e,
    BTN_TOOL_QUADTAP = 0x14f,
    BTN_WHEEL = 0x150,
    BTN_GEAR_DOWN = 0x150,
    BTN_GEAR_UP = 0x151,
    KEY_OK = 0x160,
    KEY_SELECT = 0x161,
    KEY_GOTO = 0x162,
    KEY_CLEAR = 0x163,
    KEY_POWER2 = 0x164,
    KEY_OPTION = 0x165,
    KEY_INFO = 0x166,
    KEY_TIME = 0x167,
    KEY_VENDOR = 0x168,
    KEY_ARCHIVE = 0x169,
    KEY_PROGRAM = 0x16a,
    KEY_CHANNEL = 0x16b,
    KEY_FAVORITES = 0x16c,
    KEY_EPG = 0x16d,
    KEY_PVR = 0x16e,
    KEY_MHP = 0x16f,
    KEY_LANGUAGE = 0x170,
    KEY_TITLE = 0x171,
    KEY_SUBTITLE = 0x172,
    KEY_ANGLE = 0x173,
    KEY_FULL_SCREEN = 0x174,
    KEY_MODE = 0x175,
    KEY_KEYBOARD = 0x176,
    KEY_ASPECT_RATIO = 0x177,
    KEY_PC = 0x178,
    KEY_TV = 0x179,
    KEY_TV2 = 0x17a,
    KEY_VCR = 0x17b,
    KEY_VCR2 = 0x17c,
    KEY_SAT = 0x17d,
    KEY_SAT2 = 0x17e,
    KEY_CD = 0x17f,
    KEY_TAPE = 0x180,
    KEY_RADIO = 0x181,
    KEY_TUNER = 0x182,
    KEY_PLAYER = 0x183,
    KEY_TEXT = 0x184,
    KEY_DVD = 0x185,
    KEY_AUX = 0x186,
    KEY_MP3 = 0x187,
    KEY_AUDIO = 0x188,
    KEY_VIDEO = 0x189,
    KEY_DIRECTORY = 0x18a,
    KEY_LIST = 0x18b,
    KEY_MEMO = 0x18c,
    KEY_CALENDAR = 0x18d,
    KEY_RED = 0x18e,
    KEY_GREEN = 0x18f,
    KEY_YELLOW = 0x190,
    KEY_BLUE = 0x191,
    KEY_CHANNELUP = 0x192,
    KEY_CHANNELDOWN = 0x193,
    KEY_FIRST = 0x194,
    KEY_LAST = 0x195,
    KEY_AB = 0x196,
    KEY_NEXT = 0x197,
    KEY_RESTART = 0x198,
    KEY_SLOW = 0x199,
    KEY_SHUFFLE = 0x19a,
    KEY_BREAK = 0x19b,
    KEY_PREVIOUS = 0x19c,
    KEY_DIGITS = 0x19d,
    KEY_TEEN = 0x19e,
    KEY_TWEN = 0x19f,
    KEY_VIDEOPHONE = 0x1a0,
    KEY_GAMES = 0x1a1,
    KEY_ZOOMIN = 0x1a2,
    KEY_ZOOMOUT = 0x1a3,
    KEY_ZOOMRESET = 0x1a4,
    KEY_WORDPROCESSOR = 0x1a5,
    KEY_EDITOR = 0x1a6,
    KEY_SPREADSHEET = 0x1a7,
    KEY_GRAPHICSEDITOR = 0x1a8,
    KEY_PRESENTATION = 0x1a9,
    KEY_DATABASE = 0x1aa,
    KEY_NEWS = 0x1ab,
    KEY_VOICEMAIL = 0x1ac,
    KEY_ADDRESSBOOK = 0x1ad,
    KEY_MESSENGER = 0x1ae,
    KEY_DISPLAYTOGGLE = 0x1af,
    KEY_SPELLCHECK = 0x1b0,
    KEY_LOGOFF = 0x1b1,
    KEY_DOLLAR = 0x1b2,
    KEY_EURO = 0x1b3,
    KEY_FRAMEBACK = 0x1b4,
    KEY_FRAMEFORWARD = 0x1b5,
    KEY_CONTEXT_MENU = 0x1b6,
    KEY_MEDIA_REPEAT = 0x1b7,
    KEY_10CHANNELSUP = 0x1b8,
    KEY_10CHANNELSDOWN = 0x1b9,
    KEY_IMAGES = 0x1ba,
    KEY_NOTIFICATION_CENTER = 0x1bc,
    KEY_PICKUP_PHONE = 0x1bd,
    KEY_HANGUP_PHONE = 0x1be,
    KEY_DEL_EOL = 0x1c0,
    KEY_DEL_EOS = 0x1c1,
    KEY_INS_LINE = 0x1c2,
    KEY_DEL_LINE = 0x1c3,
    KEY_FN = 0x1d0,
    KEY_FN_ESC = 0x1d1,
    KEY_FN_F1 = 0x1d2,
    KEY_FN_F2 = 0x1d3,
    KEY_FN_F3 = 0x1d4,
    KEY_FN_F4 = 0x1d5,
    KEY_FN_F5 = 0x1d6,
    KEY_FN_F6 = 0x1d7,
    KEY_FN_F7 = 0x1d8,
    KEY_FN_F8 = 0x1d9,
    KEY_FN_F9 = 0x1da,
    KEY_FN_F10 = 0x1db,
    KEY_FN_F11 = 0x1dc,
    KEY_FN_F12 = 0x1dd,
    KEY_FN_1 = 0x1de,
    KEY_FN_2 = 0x1df,
    KEY_FN_D = 0x1e0,
    KEY_FN_E = 0x1e1,
    KEY_FN_F = 0x1e2,
    KEY_FN_S = 0x1e3,
    KEY_FN_B = 0x1e4,
    KEY_FN_RIGHT_SHIFT = 0x1e5,
    KEY_BRL_DOT1 = 0x1f1,
    KEY_BRL_DOT2 = 0x1f2,
    KEY_BRL_DOT3 = 0x1f3,
    KEY_BRL_DOT4 = 0x1f4,
    KEY_BRL_DOT5 = 0x1f5,
    KEY_BRL_DOT6 = 0x1f6,
    KEY_BRL_DOT7 = 0x1f7,
    KEY_BRL_DOT8 = 0x1f8,
    KEY_BRL_DOT9 = 0x1f9,
    KEY_BRL_DOT10 = 0x1fa,
    KEY_NUMERIC_0 = 0x200,
    KEY_NUMERIC_1 = 0x201,
    KEY_NUMERIC_2 = 0x202,
    KEY_NUMERIC_3 = 0x203,
    KEY_NUMERIC_4 = 0x204,
    KEY_NUMERIC_5 = 0x205,
    KEY_NUMERIC_6 = 0x206,
    KEY_NUMERIC_7 = 0x207,
    KEY_NUMERIC_8 = 0x208,
    KEY_NUMERIC_9 = 0x209,
    KEY_NUMERIC_STAR = 0x20a,
    KEY_NUMERIC_POUND = 0x20b,
    KEY_NUMERIC_A = 0x20c,
    KEY_NUMERIC_B = 0x20d,
    KEY_NUMERIC_C = 0x20e,
    KEY_NUMERIC_D = 0x20f,
    KEY_CAMERA_FOCUS = 0x210,
    KEY_WPS_BUTTON = 0x211,
    KEY_TOUCHPAD_TOGGLE = 0x212,
    KEY_TOUCHPAD_ON = 0x213,
    KEY_TOUCHPAD_OFF = 0x214,
    KEY_CAMERA_ZOOMIN = 0x215,
    KEY_CAMERA_ZOOMOUT = 0x216,
    KEY_CAMERA_UP = 0x217,
    KEY_CAMERA_DOWN = 0x218,
    KEY_CAMERA_LEFT = 0x219,
    KEY_CAMERA_RIGHT = 0x21a,
    KEY_ATTENDANT_ON = 0x21b,
    KEY_ATTENDANT_OFF = 0x21c,
    KEY_ATTENDANT_TOGGLE = 0x21d,
    KEY_LIGHTS_TOGGLE = 0x21e,
    BTN_DPAD_UP = 0x220,
    BTN_DPAD_DOWN = 0x221,
    BTN_DPAD_LEFT = 0x222,
    BTN_DPAD_RIGHT = 0x223,
    KEY_ALS_TOGGLE = 0x230,
    KEY_ROTATE_LOCK_TOGGLE = 0x231,
    KEY_REFRESH_RATE_TOGGLE = 0x232,
    KEY_BUTTONCONFIG = 0x240,
    KEY_TASKMANAGER = 0x241,
    KEY_JOURNAL = 0x242,
    KEY_CONTROLPANEL = 0x243,
    KEY_APPSELECT = 0x244,
    KEY_SCREENSAVER = 0x245,
    KEY_VOICECOMMAND = 0x246,
    KEY_ASSISTANT = 0x247,
    KEY_KBD_LAYOUT_NEXT = 0x248,
    KEY_EMOJI_PICKER = 0x249,
    KEY_DICTATE = 0x24a,
    KEY_CAMERA_ACCESS_ENABLE = 0x24b,
    KEY_CAMERA_ACCESS_DISABLE = 0x24c,
    KEY_CAMERA_ACCESS_TOGGLE = 0x24d,
    KEY_ACCESSIBILITY = 0x24e,
    KEY_DO_NOT_DISTURB = 0x24f,
    KEY_BRIGHTNESS_MIN = 0x250,
    KEY_BRIGHTNESS_MAX = 0x251,
    KEY_KBDINPUTASSIST_PREV = 0x260,
    KEY_KBDINPUTASSIST_NEXT = 0x261,
    KEY_KBDINPUTASSIST_PREVGROUP = 0x262,
    KEY_KBDINPUTASSIST_NEXTGROUP = 0x263,
    KEY_KBDINPUTASSIST_ACCEPT = 0x264,
    KEY_KBDINPUTASSIST_CANCEL = 0x265,
    KEY_RIGHT_UP = 0x266,
    KEY_RIGHT_DOWN = 0x267,
    KEY_LEFT_UP = 0x268,
    KEY_LEFT_DOWN = 0x269,
    KEY_ROOT_MENU = 0x26a,
    KEY_MEDIA_TOP_MENU = 0x26b,
    KEY_NUMERIC_11 = 0x26c,
    KEY_NUMERIC_12 = 0x26d,
    KEY_AUDIO_DESC = 0x26e,
    KEY_3D_MODE = 0x26f,
    KEY_NEXT_FAVORITE = 0x270,
    KEY_STOP_RECORD = 0x271,
    KEY_PAUSE_RECORD = 0x272,
    KEY_VOD = 0x273,
    KEY_UNMUTE = 0x274,
    KEY_FASTREVERSE = 0x275,
    KEY_SLOWREVERSE = 0x276,
    KEY_DATA = 0x277,
    KEY_ONSCREEN_KEYBOARD = 0x278,
    KEY_PRIVACY_SCREEN_TOGGLE = 0x279,
    KEY_SELECTIVE_SCREENSHOT = 0x27a,
    KEY_NEXT_ELEMENT = 0x27b,
    KEY_PREVIOUS_ELEMENT = 0x27c,
    KEY_AUTOPILOT_ENGAGE_TOGGLE = 0x27d,
    KEY_MARK_WAYPOINT = 0x27e,
    KEY_SOS = 0x27f,
    KEY_NAV_CHART = 0x280,
    KEY_FISHING_CHART = 0x281,
    KEY_SINGLE_RANGE_RADAR = 0x282,
    KEY_DUAL_RANGE_RADAR = 0x283,
    KEY_RADAR_OVERLAY = 0x284,
    KEY_TRADITIONAL_SONAR = 0x285,
    KEY_CLEARVU_SONAR = 0x286,
    KEY_SIDEVU_SONAR = 0x287,
    KEY_NAV_INFO = 0x288,
    KEY_BRIGHTNESS_MENU = 0x289,
    KEY_MACRO1 = 0x290,
    KEY_MACRO2 = 0x291,
    KEY_MACRO3 = 0x292,
    KEY_MACRO4 = 0x293,
    KEY_MACRO5 = 0x294,
    KEY_MACRO6 = 0x295,
    KEY_MACRO7 = 0x296,
    KEY_MACRO8 = 0x297,
    KEY_MACRO9 = 0x298,
    KEY_MACRO10 = 0x299,
    KEY_MACRO11 = 0x29a,
    KEY_MACRO12 = 0x29b,
    KEY_MACRO13 = 0x29c,
    KEY_MACRO14 = 0x29d,
    KEY_MACRO15 = 0x29e,
    KEY_MACRO16 = 0x29f,
    KEY_MACRO17 = 0x2a0,
    KEY_MACRO18 = 0x2a1,
    KEY_MACRO19 = 0x2a2,
    KEY_MACRO20 = 0x2a3,
    KEY_MACRO21 = 0x2a4,
    KEY_MACRO22 = 0x2a5,
    KEY_MACRO23 = 0x2a6,
    KEY_MACRO24 = 0x2a7,
    KEY_MACRO25 = 0x2a8,
    KEY_MACRO26 = 0x2a9,
    KEY_MACRO27 = 0x2aa,
    KEY_MACRO28 = 0x2ab,
    KEY_MACRO29 = 0x2ac,
    KEY_MACRO30 = 0x2ad,
    KEY_MACRO_RECORD_START = 0x2b0,
    KEY_MACRO_RECORD_STOP = 0x2b1,
    KEY_MACRO_PRESET_CYCLE = 0x2b2,
    KEY_MACRO_PRESET1 = 0x2b3,
    KEY_MACRO_PRESET2 = 0x2b4,
    KEY_MACRO_PRESET3 = 0x2b5,
    KEY_KBD_LCD_MENU1 = 0x2b8,
    KEY_KBD_LCD_MENU2 = 0x2b9,
    KEY_KBD_LCD_MENU3 = 0x2ba,
    KEY_KBD_LCD_MENU4 = 0x2bb,
    KEY_KBD_LCD_MENU5 = 0x2bc,
    BTN_TRIGGER_HAPPY = 0x2c0,
    BTN_TRIGGER_HAPPY1 = 0x2c0,
    BTN_TRIGGER_HAPPY2 = 0x2c1,
    BTN_TRIGGER_HAPPY3 = 0x2c2,
    BTN_TRIGGER_HAPPY4 = 0x2c3,
    BTN_TRIGGER_HAPPY5 = 0x2c4,
    BTN_TRIGGER_HAPPY6 = 0x2c5,
    BTN_TRIGGER_HAPPY7 = 0x2c6,
    BTN_TRIGGER_HAPPY8 = 0x2c7,
    BTN_TRIGGER_HAPPY9 = 0x2c8,
    BTN_TRIGGER_HAPPY10 = 0x2c9,
    BTN_TRIGGER_HAPPY11 = 0x2ca,
    BTN_TRIGGER_HAPPY12 = 0x2cb,
    BTN_TRIGGER_HAPPY13 = 0x2cc,
    BTN_TRIGGER_HAPPY14 = 0x2cd,
    BTN_TRIGGER_HAPPY15 = 0x2ce,
    BTN_TRIGGER_HAPPY16 = 0x2cf,
    BTN_TRIGGER_HAPPY17 = 0x2d0,
    BTN_TRIGGER_HAPPY18 = 0x2d1,
    BTN_TRIGGER_HAPPY19 = 0x2d2,
    BTN_TRIGGER_HAPPY20 = 0x2d3,
    BTN_TRIGGER_HAPPY21 = 0x2d4,
    BTN_TRIGGER_HAPPY22 = 0x2d5,
    BTN_TRIGGER_HAPPY23 = 0x2d6,
    BTN_TRIGGER_HAPPY24 = 0x2d7,
    BTN_TRIGGER_HAPPY25 = 0x2d8,
    BTN_TRIGGER_HAPPY26 = 0x2d9,
    BTN_TRIGGER_HAPPY27 = 0x2da,
    BTN_TRIGGER_HAPPY28 = 0x2db,
    BTN_TRIGGER_HAPPY29 = 0x2dc,
    BTN_TRIGGER_HAPPY30 = 0x2dd,
    BTN_TRIGGER_HAPPY31 = 0x2de,
    BTN_TRIGGER_HAPPY32 = 0x2df,
    BTN_TRIGGER_HAPPY33 = 0x2e0,
    BTN_TRIGGER_HAPPY34 = 0x2e1,
    BTN_TRIGGER_HAPPY35 = 0x2e2,
    BTN_TRIGGER_HAPPY36 = 0x2e3,
    BTN_TRIGGER_HAPPY37 = 0x2e4,
    BTN_TRIGGER_HAPPY38 = 0x2e5,
    BTN_TRIGGER_HAPPY39 = 0x2e6,
    BTN_TRIGGER_HAPPY40 = 0x2e7,
    KEY_MAX = 0x2ff,
};

export enum class evdev_syn_code : uint16_t {
    SYN_REPORT = 0x0,
    SYN_CONFIG = 0x1,
    SYN_MT_REPORT = 0x2,
    SYN_DROPPED = 0x3,
    SYN_MAX = 0xf,
};

export enum class evdev_rel_code : uint16_t {
    REL_X = 0x00,
    REL_Y = 0x01,
    REL_Z = 0x02,
    REL_RX = 0x03,
    REL_RY = 0x04,
    REL_RZ = 0x05,
    REL_HWHEEL = 0x06,
    REL_DIAL = 0x07,
    REL_WHEEL = 0x08,
    REL_MISC = 0x09,
    REL_RESERVED = 0x0a,
    REL_WHEEL_HI_RES = 0x0b,
    REL_HWHEEL_HI_RES = 0x0c,
    REL_MAX = 0x0f,
};

export enum class evdev_code : uint16_t {
    ABS_X = 0x00,
    ABS_Y = 0x01,
    ABS_Z = 0x02,
    ABS_RX = 0x03,
    ABS_RY = 0x04,
    ABS_RZ = 0x05,
    ABS_THROTTLE = 0x06,
    ABS_RUDDER = 0x07,
    ABS_WHEEL = 0x08,
    ABS_GAS = 0x09,
    ABS_BRAKE = 0x0a,
    ABS_HAT0X = 0x10,
    ABS_HAT0Y = 0x11,
    ABS_HAT1X = 0x12,
    ABS_HAT1Y = 0x13,
    ABS_HAT2X = 0x14,
    ABS_HAT2Y = 0x15,
    ABS_HAT3X = 0x16,
    ABS_HAT3Y = 0x17,
    ABS_PRESSURE = 0x18,
    ABS_DISTANCE = 0x19,
    ABS_TILT_X = 0x1a,
    ABS_TILT_Y = 0x1b,
    ABS_TOOL_WIDTH = 0x1c,
    ABS_VOLUME = 0x20,
    ABS_PROFILE = 0x21,
    ABS_SND_PROFILE = 0x22,
    ABS_MISC = 0x28,
    ABS_RESERVED = 0x2e,
    ABS_MT_SLOT = 0x2f,
    ABS_MT_TOUCH_MAJOR = 0x30,
    ABS_MT_TOUCH_MINOR = 0x31,
    ABS_MT_WIDTH_MAJOR = 0x32,
    ABS_MT_WIDTH_MINOR = 0x33,
    ABS_MT_ORIENTATION = 0x34,
    ABS_MT_POSITION_X = 0x35,
    ABS_MT_POSITION_Y = 0x36,
    ABS_MT_TOOL_TYPE = 0x37,
    ABS_MT_BLOB_ID = 0x38,
    ABS_MT_TRACKING_ID = 0x39,
    ABS_MT_PRESSURE = 0x3a,
    ABS_MT_DISTANCE = 0x3b,
    ABS_MT_TOOL_X = 0x3c,
    ABS_MT_TOOL_Y = 0x3d,
    ABS_MAX = 0x3f,
};

export enum class evdev_led : uint16_t {
    LED_NUML = 0x00,
    LED_CAPSL = 0x01,
    LED_SCROLLL = 0x02,
    LED_COMPOSE = 0x03,
    LED_KANA = 0x04,
    LED_SLEEP = 0x05,
    LED_SUSPEND = 0x06,
    LED_MUTE = 0x07,
    LED_MISC = 0x08,
    LED_MAIL = 0x09,
    LED_CHARGING = 0x0a,
    LED_MAX = 0x0f,
};

export enum class evdev_type : uint16_t {
    EV_SYN = 0x00,
    EV_KEY = 0x01,
    EV_REL = 0x02,
    EV_ABS = 0x03,
    EV_MSC = 0x04,
    EV_SW = 0x05,
    EV_LED = 0x11,
    EV_SND = 0x12,
    EV_REP = 0x14,
    EV_FF = 0x15,
    EV_PWR = 0x16,
    EV_FF_STATUS = 0x17,
    EV_MAX = 0x1f,
};

export span<char> do_format_arg(span<char> sp, string_view, evdev_type arg) {
    switch (arg) {
        case evdev_type::EV_SYN:
            return do_format_arg(sp, "", "EV_SYN");
        case evdev_type::EV_KEY:
            return do_format_arg(sp, "", "EV_KEY");
        case evdev_type::EV_REL:
            return do_format_arg(sp, "", "EV_REL");
        case evdev_type::EV_ABS:
            return do_format_arg(sp, "", "EV_ABS");
        case evdev_type::EV_MSC:
            return do_format_arg(sp, "", "EV_MSC");
        case evdev_type::EV_SW:
            return do_format_arg(sp, "", "EV_SW");
        case evdev_type::EV_LED:
            return do_format_arg(sp, "", "EV_LED");
        case evdev_type::EV_SND:
            return do_format_arg(sp, "", "EV_SND");
        case evdev_type::EV_REP:
            return do_format_arg(sp, "", "EV_REP");
        case evdev_type::EV_FF:
            return do_format_arg(sp, "", "EV_FF");
        case evdev_type::EV_PWR:
            return do_format_arg(sp, "", "EV_PWR");
        case evdev_type::EV_FF_STATUS:
            return do_format_arg(sp, "", "EV_FF_STATUS");
        default:
            return do_format_arg(sp, ":x", (uint32_t)arg);
    }
}

template<>
consteval bool format_spec_valid<evdev_key_code>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, evdev_key_code arg) {
    switch (arg) {
        using enum evdev_key_code;

        case KEY_RESERVED:
            return do_format_arg(sp, "", "KEY_RESERVED");
        case KEY_ESC:
            return do_format_arg(sp, "", "KEY_ESC");
        case KEY_1:
            return do_format_arg(sp, "", "KEY_1");
        case KEY_2:
            return do_format_arg(sp, "", "KEY_2");
        case KEY_3:
            return do_format_arg(sp, "", "KEY_3");
        case KEY_4:
            return do_format_arg(sp, "", "KEY_4");
        case KEY_5:
            return do_format_arg(sp, "", "KEY_5");
        case KEY_6:
            return do_format_arg(sp, "", "KEY_6");
        case KEY_7:
            return do_format_arg(sp, "", "KEY_7");
        case KEY_8:
            return do_format_arg(sp, "", "KEY_8");
        case KEY_9:
            return do_format_arg(sp, "", "KEY_9");
        case KEY_0:
            return do_format_arg(sp, "", "KEY_0");
        case KEY_MINUS:
            return do_format_arg(sp, "", "KEY_MINUS");
        case KEY_EQUAL:
            return do_format_arg(sp, "", "KEY_EQUAL");
        case KEY_BACKSPACE:
            return do_format_arg(sp, "", "KEY_BACKSPACE");
        case KEY_TAB:
            return do_format_arg(sp, "", "KEY_TAB");
        case KEY_Q:
            return do_format_arg(sp, "", "KEY_Q");
        case KEY_W:
            return do_format_arg(sp, "", "KEY_W");
        case KEY_E:
            return do_format_arg(sp, "", "KEY_E");
        case KEY_R:
            return do_format_arg(sp, "", "KEY_R");
        case KEY_T:
            return do_format_arg(sp, "", "KEY_T");
        case KEY_Y:
            return do_format_arg(sp, "", "KEY_Y");
        case KEY_U:
            return do_format_arg(sp, "", "KEY_U");
        case KEY_I:
            return do_format_arg(sp, "", "KEY_I");
        case KEY_O:
            return do_format_arg(sp, "", "KEY_O");
        case KEY_P:
            return do_format_arg(sp, "", "KEY_P");
        case KEY_LEFTBRACE:
            return do_format_arg(sp, "", "KEY_LEFTBRACE");
        case KEY_RIGHTBRACE:
            return do_format_arg(sp, "", "KEY_RIGHTBRACE");
        case KEY_ENTER:
            return do_format_arg(sp, "", "KEY_ENTER");
        case KEY_LEFTCTRL:
            return do_format_arg(sp, "", "KEY_LEFTCTRL");
        case KEY_A:
            return do_format_arg(sp, "", "KEY_A");
        case KEY_S:
            return do_format_arg(sp, "", "KEY_S");
        case KEY_D:
            return do_format_arg(sp, "", "KEY_D");
        case KEY_F:
            return do_format_arg(sp, "", "KEY_F");
        case KEY_G:
            return do_format_arg(sp, "", "KEY_G");
        case KEY_H:
            return do_format_arg(sp, "", "KEY_H");
        case KEY_J:
            return do_format_arg(sp, "", "KEY_J");
        case KEY_K:
            return do_format_arg(sp, "", "KEY_K");
        case KEY_L:
            return do_format_arg(sp, "", "KEY_L");
        case KEY_SEMICOLON:
            return do_format_arg(sp, "", "KEY_SEMICOLON");
        case KEY_APOSTROPHE:
            return do_format_arg(sp, "", "KEY_APOSTROPHE");
        case KEY_GRAVE:
            return do_format_arg(sp, "", "KEY_GRAVE");
        case KEY_LEFTSHIFT:
            return do_format_arg(sp, "", "KEY_LEFTSHIFT");
        case KEY_BACKSLASH:
            return do_format_arg(sp, "", "KEY_BACKSLASH");
        case KEY_Z:
            return do_format_arg(sp, "", "KEY_Z");
        case KEY_X:
            return do_format_arg(sp, "", "KEY_X");
        case KEY_C:
            return do_format_arg(sp, "", "KEY_C");
        case KEY_V:
            return do_format_arg(sp, "", "KEY_V");
        case KEY_B:
            return do_format_arg(sp, "", "KEY_B");
        case KEY_N:
            return do_format_arg(sp, "", "KEY_N");
        case KEY_M:
            return do_format_arg(sp, "", "KEY_M");
        case KEY_COMMA:
            return do_format_arg(sp, "", "KEY_COMMA");
        case KEY_DOT:
            return do_format_arg(sp, "", "KEY_DOT");
        case KEY_SLASH:
            return do_format_arg(sp, "", "KEY_SLASH");
        case KEY_RIGHTSHIFT:
            return do_format_arg(sp, "", "KEY_RIGHTSHIFT");
        case KEY_KPASTERISK:
            return do_format_arg(sp, "", "KEY_KPASTERISK");
        case KEY_LEFTALT:
            return do_format_arg(sp, "", "KEY_LEFTALT");
        case KEY_SPACE:
            return do_format_arg(sp, "", "KEY_SPACE");
        case KEY_CAPSLOCK:
            return do_format_arg(sp, "", "KEY_CAPSLOCK");
        case KEY_F1:
            return do_format_arg(sp, "", "KEY_F1");
        case KEY_F2:
            return do_format_arg(sp, "", "KEY_F2");
        case KEY_F3:
            return do_format_arg(sp, "", "KEY_F3");
        case KEY_F4:
            return do_format_arg(sp, "", "KEY_F4");
        case KEY_F5:
            return do_format_arg(sp, "", "KEY_F5");
        case KEY_F6:
            return do_format_arg(sp, "", "KEY_F6");
        case KEY_F7:
            return do_format_arg(sp, "", "KEY_F7");
        case KEY_F8:
            return do_format_arg(sp, "", "KEY_F8");
        case KEY_F9:
            return do_format_arg(sp, "", "KEY_F9");
        case KEY_F10:
            return do_format_arg(sp, "", "KEY_F10");
        case KEY_NUMLOCK:
            return do_format_arg(sp, "", "KEY_NUMLOCK");
        case KEY_SCROLLLOCK:
            return do_format_arg(sp, "", "KEY_SCROLLLOCK");
        case KEY_KP7:
            return do_format_arg(sp, "", "KEY_KP7");
        case KEY_KP8:
            return do_format_arg(sp, "", "KEY_KP8");
        case KEY_KP9:
            return do_format_arg(sp, "", "KEY_KP9");
        case KEY_KPMINUS:
            return do_format_arg(sp, "", "KEY_KPMINUS");
        case KEY_KP4:
            return do_format_arg(sp, "", "KEY_KP4");
        case KEY_KP5:
            return do_format_arg(sp, "", "KEY_KP5");
        case KEY_KP6:
            return do_format_arg(sp, "", "KEY_KP6");
        case KEY_KPPLUS:
            return do_format_arg(sp, "", "KEY_KPPLUS");
        case KEY_KP1:
            return do_format_arg(sp, "", "KEY_KP1");
        case KEY_KP2:
            return do_format_arg(sp, "", "KEY_KP2");
        case KEY_KP3:
            return do_format_arg(sp, "", "KEY_KP3");
        case KEY_KP0:
            return do_format_arg(sp, "", "KEY_KP0");
        case KEY_KPDOT:
            return do_format_arg(sp, "", "KEY_KPDOT");
        case KEY_ZENKAKUHANKAKU:
            return do_format_arg(sp, "", "KEY_ZENKAKUHANKAKU");
        case KEY_102ND:
            return do_format_arg(sp, "", "KEY_102ND");
        case KEY_F11:
            return do_format_arg(sp, "", "KEY_F11");
        case KEY_F12:
            return do_format_arg(sp, "", "KEY_F12");
        case KEY_RO:
            return do_format_arg(sp, "", "KEY_RO");
        case KEY_KATAKANA:
            return do_format_arg(sp, "", "KEY_KATAKANA");
        case KEY_HIRAGANA:
            return do_format_arg(sp, "", "KEY_HIRAGANA");
        case KEY_HENKAN:
            return do_format_arg(sp, "", "KEY_HENKAN");
        case KEY_KATAKANAHIRAGANA:
            return do_format_arg(sp, "", "KEY_KATAKANAHIRAGANA");
        case KEY_MUHENKAN:
            return do_format_arg(sp, "", "KEY_MUHENKAN");
        case KEY_KPJPCOMMA:
            return do_format_arg(sp, "", "KEY_KPJPCOMMA");
        case KEY_KPENTER:
            return do_format_arg(sp, "", "KEY_KPENTER");
        case KEY_RIGHTCTRL:
            return do_format_arg(sp, "", "KEY_RIGHTCTRL");
        case KEY_KPSLASH:
            return do_format_arg(sp, "", "KEY_KPSLASH");
        case KEY_SYSRQ:
            return do_format_arg(sp, "", "KEY_SYSRQ");
        case KEY_RIGHTALT:
            return do_format_arg(sp, "", "KEY_RIGHTALT");
        case KEY_LINEFEED:
            return do_format_arg(sp, "", "KEY_LINEFEED");
        case KEY_HOME:
            return do_format_arg(sp, "", "KEY_HOME");
        case KEY_UP:
            return do_format_arg(sp, "", "KEY_UP");
        case KEY_PAGEUP:
            return do_format_arg(sp, "", "KEY_PAGEUP");
        case KEY_LEFT:
            return do_format_arg(sp, "", "KEY_LEFT");
        case KEY_RIGHT:
            return do_format_arg(sp, "", "KEY_RIGHT");
        case KEY_END:
            return do_format_arg(sp, "", "KEY_END");
        case KEY_DOWN:
            return do_format_arg(sp, "", "KEY_DOWN");
        case KEY_PAGEDOWN:
            return do_format_arg(sp, "", "KEY_PAGEDOWN");
        case KEY_INSERT:
            return do_format_arg(sp, "", "KEY_INSERT");
        case KEY_DELETE:
            return do_format_arg(sp, "", "KEY_DELETE");
        case KEY_MACRO:
            return do_format_arg(sp, "", "KEY_MACRO");
        case KEY_MUTE:
            return do_format_arg(sp, "", "KEY_MUTE");
        case KEY_VOLUMEDOWN:
            return do_format_arg(sp, "", "KEY_VOLUMEDOWN");
        case KEY_VOLUMEUP:
            return do_format_arg(sp, "", "KEY_VOLUMEUP");
        case KEY_POWER:
            return do_format_arg(sp, "", "KEY_POWER");
        case KEY_KPEQUAL:
            return do_format_arg(sp, "", "KEY_KPEQUAL");
        case KEY_KPPLUSMINUS:
            return do_format_arg(sp, "", "KEY_KPPLUSMINUS");
        case KEY_PAUSE:
            return do_format_arg(sp, "", "KEY_PAUSE");
        case KEY_SCALE:
            return do_format_arg(sp, "", "KEY_SCALE");
        case KEY_KPCOMMA:
            return do_format_arg(sp, "", "KEY_KPCOMMA");
        case KEY_HANGEUL:
            return do_format_arg(sp, "", "KEY_HANGEUL");
        case KEY_HANJA:
            return do_format_arg(sp, "", "KEY_HANJA");
        case KEY_YEN:
            return do_format_arg(sp, "", "KEY_YEN");
        case KEY_LEFTMETA:
            return do_format_arg(sp, "", "KEY_LEFTMETA");
        case KEY_RIGHTMETA:
            return do_format_arg(sp, "", "KEY_RIGHTMETA");
        case KEY_COMPOSE:
            return do_format_arg(sp, "", "KEY_COMPOSE");
        case KEY_STOP:
            return do_format_arg(sp, "", "KEY_STOP");
        case KEY_AGAIN:
            return do_format_arg(sp, "", "KEY_AGAIN");
        case KEY_PROPS:
            return do_format_arg(sp, "", "KEY_PROPS");
        case KEY_UNDO:
            return do_format_arg(sp, "", "KEY_UNDO");
        case KEY_FRONT:
            return do_format_arg(sp, "", "KEY_FRONT");
        case KEY_COPY:
            return do_format_arg(sp, "", "KEY_COPY");
        case KEY_OPEN:
            return do_format_arg(sp, "", "KEY_OPEN");
        case KEY_PASTE:
            return do_format_arg(sp, "", "KEY_PASTE");
        case KEY_FIND:
            return do_format_arg(sp, "", "KEY_FIND");
        case KEY_CUT:
            return do_format_arg(sp, "", "KEY_CUT");
        case KEY_HELP:
            return do_format_arg(sp, "", "KEY_HELP");
        case KEY_MENU:
            return do_format_arg(sp, "", "KEY_MENU");
        case KEY_CALC:
            return do_format_arg(sp, "", "KEY_CALC");
        case KEY_SETUP:
            return do_format_arg(sp, "", "KEY_SETUP");
        case KEY_SLEEP:
            return do_format_arg(sp, "", "KEY_SLEEP");
        case KEY_WAKEUP:
            return do_format_arg(sp, "", "KEY_WAKEUP");
        case KEY_FILE:
            return do_format_arg(sp, "", "KEY_FILE");
        case KEY_SENDFILE:
            return do_format_arg(sp, "", "KEY_SENDFILE");
        case KEY_DELETEFILE:
            return do_format_arg(sp, "", "KEY_DELETEFILE");
        case KEY_XFER:
            return do_format_arg(sp, "", "KEY_XFER");
        case KEY_PROG1:
            return do_format_arg(sp, "", "KEY_PROG1");
        case KEY_PROG2:
            return do_format_arg(sp, "", "KEY_PROG2");
        case KEY_WWW:
            return do_format_arg(sp, "", "KEY_WWW");
        case KEY_MSDOS:
            return do_format_arg(sp, "", "KEY_MSDOS");
        case KEY_COFFEE:
            return do_format_arg(sp, "", "KEY_COFFEE");
        case KEY_ROTATE_DISPLAY:
            return do_format_arg(sp, "", "KEY_ROTATE_DISPLAY");
        case KEY_CYCLEWINDOWS:
            return do_format_arg(sp, "", "KEY_CYCLEWINDOWS");
        case KEY_MAIL:
            return do_format_arg(sp, "", "KEY_MAIL");
        case KEY_BOOKMARKS:
            return do_format_arg(sp, "", "KEY_BOOKMARKS");
        case KEY_COMPUTER:
            return do_format_arg(sp, "", "KEY_COMPUTER");
        case KEY_BACK:
            return do_format_arg(sp, "", "KEY_BACK");
        case KEY_FORWARD:
            return do_format_arg(sp, "", "KEY_FORWARD");
        case KEY_CLOSECD:
            return do_format_arg(sp, "", "KEY_CLOSECD");
        case KEY_EJECTCD:
            return do_format_arg(sp, "", "KEY_EJECTCD");
        case KEY_EJECTCLOSECD:
            return do_format_arg(sp, "", "KEY_EJECTCLOSECD");
        case KEY_NEXTSONG:
            return do_format_arg(sp, "", "KEY_NEXTSONG");
        case KEY_PLAYPAUSE:
            return do_format_arg(sp, "", "KEY_PLAYPAUSE");
        case KEY_PREVIOUSSONG:
            return do_format_arg(sp, "", "KEY_PREVIOUSSONG");
        case KEY_STOPCD:
            return do_format_arg(sp, "", "KEY_STOPCD");
        case KEY_RECORD:
            return do_format_arg(sp, "", "KEY_RECORD");
        case KEY_REWIND:
            return do_format_arg(sp, "", "KEY_REWIND");
        case KEY_PHONE:
            return do_format_arg(sp, "", "KEY_PHONE");
        case KEY_ISO:
            return do_format_arg(sp, "", "KEY_ISO");
        case KEY_CONFIG:
            return do_format_arg(sp, "", "KEY_CONFIG");
        case KEY_HOMEPAGE:
            return do_format_arg(sp, "", "KEY_HOMEPAGE");
        case KEY_REFRESH:
            return do_format_arg(sp, "", "KEY_REFRESH");
        case KEY_EXIT:
            return do_format_arg(sp, "", "KEY_EXIT");
        case KEY_MOVE:
            return do_format_arg(sp, "", "KEY_MOVE");
        case KEY_EDIT:
            return do_format_arg(sp, "", "KEY_EDIT");
        case KEY_SCROLLUP:
            return do_format_arg(sp, "", "KEY_SCROLLUP");
        case KEY_SCROLLDOWN:
            return do_format_arg(sp, "", "KEY_SCROLLDOWN");
        case KEY_KPLEFTPAREN:
            return do_format_arg(sp, "", "KEY_KPLEFTPAREN");
        case KEY_KPRIGHTPAREN:
            return do_format_arg(sp, "", "KEY_KPRIGHTPAREN");
        case KEY_NEW:
            return do_format_arg(sp, "", "KEY_NEW");
        case KEY_REDO:
            return do_format_arg(sp, "", "KEY_REDO");
        case KEY_F13:
            return do_format_arg(sp, "", "KEY_F13");
        case KEY_F14:
            return do_format_arg(sp, "", "KEY_F14");
        case KEY_F15:
            return do_format_arg(sp, "", "KEY_F15");
        case KEY_F16:
            return do_format_arg(sp, "", "KEY_F16");
        case KEY_F17:
            return do_format_arg(sp, "", "KEY_F17");
        case KEY_F18:
            return do_format_arg(sp, "", "KEY_F18");
        case KEY_F19:
            return do_format_arg(sp, "", "KEY_F19");
        case KEY_F20:
            return do_format_arg(sp, "", "KEY_F20");
        case KEY_F21:
            return do_format_arg(sp, "", "KEY_F21");
        case KEY_F22:
            return do_format_arg(sp, "", "KEY_F22");
        case KEY_F23:
            return do_format_arg(sp, "", "KEY_F23");
        case KEY_F24:
            return do_format_arg(sp, "", "KEY_F24");
        case KEY_PLAYCD:
            return do_format_arg(sp, "", "KEY_PLAYCD");
        case KEY_PAUSECD:
            return do_format_arg(sp, "", "KEY_PAUSECD");
        case KEY_PROG3:
            return do_format_arg(sp, "", "KEY_PROG3");
        case KEY_PROG4:
            return do_format_arg(sp, "", "KEY_PROG4");
        case KEY_ALL_APPLICATIONS:
            return do_format_arg(sp, "", "KEY_ALL_APPLICATIONS");
        case KEY_SUSPEND:
            return do_format_arg(sp, "", "KEY_SUSPEND");
        case KEY_CLOSE:
            return do_format_arg(sp, "", "KEY_CLOSE");
        case KEY_PLAY:
            return do_format_arg(sp, "", "KEY_PLAY");
        case KEY_FASTFORWARD:
            return do_format_arg(sp, "", "KEY_FASTFORWARD");
        case KEY_BASSBOOST:
            return do_format_arg(sp, "", "KEY_BASSBOOST");
        case KEY_PRINT:
            return do_format_arg(sp, "", "KEY_PRINT");
        case KEY_HP:
            return do_format_arg(sp, "", "KEY_HP");
        case KEY_CAMERA:
            return do_format_arg(sp, "", "KEY_CAMERA");
        case KEY_SOUND:
            return do_format_arg(sp, "", "KEY_SOUND");
        case KEY_QUESTION:
            return do_format_arg(sp, "", "KEY_QUESTION");
        case KEY_EMAIL:
            return do_format_arg(sp, "", "KEY_EMAIL");
        case KEY_CHAT:
            return do_format_arg(sp, "", "KEY_CHAT");
        case KEY_SEARCH:
            return do_format_arg(sp, "", "KEY_SEARCH");
        case KEY_CONNECT:
            return do_format_arg(sp, "", "KEY_CONNECT");
        case KEY_FINANCE:
            return do_format_arg(sp, "", "KEY_FINANCE");
        case KEY_SPORT:
            return do_format_arg(sp, "", "KEY_SPORT");
        case KEY_SHOP:
            return do_format_arg(sp, "", "KEY_SHOP");
        case KEY_ALTERASE:
            return do_format_arg(sp, "", "KEY_ALTERASE");
        case KEY_CANCEL:
            return do_format_arg(sp, "", "KEY_CANCEL");
        case KEY_BRIGHTNESSDOWN:
            return do_format_arg(sp, "", "KEY_BRIGHTNESSDOWN");
        case KEY_BRIGHTNESSUP:
            return do_format_arg(sp, "", "KEY_BRIGHTNESSUP");
        case KEY_MEDIA:
            return do_format_arg(sp, "", "KEY_MEDIA");
        case KEY_SWITCHVIDEOMODE:
            return do_format_arg(sp, "", "KEY_SWITCHVIDEOMODE");
        case KEY_KBDILLUMTOGGLE:
            return do_format_arg(sp, "", "KEY_KBDILLUMTOGGLE");
        case KEY_KBDILLUMDOWN:
            return do_format_arg(sp, "", "KEY_KBDILLUMDOWN");
        case KEY_KBDILLUMUP:
            return do_format_arg(sp, "", "KEY_KBDILLUMUP");
        case KEY_SEND:
            return do_format_arg(sp, "", "KEY_SEND");
        case KEY_REPLY:
            return do_format_arg(sp, "", "KEY_REPLY");
        case KEY_FORWARDMAIL:
            return do_format_arg(sp, "", "KEY_FORWARDMAIL");
        case KEY_SAVE:
            return do_format_arg(sp, "", "KEY_SAVE");
        case KEY_DOCUMENTS:
            return do_format_arg(sp, "", "KEY_DOCUMENTS");
        case KEY_BATTERY:
            return do_format_arg(sp, "", "KEY_BATTERY");
        case KEY_BLUETOOTH:
            return do_format_arg(sp, "", "KEY_BLUETOOTH");
        case KEY_WLAN:
            return do_format_arg(sp, "", "KEY_WLAN");
        case KEY_UWB:
            return do_format_arg(sp, "", "KEY_UWB");
        case KEY_UNKNOWN:
            return do_format_arg(sp, "", "KEY_UNKNOWN");
        case KEY_VIDEO_NEXT:
            return do_format_arg(sp, "", "KEY_VIDEO_NEXT");
        case KEY_VIDEO_PREV:
            return do_format_arg(sp, "", "KEY_VIDEO_PREV");
        case KEY_BRIGHTNESS_CYCLE:
            return do_format_arg(sp, "", "KEY_BRIGHTNESS_CYCLE");
        case KEY_BRIGHTNESS_AUTO:
            return do_format_arg(sp, "", "KEY_BRIGHTNESS_AUTO");
        case KEY_DISPLAY_OFF:
            return do_format_arg(sp, "", "KEY_DISPLAY_OFF");
        case KEY_WWAN:
            return do_format_arg(sp, "", "KEY_WWAN");
        case KEY_RFKILL:
            return do_format_arg(sp, "", "KEY_RFKILL");
        case KEY_MICMUTE:
            return do_format_arg(sp, "", "KEY_MICMUTE");
        case BTN_0:
            return do_format_arg(sp, "", "BTN_0");
        case BTN_1:
            return do_format_arg(sp, "", "BTN_1");
        case BTN_2:
            return do_format_arg(sp, "", "BTN_2");
        case BTN_3:
            return do_format_arg(sp, "", "BTN_3");
        case BTN_4:
            return do_format_arg(sp, "", "BTN_4");
        case BTN_5:
            return do_format_arg(sp, "", "BTN_5");
        case BTN_6:
            return do_format_arg(sp, "", "BTN_6");
        case BTN_7:
            return do_format_arg(sp, "", "BTN_7");
        case BTN_8:
            return do_format_arg(sp, "", "BTN_8");
        case BTN_9:
            return do_format_arg(sp, "", "BTN_9");
        case BTN_LEFT:
            return do_format_arg(sp, "", "BTN_LEFT");
        case BTN_RIGHT:
            return do_format_arg(sp, "", "BTN_RIGHT");
        case BTN_MIDDLE:
            return do_format_arg(sp, "", "BTN_MIDDLE");
        case BTN_SIDE:
            return do_format_arg(sp, "", "BTN_SIDE");
        case BTN_EXTRA:
            return do_format_arg(sp, "", "BTN_EXTRA");
        case BTN_FORWARD:
            return do_format_arg(sp, "", "BTN_FORWARD");
        case BTN_BACK:
            return do_format_arg(sp, "", "BTN_BACK");
        case BTN_TASK:
            return do_format_arg(sp, "", "BTN_TASK");
        case BTN_TRIGGER:
            return do_format_arg(sp, "", "BTN_TRIGGER");
        case BTN_THUMB:
            return do_format_arg(sp, "", "BTN_THUMB");
        case BTN_THUMB2:
            return do_format_arg(sp, "", "BTN_THUMB2");
        case BTN_TOP:
            return do_format_arg(sp, "", "BTN_TOP");
        case BTN_TOP2:
            return do_format_arg(sp, "", "BTN_TOP2");
        case BTN_PINKIE:
            return do_format_arg(sp, "", "BTN_PINKIE");
        case BTN_BASE:
            return do_format_arg(sp, "", "BTN_BASE");
        case BTN_BASE2:
            return do_format_arg(sp, "", "BTN_BASE2");
        case BTN_BASE3:
            return do_format_arg(sp, "", "BTN_BASE3");
        case BTN_BASE4:
            return do_format_arg(sp, "", "BTN_BASE4");
        case BTN_BASE5:
            return do_format_arg(sp, "", "BTN_BASE5");
        case BTN_BASE6:
            return do_format_arg(sp, "", "BTN_BASE6");
        case BTN_DEAD:
            return do_format_arg(sp, "", "BTN_DEAD");
        case BTN_SOUTH:
            return do_format_arg(sp, "", "BTN_SOUTH");
        case BTN_EAST:
            return do_format_arg(sp, "", "BTN_EAST");
        case BTN_C:
            return do_format_arg(sp, "", "BTN_C");
        case BTN_NORTH:
            return do_format_arg(sp, "", "BTN_NORTH");
        case BTN_WEST:
            return do_format_arg(sp, "", "BTN_WEST");
        case BTN_Z:
            return do_format_arg(sp, "", "BTN_Z");
        case BTN_TL:
            return do_format_arg(sp, "", "BTN_TL");
        case BTN_TR:
            return do_format_arg(sp, "", "BTN_TR");
        case BTN_TL2:
            return do_format_arg(sp, "", "BTN_TL2");
        case BTN_TR2:
            return do_format_arg(sp, "", "BTN_TR2");
        case BTN_SELECT:
            return do_format_arg(sp, "", "BTN_SELECT");
        case BTN_START:
            return do_format_arg(sp, "", "BTN_START");
        case BTN_MODE:
            return do_format_arg(sp, "", "BTN_MODE");
        case BTN_THUMBL:
            return do_format_arg(sp, "", "BTN_THUMBL");
        case BTN_THUMBR:
            return do_format_arg(sp, "", "BTN_THUMBR");
        case BTN_TOOL_PEN:
            return do_format_arg(sp, "", "BTN_TOOL_PEN");
        case BTN_TOOL_RUBBER:
            return do_format_arg(sp, "", "BTN_TOOL_RUBBER");
        case BTN_TOOL_BRUSH:
            return do_format_arg(sp, "", "BTN_TOOL_BRUSH");
        case BTN_TOOL_PENCIL:
            return do_format_arg(sp, "", "BTN_TOOL_PENCIL");
        case BTN_TOOL_AIRBRUSH:
            return do_format_arg(sp, "", "BTN_TOOL_AIRBRUSH");
        case BTN_TOOL_FINGER:
            return do_format_arg(sp, "", "BTN_TOOL_FINGER");
        case BTN_TOOL_MOUSE:
            return do_format_arg(sp, "", "BTN_TOOL_MOUSE");
        case BTN_TOOL_LENS:
            return do_format_arg(sp, "", "BTN_TOOL_LENS");
        case BTN_TOOL_QUINTTAP:
            return do_format_arg(sp, "", "BTN_TOOL_QUINTTAP");
        case BTN_STYLUS3:
            return do_format_arg(sp, "", "BTN_STYLUS3");
        case BTN_TOUCH:
            return do_format_arg(sp, "", "BTN_TOUCH");
        case BTN_STYLUS:
            return do_format_arg(sp, "", "BTN_STYLUS");
        case BTN_STYLUS2:
            return do_format_arg(sp, "", "BTN_STYLUS2");
        case BTN_TOOL_DOUBLETAP:
            return do_format_arg(sp, "", "BTN_TOOL_DOUBLETAP");
        case BTN_TOOL_TRIPLETAP:
            return do_format_arg(sp, "", "BTN_TOOL_TRIPLETAP");
        case BTN_TOOL_QUADTAP:
            return do_format_arg(sp, "", "BTN_TOOL_QUADTAP");
        case BTN_GEAR_DOWN:
            return do_format_arg(sp, "", "BTN_GEAR_DOWN");
        case BTN_GEAR_UP:
            return do_format_arg(sp, "", "BTN_GEAR_UP");
        case KEY_OK:
            return do_format_arg(sp, "", "KEY_OK");
        case KEY_SELECT:
            return do_format_arg(sp, "", "KEY_SELECT");
        case KEY_GOTO:
            return do_format_arg(sp, "", "KEY_GOTO");
        case KEY_CLEAR:
            return do_format_arg(sp, "", "KEY_CLEAR");
        case KEY_POWER2:
            return do_format_arg(sp, "", "KEY_POWER2");
        case KEY_OPTION:
            return do_format_arg(sp, "", "KEY_OPTION");
        case KEY_INFO:
            return do_format_arg(sp, "", "KEY_INFO");
        case KEY_TIME:
            return do_format_arg(sp, "", "KEY_TIME");
        case KEY_VENDOR:
            return do_format_arg(sp, "", "KEY_VENDOR");
        case KEY_ARCHIVE:
            return do_format_arg(sp, "", "KEY_ARCHIVE");
        case KEY_PROGRAM:
            return do_format_arg(sp, "", "KEY_PROGRAM");
        case KEY_CHANNEL:
            return do_format_arg(sp, "", "KEY_CHANNEL");
        case KEY_FAVORITES:
            return do_format_arg(sp, "", "KEY_FAVORITES");
        case KEY_EPG:
            return do_format_arg(sp, "", "KEY_EPG");
        case KEY_PVR:
            return do_format_arg(sp, "", "KEY_PVR");
        case KEY_MHP:
            return do_format_arg(sp, "", "KEY_MHP");
        case KEY_LANGUAGE:
            return do_format_arg(sp, "", "KEY_LANGUAGE");
        case KEY_TITLE:
            return do_format_arg(sp, "", "KEY_TITLE");
        case KEY_SUBTITLE:
            return do_format_arg(sp, "", "KEY_SUBTITLE");
        case KEY_ANGLE:
            return do_format_arg(sp, "", "KEY_ANGLE");
        case KEY_FULL_SCREEN:
            return do_format_arg(sp, "", "KEY_FULL_SCREEN");
        case KEY_MODE:
            return do_format_arg(sp, "", "KEY_MODE");
        case KEY_KEYBOARD:
            return do_format_arg(sp, "", "KEY_KEYBOARD");
        case KEY_ASPECT_RATIO:
            return do_format_arg(sp, "", "KEY_ASPECT_RATIO");
        case KEY_PC:
            return do_format_arg(sp, "", "KEY_PC");
        case KEY_TV:
            return do_format_arg(sp, "", "KEY_TV");
        case KEY_TV2:
            return do_format_arg(sp, "", "KEY_TV2");
        case KEY_VCR:
            return do_format_arg(sp, "", "KEY_VCR");
        case KEY_VCR2:
            return do_format_arg(sp, "", "KEY_VCR2");
        case KEY_SAT:
            return do_format_arg(sp, "", "KEY_SAT");
        case KEY_SAT2:
            return do_format_arg(sp, "", "KEY_SAT2");
        case KEY_CD:
            return do_format_arg(sp, "", "KEY_CD");
        case KEY_TAPE:
            return do_format_arg(sp, "", "KEY_TAPE");
        case KEY_RADIO:
            return do_format_arg(sp, "", "KEY_RADIO");
        case KEY_TUNER:
            return do_format_arg(sp, "", "KEY_TUNER");
        case KEY_PLAYER:
            return do_format_arg(sp, "", "KEY_PLAYER");
        case KEY_TEXT:
            return do_format_arg(sp, "", "KEY_TEXT");
        case KEY_DVD:
            return do_format_arg(sp, "", "KEY_DVD");
        case KEY_AUX:
            return do_format_arg(sp, "", "KEY_AUX");
        case KEY_MP3:
            return do_format_arg(sp, "", "KEY_MP3");
        case KEY_AUDIO:
            return do_format_arg(sp, "", "KEY_AUDIO");
        case KEY_VIDEO:
            return do_format_arg(sp, "", "KEY_VIDEO");
        case KEY_DIRECTORY:
            return do_format_arg(sp, "", "KEY_DIRECTORY");
        case KEY_LIST:
            return do_format_arg(sp, "", "KEY_LIST");
        case KEY_MEMO:
            return do_format_arg(sp, "", "KEY_MEMO");
        case KEY_CALENDAR:
            return do_format_arg(sp, "", "KEY_CALENDAR");
        case KEY_RED:
            return do_format_arg(sp, "", "KEY_RED");
        case KEY_GREEN:
            return do_format_arg(sp, "", "KEY_GREEN");
        case KEY_YELLOW:
            return do_format_arg(sp, "", "KEY_YELLOW");
        case KEY_BLUE:
            return do_format_arg(sp, "", "KEY_BLUE");
        case KEY_CHANNELUP:
            return do_format_arg(sp, "", "KEY_CHANNELUP");
        case KEY_CHANNELDOWN:
            return do_format_arg(sp, "", "KEY_CHANNELDOWN");
        case KEY_FIRST:
            return do_format_arg(sp, "", "KEY_FIRST");
        case KEY_LAST:
            return do_format_arg(sp, "", "KEY_LAST");
        case KEY_AB:
            return do_format_arg(sp, "", "KEY_AB");
        case KEY_NEXT:
            return do_format_arg(sp, "", "KEY_NEXT");
        case KEY_RESTART:
            return do_format_arg(sp, "", "KEY_RESTART");
        case KEY_SLOW:
            return do_format_arg(sp, "", "KEY_SLOW");
        case KEY_SHUFFLE:
            return do_format_arg(sp, "", "KEY_SHUFFLE");
        case KEY_BREAK:
            return do_format_arg(sp, "", "KEY_BREAK");
        case KEY_PREVIOUS:
            return do_format_arg(sp, "", "KEY_PREVIOUS");
        case KEY_DIGITS:
            return do_format_arg(sp, "", "KEY_DIGITS");
        case KEY_TEEN:
            return do_format_arg(sp, "", "KEY_TEEN");
        case KEY_TWEN:
            return do_format_arg(sp, "", "KEY_TWEN");
        case KEY_VIDEOPHONE:
            return do_format_arg(sp, "", "KEY_VIDEOPHONE");
        case KEY_GAMES:
            return do_format_arg(sp, "", "KEY_GAMES");
        case KEY_ZOOMIN:
            return do_format_arg(sp, "", "KEY_ZOOMIN");
        case KEY_ZOOMOUT:
            return do_format_arg(sp, "", "KEY_ZOOMOUT");
        case KEY_ZOOMRESET:
            return do_format_arg(sp, "", "KEY_ZOOMRESET");
        case KEY_WORDPROCESSOR:
            return do_format_arg(sp, "", "KEY_WORDPROCESSOR");
        case KEY_EDITOR:
            return do_format_arg(sp, "", "KEY_EDITOR");
        case KEY_SPREADSHEET:
            return do_format_arg(sp, "", "KEY_SPREADSHEET");
        case KEY_GRAPHICSEDITOR:
            return do_format_arg(sp, "", "KEY_GRAPHICSEDITOR");
        case KEY_PRESENTATION:
            return do_format_arg(sp, "", "KEY_PRESENTATION");
        case KEY_DATABASE:
            return do_format_arg(sp, "", "KEY_DATABASE");
        case KEY_NEWS:
            return do_format_arg(sp, "", "KEY_NEWS");
        case KEY_VOICEMAIL:
            return do_format_arg(sp, "", "KEY_VOICEMAIL");
        case KEY_ADDRESSBOOK:
            return do_format_arg(sp, "", "KEY_ADDRESSBOOK");
        case KEY_MESSENGER:
            return do_format_arg(sp, "", "KEY_MESSENGER");
        case KEY_DISPLAYTOGGLE:
            return do_format_arg(sp, "", "KEY_DISPLAYTOGGLE");
        case KEY_SPELLCHECK:
            return do_format_arg(sp, "", "KEY_SPELLCHECK");
        case KEY_LOGOFF:
            return do_format_arg(sp, "", "KEY_LOGOFF");
        case KEY_DOLLAR:
            return do_format_arg(sp, "", "KEY_DOLLAR");
        case KEY_EURO:
            return do_format_arg(sp, "", "KEY_EURO");
        case KEY_FRAMEBACK:
            return do_format_arg(sp, "", "KEY_FRAMEBACK");
        case KEY_FRAMEFORWARD:
            return do_format_arg(sp, "", "KEY_FRAMEFORWARD");
        case KEY_CONTEXT_MENU:
            return do_format_arg(sp, "", "KEY_CONTEXT_MENU");
        case KEY_MEDIA_REPEAT:
            return do_format_arg(sp, "", "KEY_MEDIA_REPEAT");
        case KEY_10CHANNELSUP:
            return do_format_arg(sp, "", "KEY_10CHANNELSUP");
        case KEY_10CHANNELSDOWN:
            return do_format_arg(sp, "", "KEY_10CHANNELSDOWN");
        case KEY_IMAGES:
            return do_format_arg(sp, "", "KEY_IMAGES");
        case KEY_NOTIFICATION_CENTER:
            return do_format_arg(sp, "", "KEY_NOTIFICATION_CENTER");
        case KEY_PICKUP_PHONE:
            return do_format_arg(sp, "", "KEY_PICKUP_PHONE");
        case KEY_HANGUP_PHONE:
            return do_format_arg(sp, "", "KEY_HANGUP_PHONE");
        case KEY_DEL_EOL:
            return do_format_arg(sp, "", "KEY_DEL_EOL");
        case KEY_DEL_EOS:
            return do_format_arg(sp, "", "KEY_DEL_EOS");
        case KEY_INS_LINE:
            return do_format_arg(sp, "", "KEY_INS_LINE");
        case KEY_DEL_LINE:
            return do_format_arg(sp, "", "KEY_DEL_LINE");
        case KEY_FN:
            return do_format_arg(sp, "", "KEY_FN");
        case KEY_FN_ESC:
            return do_format_arg(sp, "", "KEY_FN_ESC");
        case KEY_FN_F1:
            return do_format_arg(sp, "", "KEY_FN_F1");
        case KEY_FN_F2:
            return do_format_arg(sp, "", "KEY_FN_F2");
        case KEY_FN_F3:
            return do_format_arg(sp, "", "KEY_FN_F3");
        case KEY_FN_F4:
            return do_format_arg(sp, "", "KEY_FN_F4");
        case KEY_FN_F5:
            return do_format_arg(sp, "", "KEY_FN_F5");
        case KEY_FN_F6:
            return do_format_arg(sp, "", "KEY_FN_F6");
        case KEY_FN_F7:
            return do_format_arg(sp, "", "KEY_FN_F7");
        case KEY_FN_F8:
            return do_format_arg(sp, "", "KEY_FN_F8");
        case KEY_FN_F9:
            return do_format_arg(sp, "", "KEY_FN_F9");
        case KEY_FN_F10:
            return do_format_arg(sp, "", "KEY_FN_F10");
        case KEY_FN_F11:
            return do_format_arg(sp, "", "KEY_FN_F11");
        case KEY_FN_F12:
            return do_format_arg(sp, "", "KEY_FN_F12");
        case KEY_FN_1:
            return do_format_arg(sp, "", "KEY_FN_1");
        case KEY_FN_2:
            return do_format_arg(sp, "", "KEY_FN_2");
        case KEY_FN_D:
            return do_format_arg(sp, "", "KEY_FN_D");
        case KEY_FN_E:
            return do_format_arg(sp, "", "KEY_FN_E");
        case KEY_FN_F:
            return do_format_arg(sp, "", "KEY_FN_F");
        case KEY_FN_S:
            return do_format_arg(sp, "", "KEY_FN_S");
        case KEY_FN_B:
            return do_format_arg(sp, "", "KEY_FN_B");
        case KEY_FN_RIGHT_SHIFT:
            return do_format_arg(sp, "", "KEY_FN_RIGHT_SHIFT");
        case KEY_BRL_DOT1:
            return do_format_arg(sp, "", "KEY_BRL_DOT1");
        case KEY_BRL_DOT2:
            return do_format_arg(sp, "", "KEY_BRL_DOT2");
        case KEY_BRL_DOT3:
            return do_format_arg(sp, "", "KEY_BRL_DOT3");
        case KEY_BRL_DOT4:
            return do_format_arg(sp, "", "KEY_BRL_DOT4");
        case KEY_BRL_DOT5:
            return do_format_arg(sp, "", "KEY_BRL_DOT5");
        case KEY_BRL_DOT6:
            return do_format_arg(sp, "", "KEY_BRL_DOT6");
        case KEY_BRL_DOT7:
            return do_format_arg(sp, "", "KEY_BRL_DOT7");
        case KEY_BRL_DOT8:
            return do_format_arg(sp, "", "KEY_BRL_DOT8");
        case KEY_BRL_DOT9:
            return do_format_arg(sp, "", "KEY_BRL_DOT9");
        case KEY_BRL_DOT10:
            return do_format_arg(sp, "", "KEY_BRL_DOT10");
        case KEY_NUMERIC_0:
            return do_format_arg(sp, "", "KEY_NUMERIC_0");
        case KEY_NUMERIC_1:
            return do_format_arg(sp, "", "KEY_NUMERIC_1");
        case KEY_NUMERIC_2:
            return do_format_arg(sp, "", "KEY_NUMERIC_2");
        case KEY_NUMERIC_3:
            return do_format_arg(sp, "", "KEY_NUMERIC_3");
        case KEY_NUMERIC_4:
            return do_format_arg(sp, "", "KEY_NUMERIC_4");
        case KEY_NUMERIC_5:
            return do_format_arg(sp, "", "KEY_NUMERIC_5");
        case KEY_NUMERIC_6:
            return do_format_arg(sp, "", "KEY_NUMERIC_6");
        case KEY_NUMERIC_7:
            return do_format_arg(sp, "", "KEY_NUMERIC_7");
        case KEY_NUMERIC_8:
            return do_format_arg(sp, "", "KEY_NUMERIC_8");
        case KEY_NUMERIC_9:
            return do_format_arg(sp, "", "KEY_NUMERIC_9");
        case KEY_NUMERIC_STAR:
            return do_format_arg(sp, "", "KEY_NUMERIC_STAR");
        case KEY_NUMERIC_POUND:
            return do_format_arg(sp, "", "KEY_NUMERIC_POUND");
        case KEY_NUMERIC_A:
            return do_format_arg(sp, "", "KEY_NUMERIC_A");
        case KEY_NUMERIC_B:
            return do_format_arg(sp, "", "KEY_NUMERIC_B");
        case KEY_NUMERIC_C:
            return do_format_arg(sp, "", "KEY_NUMERIC_C");
        case KEY_NUMERIC_D:
            return do_format_arg(sp, "", "KEY_NUMERIC_D");
        case KEY_CAMERA_FOCUS:
            return do_format_arg(sp, "", "KEY_CAMERA_FOCUS");
        case KEY_WPS_BUTTON:
            return do_format_arg(sp, "", "KEY_WPS_BUTTON");
        case KEY_TOUCHPAD_TOGGLE:
            return do_format_arg(sp, "", "KEY_TOUCHPAD_TOGGLE");
        case KEY_TOUCHPAD_ON:
            return do_format_arg(sp, "", "KEY_TOUCHPAD_ON");
        case KEY_TOUCHPAD_OFF:
            return do_format_arg(sp, "", "KEY_TOUCHPAD_OFF");
        case KEY_CAMERA_ZOOMIN:
            return do_format_arg(sp, "", "KEY_CAMERA_ZOOMIN");
        case KEY_CAMERA_ZOOMOUT:
            return do_format_arg(sp, "", "KEY_CAMERA_ZOOMOUT");
        case KEY_CAMERA_UP:
            return do_format_arg(sp, "", "KEY_CAMERA_UP");
        case KEY_CAMERA_DOWN:
            return do_format_arg(sp, "", "KEY_CAMERA_DOWN");
        case KEY_CAMERA_LEFT:
            return do_format_arg(sp, "", "KEY_CAMERA_LEFT");
        case KEY_CAMERA_RIGHT:
            return do_format_arg(sp, "", "KEY_CAMERA_RIGHT");
        case KEY_ATTENDANT_ON:
            return do_format_arg(sp, "", "KEY_ATTENDANT_ON");
        case KEY_ATTENDANT_OFF:
            return do_format_arg(sp, "", "KEY_ATTENDANT_OFF");
        case KEY_ATTENDANT_TOGGLE:
            return do_format_arg(sp, "", "KEY_ATTENDANT_TOGGLE");
        case KEY_LIGHTS_TOGGLE:
            return do_format_arg(sp, "", "KEY_LIGHTS_TOGGLE");
        case BTN_DPAD_UP:
            return do_format_arg(sp, "", "BTN_DPAD_UP");
        case BTN_DPAD_DOWN:
            return do_format_arg(sp, "", "BTN_DPAD_DOWN");
        case BTN_DPAD_LEFT:
            return do_format_arg(sp, "", "BTN_DPAD_LEFT");
        case BTN_DPAD_RIGHT:
            return do_format_arg(sp, "", "BTN_DPAD_RIGHT");
        case KEY_ALS_TOGGLE:
            return do_format_arg(sp, "", "KEY_ALS_TOGGLE");
        case KEY_ROTATE_LOCK_TOGGLE:
            return do_format_arg(sp, "", "KEY_ROTATE_LOCK_TOGGLE");
        case KEY_REFRESH_RATE_TOGGLE:
            return do_format_arg(sp, "", "KEY_REFRESH_RATE_TOGGLE");
        case KEY_BUTTONCONFIG:
            return do_format_arg(sp, "", "KEY_BUTTONCONFIG");
        case KEY_TASKMANAGER:
            return do_format_arg(sp, "", "KEY_TASKMANAGER");
        case KEY_JOURNAL:
            return do_format_arg(sp, "", "KEY_JOURNAL");
        case KEY_CONTROLPANEL:
            return do_format_arg(sp, "", "KEY_CONTROLPANEL");
        case KEY_APPSELECT:
            return do_format_arg(sp, "", "KEY_APPSELECT");
        case KEY_SCREENSAVER:
            return do_format_arg(sp, "", "KEY_SCREENSAVER");
        case KEY_VOICECOMMAND:
            return do_format_arg(sp, "", "KEY_VOICECOMMAND");
        case KEY_ASSISTANT:
            return do_format_arg(sp, "", "KEY_ASSISTANT");
        case KEY_KBD_LAYOUT_NEXT:
            return do_format_arg(sp, "", "KEY_KBD_LAYOUT_NEXT");
        case KEY_EMOJI_PICKER:
            return do_format_arg(sp, "", "KEY_EMOJI_PICKER");
        case KEY_DICTATE:
            return do_format_arg(sp, "", "KEY_DICTATE");
        case KEY_CAMERA_ACCESS_ENABLE:
            return do_format_arg(sp, "", "KEY_CAMERA_ACCESS_ENABLE");
        case KEY_CAMERA_ACCESS_DISABLE:
            return do_format_arg(sp, "", "KEY_CAMERA_ACCESS_DISABLE");
        case KEY_CAMERA_ACCESS_TOGGLE:
            return do_format_arg(sp, "", "KEY_CAMERA_ACCESS_TOGGLE");
        case KEY_ACCESSIBILITY:
            return do_format_arg(sp, "", "KEY_ACCESSIBILITY");
        case KEY_DO_NOT_DISTURB:
            return do_format_arg(sp, "", "KEY_DO_NOT_DISTURB");
        case KEY_BRIGHTNESS_MIN:
            return do_format_arg(sp, "", "KEY_BRIGHTNESS_MIN");
        case KEY_BRIGHTNESS_MAX:
            return do_format_arg(sp, "", "KEY_BRIGHTNESS_MAX");
        case KEY_KBDINPUTASSIST_PREV:
            return do_format_arg(sp, "", "KEY_KBDINPUTASSIST_PREV");
        case KEY_KBDINPUTASSIST_NEXT:
            return do_format_arg(sp, "", "KEY_KBDINPUTASSIST_NEXT");
        case KEY_KBDINPUTASSIST_PREVGROUP:
            return do_format_arg(sp, "", "KEY_KBDINPUTASSIST_PREVGROUP");
        case KEY_KBDINPUTASSIST_NEXTGROUP:
            return do_format_arg(sp, "", "KEY_KBDINPUTASSIST_NEXTGROUP");
        case KEY_KBDINPUTASSIST_ACCEPT:
            return do_format_arg(sp, "", "KEY_KBDINPUTASSIST_ACCEPT");
        case KEY_KBDINPUTASSIST_CANCEL:
            return do_format_arg(sp, "", "KEY_KBDINPUTASSIST_CANCEL");
        case KEY_RIGHT_UP:
            return do_format_arg(sp, "", "KEY_RIGHT_UP");
        case KEY_RIGHT_DOWN:
            return do_format_arg(sp, "", "KEY_RIGHT_DOWN");
        case KEY_LEFT_UP:
            return do_format_arg(sp, "", "KEY_LEFT_UP");
        case KEY_LEFT_DOWN:
            return do_format_arg(sp, "", "KEY_LEFT_DOWN");
        case KEY_ROOT_MENU:
            return do_format_arg(sp, "", "KEY_ROOT_MENU");
        case KEY_MEDIA_TOP_MENU:
            return do_format_arg(sp, "", "KEY_MEDIA_TOP_MENU");
        case KEY_NUMERIC_11:
            return do_format_arg(sp, "", "KEY_NUMERIC_11");
        case KEY_NUMERIC_12:
            return do_format_arg(sp, "", "KEY_NUMERIC_12");
        case KEY_AUDIO_DESC:
            return do_format_arg(sp, "", "KEY_AUDIO_DESC");
        case KEY_3D_MODE:
            return do_format_arg(sp, "", "KEY_3D_MODE");
        case KEY_NEXT_FAVORITE:
            return do_format_arg(sp, "", "KEY_NEXT_FAVORITE");
        case KEY_STOP_RECORD:
            return do_format_arg(sp, "", "KEY_STOP_RECORD");
        case KEY_PAUSE_RECORD:
            return do_format_arg(sp, "", "KEY_PAUSE_RECORD");
        case KEY_VOD:
            return do_format_arg(sp, "", "KEY_VOD");
        case KEY_UNMUTE:
            return do_format_arg(sp, "", "KEY_UNMUTE");
        case KEY_FASTREVERSE:
            return do_format_arg(sp, "", "KEY_FASTREVERSE");
        case KEY_SLOWREVERSE:
            return do_format_arg(sp, "", "KEY_SLOWREVERSE");
        case KEY_DATA:
            return do_format_arg(sp, "", "KEY_DATA");
        case KEY_ONSCREEN_KEYBOARD:
            return do_format_arg(sp, "", "KEY_ONSCREEN_KEYBOARD");
        case KEY_PRIVACY_SCREEN_TOGGLE:
            return do_format_arg(sp, "", "KEY_PRIVACY_SCREEN_TOGGLE");
        case KEY_SELECTIVE_SCREENSHOT:
            return do_format_arg(sp, "", "KEY_SELECTIVE_SCREENSHOT");
        case KEY_NEXT_ELEMENT:
            return do_format_arg(sp, "", "KEY_NEXT_ELEMENT");
        case KEY_PREVIOUS_ELEMENT:
            return do_format_arg(sp, "", "KEY_PREVIOUS_ELEMENT");
        case KEY_AUTOPILOT_ENGAGE_TOGGLE:
            return do_format_arg(sp, "", "KEY_AUTOPILOT_ENGAGE_TOGGLE");
        case KEY_MARK_WAYPOINT:
            return do_format_arg(sp, "", "KEY_MARK_WAYPOINT");
        case KEY_SOS:
            return do_format_arg(sp, "", "KEY_SOS");
        case KEY_NAV_CHART:
            return do_format_arg(sp, "", "KEY_NAV_CHART");
        case KEY_FISHING_CHART:
            return do_format_arg(sp, "", "KEY_FISHING_CHART");
        case KEY_SINGLE_RANGE_RADAR:
            return do_format_arg(sp, "", "KEY_SINGLE_RANGE_RADAR");
        case KEY_DUAL_RANGE_RADAR:
            return do_format_arg(sp, "", "KEY_DUAL_RANGE_RADAR");
        case KEY_RADAR_OVERLAY:
            return do_format_arg(sp, "", "KEY_RADAR_OVERLAY");
        case KEY_TRADITIONAL_SONAR:
            return do_format_arg(sp, "", "KEY_TRADITIONAL_SONAR");
        case KEY_CLEARVU_SONAR:
            return do_format_arg(sp, "", "KEY_CLEARVU_SONAR");
        case KEY_SIDEVU_SONAR:
            return do_format_arg(sp, "", "KEY_SIDEVU_SONAR");
        case KEY_NAV_INFO:
            return do_format_arg(sp, "", "KEY_NAV_INFO");
        case KEY_BRIGHTNESS_MENU:
            return do_format_arg(sp, "", "KEY_BRIGHTNESS_MENU");
        case KEY_MACRO1:
            return do_format_arg(sp, "", "KEY_MACRO1");
        case KEY_MACRO2:
            return do_format_arg(sp, "", "KEY_MACRO2");
        case KEY_MACRO3:
            return do_format_arg(sp, "", "KEY_MACRO3");
        case KEY_MACRO4:
            return do_format_arg(sp, "", "KEY_MACRO4");
        case KEY_MACRO5:
            return do_format_arg(sp, "", "KEY_MACRO5");
        case KEY_MACRO6:
            return do_format_arg(sp, "", "KEY_MACRO6");
        case KEY_MACRO7:
            return do_format_arg(sp, "", "KEY_MACRO7");
        case KEY_MACRO8:
            return do_format_arg(sp, "", "KEY_MACRO8");
        case KEY_MACRO9:
            return do_format_arg(sp, "", "KEY_MACRO9");
        case KEY_MACRO10:
            return do_format_arg(sp, "", "KEY_MACRO10");
        case KEY_MACRO11:
            return do_format_arg(sp, "", "KEY_MACRO11");
        case KEY_MACRO12:
            return do_format_arg(sp, "", "KEY_MACRO12");
        case KEY_MACRO13:
            return do_format_arg(sp, "", "KEY_MACRO13");
        case KEY_MACRO14:
            return do_format_arg(sp, "", "KEY_MACRO14");
        case KEY_MACRO15:
            return do_format_arg(sp, "", "KEY_MACRO15");
        case KEY_MACRO16:
            return do_format_arg(sp, "", "KEY_MACRO16");
        case KEY_MACRO17:
            return do_format_arg(sp, "", "KEY_MACRO17");
        case KEY_MACRO18:
            return do_format_arg(sp, "", "KEY_MACRO18");
        case KEY_MACRO19:
            return do_format_arg(sp, "", "KEY_MACRO19");
        case KEY_MACRO20:
            return do_format_arg(sp, "", "KEY_MACRO20");
        case KEY_MACRO21:
            return do_format_arg(sp, "", "KEY_MACRO21");
        case KEY_MACRO22:
            return do_format_arg(sp, "", "KEY_MACRO22");
        case KEY_MACRO23:
            return do_format_arg(sp, "", "KEY_MACRO23");
        case KEY_MACRO24:
            return do_format_arg(sp, "", "KEY_MACRO24");
        case KEY_MACRO25:
            return do_format_arg(sp, "", "KEY_MACRO25");
        case KEY_MACRO26:
            return do_format_arg(sp, "", "KEY_MACRO26");
        case KEY_MACRO27:
            return do_format_arg(sp, "", "KEY_MACRO27");
        case KEY_MACRO28:
            return do_format_arg(sp, "", "KEY_MACRO28");
        case KEY_MACRO29:
            return do_format_arg(sp, "", "KEY_MACRO29");
        case KEY_MACRO30:
            return do_format_arg(sp, "", "KEY_MACRO30");
        case KEY_MACRO_RECORD_START:
            return do_format_arg(sp, "", "KEY_MACRO_RECORD_START");
        case KEY_MACRO_RECORD_STOP:
            return do_format_arg(sp, "", "KEY_MACRO_RECORD_STOP");
        case KEY_MACRO_PRESET_CYCLE:
            return do_format_arg(sp, "", "KEY_MACRO_PRESET_CYCLE");
        case KEY_MACRO_PRESET1:
            return do_format_arg(sp, "", "KEY_MACRO_PRESET1");
        case KEY_MACRO_PRESET2:
            return do_format_arg(sp, "", "KEY_MACRO_PRESET2");
        case KEY_MACRO_PRESET3:
            return do_format_arg(sp, "", "KEY_MACRO_PRESET3");
        case KEY_KBD_LCD_MENU1:
            return do_format_arg(sp, "", "KEY_KBD_LCD_MENU1");
        case KEY_KBD_LCD_MENU2:
            return do_format_arg(sp, "", "KEY_KBD_LCD_MENU2");
        case KEY_KBD_LCD_MENU3:
            return do_format_arg(sp, "", "KEY_KBD_LCD_MENU3");
        case KEY_KBD_LCD_MENU4:
            return do_format_arg(sp, "", "KEY_KBD_LCD_MENU4");
        case KEY_KBD_LCD_MENU5:
            return do_format_arg(sp, "", "KEY_KBD_LCD_MENU5");
        case BTN_TRIGGER_HAPPY1:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY1");
        case BTN_TRIGGER_HAPPY2:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY2");
        case BTN_TRIGGER_HAPPY3:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY3");
        case BTN_TRIGGER_HAPPY4:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY4");
        case BTN_TRIGGER_HAPPY5:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY5");
        case BTN_TRIGGER_HAPPY6:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY6");
        case BTN_TRIGGER_HAPPY7:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY7");
        case BTN_TRIGGER_HAPPY8:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY8");
        case BTN_TRIGGER_HAPPY9:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY9");
        case BTN_TRIGGER_HAPPY10:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY10");
        case BTN_TRIGGER_HAPPY11:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY11");
        case BTN_TRIGGER_HAPPY12:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY12");
        case BTN_TRIGGER_HAPPY13:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY13");
        case BTN_TRIGGER_HAPPY14:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY14");
        case BTN_TRIGGER_HAPPY15:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY15");
        case BTN_TRIGGER_HAPPY16:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY16");
        case BTN_TRIGGER_HAPPY17:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY17");
        case BTN_TRIGGER_HAPPY18:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY18");
        case BTN_TRIGGER_HAPPY19:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY19");
        case BTN_TRIGGER_HAPPY20:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY20");
        case BTN_TRIGGER_HAPPY21:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY21");
        case BTN_TRIGGER_HAPPY22:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY22");
        case BTN_TRIGGER_HAPPY23:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY23");
        case BTN_TRIGGER_HAPPY24:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY24");
        case BTN_TRIGGER_HAPPY25:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY25");
        case BTN_TRIGGER_HAPPY26:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY26");
        case BTN_TRIGGER_HAPPY27:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY27");
        case BTN_TRIGGER_HAPPY28:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY28");
        case BTN_TRIGGER_HAPPY29:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY29");
        case BTN_TRIGGER_HAPPY30:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY30");
        case BTN_TRIGGER_HAPPY31:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY31");
        case BTN_TRIGGER_HAPPY32:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY32");
        case BTN_TRIGGER_HAPPY33:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY33");
        case BTN_TRIGGER_HAPPY34:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY34");
        case BTN_TRIGGER_HAPPY35:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY35");
        case BTN_TRIGGER_HAPPY36:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY36");
        case BTN_TRIGGER_HAPPY37:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY37");
        case BTN_TRIGGER_HAPPY38:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY38");
        case BTN_TRIGGER_HAPPY39:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY39");
        case BTN_TRIGGER_HAPPY40:
            return do_format_arg(sp, "", "BTN_TRIGGER_HAPPY40");
        default:
            return do_format_arg(sp, ":x", (uint32_t)arg);
    }
}

template<>
consteval bool format_spec_valid<evdev_syn_code>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, evdev_syn_code arg) {
    switch (arg) {
        using enum evdev_syn_code;

        case SYN_REPORT:
            return do_format_arg(sp, "", "SYN_REPORT");
        case SYN_CONFIG:
            return do_format_arg(sp, "", "SYN_CONFIG");
        case SYN_MT_REPORT:
            return do_format_arg(sp, "", "SYN_MT_REPORT");
        case SYN_DROPPED:
            return do_format_arg(sp, "", "SYN_DROPPED");
        case SYN_MAX:
            return do_format_arg(sp, "", "SYN_MAX");
        default:
            return do_format_arg(sp, ":x", (uint32_t)arg);
    }
}

template<>
consteval bool format_spec_valid<evdev_rel_code>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, evdev_rel_code arg) {
    switch (arg) {
        using enum evdev_rel_code;

        case REL_X:
            return do_format_arg(sp, "", "REL_X");
        case REL_Y:
            return do_format_arg(sp, "", "REL_Y");
        case REL_Z:
            return do_format_arg(sp, "", "REL_Z");
        case REL_RX:
            return do_format_arg(sp, "", "REL_RX");
        case REL_RY:
            return do_format_arg(sp, "", "REL_RY");
        case REL_RZ:
            return do_format_arg(sp, "", "REL_RZ");
        case REL_HWHEEL:
            return do_format_arg(sp, "", "REL_HWHEEL");
        case REL_DIAL:
            return do_format_arg(sp, "", "REL_DIAL");
        case REL_WHEEL:
            return do_format_arg(sp, "", "REL_WHEEL");
        case REL_MISC:
            return do_format_arg(sp, "", "REL_MISC");
        case REL_RESERVED:
            return do_format_arg(sp, "", "REL_RESERVED");
        case REL_WHEEL_HI_RES:
            return do_format_arg(sp, "", "REL_WHEEL_HI_RES");
        case REL_HWHEEL_HI_RES:
            return do_format_arg(sp, "", "REL_HWHEEL_HI_RES");
        case REL_MAX:
            return do_format_arg(sp, "", "REL_MAX");
        default:
            return do_format_arg(sp, ":x", (uint32_t)arg);
    }
}

template<>
consteval bool format_spec_valid<evdev_code>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, evdev_code arg) {
    switch (arg) {
        default:
            return do_format_arg(sp, ":x", (uint32_t)arg);
    }
}

template<>
consteval bool format_spec_valid<evdev_type>(string_view fmt) {
    return fmt.empty();
}

struct _timeval {
    uint64_t tv_sec;
    uint32_t tv_usec;
};

export struct input_event {
    struct _timeval time;
    enum evdev_type type;
    enum evdev_code code;
    int32_t value;
};

static_assert(sizeof(input_event) == 0x18);

export struct input_event_with_id {
    uint32_t id;
    input_event ev;
} __attribute__((packed));

static_assert(sizeof(input_event_with_id) == 0x1c);
