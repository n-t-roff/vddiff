#include "fkeyListDisplay.h"
#include "compat.h"
#include "main.h"
#include "ui.h"
#include "ui2.h"
#include <stdlib.h> /* getenv() */
#include <string.h> /* strstr() */

enum terminalType
{
    UNINITIALIZED,
    UNKNOWN_TERMINAL,
    XTERM,
    RXVT,
    LINUX
};

static void print_fkey_set(void);
static enum terminalType getTerminalType(void);
static void printAlias(int i, enum terminalType terminalType);

static unsigned topFkeyNumber;

void disp_fkey_list(void)
{
    while (1) {
        print_fkey_set();
        {
            int i;
            int lkey_ = anykey("Press '0'..'9', <F1>..<F48>, or any other key");
            unsigned screen_heigth = fkey_set ? listh - 2 : listh;

            if (lkey_ >= '1' && lkey_ <= '9')
            {
                fkey_set = lkey_ - '1';
                topFkeyNumber = 0;
                continue;
            }
            switch (lkey_)
            {
            case KEY_RIGHT:
            case 'l':
                if (fkey_set >= FKEY_MUX_NUM - 1)
                {
                    fkey_set = 0;
                }
                else
                {
                    ++fkey_set;
                }
                topFkeyNumber = 0;
                continue;

            case KEY_LEFT:
            case 'h':
                if (fkey_set <= 0)
                {
                    fkey_set = FKEY_MUX_NUM - 1;
                }
                else
                {
                    --fkey_set;
                }
                topFkeyNumber = 0;
                continue;

            case KEY_DOWN:
            case 'j':
            case '+':
            case CTRL('e'):
            case '\n':
                if (topFkeyNumber + 1 < FKEY_NUM)
                {
                    ++topFkeyNumber;
                }
                continue;

            case KEY_UP:
            case 'k':
            case '-':
            case CTRL('y'):
                if (topFkeyNumber != 0)
                {
                    --topFkeyNumber;
                }
                continue;

            case KEY_NPAGE:
            case ' ':
                if (topFkeyNumber + screen_heigth < FKEY_NUM)
                {
                    topFkeyNumber += screen_heigth;
                }
                continue;

            case KEY_PPAGE:
            case KEY_BACKSPACE:
            case CERASE:
                if (topFkeyNumber)
                {
                    topFkeyNumber = topFkeyNumber > screen_heigth ? topFkeyNumber - screen_heigth : 0;
                }
                continue;

            case 'Q':
                keep_ungetch(lkey_);
                break;
            }
            for (i = 0; i < FKEY_NUM; i++)
            {
                if (lkey_ == KEY_F(i + 1))
                {
                    keep_ungetch(lkey_);
                    break;
                }
            }
        }
        break;
    }
}

static void print_fkey_set(void)
{
    standendc(wlist);
    werase(wlist);
    if (fkey_set)
    {
        mvwprintw(wlist, 0, 0, "Set %d", fkey_set + 1);
    }

    int textColumn = 4;
    enum terminalType terminalType = getTerminalType();

    switch (terminalType)
    {
    case XTERM:
        textColumn = 11;
        break;

    case RXVT:
        textColumn = 16;
        break;

    case LINUX:
        textColumn = 9;
        break;

    default:
        break;
    }

    int currentLineNumber;
    int currentFkeyNumber;
    for (currentLineNumber = 0, currentFkeyNumber = topFkeyNumber; currentFkeyNumber < FKEY_NUM; ++currentLineNumber, ++currentFkeyNumber)
    {
        int j = fkey_set ? currentLineNumber + 2 : currentLineNumber; /* display line */
        if (j == (int)listh)
        {
            break;
        }
        mvwprintw(wlist, j, 0, "F%d", currentFkeyNumber + 1);
        printAlias(currentFkeyNumber + 1, terminalType);

        if (fkey_cmd[fkey_set][currentFkeyNumber])
        {
            if (fkey_comment[fkey_set][currentFkeyNumber])
            {
                mvwprintw(wlist, j, textColumn, " \"%c %s\" (%s)",
                    FKEY_CMD_CHR(currentFkeyNumber), fkey_cmd[fkey_set][currentFkeyNumber],
                    fkey_comment[fkey_set][currentFkeyNumber]);
            }
            else
            {
                mvwprintw(wlist, j, textColumn, " \"%c %s\"",
                    FKEY_CMD_CHR(currentFkeyNumber), fkey_cmd[fkey_set][currentFkeyNumber]);
            }
            continue;
        }
        if (!sh_str[fkey_set][currentFkeyNumber])
        {
            continue;
        }
        if (fkey_comment[fkey_set][currentFkeyNumber])
        {
            mvwprintw(wlist, j, textColumn, " \"%ls\" (%s)",
                      sh_str[fkey_set][currentFkeyNumber], fkey_comment[fkey_set][currentFkeyNumber]);
        }
        else
        {
            mvwprintw(wlist, j, textColumn, " \"%ls\"", sh_str[fkey_set][currentFkeyNumber]);
        }
    }
}

static enum terminalType getTerminalType(void)
{
    static enum terminalType terminalType = UNINITIALIZED;

    if (terminalType == UNINITIALIZED)
    {
        terminalType = UNKNOWN_TERMINAL;
        const char *const term = getenv("TERM");

        if (term == NULL)
        {
            /* do nothing */
        }
        else if (strstr(term, "xterm"))
        {
            terminalType = XTERM;
        }
        else if (strstr(term, "rxvt"))
        {
            terminalType = RXVT;
        }
        else if (!strcmp(term, "linux"))
        {
            terminalType = LINUX;
        }
    }
    return terminalType;
}

static void printAlias(const int i, const enum terminalType terminalType)
{
    int s0  = -1, sn  = -1;
    int c0  = -1, cn  = -1;
    int cs0 = -1, csn = -1;

    switch (terminalType)
    {
    case XTERM:
        s0  = 13; sn  = 24;
        c0  = 25; cn  = 36;
        cs0 = 37; csn = 48;
        break;

    case RXVT:
        s0  = 11; sn  = 22;
        c0  = 23; cn  = 34;
        cs0 = 33; csn = 44;
        break;

    case LINUX:
        s0 = 13; sn = 20;
        break;

    default:
        return;
    }

    /* Ranges may overlap, don't use "else if"! */
    if (i >= s0 && i <= sn)
    {
        wprintw(wlist, " S-F%d", i - s0 + 1);
    }
    if (i >= c0 && i <= cn)
    {
        wprintw(wlist, " C-F%d", i - c0 + 1);
    }
    if (i >= cs0 && i <= csn)
    {
        wprintw(wlist, " CS-F%d", i - cs0 + 1);
    }
}
