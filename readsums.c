/*=                     -*- c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *
 * libhsync -- the library for network deltas
 * $Id$
 * 
 * Copyright (C) 1999, 2000, 2001 by Martin Pool <mbp@samba.org>
 * Copyright (C) 1999 by Andrew Tridgell <tridge@samba.org>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


/*
 * readsums.c -- Load signatures from a file into an ::hs_signature_t.
 */

#include <config.h>

#include <assert.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include <sys/types.h>
#include <limits.h>
#include <inttypes.h>
#include <stdlib.h>

#include "hsync.h"
#include "sumset.h"
#include "job.h"
#include "trace.h"
#include "netint.h"
#include "protocol.h"
#include "util.h"


static hs_result hs_loadsig_s_weak(hs_job_t *job);
static hs_result hs_loadsig_s_strong(hs_job_t *job);



/**
 * Add a just-read-in checksum pair to the signature block.
 */
static hs_result hs_loadsig_add_sum(hs_job_t *job, uint32_t weak,
                                    char const *strong)
{
    size_t              new_size;
    hs_signature_t      *sig = job->signature;
    hs_block_sig_t      *asignature;

    sig->count++;
    new_size = sig->count * sizeof(hs_block_sig_t);

    sig->block_sigs = realloc(sig->block_sigs, new_size);
    
    if (sig->block_sigs == NULL) {
        return HS_MEM_ERROR;
    }
    asignature = &(sig->block_sigs[sig->count - 1]);

    asignature->weak_sum = weak;
    asignature->i = sig->count;

    memcpy(asignature->strong_sum, strong, sig->strong_sum_len);

    return HS_RUNNING;
}


static hs_result hs_loadsig_s_weak(hs_job_t *job)
{
    int                 l;
    hs_result           result;

    if ((result = hs_suck_n32(job->stream, &l)) != HS_DONE)
        return result;

    job->weak_sig = l;

    job->statefn = hs_loadsig_s_strong;

    return HS_RUNNING;
}


static hs_result hs_loadsig_s_strong(hs_job_t *job)
{
    return HS_UNIMPLEMENTED;
}


static hs_result hs_loadsig_s_stronglen(hs_job_t *job)
{
    int                 l;
    hs_result           result;

    if ((result = hs_suck_n32(job->stream, &l)) != HS_DONE)
        return result;
    job->strong_sum_len = l;
    
    if (l < 0  ||  l > HS_MD4_LENGTH) {
        hs_error("strong sum length %d is implausible", l);
        return HS_CORRUPT;
    }

    job->signature = hs_alloc_struct(hs_signature_t);
    job->signature->block_len = job->block_len;
    
    hs_trace("allocated sigset_t (strong_sum_len=%d, block_len=%d)",
             job->strong_sum_len, job->block_len);

    job->statefn = hs_loadsig_s_weak;
    
    return HS_RUNNING;
}


static hs_result hs_loadsig_s_blocklen(hs_job_t *job)
{
    int                 l;
    hs_result           result;

    if ((result = hs_suck_n32(job->stream, &l)) != HS_DONE)
        return result;
    job->block_len = l;

    if (job->block_len < 1) {
        hs_error("block length of %d is bogus", job->block_len);
        return HS_CORRUPT;
    }

    job->statefn = hs_loadsig_s_stronglen;
    return HS_RUNNING;
}


static hs_result hs_loadsig_s_magic(hs_job_t *job)
{
    int                 l;
    hs_result           result;

    if ((result = hs_suck_n32(job->stream, &l)) != HS_DONE) {
        return result;
    } else if (l != HS_SIG_MAGIC) {
        hs_error("wrong magic number %#10x for signature", l);
        return HS_BAD_MAGIC;
    } else {
        hs_trace("got signature magic %#10x", l);
    }

    job->statefn = hs_loadsig_s_blocklen;

    return HS_RUNNING;
}


/**
 * \brief Read a signature from a file into an ::hs_signature_t structure
 * in memory.
 *
 * Once there, it can be used to generate a delta to a newer version of
 * the file.
 *
 * \note After loading the signatures, you must call
 * hs_build_hash_table() before you can use them.
 */
hs_job_t *hs_loadsig_begin(hs_stream_t *stream, hs_signature_t **sumset)
{
    hs_job_t *job;

    job = hs_job_new(stream, "loadsig");
    job->statefn = hs_loadsig_s_magic;
        
    return job;
}

