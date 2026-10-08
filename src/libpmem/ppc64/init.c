/*
 * Copyright 2019, IBM Corporation
 * Copyright 2019-2020, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ppc64/init.c -- libpmem ppc64 init, backported from PMDK 1.9 libpmem2/ppc64
 */

#include <stdlib.h>
#include <string.h>
#include "libpmem.h"

#include "os.h"
#include "out.h"
#include "pmem.h"
#include "util.h"
#include "valgrind_internal.h"

/*
 * Raw encodings so older assemblers accept the POWER ISA 3.1 forms: phwsync
 * (sync L=4) and dcbstps (dcbf L=6) execute as hwsync and dcbf on earlier CPUs.
 */
#define __SYNC(l) ".long (0x7c0004AC | ((" #l ") << 21))"
#define __DCBF(ra, rb, l) ".long (0x7c0000AC | ((" #l ") << 21)"	\
	" | ((" #ra ") << 16) | ((" #rb ") << 11))"

/*
 * predrain_fence_sync -- (internal) heavyweight sync, orders prior dcbf
 */
static void
predrain_fence_sync(void)
{
	LOG(15, NULL);

	VALGRIND_DO_FENCE;
	asm volatile(__SYNC(4) : : : "memory");
}

/*
 * flush_dcbf -- (internal) flush the CPU cache, one dcbf per cache line
 */
static void
flush_dcbf(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	uintptr_t uptr = (uintptr_t)addr & ~(CACHELINE_SIZE - 1);
	uintptr_t end = (uintptr_t)addr + len;

	for (; uptr < end; uptr += CACHELINE_SIZE) {
		asm volatile(__DCBF(0, %0, 6) : : "r"(uptr) : "memory");
	}
}

/*
 * flush_empty -- (internal) do not flush the CPU cache
 */
static void
flush_empty(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	flush_empty_nolog(addr, len);
}

/*
 * pmem_init_funcs -- initialize architecture-specific list of pmem operations
 */
void
pmem_init_funcs(struct pmem_funcs *funcs)
{
	LOG(3, "libpmem: PPC64 support");

	funcs->predrain_fence = predrain_fence_sync;
	funcs->deep_flush = flush_dcbf;
	funcs->is_pmem = is_pmem_detect;
	funcs->memmove_nodrain = memmove_nodrain_generic;
	funcs->memset_nodrain = memset_nodrain_generic;

	char *e = os_getenv("PMEM_NO_FLUSH");
	if (e && (strcmp(e, "1") == 0)) {
		LOG(3, "Forced not flushing CPU_cache");
		funcs->flush = flush_empty;
	} else {
		funcs->flush = funcs->deep_flush;
	}
}
