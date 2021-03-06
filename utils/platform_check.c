//----------------------------------------------------------------------------
// ZetaScale
// Copyright (c) 2016, SanDisk Corp. and/or all its affiliates.
//
// This program is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License version 2.1 as published by the Free
// Software Foundation;
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License v2.1 for more details.
//
// A copy of the GNU Lesser General Public License v2.1 is provided with this package and
// can also be found at: http://opensource.org/licenses/LGPL-2.1
// You should have received a copy of the GNU Lesser General Public License along with
// this program; if not, write to the Free Software Foundation, Inc., 59 Temple
// Place, Suite 330, Boston, MA 02111-1307 USA.
//----------------------------------------------------------------------------

/*
 * File:   platform_check.c
 * Author: darryl
 *
 * Created on  February 17, 2010, 10:35 AM
 *
 * Copyright (c) 2010 Schooner Information Technology, Inc.
 * http://www.schoonerinfotech.com/
 */


/* <PREFACE>
 * Check for a valid hardware platform.
 * Intended for use by applications that must only run on
 * authorized hardware.
 *
 * Returns 1 if ok to run, 0 if not ok.
 */
#include <stdio.h>
#include <string.h>
#include "platform_check.h"

#undef PLATFORM_CHECK
#ifdef  PLATFORM_CHECK

#define DEBUG             0
#define VM_ID1            "Product Name: V"
#define VM_ID2            "domU"
#define APPLIANCE_ID      "Schooner Appliance"
#define MAXOUT             256

static char *find_vm = "/usr/sbin/dmidecode show | grep \"Product Name:\"";
static char *find_hw = "/usr/sbin/dmidecode show | grep \"Schooner Appliance\"";

static char product_id[MAXOUT];

/*
 * Fill in the global product id variable with the
 * output from dmidecode | grep "Product Name:".
 *
 * Return 1 on success, 0 on failure.
 */

static int get_product_id(char *cmd) {

    FILE *fp = NULL;

    memset(product_id, '\0', MAXOUT);

    fp = popen(cmd, "r");

    if (fp == NULL) {

	/* Probably don't have dmidecode - not Schooner VM */
	if (DEBUG) fprintf(stderr, "FAILED: cannot access dmidecode\n");
	return 0;

    }

    if ((fgets(product_id, MAXOUT, fp) == NULL)) {

	/* Could not read product ID */
	if (DEBUG) fprintf(stderr, "FAILED: cannot read product ID\n");
	pclose(fp);
	return 0;
    }

    /* Got the product ID */
    pclose(fp);
    return 1;
}


/*
 * Check for a Schooner VM.
 *
 * Return 1 if Schooner VM.
 *
 * Return 0 if not Schooner VM.
 *
 */
static int is_schooner_vm() {

    if (strstr(product_id, VM_ID1) ||
        strstr(product_id, VM_ID2)) {

	/* Good VM */
	if (DEBUG) fprintf(stderr, "is_schooner_vm: is VM\n");
	return 1;

    } else {

	/* Not a good VM or a VM at all */
	if (DEBUG) fprintf(stderr, "is_schooner_vm: is not VM\n");
	return 0;

    }
}

/*
 * Check for a Schooner HW platform.
 *
 * Return 1 if Schooner HW.
 *
 * Return 0 if not Schooner HW.
 *
 */
static int is_schooner_hw() {

    if (strstr(product_id, APPLIANCE_ID)) {

	/* Valid HW */
	if (DEBUG) fprintf(stderr, "is_schooner_hw: good HW - %s\n", product_id);
	return 1;

    } else {

	/* Invalid HW */
	if (DEBUG) fprintf(stderr, "is_schooner_hw: bad HW - %s\n", product_id);
	return 0;
    }
}

int is_valid_schooner_platform() {

    if ((get_product_id(find_vm) && is_schooner_vm()) ||
	(get_product_id(find_hw) && is_schooner_hw())) {

	return 1;

    } else {

	return 0;

    }
}
#else
int is_valid_schooner_platform() {

    return 1;

}
#endif
