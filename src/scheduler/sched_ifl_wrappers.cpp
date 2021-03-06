/*
 * Copyright (C) 1994-2020 Altair Engineering, Inc.
 * For more information, contact Altair at www.altair.com.
 *
 * This file is part of both the OpenPBS software ("OpenPBS")
 * and the PBS Professional ("PBS Pro") software.
 *
 * Open Source License Information:
 *
 * OpenPBS is free software. You can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * OpenPBS is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Commercial License Information:
 *
 * PBS Pro is commercially licensed software that shares a common core with
 * the OpenPBS software.  For a copy of the commercial license terms and
 * conditions, go to: (http://www.pbspro.com/agreement.html) or contact the
 * Altair Legal Department.
 *
 * Altair's dual-license business model allows companies, individuals, and
 * organizations to create proprietary derivative works of OpenPBS and
 * distribute them - whether embedded or bundled with other software -
 * under a commercial license agreement.
 *
 * Use of Altair's trademarks, including but not limited to "PBS™",
 * "OpenPBS®", "PBS Professional®", and "PBS Pro™" and Altair's logos is
 * subject to Altair's trademark licensing policies.
 */

#include <pbs_config.h>

#include <stdlib.h>
#include <pbs_ifl.h>
#include <libpbs.h>
#include "data_types.h"
#include "fifo.h"
#include "globals.h"
#include "job_info.h"
#include "log.h"


/**
 * @brief	Send the relevant runjob request to server
 *
 * @param[in]	virtual_sd	-	virtual sd for the cluster
 * @param[in]	has_runjob_hook	- does server have a runjob hook?
 * @param[in]	jobid	-	id of the job to run
 * @param[in]	execvnode	-	the execvnode to run the job on
 * @param[in]	svr_id_node	-	server id of the first node in execvnode
 * @param[in]	svr_id_job -	server id of the job
 *
 * @return	int
 * @retval	return value of the runjob call
 */
int
send_run_job(int virtual_sd, int has_runjob_hook, char *jobid, char *execvnode,
 	     char *svr_id_node, char *svr_id_job)
{
	char extend[PBS_MAXHOSTNAME + 6];
 	int job_owner_sd;

	if (jobid == NULL || execvnode == NULL)
		return 1;

  	job_owner_sd = get_svr_inst_fd(virtual_sd, svr_id_job);

  	extend[0] = '\0';
 	if (svr_id_node && svr_id_job && strcmp(svr_id_node, svr_id_job) != 0)
 		snprintf(extend, sizeof(extend), "%s=%s", SERVER_IDENTIFIER, svr_id_node);

	if (sc_attrs.runjob_mode == RJ_EXECJOB_HOOK)
		return pbs_runjob(job_owner_sd, jobid, execvnode, extend);
	else if (((sc_attrs.runjob_mode == RJ_RUNJOB_HOOK) && has_runjob_hook))
		return pbs_asyrunjob_ack(job_owner_sd, jobid, execvnode, extend);
	else
		return pbs_asyrunjob(job_owner_sd, jobid, execvnode, extend);
}

/**
 * @brief
 * 		send delayed attributes to the server for a job
 *
 * @param[in]	job_owner_sd	-	server connection descriptor of the job owner
 * @param[in]	job_name	-	name of job for pbs_asyalterjob()
 * @param[in]	pattr	-	attrl list to update on the server
 *
 * @return	int
 * @retval	1	success
 * @retval	0	failure to update
 */
int
send_attr_updates(int job_owner_sd, char *job_name, struct attrl *pattr)
{
	const char *errbuf;
	int one_attr = 0;

	if (job_name == NULL || pattr == NULL)
		return 0;

	if (job_owner_sd == SIMULATE_SD)
		return 1; /* simulation always successful */

	if (pattr->next == NULL)
		one_attr = 1;

	if (pbs_asyalterjob(job_owner_sd, job_name, pattr, NULL) == 0) {
		last_attr_updates = time(NULL);
		return 1;
	}

	if (is_finished_job(pbs_errno) == 1) {
		if (one_attr)
			log_eventf(PBSEVENT_SCHED, PBS_EVENTCLASS_JOB, LOG_INFO, job_name,
				   "Failed to update attr \'%s\' = %s, Job already finished",
				   pattr->name, pattr->value);
		else
			log_event(PBSEVENT_SCHED, PBS_EVENTCLASS_JOB, LOG_INFO, job_name,
				"Failed to update job attributes, Job already finished");
		return 0;
	}

	errbuf = pbs_geterrmsg(job_owner_sd);
	if (errbuf == NULL)
		errbuf = "";
	if (one_attr)
		log_eventf(PBSEVENT_SCHED, PBS_EVENTCLASS_SCHED, LOG_WARNING, job_name,
			   "Failed to update attr \'%s\' = %s: %s (%d)",
			   pattr->name, pattr->value, errbuf, pbs_errno);
	else
		log_eventf(PBSEVENT_SCHED, PBS_EVENTCLASS_SCHED, LOG_WARNING, job_name,
			"Failed to update job attributes: %s (%d)",
			errbuf, pbs_errno);

	return 0;
}

/**
 * @brief	Wrapper for pbs_preempt_jobs
 *
 * @param[in]	virtual_sd - virtual sd for the cluster
 * @param[in]	preempt_jobs_list - list of jobs to preempt
 *
 * @return	preempt_job_info *
 * @retval	return value of pbs_preempt_jobs
 */
preempt_job_info *
send_preempt_jobs(int virtual_sd, char **preempt_jobs_list)
{
    return pbs_preempt_jobs(virtual_sd, preempt_jobs_list);
}
