#pragma once

#include <Windows.h>

#define SCI_START 2000

#define SCI_ADDSTYLEDTEXT SCI_START + 2
#define SCI_CLEARALL SCI_START + 4
#define SCI_GETLENGTH SCI_START + 6
#define SCI_GETCHARAT SCI_START + 7

#define SCI_REDO SCI_START + 11
#define SCI_SETUNDOCOLLECTION SCI_START + 12
#define SCI_SELECTALL SCI_START + 13
#define SCI_SETSAVEPOINT  SCI_START + 14

#define SCI_GOTOPOS SCI_START + 25
#define SCI_SETMARGINWIDTH SCI_START + 34
#define SCI_SETTABWIDTH SCI_START + 36
#define SCI_SETCODEPAGE SCI_START + 37
#define SCI_MARKERADD SCI_START + 43
#define SCI_MARKERDELETEALL SCI_START + 45
#define SCI_STYLECLEARALL SCI_START + 50
#define SCI_STYLESETFORE SCI_START + 51
#define SCI_STYLESETBACK SCI_START + 52
#define SCI_STYLESETBOLD SCI_START + 53
#define SCI_STYLESETITALIC SCI_START + 54
#define SCI_STYLESETSIZE SCI_START + 55
#define SCI_STYLESETFONT SCI_START + 56
#define SCI_SETFORE SCI_START + 60
#define SCI_SETBACK SCI_START + 61
#define SCI_SETSIZE SCI_START + 64
#define SCI_SETFONT SCI_START + 65
#define SCI_SETSELFORE SCI_START + 67
#define SCI_SETSELBACK SCI_START + 68

#define STYLE_MAX 31

#define SC_EOL_CRLF 0
#define SC_EOL_CR 1
#define SC_EOL_LF 2

#define SCN_STYLENEEDED 2000
#define SCN_CHARADDED 2001

#define SCI_GETSTYLEAT SCI_START + 10
#define SCI_GETENDSTYLED SCI_START + 28
#define SCI_AUTOCCANCEL SCI_START + 101
#define SCI_AUTOCACTIVE SCI_START + 102
#define SCI_CALLTIPCANCEL SCI_START + 201
#define SCI_CALLTIPACTIVE SCI_START + 202

// Key messages
#define SCI_LINEDOWN SCI_START + 300
#define SCI_LINEDOWNEXTEND SCI_START + 301
#define SCI_LINEUP SCI_START + 302
#define SCI_LINEUPEXTEND SCI_START + 303
#define SCI_CHARLEFT SCI_START + 304
#define SCI_CHARLEFTEXTEND SCI_START + 305
#define SCI_CHARRIGHT SCI_START + 306
#define SCI_CHARRIGHTEXTEND SCI_START + 307
#define SCI_WORDLEFT SCI_START + 308
#define SCI_WORDLEFTEXTEND SCI_START + 309
#define SCI_WORDRIGHT SCI_START + 310
#define SCI_WORDRIGHTEXTEND SCI_START + 311
#define SCI_HOME SCI_START + 312
#define SCI_HOMEEXTEND SCI_START + 313
#define SCI_LINEEND SCI_START + 314
#define SCI_LINEENDEXTEND SCI_START + 315
#define SCI_DOCUMENTSTART SCI_START + 316
#define SCI_DOCUMENTSTARTEXTEND SCI_START + 317
#define SCI_DOCUMENTEND SCI_START + 318
#define SCI_DOCUMENTENDEXTEND SCI_START + 319
#define SCI_PAGEUP SCI_START + 320
#define SCI_PAGEUPEXTEND SCI_START + 321
#define SCI_PAGEDOWN SCI_START + 322
#define SCI_PAGEDOWNEXTEND SCI_START + 323
#define SCI_EDITTOGGLEOVERTYPE SCI_START + 324
#define SCI_CANCEL SCI_START + 325
#define SCI_DELETEBACK SCI_START + 326
#define SCI_TAB SCI_START + 327
#define SCI_BACKTAB SCI_START + 328
#define SCI_NEWLINE SCI_START + 329
#define SCI_FORMFEED SCI_START + 330
#define SCI_VCHOME SCI_START + 331
#define SCI_VCHOMEEXTEND SCI_START + 332

#define INDIC_MAX 2

#define INDIC0_MASK 32
#define INDIC1_MASK 64
#define INDIC2_MASK 128
#define INDICS_MASK (INDIC0_MASK | INDIC1_MASK | INDIC2_MASK)

void scintilla_register_classes(HINSTANCE);
