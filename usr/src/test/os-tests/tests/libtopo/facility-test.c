/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */
/*
 * Copyright 2020 Joyent, Inc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fm/libtopo.h>
#include <fm/topo_hc.h>
#include <fm/topo_list.h>
#include <sys/varargs.h>

static const char *pname;
static const char optstr[] = "dR:";

struct factest_cb {
	boolean_t ftcb_gotfails;
};

static void
usage()
{
	(void) fprintf(stderr, "usage: %s [-R root]\n\n", pname);
}

/*
 * Generate an ISO 8601 timestamp
 */
static void
get_timestamp(char *buf, size_t bufsize)
{
	time_t utc_time;
	struct tm *p_tm;

	(void) time(&utc_time);
	p_tm = localtime(&utc_time);

	(void) strftime(buf, bufsize, "%FT%TZ", p_tm);
}

/* PRINTFLIKE1 */
static void
logmsg(const char *format, ...)
{
	char timestamp[128];
	va_list ap;

	get_timestamp(timestamp, sizeof (timestamp));
	(void) fprintf(stdout, "%s ", timestamp);
	va_start(ap, format);
	(void) vfprintf(stdout, format, ap);
	va_end(ap);
	(void) fprintf(stdout, "\n");
	(void) fflush(stdout);
}

/*
 * This routine takes a string containing an HC-scheme FMRI and returns
 * a new string with the authority portion of the FMRI stripped out.  We
 * do this to make the FMRI more readable in log messages.
 *
 * Caller is responsible for freeing returned string.
 */
static char *
fmri_strip(const char *fmri)
{
	char *stripped = NULL, *fmriend;

	/*
	 * HC scheme FMRI's have the format:
	 *  hc://<authority>/<hc-name>=<hc-inst>...
	 *
	 * So we want the part of the FMRI after the 3rd "/"
	 */
	fmriend = strstr(fmri + 5, "/");
	if (fmriend == NULL) {
		logmsg("%s: malformed FMRI: %s", __func__, fmri);
		return (NULL);
	}
	if (asprintf(&stripped, "hc://%s", fmriend) < 0) {
		logmsg("%s: failed to alloc string (%s)", __func__,
		    strerror(errno));
		return (NULL);
	}
	return (stripped);
}

static char *get_fmristr(topo_hdl_t *thp, tnode_t *node)
{
	nvlist_t *fmri = NULL;
	char *fullfmristr = NULL, *fmristr = NULL;
	int err;

	if (topo_node_resource(node, &fmri, &err) != 0 ||
	    topo_fmri_nvl2str(thp, fmri, &fullfmristr, &err) != 0) {
		logmsg("failed to get FMRI of node: %s", topo_strerror(err));
		nvlist_free(fmri);
		return (NULL);
	}
	nvlist_free(fmri);
	fmristr = fmri_strip(fullfmristr);
	topo_hdl_strfree(thp, fullfmristr);

	return (fmristr);
}

