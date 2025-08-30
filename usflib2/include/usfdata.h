#ifndef USFDATA_H
#define USFDATA_H

#include <stdint.h>
#define USFNULL ((usf_data) { .u = 0 })
#define USFTRUE ((usf_data) { .u = 1 })
#define USFDATAP(d) ((usf_data) { .p = (d) })
#define USFDATAU(d) ((usf_data) { .u = (d) })
#define USFDATAI(d) ((usf_data) { .i = (d) })
#define USFDATAD(d) ((usf_data) { .d = (d) })

typedef union usf_data {
	void *p;
	uint64_t u;
	int64_t i;
	double d;
} usf_data;

#endif
