#include "memcached.h"
#include "3q_compressor.h"
#include "slabs.h"

#include <stdlib.h>
#include <string.h>
#include <execinfo.h>
#include <zstd.h>
#include <lz4.h>
#include <snappy-c.h>
#include <time.h>

#define MILLION  1000000.0 // for time from ns to ms
#define NVAL 100

struct compression_resources {
	char* buffer;
	size_t buffer_size;

	// Compression & Decompression context (for ZSTD only)
	ZSTD_CCtx* cctx;
	ZSTD_DCtx* dctx;
};

static struct compression_resources* _rcs;
static int _num_threads;

static struct timespec _start, _end;
static int _pos;
static double _times[NVAL];
static size_t _sizes[NVAL];

void compression_resources_init(void)
{
	// +1 for the LRU maintainer thread
	_num_threads = settings.num_threads + 1;
	_rcs = calloc(_num_threads, sizeof(struct compression_resources));
	if (_rcs == NULL) {
		fprintf(stderr, "ERROR: unable to allocate memory for compression ressources\n");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < _num_threads; ++i) {
		switch (settings.comp_algo) {
		case COMPRESSION_ZSTD:
			_rcs[i].buffer_size = ZSTD_compressBound(settings.item_size_max);
			_rcs[i].cctx = ZSTD_createCCtx();
			_rcs[i].dctx = ZSTD_createDCtx();
			break;
		
		case COMPRESSION_LZ4:
			_rcs[i].buffer_size = LZ4_compressBound(settings.item_size_max);
			break;

		case COMPRESSION_SNAPPY:
			_rcs[i].buffer_size = snappy_max_compressed_length(settings.item_size_max);
			break;
		}
		_rcs[i].buffer = malloc(_rcs[i].buffer_size * sizeof(char));
	}
}

void compression_resources_cleanup(void)
{
	for (int i = 0; i < _num_threads; ++i) {
		if (settings.comp_algo == COMPRESSION_ZSTD) {
			ZSTD_freeCCtx(_rcs[i].cctx);
			ZSTD_freeDCtx(_rcs[i].dctx);
		}
	}
}

bool do_compress_item(item** ptr)
{
	item* it = *ptr;
	assert(ITEM_lruid(it) == WARM_LRU);

	int tid = (get_thread_base_id() >= 0) ? get_thread_base_id() : _num_threads - 1;
	struct compression_resources rc = _rcs[tid];
	size_t compressed_size = 0;
	size_t old_ntotal = ITEM_ntotal(it);
	size_t new_ntotal = 0;

	switch (settings.comp_algo) {
	case COMPRESSION_ZSTD:
		compressed_size = 
			ZSTD_compressCCtx(rc.cctx, rc.buffer, rc.buffer_size, ITEM_data(it), it->nbytes, 1);
		if (ZSTD_isError(compressed_size)) {
			fprintf(stderr, "[ERROR] ZSTD compression failed : %s\n", 
					  ZSTD_getErrorName(compressed_size));
			return false;
		}
		break;

	case COMPRESSION_LZ4:
		{
			int comp_sz = LZ4_compress_default(ITEM_data(it), rc.buffer, it->nbytes, rc.buffer_size);
			if (comp_sz <= 0) {
				fprintf(stderr, "[ERROR] LZ4 compression failed\n");
				return false;
			}
			compressed_size = comp_sz;
		}
		break;

	case COMPRESSION_SNAPPY:
		{
			compressed_size = rc.buffer_size;
			snappy_status st = 
				snappy_compress(ITEM_data(it), it->nbytes, rc.buffer, &compressed_size);
			if (st != SNAPPY_OK) {
				fprintf(stderr, "[ERROR] SNAPPY compression failed\n");
				return false;
			}
		}
		break;
	}

	new_ntotal = old_ntotal - (it->nbytes - compressed_size);
	// If the compression ratio is not high enough, the compression is dropped
	if (((double)it->nbytes / compressed_size) < settings.compression_ratio_min) {
		return false;
	}

	if (settings.verbose > 0) {
		fprintf(stderr, "[DEBUG] item compressed with compression ratio = %f\n", (double)it->nbytes / compressed_size);
	}

	if (!change_item_slabs_cls(ptr, old_ntotal, new_ntotal)) {
		return false;
	}
	
	memcpy(ITEM_data(*ptr), rc.buffer, compressed_size);
	return true;
}

bool do_decompress_item(item** ptr)
{
	item* it = *ptr;
	assert(ITEM_lruid(it) == COLD_LRU);

	int tid = (get_thread_base_id() >= 0) ? get_thread_base_id() : _num_threads - 1;
	struct compression_resources rc = _rcs[tid];
	size_t decompressed_size = 0;
	size_t old_ntotal = ITEM_ntotal(it);
	size_t new_ntotal = 0;

	clock_gettime(CLOCK_MONOTONIC, &_start);

	switch (settings.comp_algo) {
	case COMPRESSION_ZSTD:
		decompressed_size =
			ZSTD_decompressDCtx(rc.dctx, rc.buffer, rc.buffer_size, ITEM_data(it), it->nbytes);
		if (ZSTD_isError(decompressed_size)) {
			fprintf(stderr, "[ERROR] ZSTD decompression failed : %s \n",
					  ZSTD_getErrorName(decompressed_size));
			return false;
		}
		break;

	case COMPRESSION_LZ4:
		{
			int decomp_sz = LZ4_decompress_safe(ITEM_data(it), rc.buffer, it->nbytes, rc.buffer_size);
			if (decomp_sz <= 0) {
				fprintf(stderr, "[ERROR] LZ4 decompression failed\n");
				return false;
			}
			decompressed_size = decomp_sz;
		}
		break;

	case COMPRESSION_SNAPPY:
		{
			decompressed_size = rc.buffer_size;
			snappy_status st = 
				snappy_uncompress(ITEM_data(it), it->nbytes, rc.buffer, &decompressed_size);
			if (st != SNAPPY_OK) {
				fprintf(stderr, "[ERROR] SNAPPY decompression failed\n");
				return false;
			}
		}
		break;
	}

	clock_gettime(CLOCK_MONOTONIC, &_end);
	_times[_pos] = (_end.tv_nsec - _start.tv_nsec) / MILLION;

	new_ntotal = old_ntotal + (decompressed_size - it->nbytes);
	assert(it->nbytes < decompressed_size);

	if (!change_item_slabs_cls(ptr, old_ntotal, new_ntotal)) {
		return false;
	}

	clock_gettime(CLOCK_MONOTONIC, &_start);

	memcpy(ITEM_data(*ptr), rc.buffer, decompressed_size);

	clock_gettime(CLOCK_MONOTONIC, &_end);
	_times[_pos] += (_end.tv_nsec - _start.tv_nsec) / MILLION;
	_sizes[_pos] = decompressed_size;
	_pos = (_pos + 1) % NVAL;
	
	return true;
}

double get_decompression_bw(void)
{
	double ttime = 0;
	size_t tsize = 0;
	for (int i = 0; i < NVAL; ++i) {
		ttime += _times[i];
		tsize += _sizes[i];
	}

	if (ttime == 0) return 214748364; // if ttime is zero, we set the bandwidth to 200 GB/s
	
	return tsize / ttime;
}

size_t get_average_size()
{
	size_t tsize = 0;
	short n;
	for (n = 0; n < NVAL && _sizes[n] > 0; ++n) {
		tsize += _sizes[n];
	}
	return (n > 0) ? (tsize / n) : 0;
}