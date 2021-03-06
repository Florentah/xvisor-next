/**
 * Copyright (c) 2010 Anup Patel.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file vmm_cmdmgr.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file of command manager
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>

#define VMM_CMD_DELIM_CHAR	';'
#define VMM_CMD_ARG_MAXCOUNT	32
#define VMM_CMD_ARG_DELIM_CHAR	' '
#define VMM_CMD_ARG_DELIM_CHAR1	'\t'

struct vmm_cmdmgr_ctrl {
        struct dlist cmd_list;
};

static struct vmm_cmdmgr_ctrl cmctrl;

int vmm_cmdmgr_register_cmd(struct vmm_cmd *cmd)
{
	bool found;
	struct dlist *l;
	struct vmm_cmd *c;

	if (cmd == NULL) {
		return VMM_EFAIL;
	}

	c = NULL;
	found = FALSE;
	list_for_each(l, &cmctrl.cmd_list) {
		c = list_entry(l, struct vmm_cmd, head);
		if (strcmp(c->name, cmd->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&cmd->head);

	list_add_tail(&cmd->head, &cmctrl.cmd_list);

	return VMM_OK;
}

int vmm_cmdmgr_unregister_cmd(struct vmm_cmd *cmd)
{
	bool found;
	struct dlist *l;
	struct vmm_cmd *c;

	if (cmd == NULL || list_empty(&cmctrl.cmd_list)) {
		return VMM_EFAIL;
	}

	c = NULL;
	found = FALSE;
	list_for_each(l, &cmctrl.cmd_list) {
		c = list_entry(l, struct vmm_cmd, head);
		if (strcmp(c->name, cmd->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return VMM_ENOTAVAIL;
	}

	list_del(&c->head);

	return VMM_OK;
}

struct vmm_cmd *vmm_cmdmgr_cmd_find(const char *cmd_name)
{
	bool found;
	struct dlist *l;
	struct vmm_cmd *c;

	if (!cmd_name) {
		return NULL;
	}

	found = FALSE;
	c = NULL;

	list_for_each(l, &cmctrl.cmd_list) {
		c = list_entry(l, struct vmm_cmd, head);
		if (strcmp(c->name, cmd_name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		return NULL;
	}

	return c;
}

struct vmm_cmd *vmm_cmdmgr_cmd(int index)
{
	bool found;
	struct dlist *l;
	struct vmm_cmd *c;

	if (index < 0) {
		return NULL;
	}

	c = NULL;
	found = FALSE;

	list_for_each(l, &cmctrl.cmd_list) {
		c = list_entry(l, struct vmm_cmd, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	if (!found) {
		return NULL;
	}

	return c;
}

u32 vmm_cmdmgr_cmd_count(void)
{
	u32 retval;
	struct dlist *l;

	retval = 0;

	list_for_each(l, &cmctrl.cmd_list) {
		retval++;
	}

	return retval;
}

int vmm_cmdmgr_execute_cmd(struct vmm_chardev *cdev, int argc, char **argv)
{
	int ret;
	struct vmm_cmd *cmd = NULL;

	/* Find & execute the commad */
	if ((cmd = vmm_cmdmgr_cmd_find(argv[0]))) {
		/* Found a matching command so execute it. */
		if ((ret = cmd->exec(cdev, argc, argv))) {
			vmm_cprintf(cdev, "Error: command %s failed "
					  "(code %d)\n", argv[0], ret);
			return ret;
		}
	} else {
		/* Did not find command. */
		vmm_cprintf(cdev, "Error: unknown command %s\n", argv[0]);
		return VMM_ENOTAVAIL;
	}

	return VMM_OK;
}

int vmm_cmdmgr_execute_cmdstr(struct vmm_chardev *cdev, char *cmds, 
	bool (*filter)(struct vmm_chardev *cdev, int argc, char **argv))
{
	int argc, ret;
	char *argv[VMM_CMD_ARG_MAXCOUNT];
	char *c = cmds;
	bool eos = 0;
	argc = 0;
	while (*c) {
		while (*c == VMM_CMD_ARG_DELIM_CHAR ||
		       *c == VMM_CMD_ARG_DELIM_CHAR1) {
			c++;
		}
		if (*c == '\0') {
			break;
		}
		if (argc < VMM_CMD_ARG_MAXCOUNT && *c != VMM_CMD_DELIM_CHAR) {
			argv[argc] = c;
			argc++;
		}
		while (*c != VMM_CMD_ARG_DELIM_CHAR &&
		       *c != VMM_CMD_ARG_DELIM_CHAR1 &&
		       *c != VMM_CMD_DELIM_CHAR && *c != '\0') {
			c++;
		}
		if (*c == '\0')
			eos = 1;
		if ((*c == VMM_CMD_DELIM_CHAR || *c == '\0') && argc > 0) {
			*c = '\0';
			c++;
			if (filter && filter(cdev, argc, argv)) {
				vmm_cprintf(cdev, "Error: command %s "
						  "filtered\n", argv[0]);
			} else {
				ret = vmm_cmdmgr_execute_cmd(cdev, argc, argv);
			}
			if (ret)
				return ret;
			argc = 0;
			if (eos)
				break;
		} else {
			*c = '\0';
			c++;
		}
	}
	if (argc > 0) {
		ret = vmm_cmdmgr_execute_cmd(cdev, argc, argv);
		if (ret) {
			return ret;
		}
	}
	return VMM_OK;
}

static void cmd_help_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage: ");
	vmm_cprintf(cdev, "   help\n");
	vmm_cprintf(cdev, "   help <cmd_name1> [<cmd_name2>] ...\n");
}

static int cmd_help_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	u32 i, cmd_count;
	struct vmm_cmd *cmd;
	
	if (argc == 1) {
		cmd_count = vmm_cmdmgr_cmd_count();
		for (i = 0; i < cmd_count; i++) {
			if ((cmd = vmm_cmdmgr_cmd(i))) {
				vmm_cprintf(cdev, "%-12s - %s\n", 
						  cmd->name, cmd->desc);
			}
		}
	} else if (argc > 1) {
		for (i = 1; i < argc; i++) {
			if ((cmd = vmm_cmdmgr_cmd_find(argv[i]))) {
				vmm_cprintf(cdev, "%-12s - %s\n",
						  cmd->name, cmd->desc);
				cmd->usage(cdev);
			}
			vmm_printf("\n");
		}
	}
	return 0;
}

static struct vmm_cmd help_cmd = {
	.name = "help",
	.desc = "displays list of all commands",
	.usage = cmd_help_usage,
	.exec = cmd_help_exec,
};

int __init vmm_cmdmgr_init(void)
{
	memset(&cmctrl, 0, sizeof(cmctrl));

	INIT_LIST_HEAD(&cmctrl.cmd_list);

	return vmm_cmdmgr_register_cmd(&help_cmd);
}

