/* C:B**************************************************************************
This software is Copyright 2014 Michael Romeo <r0m30@r0m30.com>

This file is part of msed.

msed is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

msed is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with msed.  If not, see <http://www.gnu.org/licenses/>.

* C:E********************************************************************** */
#include "os.h"
#include <stdio.h>
#include <ctype.h>
void MsedHexDump(void * address, int length) {
	uint8_t display[17];
	uint8_t * cpos = (uint8_t *)address;
	uint8_t * epos = cpos + length;
	LOG(D4) << "Entering hexDump";
	int rpos = 0;
	int dpos = 0;
	printf("%04x ",rpos);
	while (cpos < epos){
		printf("%02x", cpos[0]);
		if (!((++rpos) % 4)) printf(" ");
		display[dpos++] = (isprint(cpos[0]) ? cpos[0] : 0x2e );
		cpos += 1;
		if (16 == dpos) {
			dpos = 0;
			display[16] = 0x00;
			printf(" %s \n", display);
			if(cpos < epos) printf("%04x ", rpos);
			memset(&display,0,sizeof(display));
		}
	}
	if (dpos != 0) {
		if (dpos % 4) printf(" ");
			printf("  ");
		for (int i = dpos ; i < 15; i++) {
			if (!(i % 4)) printf(" ");
			printf("  ");
		}
		display[dpos] = 0x00;
		printf(" %s\n", display);
	}
}