static int
run_sensor_tests(topo_hdl_t *thp, tnode_t *node)
{
	char *class = NULL, *fmristr = NULL;
	double reading;
	uint32_t units, state, type;
	int err, ret = 0;

	if ((fmristr = get_fmristr(thp, node)) == NULL) {
		logmsg("Failed to get string FMRI");
		return (-1);
	}
	logmsg("Running sensor tests on: %s", fmristr);
	free(fmristr);

	/*
	 * Assertion: All sensor nodes (discrete and threshold), have a
	 * "facility/class" property that we can get.
	 */
	if (topo_prop_get_string(node, TOPO_PGROUP_FACILITY, TOPO_SENSOR_CLASS,
	    &class, &err) != 0) {
		logmsg("failed to get prop %s/%s (%s)", TOPO_PGROUP_FACILITY,
		    TOPO_SENSOR_CLASS, topo_strerror(err));
		return (-1);
	}

	if (strcmp(class, TOPO_SENSOR_CLASS_THRESHOLD) == 0) {
		/*
		 * Assertion: Threshold sensors have a "facility/reading"
		 * property that we can get.
		 */
		if (topo_prop_get_double(node, TOPO_PGROUP_FACILITY,
		    TOPO_SENSOR_READING, &reading, &err) != 0) {
			logmsg("failed to get prop %s/%s (%s)",
			    TOPO_PGROUP_FACILITY, TOPO_SENSOR_READING,
			    topo_strerror(err));
			ret = -1;
		}
		/*
		 * Assertion: Threshold sensors have a "facility/units"
		 * property that we can get.
		 */
		if (topo_prop_get_uint32(node, TOPO_PGROUP_FACILITY,
		    TOPO_SENSOR_UNITS, &units, &err) != 0) {
			logmsg("failed to get prop %s/%s (%s)",
			    TOPO_PGROUP_FACILITY, TOPO_SENSOR_UNITS,
			    topo_strerror(err));
			ret = -1;
		}
	} else if (strcmp(class, TOPO_SENSOR_CLASS_DISCRETE) == 0) {
		/*
		 * Assertion: Discrete sensors have a "facility/state"
		 * property that we can get.
		 */
		if (topo_prop_get_uint32(node, TOPO_PGROUP_FACILITY,
		    TOPO_SENSOR_STATE, &state, &err) != 0) {
			logmsg("failed to get prop %s/%s (%s)",
			    TOPO_PGROUP_FACILITY, TOPO_SENSOR_STATE,
			    topo_strerror(err));
			ret = -1;
		}
		/*
		 * Assertion: Discrete sensors have a "facility/type"
		 * property that we can get.
		 */
		if (topo_prop_get_uint32(node, TOPO_PGROUP_FACILITY,
		    TOPO_FACILITY_TYPE, &type, &err) != 0) {
			logmsg("failed to get prop %s/%s (%s)",
			    TOPO_PGROUP_FACILITY, TOPO_FACILITY_TYPE,
			    topo_strerror(err));
			ret = -1;
		}
	} else {
		/*
		 * Assertion: The value of the "facility/class" property will be
		 * either "threshold" or "discrete".
		 */
		logmsg("unexpected value for %s/%s: %s", TOPO_PGROUP_FACILITY,
		    TOPO_SENSOR_CLASS, class);
		ret = -1;
	}
	topo_hdl_strfree(thp, class);

	return (ret);
}

static int
run_indicator_tests(topo_hdl_t *thp, tnode_t *node)
{
	char *fmristr = NULL;
	uint32_t type, origmode, chkmode, newmode;
	int err, ret = 0;

	if ((fmristr = get_fmristr(thp, node)) == NULL) {
		logmsg("Failed to get string FMRI");
		return (-1);
	}
	logmsg("Running indicator tests on: %s", fmristr);
	free(fmristr);

	/*
	 * Assertion: All indicator nodes, have a "facility/type" property
	 * that we can get.
	 */
	if (topo_prop_get_uint32(node, TOPO_PGROUP_FACILITY, TOPO_FACILITY_TYPE,
	    &type, &err) != 0) {
		logmsg("failed to get prop %s/%s (%s)", TOPO_PGROUP_FACILITY,
		    TOPO_FACILITY_TYPE, topo_strerror(err));
		ret = -1;
	}
	/*
	 * Assertion: All indicator nodes, have a "facility/mode" property
	 * that we can get and set to TOPO_LED_STATE_{ON|OFF}.
	 */
	if (topo_prop_get_uint32(node, TOPO_PGROUP_FACILITY,
	    TOPO_LED_MODE, &origmode, &err) != 0) {
		logmsg("failed to get prop %s/%s (%s)", TOPO_PGROUP_FACILITY,
		    TOPO_LED_MODE, topo_strerror(err));
		ret = -1;
	}
	newmode =
	    (origmode == TOPO_LED_STATE_ON ?
	    TOPO_LED_STATE_OFF : TOPO_LED_STATE_ON);

	if (topo_prop_set_uint32(node, TOPO_PGROUP_FACILITY,
	    TOPO_LED_MODE, TOPO_PROP_MUTABLE, newmode, &err) != 0) {
		logmsg("failed to set prop %s/%s (%s)", TOPO_PGROUP_FACILITY,
		    TOPO_LED_MODE, topo_strerror(err));
		ret = -1;
	}
	if (topo_prop_get_uint32(node, TOPO_PGROUP_FACILITY,
	    TOPO_LED_MODE, &chkmode, &err) != 0) {
		logmsg("failed to get prop %s/%s (%s)", TOPO_PGROUP_FACILITY,
		    TOPO_LED_MODE, topo_strerror(err));
		ret = -1;
	}
	if (chkmode != newmode) {
		return (-1);
	}

	if (topo_prop_set_uint32(node, TOPO_PGROUP_FACILITY,
	    TOPO_LED_MODE, TOPO_PROP_MUTABLE, origmode, &err) != 0) {
		logmsg("failed to set prop %s/%s (%s)", TOPO_PGROUP_FACILITY,
		    TOPO_LED_MODE, topo_strerror(err));
		ret = -1;
	}
	if (topo_prop_get_uint32(node, TOPO_PGROUP_FACILITY,
	    TOPO_LED_MODE, &chkmode, &err) != 0) {
		logmsg("failed to get prop %s/%s (%s)", TOPO_PGROUP_FACILITY,
		    TOPO_LED_MODE, topo_strerror(err));
		ret = -1;
	}
	if (chkmode != origmode) {
		return (-1);
	}

	return (ret);
}

