/*
 * cossingen.c
 *
 *  Created on: 2017Äê2ÔÂ16ÈÕ
 *      Author: Eric
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#define FQ 1024
#define AM 8191
#define PI 3.141592

void cossingen(short *buf, int freq, uint8_t ch)
{
	int i = 0, j = 0;
	double value = 0, a = 0, b = 0;
	value = (2 * PI / 32);
	for (i = 0; i < freq; i++) {
		a = AM * sin(value * i);
		b = AM * cos(value * i);
		buf[j] = (short)b<<2;
		buf[j+1] = (short)a<<2;
		if(ch == 2){
			buf[j+2] = (short)b<<2;
			buf[j+3] = (short)a<<2;
			j += 2;
		}
		j += 2;
	}
#if 0
	FILE *tmp;
	uint32_t n;

	tmp = fopen("yunsdr_tx_data.bin", "wb+");
	if(tmp == NULL) {
		fprintf(stderr, "Open yunsdr_tx_data.bin failed\n");
	} else {
		n = fwrite(buf, sizeof(short), freq*(ch == 2?4:2), tmp);
		if(n < 0) {
			perror("fwrite");
		}
		fclose(tmp);
	}
#endif
}

int test()
{
	FILE *fp;
	int ch = 2;

	short *buf = malloc(FQ*2*ch*sizeof(short));
	fp = fopen("sine_wave.raw", "wb+");
	cossingen(buf, FQ, 2);
	fwrite(buf, sizeof(short), FQ*2*ch, fp);
	fclose(fp);
	free(buf);
	return 0;
}
