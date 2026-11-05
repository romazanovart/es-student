#include "api/api.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "api/api-commands.h"

static void command_help(void)
{
    printf("%-16s %s\n", "help", "list of commands");
    for (uint32_t i = 0; i < api_command_count; i++)
    {
        printf("%-16s %s\n", api_commands[i].name, api_commands[i].help);
    }
}

void api_handle(const command_t *command)
{
    if (command->truncated)
    {
        printf("error: command longer than 63 characters or 4 arguments\n");
        return;
    }

    if (strcmp(command->name, "help") == 0)
    {
        command_help();
        return;
    }

    for (uint32_t i = 0; i < api_command_count; i++)
    {
        if (strcmp(command->name, api_commands[i].name) == 0)
        {
            api_commands[i].callback(command);
            return;
        }
    }

    printf("error: unknown command '%s', try help\n", command->name);
}
