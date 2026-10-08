#include "stdio-text-protocol.h"

#include <stddef.h>
#include <stdio.h>
#include "pico/stdlib.h"

static char line[STDIO_TEXT_PROTOCOL_LINE_SIZE];
static uint32_t line_length;
static uint32_t overflow_length;
static bool skip_lf;
static command_t command;

static bool separator(char symbol)
{
    return symbol == ' ' || symbol == '\t';
}

static const command_t *finish_line(void)
{
    line[line_length] = '\0';
    command.name = NULL;
    command.argc = 0;
    command.truncated = overflow_length != 0;

    char *word = line;
    while (separator(*word))
    {
        word++;
    }

    if (*word != '\0')
    {
        command.name = word;

        while (*word != '\0')
        {
            while (*word != '\0' && !separator(*word))
            {
                word++;
            }

            if (*word == '\0')
            {
                break;
            }

            *word++ = '\0';
            while (separator(*word))
            {
                word++;
            }

            if (*word != '\0')
            {
                if (command.argc < STDIO_TEXT_PROTOCOL_MAX_ARGS)
                {
                    command.argv[command.argc++] = word;
                }
                else
                {
                    command.truncated = true;
                }
            }
        }
    }

    line_length = 0;
    overflow_length = 0;
    return command.name == NULL ? NULL : &command;
}

void stdio_text_protocol_init(void)
{
    line_length = 0;
    overflow_length = 0;
    skip_lf = false;
}

const command_t *stdio_text_protocol_handle(void)
{
    int input = getchar_timeout_us(0);
    if (input == PICO_ERROR_TIMEOUT)
    {
        return NULL;
    }

    char symbol = (char)input;
    if (skip_lf && symbol == '\n')
    {
        skip_lf = false;
        return NULL;
    }
    skip_lf = false;

    if (symbol == '\r' || symbol == '\n')
    {
        skip_lf = symbol == '\r';
        putchar('\n');
        return finish_line();
    }

    if (symbol == '\b' || symbol == 0x7f)
    {
        if (overflow_length > 0)
        {
            overflow_length--;
            printf("\b \b");
        }
        else if (line_length > 0)
        {
            line_length--;
            printf("\b \b");
        }
        return NULL;
    }

    if (symbol >= 0x20 && symbol <= 0x7e)
    {
        putchar(symbol);
        if (line_length + 1 < STDIO_TEXT_PROTOCOL_LINE_SIZE)
        {
            line[line_length++] = symbol;
        }
        else
        {
            overflow_length++;
        }
    }

    return NULL;
}
