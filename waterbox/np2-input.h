/* np2-input.h - the core's buttons, in the order Chimera numbers them.
 *
 * The index of a button here IS the movie format, so the order never
 * changes once published. It is the order docs/PLAN.md section 6 settled on
 * (user-decided, 2026-10-23): the keyboard as it sits on a PC-98, starting
 * top-left, row by row, then the four arrow keys together (owner,
 * 2026-10-25), then the whole number pad together (owner, 2026-10-24), then
 * the mouse, the joystick and the disk buttons.
 *
 * Each entry: the button's name, a short column header (at most 8
 * characters, for TAStudio), and what it is: a PC-98 key code (0x00-0x7f,
 * what keystat_keydown takes) or one of the NP2BTN_* kinds below.
 * tools/gen-config.py reads this table to write waterbox.config, so the
 * package and the driver cannot disagree.
 */
#ifndef NP2_INPUT_H
#define NP2_INPUT_H

enum {
	NP2BTN_MOUSE_LEFT = 0x100,
	NP2BTN_MOUSE_RIGHT,
	NP2BTN_JOY1_UP, NP2BTN_JOY1_DOWN, NP2BTN_JOY1_LEFT, NP2BTN_JOY1_RIGHT,
	NP2BTN_JOY1_A, NP2BTN_JOY1_B,
	NP2BTN_FDD1_PREV, NP2BTN_FDD1_NEXT, NP2BTN_FDD1_SWAP,
	NP2BTN_FDD2_PREV, NP2BTN_FDD2_NEXT, NP2BTN_FDD2_SWAP,
	NP2BTN_RESET
};

typedef struct {
	const char *name;
	const char *header;
	int what;
} NP2BUTTON;