static int
faccb(topo_hdl_t *thp, tnode_t *node, void *arg)
{
	nvlist_t *rsrc = NULL;
	topo_faclist_t faclist, *lp;
	int present, err;
	boolean_t is_occupied = B_FALSE;
	struct factest_cb *cbarg = (struct factest_cb *)arg;

	if (topo_node_flags(node) == TOPO_NODE_FACILITY)
		return (TOPO_WALK_NEXT);

	if (topo_node_resource(node, &rsrc, &err) < 0 ||
	    (present = topo_fmri_present(thp, rsrc, &err)) < 0) {
		nvlist_free(rsrc);
		return (TOPO_WALK_ERR);
	}
	nvlist_free(rsrc);

	if (present == 1 &&
	    topo_node_facility(thp, node, TOPO_FAC_TYPE_SENSOR,
	    TOPO_FAC_TYPE_ANY, &faclist, &err) == 0) {
		for (lp = topo_list_next(&faclist.tf_list); lp != NULL;
		    lp = topo_list_next(lp)) {
			if (run_sensor_tests(thp, lp->tf_node) != 0)
				cbarg->ftcb_gotfails = B_TRUE;
		}
	}

	/*
	 * It's common for drive bay indicators to not be controllable when the
	 * bay is empty.  So check if the bay is occupied and skip running
	 * the indicator tests if it is not.
	 */
	if (strcmp(BAY, topo_node_name(node)) == 0 &&
	    topo_node_occupied(node, &is_occupied) == 0 &&
	    !is_occupied) {
		return (TOPO_WALK_NEXT);
	}
	if (topo_node_facility(thp, node, TOPO_FAC_TYPE_INDICATOR,
	    TOPO_FAC_TYPE_ANY, &faclist, &err) == 0) {
		for (lp = topo_list_next(&faclist.tf_list); lp != NULL;
		    lp = topo_list_next(lp)) {
			if (run_indicator_tests(thp, lp->tf_node) != 0)
				cbarg->ftcb_gotfails = B_TRUE;
		}
	}
	return (TOPO_WALK_NEXT);
}

int
main(int argc, char *argv[])
{
	topo_hdl_t *thp;
	topo_walk_t *twp;
	char c, *root = "/";
	int err, status = 1;
	boolean_t debug_on = B_FALSE;
	struct factest_cb cbarg = { 0 };

	pname = argv[0];

	while (optind < argc) {
		while ((c = getopt(argc, argv, optstr)) != -1) {
			switch (c) {
			case 'd':
				debug_on = B_TRUE;
				break;
			case 'R':
				root = optarg;
				break;
			default:
				usage();
				return (2);
			}
		}
	}

	if ((thp = topo_open(TOPO_VERSION, root, &err)) == NULL) {
		logmsg("failed to get topo handle: %s", topo_strerror(err));
		goto out;
	}

	if (debug_on) {
		topo_debug_set(thp, "module", "stderr");
		(void) putenv("TOPOFACIPMIDEBUG=1");
		(void) putenv("TOPOFACAHCIDEBUG=1");
		(void) putenv("TOPOFACMPTSASDEBUG=1");
		(void) putenv("TOPOSESDEBUG=1");
	}

	logmsg("Taking topo snapshot");
	if (topo_snap_hold(thp, NULL, &err) == NULL) {
		logmsg("failed to take topo snapshot: %s", topo_strerror(err));
		goto out;
	}
	if ((twp = topo_walk_init(thp, "hc", faccb, &cbarg, &err)) ==
	    NULL) {
		logmsg("failed to init topo walker: %s", topo_strerror(err));
		goto out;
	}
	if (topo_walk_step(twp, TOPO_WALK_CHILD) == TOPO_WALK_ERR) {
		logmsg("failed to walk topology");
		topo_walk_fini(twp);
		goto out;
	}
	topo_walk_fini(twp);

	status = (cbarg.ftcb_gotfails ? 1 : 0);
out:
	if (thp != NULL)  {
		topo_snap_release(thp);
		topo_close(thp);
	}
	return (status);
}
