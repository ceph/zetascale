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
 * File:   protocol/init_protocol.h
 * Author: Darpan Dinker
 *
 * Created on March 28, 2008, 3:08 PM
 *
 * (c) Copyright 2008, Schooner Information Technology, Inc.
 * http://www.schoonerinfotech.com/
 *
 * $Id: init_protocol.h 843 2008-04-01 17:30:08Z darpan $
 */

#ifndef _INIT_PROTOCOL_H
#define	_INIT_PROTOCOL_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "common/sdftypes.h"
#include "utils/hashmap.h"

/**
 * @brief Create and initialize resource needed by SDF protocol engine
 *
 * @param rank id of the daemon process
 * @return SDF_TRUE on success
 */
SDF_boolean_t sdf_protocol_initialize(uint32_t rank, SDF_boolean_t usingFth);

/**
 * @brief Free up resources held by SDF protocol, e.g. HashMap
 * @return SDF_TRUE on success
 */
SDF_boolean_t sdf_protocol_reset();

/**
 * @brief HashMap storing the queue pairs for various purposes, available after
 * a successful init.
 *
 * @see sdf_protocol_initialize
 * @see HashMap
 */
HashMap getGlobalQueueMap();

/**
 * Get my ID as provided by MPI or future cluster manager
 */
uint32_t getRank();

#ifdef	__cplusplus
}
#endif

#endif	/* _INIT_PROTOCOL_H */