#define NP2_BUTTONS(X) \
	/* row 1 */ \
	X("Key STOP", "STOP", 0x60) X("Key COPY", "COPY", 0x61) \
	X("Key f.1", "f1", 0x62) X("Key f.2", "f2", 0x63) X("Key f.3", "f3", 0x64) \
	X("Key f.4", "f4", 0x65) X("Key f.5", "f5", 0x66) X("Key f.6", "f6", 0x67) \
	X("Key f.7", "f7", 0x68) X("Key f.8", "f8", 0x69) X("Key f.9", "f9", 0x6a) \
	X("Key f.10", "f10", 0x6b) \
	X("Key vf.1", "vf1", 0x52) X("Key vf.2", "vf2", 0x53) X("Key vf.3", "vf3", 0x54) \
	X("Key vf.4", "vf4", 0x55) X("Key vf.5", "vf5", 0x56) \
	/* row 2 */ \
	X("Key ESC", "ESC", 0x00) \
	X("Key 1", "1", 0x01) X("Key 2", "2", 0x02) X("Key 3", "3", 0x03) X("Key 4", "4", 0x04) \
	X("Key 5", "5", 0x05) X("Key 6", "6", 0x06) X("Key 7", "7", 0x07) X("Key 8", "8", 0x08) \
	X("Key 9", "9", 0x09) X("Key 0", "0", 0x0a) \
	X("Key Minus", "-", 0x0b) X("Key Caret", "^", 0x0c) X("Key Yen", "Yen", 0x0d) \
	X("Key BS", "BS", 0x0e) \
	X("Key ROLL UP", "RLUP", 0x36) X("Key ROLL DOWN", "RLDN", 0x37) \
	X("Key HOME CLR", "HOME", 0x3e) X("Key HELP", "HELP", 0x3f) \
	/* row 3 */ \
	X("Key TAB", "TAB", 0x0f) \
	X("Key Q", "Q", 0x10) X("Key W", "W", 0x11) X("Key E", "E", 0x12) X("Key R", "R", 0x13) \
	X("Key T", "T", 0x14) X("Key Y", "Y", 0x15) X("Key U", "U", 0x16) X("Key I", "I", 0x17) \
	X("Key O", "O", 0x18) X("Key P", "P", 0x19) \
	X("Key At", "@", 0x1a) X("Key Left Bracket", "[", 0x1b) X("Key RETURN", "RET", 0x1c) \
	X("Key INS", "INS", 0x38) X("Key DEL", "DEL", 0x39) \
	/* row 4 */ \
	X("Key CTRL", "CTRL", 0x74) \
	X("Key A", "A", 0x1d) X("Key S", "S", 0x1e) X("Key D", "D", 0x1f) X("Key F", "F", 0x20) \
	X("Key G", "G", 0x21) X("Key H", "H", 0x22) X("Key J", "J", 0x23) X("Key K", "K", 0x24) \
	X("Key L", "L", 0x25) \
	X("Key Semicolon", ";", 0x26) X("Key Colon", ":", 0x27) X("Key Right Bracket", "]", 0x28) \
	/* row 5 */ \
	X("Key SHIFT", "SHIFT", 0x70) \
	X("Key Z", "Z", 0x29) X("Key X", "X", 0x2a) X("Key C", "C", 0x2b) X("Key V", "V", 0x2c) \
	X("Key B", "B", 0x2d) X("Key N", "N", 0x2e) X("Key M", "M", 0x2f) \
	X("Key Comma", ",", 0x30) X("Key Period", ".", 0x31) X("Key Slash", "/", 0x32) \
	X("Key Underscore", "_", 0x33) X("Key Right SHIFT", "RSHIFT", 0x7d) \
	/* row 6 */ \
	X("Key CAPS", "CAPS", 0x71) X("Key KANA", "KANA", 0x72) X("Key GRPH", "GRPH", 0x73) \
	X("Key NFER", "NFER", 0x51) X("Key SPACE", "SPACE", 0x34) X("Key XFER", "XFER", 0x35) \
	/* the arrow keys */ \
	X("Key Up", "Up", 0x3a) X("Key Down", "Down", 0x3d) \
	X("Key Left", "Left", 0x3b) X("Key Right", "Right", 0x3c) \
	/* the number pad, row by row */ \
	X("Key Pad Minus", "P-", 0x40) X("Key Pad Slash", "P/", 0x41) \
	X("Key Pad 7", "P7", 0x42) X("Key Pad 8", "P8", 0x43) X("Key Pad 9", "P9", 0x44) X("Key Pad Asterisk", "P*", 0x45) \
	X("Key Pad 4", "P4", 0x46) X("Key Pad 5", "P5", 0x47) X("Key Pad 6", "P6", 0x48) X("Key Pad Plus", "P+", 0x49) \
	X("Key Pad 1", "P1", 0x4a) X("Key Pad 2", "P2", 0x4b) X("Key Pad 3", "P3", 0x4c) X("Key Pad Equals", "P=", 0x4d) \
	X("Key Pad 0", "P0", 0x4e) X("Key Pad Comma", "P,", 0x4f) X("Key Pad Period", "P.", 0x50) \
	/* the mouse */ \
	X("Mouse Left Button", "MLeft", NP2BTN_MOUSE_LEFT) \
	X("Mouse Right Button", "MRight", NP2BTN_MOUSE_RIGHT) \
	/* the joystick on the sound board. NP2kai wires only its first port \
	 * to a joystick (sound/fmboard.c, fmboard_getjoy); the second port \
	 * reads the keyboard there, so no second stick is offered. */ \
	X("P1 Joystick Up", "J1Up", NP2BTN_JOY1_UP) X("P1 Joystick Down", "J1Down", NP2BTN_JOY1_DOWN) \
	X("P1 Joystick Left", "J1Left", NP2BTN_JOY1_LEFT) X("P1 Joystick Right", "J1Right", NP2BTN_JOY1_RIGHT) \
	X("P1 Joystick Button 1", "J1A", NP2BTN_JOY1_A) X("P1 Joystick Button 2", "J1B", NP2BTN_JOY1_B) \
	/* the disks */ \
	X("Drive 1 Previous Floppy Disk", "D1Prev", NP2BTN_FDD1_PREV) \
	X("Drive 1 Next Floppy Disk", "D1Next", NP2BTN_FDD1_NEXT) \
	X("Drive 1 Swap Floppy Disk", "D1Swap", NP2BTN_FDD1_SWAP) \
	X("Drive 2 Previous Floppy Disk", "D2Prev", NP2BTN_FDD2_PREV) \
	X("Drive 2 Next Floppy Disk", "D2Next", NP2BTN_FDD2_NEXT) \
	X("Drive 2 Swap Floppy Disk", "D2Swap", NP2BTN_FDD2_SWAP) \
	X("Reset", "Reset", NP2BTN_RESET)

/* The axes: the bus mouse's movement since the last frame. */
#define NP2_AXIS_MOUSE_X 0
#define NP2_AXIS_MOUSE_Y 1
#define NP2_AXIS_COUNT 2

#endif
