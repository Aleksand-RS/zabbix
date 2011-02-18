/*
** Zabbix
** Copyright (C) 2000-2011 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**/

#include "common.h"
#include "cfg.h"
#include "db.h"
#include "log.h"
#include "zlog.h"

#include "nodewatcher.h"
#include "nodesender.h"
#include "history.h"

/******************************************************************************
 *                                                                            *
 * Function: is_master_node                                                   *
 *                                                                            *
 * Purpose:                                                                   *
 *                                                                            *
 * Parameters:                                                                *
 *                                                                            *
 * Return value:  SUCCEED - master_nodeid is a master node of current_nodeid  *
 *                FAIL - otherwise                                            *
 *                                                                            *
 * Author: Aleksander Vladishev                                               *
 *                                                                            *
 * Comments:                                                                  *
 *                                                                            *
 ******************************************************************************/
int	is_master_node(int current_nodeid, int master_nodeid)
{
	DB_RESULT	result;
	DB_ROW		row;
	int		res = FAIL;

	result = DBselect("select masterid from nodes where nodeid=%d",
		current_nodeid);

	if (NULL != (row = DBfetch(result)))
	{
		current_nodeid = (SUCCEED == DBis_null(row[0])) ? 0 : atoi(row[0]);
		if (current_nodeid == master_nodeid)
			res = SUCCEED;
		else if (0 != current_nodeid)
			res = is_master_node(current_nodeid, master_nodeid);
	}
	DBfree_result(result);

	return res;
}

/******************************************************************************
 *                                                                            *
 * Function: is_slave_node                                                    *
 *                                                                            *
 * Purpose:                                                                   *
 *                                                                            *
 * Parameters:                                                                *
 *                                                                            *
 * Return value:  SUCCEED - slave_nodeid is a slave node of current_nodeid    *
 *                FAIL - otherwise                                            *
 *                                                                            *
 * Author: Aleksandrs Saveljevs                                               *
 *                                                                            *
 * Comments:                                                                  *
 *                                                                            *
 ******************************************************************************/
int	is_slave_node(int current_nodeid, int slave_nodeid)
{
	DB_RESULT	result;
	DB_ROW		row;
	int		nodeid;
	int		ret = FAIL;

	result = DBselect(
			"select nodeid"
			" from nodes"
			" where masterid=%d",
			current_nodeid);

	while (FAIL == ret && NULL != (row = DBfetch(result)))
	{
		nodeid = atoi(row[0]);
		if (nodeid == slave_nodeid)
			ret = SUCCEED;
		else
			ret = is_slave_node(nodeid, slave_nodeid);
	}
	DBfree_result(result);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Function: is_direct_slave_node                                             *
 *                                                                            *
 * Purpose:                                                                   *
 *                                                                            *
 * Parameters:                                                                *
 *                                                                            *
 * Return value:  SUCCEED - slave_nodeid is our direct slave node             *
 *                FAIL - otherwise                                            *
 *                                                                            *
 * Author: Aleksander Vladishev                                               *
 *                                                                            *
 * Comments:                                                                  *
 *                                                                            *
 ******************************************************************************/
int	is_direct_slave_node(int slave_nodeid)
{
	DB_RESULT	result;
	DB_ROW		row;
	int		ret = FAIL;

	result = DBselect(
			"select nodeid"
			" from nodes"
			" where nodeid=%d"
				" and masterid=%d",
			slave_nodeid,
			CONFIG_NODEID);

	if (NULL != (row = DBfetch(result)))
		ret = SUCCEED;
	DBfree_result(result);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Function: main_nodewatcher_loop                                            *
 *                                                                            *
 * Purpose: periodically calculates checksum of config data                   *
 *                                                                            *
 * Parameters:                                                                *
 *                                                                            *
 * Return value:                                                              *
 *                                                                            *
 * Author: Alexei Vladishev                                                   *
 *                                                                            *
 * Comments: never returns                                                    *
 *                                                                            *
 ******************************************************************************/
int main_nodewatcher_loop()
{
	int	start, end;
	int	lastrun = 0;

	zabbix_log(LOG_LEVEL_DEBUG, "In main_nodewatcher_loop()");

	zbx_setproctitle("connecting to the database");

	DBconnect(ZBX_DB_CONNECT_NORMAL);

	for(;;)
	{
		start = time(NULL);

		zabbix_log( LOG_LEVEL_DEBUG, "Starting sync with nodes");

		if(lastrun + 120 < start)
		{
			process_nodes();

			lastrun = start;
		}

		/* Send new history data to master node */
		main_historysender();

		end = time(NULL);

		if(end-start<10)
		{
			zbx_setproctitle("sender [sleeping for %d seconds]",
				10-(end-start));
			zabbix_log( LOG_LEVEL_DEBUG, "Sleeping %d seconds",
				10-(end-start));
			sleep(10-(end-start));
		}
	}

	DBclose();
}
