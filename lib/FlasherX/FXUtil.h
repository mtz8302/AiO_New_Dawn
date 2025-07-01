//******************************************************************************
// FXUTIL.H -- FlasherX utility functions
//******************************************************************************
#ifndef FXUTIL_H_
#define FXUTIL_H_

#include <Stream.h>
#include "CRCStream.h"

void read_ascii_line( Stream *serial, char *line, int maxbytes );
void update_firmware( Stream *in, Stream *out,
			uint32_t buffer_addr, uint32_t buffer_size );
int parse_hex_line( const char *theline, char *bytes,
		unsigned int *addr, unsigned int *num, unsigned int *code );

#endif
