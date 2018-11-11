#include <generated/csr.h>
#ifdef CSR_SDRAM_BASE

#include <stdio.h>
#include <stdlib.h>

#include <generated/sdram_phy.h>
#include <generated/mem.h>
#include <hw/flags.h>
#include <system.h>

#include "sdram.h"

static void cdelay(int i)
{
	while(i > 0) {
#if defined (__lm32__)
		__asm__ volatile("nop");
#elif defined (__or1k__)
		__asm__ volatile("l.nop");
#elif defined (__picorv32__)
		__asm__ volatile("nop");
#elif defined (__vexriscv__)
		__asm__ volatile("nop");
#elif defined (__minerva__)
		__asm__ volatile("nop");
#else
#error Unsupported architecture
#endif
		i--;
	}
}

void sdrsw(void)
{
	sdram_dfii_control_write(DFII_CONTROL_CKE|DFII_CONTROL_ODT|DFII_CONTROL_RESET_N);
	printf("SDRAM now under software control\n");
}

void sdrhw(void)
{
	sdram_dfii_control_write(DFII_CONTROL_SEL);
	printf("SDRAM now under hardware control\n");
}

void sdrrow(char *_row)
{
	char *c;
	unsigned int row;

	if(*_row == 0) {
		sdram_dfii_pi0_address_write(0x0000);
		sdram_dfii_pi0_baddress_write(0);
		command_p0(DFII_COMMAND_RAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
		cdelay(15);
		printf("Precharged\n");
	} else {
		row = strtoul(_row, &c, 0);
		if(*c != 0) {
			printf("incorrect row\n");
			return;
		}
		sdram_dfii_pi0_address_write(row);
		sdram_dfii_pi0_baddress_write(0);
		command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CS);
		cdelay(15);
		printf("Activated row %d\n", row);
	}
}

void sdrrdbuf(int dq)
{
	int i, p;
	int first_byte, step;

	if(dq < 0) {
		first_byte = 0;
		step = 1;
	} else {
		first_byte = DFII_PIX_DATA_SIZE/2 - 1 - dq;
		step = DFII_PIX_DATA_SIZE/2;
	}

	for(p=0;p<DFII_NPHASES;p++)
		for(i=first_byte;i<DFII_PIX_DATA_SIZE;i+=step)
			printf("%02x", MMPTR(sdram_dfii_pix_rddata_addr[p]+4*i));
	printf("\n");
}

void sdrrd(char *startaddr, char *dq)
{
	char *c;
	unsigned int addr;
	int _dq;

	if(*startaddr == 0) {
		printf("sdrrd <address>\n");
		return;
	}
	addr = strtoul(startaddr, &c, 0);
	if(*c != 0) {
		printf("incorrect address\n");
		return;
	}
	if(*dq == 0)
		_dq = -1;
	else {
		_dq = strtoul(dq, &c, 0);
		if(*c != 0) {
			printf("incorrect DQ\n");
			return;
		}
	}

	sdram_dfii_pird_address_write(addr);
	sdram_dfii_pird_baddress_write(0);
	command_prd(DFII_COMMAND_CAS|DFII_COMMAND_CS|DFII_COMMAND_RDDATA);
	cdelay(15);
	sdrrdbuf(_dq);
}

void sdrrderr(char *count)
{
	int addr;
	char *c;
	int _count;
	int i, j, p;
	unsigned char prev_data[DFII_NPHASES*DFII_PIX_DATA_SIZE];
	unsigned char errs[DFII_NPHASES*DFII_PIX_DATA_SIZE];

	if(*count == 0) {
		printf("sdrrderr <count>\n");
		return;
	}
	_count = strtoul(count, &c, 0);
	if(*c != 0) {
		printf("incorrect count\n");
		return;
	}

	for(i=0;i<DFII_NPHASES*DFII_PIX_DATA_SIZE;i++)
			errs[i] = 0;
	for(addr=0;addr<16;addr++) {
		sdram_dfii_pird_address_write(addr*8);
		sdram_dfii_pird_baddress_write(0);
		command_prd(DFII_COMMAND_CAS|DFII_COMMAND_CS|DFII_COMMAND_RDDATA);
		cdelay(15);
		for(p=0;p<DFII_NPHASES;p++)
			for(i=0;i<DFII_PIX_DATA_SIZE;i++)
				prev_data[p*DFII_PIX_DATA_SIZE+i] = MMPTR(sdram_dfii_pix_rddata_addr[p]+4*i);

		for(j=0;j<_count;j++) {
			command_prd(DFII_COMMAND_CAS|DFII_COMMAND_CS|DFII_COMMAND_RDDATA);
			cdelay(15);
			for(p=0;p<DFII_NPHASES;p++)
				for(i=0;i<DFII_PIX_DATA_SIZE;i++) {
					unsigned char new_data;

					new_data = MMPTR(sdram_dfii_pix_rddata_addr[p]+4*i);
					errs[p*DFII_PIX_DATA_SIZE+i] |= prev_data[p*DFII_PIX_DATA_SIZE+i] ^ new_data;
					prev_data[p*DFII_PIX_DATA_SIZE+i] = new_data;
				}
		}
	}

	for(i=0;i<DFII_NPHASES*DFII_PIX_DATA_SIZE;i++)
		printf("%02x", errs[i]);
	printf("\n");
	for(p=0;p<DFII_NPHASES;p++)
		for(i=0;i<DFII_PIX_DATA_SIZE;i++)
			printf("%2x", DFII_PIX_DATA_SIZE/2 - 1 - (i % (DFII_PIX_DATA_SIZE/2)));
	printf("\n");
}

void sdrwr(char *startaddr)
{
	char *c;
	unsigned int addr;
	int i;
	int p;

	if(*startaddr == 0) {
		printf("sdrrd <address>\n");
		return;
	}
	addr = strtoul(startaddr, &c, 0);
	if(*c != 0) {
		printf("incorrect address\n");
		return;
	}

	for(p=0;p<DFII_NPHASES;p++)
		for(i=0;i<DFII_PIX_DATA_SIZE;i++)
			MMPTR(sdram_dfii_pix_wrdata_addr[p]+4*i) = 0x10*p + i;

	sdram_dfii_piwr_address_write(addr);
	sdram_dfii_piwr_baddress_write(0);
	command_pwr(DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS|DFII_COMMAND_WRDATA);
}

#ifdef CSR_DDRPHY_BASE

#ifdef KUSDDRPHY
#define ERR_DDRPHY_DELAY 512
#else
#define ERR_DDRPHY_DELAY 32
#endif
#define ERR_DDRPHY_BITSLIP 8

#ifdef CSR_DDRPHY_WLEVEL_EN_ADDR

void sdrwlon(void)
{
	printf("Qoff enabled\n");
        sdram_dfii_pi0_address_write(DDR3_MR1 | (1 << 7) | (1 << 12));  // add 1 << 12 to flip Qoff
	sdram_dfii_pi0_baddress_write(1);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
	ddrphy_wlevel_en_write(1);
}

void sdrwloff(void)
{
	sdram_dfii_pi0_address_write(DDR3_MR1);
	sdram_dfii_pi0_baddress_write(1);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
	ddrphy_wlevel_en_write(0);
}

int write_level(void)
{
	int i, j;

	int dq_address;
	unsigned char dq;

	int err_ddrphy_wdly;

	unsigned char taps_scan[ERR_DDRPHY_DELAY];

	int one_window_active;
	int one_window_start;
	int one_window_len;
	int best_one_window_len;

	int delays[DFII_PIX_DATA_SIZE/2];

    int ok;

	err_ddrphy_wdly = ERR_DDRPHY_DELAY - ddrphy_half_sys8x_taps_read();

	printf("Write leveling scan:\n");

	sdrwlon();
	cdelay(100);
	for(i=0;i<DFII_PIX_DATA_SIZE/2;i++) {
		printf("m%d: ", i);
	    dq_address = sdram_dfii_pix_rddata_addr[0]+4*(DFII_PIX_DATA_SIZE/2-1-i);

	    /* reset delay */
		ddrphy_dly_sel_write(1 << i);
		ddrphy_wdly_dq_rst_write(1);
		ddrphy_wdly_dqs_rst_write(1);
#ifdef KUSDDRPHY /* need to init manually on Ultrascale */
		for(j=0; j<ddrphy_wdly_dqs_taps_read(); j++)
			ddrphy_wdly_dqs_inc_write(1);
#endif
		/* scan taps */
		for(j=0;j<err_ddrphy_wdly;j++) {
			ddrphy_wlevel_strobe_write(1);
			cdelay(10);
			dq = MMPTR(dq_address);
			printf("%d", dq != 0);
			taps_scan[j] = (dq != 0);
			ddrphy_wdly_dq_inc_write(1);
			ddrphy_wdly_dqs_inc_write(1);
			cdelay(10);
		}
		printf("\n");

		/* find best delay */
		one_window_active = 0;
		one_window_start = 0;
		one_window_len = 0;
		best_one_window_len = 0;
		delays[i] = -1;
		for(j=0;j<err_ddrphy_wdly;j++) {
			if (one_window_active) {
				if ((taps_scan[j] == 0) || (j == err_ddrphy_wdly-1)) {
					one_window_len = j - one_window_start;
					if (one_window_len > best_one_window_len) {
						delays[i] = one_window_start;
						best_one_window_len = one_window_len;
					}
					one_window_active = 0;
				}
			} else {
				if (taps_scan[j]) {
					one_window_active = 1;
					one_window_start = j;
				}
			}
		}

		/* configure delays */
		ddrphy_wdly_dq_rst_write(1);
		ddrphy_wdly_dqs_rst_write(1);
#ifdef KUSDDRPHY /* need to init manually on Ultrascale */
		for(j=0; j<ddrphy_wdly_dqs_taps_read(); j++)
			ddrphy_wdly_dqs_inc_write(1);
#endif
		for(j=0; j<delays[i]; j++) {
			ddrphy_wdly_dq_inc_write(1);
			ddrphy_wdly_dqs_inc_write(1);
		}
	}

	sdrwloff();

	ok = 1;
	printf("Write leveling: ");
	for(i=DFII_PIX_DATA_SIZE/2-1;i>=0;i--) {
		printf("%2d ", delays[i]);
		if(delays[i] < 0)
			ok = 0;
	}

	if(ok)
		printf("completed\n");
	else
		printf("failed\n");

	return ok;
}

#endif /* CSR_DDRPHY_WLEVEL_EN_ADDR */

static void read_bitslip_inc(char m)
{
		ddrphy_dly_sel_write(1 << m);
#ifdef KUSDDRPHY
		ddrphy_rdly_dq_bitslip_write(1);
#else
		/* 7-series SERDES in DDR mode needs 3 pulses for 1 bitslip */
		ddrphy_rdly_dq_bitslip_write(1);
		ddrphy_rdly_dq_bitslip_write(1);
		ddrphy_rdly_dq_bitslip_write(1);
#endif
}

static int read_level_scan(int silent)
{
	unsigned int prv;
	unsigned char prs[DFII_NPHASES*DFII_PIX_DATA_SIZE];
	int p, i, j;
	int score;

	if (!silent)
		printf("Read delays scan:\n");

	/* Generate pseudo-random sequence */
	prv = 42;
	for(i=0;i<DFII_NPHASES*DFII_PIX_DATA_SIZE;i++) {
		prv = 1664525*prv + 1013904223;
		prs[i] = prv;
	}

	/* Activate */
	sdram_dfii_pi0_address_write(0);
	sdram_dfii_pi0_baddress_write(0);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CS);
	cdelay(15);

	/* Write test pattern */
	for(p=0;p<DFII_NPHASES;p++)
		for(i=0;i<DFII_PIX_DATA_SIZE;i++)
			MMPTR(sdram_dfii_pix_wrdata_addr[p]+4*i) = prs[DFII_PIX_DATA_SIZE*p+i];
	sdram_dfii_piwr_address_write(0);
	sdram_dfii_piwr_baddress_write(0);
	command_pwr(DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS|DFII_COMMAND_WRDATA);

	/* Calibrate each DQ in turn */
	sdram_dfii_pird_address_write(0);
	sdram_dfii_pird_baddress_write(0);
	score = 0;
	for(i=DFII_PIX_DATA_SIZE/2-1;i>=0;i--) {
		if (!silent)
			printf("m%d: ", (DFII_PIX_DATA_SIZE/2-i-1));
		ddrphy_dly_sel_write(1 << (DFII_PIX_DATA_SIZE/2-i-1));
		ddrphy_rdly_dq_rst_write(1);
		for(j=0; j<ERR_DDRPHY_DELAY;j++) {
			int working;
			command_prd(DFII_COMMAND_CAS|DFII_COMMAND_CS|DFII_COMMAND_RDDATA);
			cdelay(15);
			working = 1;
			for(p=0;p<DFII_NPHASES;p++) {
				if(MMPTR(sdram_dfii_pix_rddata_addr[p]+4*i) != prs[DFII_PIX_DATA_SIZE*p+i])
					working = 0;
				if(MMPTR(sdram_dfii_pix_rddata_addr[p]+4*(i+DFII_PIX_DATA_SIZE/2)) != prs[DFII_PIX_DATA_SIZE*p+i+DFII_PIX_DATA_SIZE/2])
					working = 0;
			}
			if (!silent)
				printf("%d", working);
			score += working;
			ddrphy_rdly_dq_inc_write(1);
		}
		if (!silent)
			printf("\n");
	}

	/* Precharge */
	sdram_dfii_pi0_address_write(0);
	sdram_dfii_pi0_baddress_write(0);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
	cdelay(15);

	return score;
}

static void read_level(void)
{
	unsigned int prv;
	unsigned char prs[DFII_NPHASES*DFII_PIX_DATA_SIZE];
	int p, i, j;
	int working;
	int delay, delay_min, delay_max;

	printf("Read delays: ");

	/* Generate pseudo-random sequence */
	prv = 42;
	for(i=0;i<DFII_NPHASES*DFII_PIX_DATA_SIZE;i++) {
		prv = 1664525*prv + 1013904223;
		prs[i] = prv;
	}

	/* Activate */
	sdram_dfii_pi0_address_write(0);
	sdram_dfii_pi0_baddress_write(0);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CS);
	cdelay(15);

	/* Write test pattern */
	for(p=0;p<DFII_NPHASES;p++)
		for(i=0;i<DFII_PIX_DATA_SIZE;i++)
			MMPTR(sdram_dfii_pix_wrdata_addr[p]+4*i) = prs[DFII_PIX_DATA_SIZE*p+i];
	sdram_dfii_piwr_address_write(0);
	sdram_dfii_piwr_baddress_write(0);
	command_pwr(DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS|DFII_COMMAND_WRDATA);

	/* Calibrate each DQ in turn */
	sdram_dfii_pird_address_write(0);
	sdram_dfii_pird_baddress_write(0);
	for(i=0;i<DFII_PIX_DATA_SIZE/2;i++) {
		ddrphy_dly_sel_write(1 << (DFII_PIX_DATA_SIZE/2-i-1));
		delay = 0;

		/* Find smallest working delay */
		ddrphy_rdly_dq_rst_write(1);
		while(1) {
			command_prd(DFII_COMMAND_CAS|DFII_COMMAND_CS|DFII_COMMAND_RDDATA);
			cdelay(15);
			working = 1;
			for(p=0;p<DFII_NPHASES;p++) {
				if(MMPTR(sdram_dfii_pix_rddata_addr[p]+4*i) != prs[DFII_PIX_DATA_SIZE*p+i])
					working = 0;
				if(MMPTR(sdram_dfii_pix_rddata_addr[p]+4*(i+DFII_PIX_DATA_SIZE/2)) != prs[DFII_PIX_DATA_SIZE*p+i+DFII_PIX_DATA_SIZE/2])
					working = 0;
			}
			if(working)
				break;
			delay++;
			if(delay >= ERR_DDRPHY_DELAY)
				break;
			ddrphy_rdly_dq_inc_write(1);
		}
		delay_min = delay;

		/* Get a bit further into the working zone */
#ifdef KUSDDRPHY
		for(j=0;j<16;j++) {
			delay += 1;
			ddrphy_rdly_dq_inc_write(1);
		}
#else
		delay++;
		ddrphy_rdly_dq_inc_write(1);
#endif

		/* Find largest working delay */
		while(1) {
			command_prd(DFII_COMMAND_CAS|DFII_COMMAND_CS|DFII_COMMAND_RDDATA);
			cdelay(15);
			working = 1;
			for(p=0;p<DFII_NPHASES;p++) {
				if(MMPTR(sdram_dfii_pix_rddata_addr[p]+4*i) != prs[DFII_PIX_DATA_SIZE*p+i])
					working = 0;
				if(MMPTR(sdram_dfii_pix_rddata_addr[p]+4*(i+DFII_PIX_DATA_SIZE/2)) != prs[DFII_PIX_DATA_SIZE*p+i+DFII_PIX_DATA_SIZE/2])
					working = 0;
			}
			if(!working)
				break;
			delay++;
			if(delay >= ERR_DDRPHY_DELAY)
				break;
			ddrphy_rdly_dq_inc_write(1);
		}
		delay_max = delay;

		printf("%d:%02d-%02d  ", DFII_PIX_DATA_SIZE/2-i-1, delay_min, delay_max);

		/* Set delay to the middle */
		printf("Set %d | ", (delay_min + delay_max) / 2); //(delay_min + delay_max) / 2);
		ddrphy_rdly_dq_rst_write(1);
		for(j=0;j< (delay_min+delay_max) /2;j++)
			ddrphy_rdly_dq_inc_write(1);
	}

	/* Precharge */
	sdram_dfii_pi0_address_write(0);
	sdram_dfii_pi0_baddress_write(0);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
	cdelay(15);

	printf("completed\n");
}
#endif /* CSR_DDRPHY_BASE */

static unsigned int seed_to_data_32(unsigned int seed, int random)
{
	if (random)
		return 1664525*seed + 1013904223;
	else
		return seed + 1;
}

static unsigned short seed_to_data_16(unsigned short seed, int random)
{
	if (random)
		return 25173*seed + 13849;
	else
		return seed + 1;
}

#ifdef TRY_RTT_COMBOS
#define ONEZERO 0xAAAAAAAA
#define ZEROONE 0x55555555

#ifndef MEMTEST_BUS_SIZE
#define MEMTEST_BUS_SIZE (512)
#endif

#define MEMTEST_BUS_DEBUG
#else
#define ONEZERO 0xAAAAAAAA
#define ZEROONE 0x55555555

#ifndef MEMTEST_BUS_SIZE
#define MEMTEST_BUS_SIZE (512)
#endif

//#define MEMTEST_BUS_DEBUG
#endif

static int memtest_bus(void)
{
	volatile unsigned int *array = (unsigned int *)MAIN_RAM_BASE;
	int i, errors;
	unsigned int rdata;

	errors = 0;

	for(i=0;i<MEMTEST_BUS_SIZE/4;i++) {
		array[i] = ONEZERO;
	}
	flush_cpu_dcache();
	flush_l2_cache();
	for(i=0;i<MEMTEST_BUS_SIZE/4;i++) {
		rdata = array[i];
		if(rdata != ONEZERO) {
			errors++;
#ifdef MEMTEST_BUS_DEBUG
			printf("[bus: %0x]: %08x vs %08x\n", i, rdata, ONEZERO);
#endif
		}
	}

	for(i=0;i<MEMTEST_BUS_SIZE/4;i++) {
		array[i] = ZEROONE;
	}
	flush_cpu_dcache();
	flush_l2_cache();
	for(i=0;i<MEMTEST_BUS_SIZE/4;i++) {
		rdata = array[i];
		if(rdata != ZEROONE) {
			errors++;
#ifdef MEMTEST_BUS_DEBUG
			printf("[bus %0x]: %08x vs %08x\n", i, rdata, ZEROONE);
#endif
		}
	}

	return errors;
}

#ifndef MEMTEST_DATA_SIZE
#define MEMTEST_DATA_SIZE (2*1024*1024)
#endif
#define MEMTEST_DATA_RANDOM 1

//#define MEMTEST_DATA_DEBUG

static int memtest_data(void)
{
	volatile unsigned int *array = (unsigned int *)MAIN_RAM_BASE;
	int i, errors;
	unsigned int seed_32;
	unsigned int rdata;

	errors = 0;
	seed_32 = 0;

	for(i=0;i<MEMTEST_DATA_SIZE/4;i++) {
		seed_32 = seed_to_data_32(seed_32, MEMTEST_DATA_RANDOM);
		array[i] = seed_32;
	}

	seed_32 = 0;
	flush_cpu_dcache();
	flush_l2_cache();
	for(i=0;i<MEMTEST_DATA_SIZE/4;i++) {
		seed_32 = seed_to_data_32(seed_32, MEMTEST_DATA_RANDOM);
		rdata = array[i];
		if(rdata != seed_32) {
			errors++;
#ifdef MEMTEST_DATA_DEBUG
			printf("[data %0x]: %08x vs %08x\n", i, rdata, seed_32);
#endif
		}
	}

	return errors;
}
#ifndef MEMTEST_ADDR_SIZE
#define MEMTEST_ADDR_SIZE (32*1024)
#endif
#define MEMTEST_ADDR_RANDOM 0

//#define MEMTEST_ADDR_DEBUG

static int memtest_addr(void)
{
	volatile unsigned int *array = (unsigned int *)MAIN_RAM_BASE;
	int i, errors;
	unsigned short seed_16;
	unsigned short rdata;

	errors = 0;
	seed_16 = 0;

	for(i=0;i<MEMTEST_ADDR_SIZE/4;i++) {
		seed_16 = seed_to_data_16(seed_16, MEMTEST_ADDR_RANDOM);
		array[(unsigned int) seed_16] = i;
	}

	seed_16 = 0;
	flush_cpu_dcache();
	flush_l2_cache();
	for(i=0;i<MEMTEST_ADDR_SIZE/4;i++) {
		seed_16 = seed_to_data_16(seed_16, MEMTEST_ADDR_RANDOM);
		rdata = array[(unsigned int) seed_16];
		if(rdata != i) {
			errors++;
#ifdef MEMTEST_ADDR_DEBUG
			printf("[addr %0x]: %08x vs %08x\n", i, rdata, i);
#endif
		}
	}

	return errors;
}

static int lfsr_state = 1;
static void lfsr_init(int seed) {
  lfsr_state = seed;
}

static unsigned int lfsr_next(void)
{
  /*
    config          : galois
    length          : 32
    taps            : (32, 25, 17, 7)
    shift-amount    : 16
    shift-direction : right
  */
  enum {
    length = 32,
    tap_0  = 32,
    tap_1  = 25,
    tap_2  = 17,
    tap_3  =  7
  };
  int v = lfsr_state;
  typedef unsigned int T;
  const T zero = (T)(0);
  const T lsb = zero + (T)(1);
  const T feedback = (
		      (lsb << (tap_0 - 1)) ^
		      (lsb << (tap_1 - 1)) ^
		      (lsb << (tap_2 - 1)) ^
		      (lsb << (tap_3 - 1))
		      );
  v = (v >> 1) ^ ((zero - (v & lsb)) & feedback);
  v = (v >> 1) ^ ((zero - (v & lsb)) & feedback);
  v = (v >> 1) ^ ((zero - (v & lsb)) & feedback);
  v = (v >> 1) ^ ((zero - (v & lsb)) & feedback);
  v = (v >> 1) ^ ((zero - (v & lsb)) & feedback);
  v = (v >> 1) ^ ((zero - (v & lsb)) & feedback);
  v = (v >> 1) ^ ((zero - (v & lsb)) & feedback);
  v = (v >> 1) ^ ((zero - (v & lsb)) & feedback);
  v = (v >> 1) ^ ((zero - (v & lsb)) & feedback);
  v = (v >> 1) ^ ((zero - (v & lsb)) & feedback);
  v = (v >> 1) ^ ((zero - (v & lsb)) & feedback);
  v = (v >> 1) ^ ((zero - (v & lsb)) & feedback);
  v = (v >> 1) ^ ((zero - (v & lsb)) & feedback);
  v = (v >> 1) ^ ((zero - (v & lsb)) & feedback);
  v = (v >> 1) ^ ((zero - (v & lsb)) & feedback);
  v = (v >> 1) ^ ((zero - (v & lsb)) & feedback);
  lfsr_state = v;
  return v;
}


#define MEM_TEST_START  0x00000000
#define MEM_TEST_LENGTH (1024 * 1024 * 1)

#define ERR_PRINT_LIMIT 20
int test_memory(void) {
  unsigned int i, j;
  unsigned int *mem = MEM_TEST_START + MAIN_RAM_BASE;
  unsigned int res = 0;
  unsigned int val;
  
  lfsr_init(0xbabe);
  for ( j = 0; j < MEM_TEST_LENGTH; j++ ) {
    mem[j] = lfsr_next();
  }
  flush_l2_cache();

  lfsr_init(0xbabe);
  for( j = 0; j < MEM_TEST_LENGTH; j++ ) {
    val = lfsr_next();
    if( mem[j] != val ) {
      if( res < ERR_PRINT_LIMIT ) {
	printf( "{\"error\":[{\"syndrome\":{\"addr\":%08x, \"got\":%08x, \"expected\":%08x}}]}\n",
		&(mem[(1 << i) + j]), mem[(1 << i) + j], val );
      }
      res++;
    }
  }
  
  return(res);
}

int memtest(void)
{
        int bus_errors, data_errors, addr_errors, ext_errors;

	bus_errors = memtest_bus();
	if(bus_errors != 0)
		printf("Memtest bus failed: %d/%d errors\n", bus_errors, 2*128);

	data_errors = memtest_data();
	if(data_errors != 0)
		printf("Memtest data failed: %d/%d errors\n", data_errors, MEMTEST_DATA_SIZE/4);

	addr_errors = memtest_addr();
	if(addr_errors != 0)
		printf("Memtest addr failed: %d/%d errors\n", addr_errors, MEMTEST_ADDR_SIZE/4);

	ext_errors = 0;
#ifdef TRY_RTT_COMBOS
	test_memory();
#endif
	if(bus_errors + data_errors + addr_errors + ext_errors != 0)
		return 0;
	else {
		printf("Memtest OK\n");
		return 1;
	}
}


#ifdef CSR_DDRPHY_BASE
int sdrlevel(int silent)
{
	int i, j;
	int bitslip;
	int score;
	int best_score;
	int best_bitslip;

	sdrsw();

	for(i=0; i<DFII_PIX_DATA_SIZE/2; i++) {
		ddrphy_dly_sel_write(1<<i);
		ddrphy_rdly_dq_rst_write(1);
		ddrphy_rdly_dq_bitslip_rst_write(1);
	}

#ifdef CSR_DDRPHY_WLEVEL_EN_ADDR
	if(!write_level())
		return 0;
#endif

	/* scan possible read windows */
	best_score = 0;
	best_bitslip = 0;
	for(bitslip=0; bitslip<ERR_DDRPHY_BITSLIP; bitslip++) {
		if (!silent)
			printf("Read bitslip: %d\n", bitslip);
		/* compute score */
		score = read_level_scan(silent);
		if (score > best_score) {
			best_bitslip = bitslip;
			best_score = score;
		}
		/* exit */
		if (bitslip == ERR_DDRPHY_BITSLIP-1)
			break;
		/* increment bitslip */
		for(i=0; i<DFII_PIX_DATA_SIZE/2; i++)
			read_bitslip_inc(i);
	}

	/* select best read window */
	printf("Best read bitslip: %d\n", best_bitslip);
	for(i=0; i<DFII_PIX_DATA_SIZE/2; i++) {
		ddrphy_dly_sel_write(1<<i);
		ddrphy_rdly_dq_bitslip_rst_write(1);
		for (j=0; j<best_bitslip; j++)
			read_bitslip_inc(i);
	}

	/* show scan and do leveling */
    read_level_scan(0);
	read_level();

	return 1;
}
#endif

static void alt_init_sequence(int mr1, int mr2) {
	/* Release reset */
	sdram_dfii_pi0_address_write(0x0);
	sdram_dfii_pi0_baddress_write(0);
	sdram_dfii_control_write(DFII_CONTROL_ODT|DFII_CONTROL_RESET_N);
	cdelay(50000);

	/* Bring CKE high */
	sdram_dfii_pi0_address_write(0x0);
	sdram_dfii_pi0_baddress_write(0);
	sdram_dfii_control_write(DFII_CONTROL_CKE|DFII_CONTROL_ODT|DFII_CONTROL_RESET_N);
	cdelay(10000);

	/* Load Mode Register 2, CWL=5 */
	sdram_dfii_pi0_address_write(mr2);
	sdram_dfii_pi0_baddress_write(2);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS);

	/* Load Mode Register 3 */
	sdram_dfii_pi0_address_write(0x0);
	sdram_dfii_pi0_baddress_write(3);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS);

	/* Load Mode Register 1 */
	sdram_dfii_pi0_address_write(mr1);
	sdram_dfii_pi0_baddress_write(1);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS);

	/* Load Mode Register 0, CL=6, BL=8 */
	sdram_dfii_pi0_address_write(0xd20);
	sdram_dfii_pi0_baddress_write(0);
	command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
	cdelay(200);

	/* ZQ Calibration */
	sdram_dfii_pi0_address_write(0x400);
	sdram_dfii_pi0_baddress_write(0);
	command_p0(DFII_COMMAND_WE|DFII_COMMAND_CS);
	cdelay(200);
}

int alt_sdrinit(char *rtt_nom_str, char *rtt_wr_str, char *ron_str) {
	printf("Initializing SDRAM with parameters...\n");

	int rtt_nom = strtoul(rtt_nom_str, NULL, 0);
	int rtt_wr = strtoul(rtt_wr_str, NULL, 0);
	int ron = strtoul(ron_str, NULL, 0);

	printf( "got rtt_nom %d rtt_wr %d, ron %d\n", rtt_nom, rtt_wr, ron );
	// 1111 1101 1001 1001 = 0xFD99
	int mr1 = 0x44 & 0xFD99;
	// 1111 1001 1111 1111 = 0xF9FF
	int mr2 = 0x0 & 0xF9FF;

	if( ron == 34 ) {
	  mr1 |= (1 << 1);
	}
	// 40 ohm is 00, and others are reserved
	
	if( rtt_nom == 60 ) {
	  mr1 |= (1 << 2);
	} else if( rtt_nom == 120 ) {
	  mr1 |= (1 << 6);
	} else if( rtt_nom == 40 ) {
	  mr1 |= (1 << 2);
	  mr1 |= (1 << 6);
	} else if( rtt_nom == 20 ) {
	  mr1 |= (1 << 9);
	} else if( rtt_nom == 30 ) {
	  mr1 |= (1 << 9);
	  mr1 |= (1 << 2);
	}
	// else disabled

	if( rtt_wr == 60 ) {
	  mr2 |= (1 << 9);
	} else if( rtt_wr == 120 ) {
	  mr2 |= (1 << 10);
	}
	// else off

	printf( "setting mr1: %x, mr2 %x\n", mr1, mr2 );
	alt_init_sequence(mr1, mr2);
	
#ifdef CSR_DDRPHY_BASE
#if CSR_DDRPHY_EN_VTC_ADDR
	ddrphy_en_vtc_write(0);
#endif
	sdrlevel(1);
#if CSR_DDRPHY_EN_VTC_ADDR
	ddrphy_en_vtc_write(1);
#endif
#endif
	sdrhw();
	if(!memtest()) {
#ifdef CSR_DDRPHY_BASE
		/* show scans */
		sdrlevel(0);
#endif
		return 0;
	}

	return 1;
}

static int iter_sdrinit(int rtt_nom, int rtt_wr, int ron) {
	printf("Initializing SDRAM with parameters...\n");

	printf( "trying rtt_nom %d rtt_wr %d, ron %d\n", rtt_nom, rtt_wr, ron );
	// 1111 1101 1001 1001 = 0xFD99
	int mr1 = 0x44 & 0xFD99;
	// 1111 1001 1111 1111 = 0xF9FF
	int mr2 = 0x0 & 0xF9FF;

	if( ron == 34 ) {
	  mr1 |= (1 << 1);
	}
	// 40 ohm is 00, and others are reserved
	
	if( rtt_nom == 60 ) {
	  mr1 |= (1 << 2);
	} else if( rtt_nom == 120 ) {
	  mr1 |= (1 << 6);
	} else if( rtt_nom == 40 ) {
	  mr1 |= (1 << 2);
	  mr1 |= (1 << 6);
	} else if( rtt_nom == 20 ) {
	  mr1 |= (1 << 9);
	} else if( rtt_nom == 30 ) {
	  mr1 |= (1 << 9);
	  mr1 |= (1 << 2);
	}
	// else disabled

	if( rtt_wr == 60 ) {
	  mr2 |= (1 << 9);
	} else if( rtt_wr == 120 ) {
	  mr2 |= (1 << 10);
	}
	// else off

	printf( "setting mr1: %x, mr2 %x\n", mr1, mr2 );
	alt_init_sequence(mr1, mr2);
	
#if CSR_DDRPHY_EN_VTC_ADDR
	ddrphy_en_vtc_write(0);
#endif
	sdrlevel(1);
#if CSR_DDRPHY_EN_VTC_ADDR
	ddrphy_en_vtc_write(1);
#endif
	sdrhw();
	if(!memtest()) {
		/* show scans */
		sdrlevel(0);
		return 0;
	}

	return 1;
}

void clear_ram() {
  int i;
  unsigned int *mem = MEM_TEST_START + MAIN_RAM_BASE;
  
  for( i = 0; i < MEM_TEST_LENGTH; i++ ) {
    mem[i] = 0;
  }
  flush_l2_cache();
}

void try_combos(void) {
  int rtt_nom_set[6] = {0, 120, 40, 20, 30, 60};
  int ron_set[2] = {34, 40};
  int rtt_wr_set[3] = {120, 0, 0};

  int i, j, k;

  for( i = 0; i < 2; i++ ) {
    for( j = 0; j < 3; j++ ) {
      for( k = 0; k < 6; k++ ) {
	iter_sdrinit( rtt_nom_set[k], rtt_wr_set[j], ron_set[i] );
	clear_ram();
      }
    }
  }
}


int sdrinit(void)
{
	printf("Initializing SDRAM...\n");

	init_sequence();
#ifdef CSR_DDRPHY_BASE
#if CSR_DDRPHY_EN_VTC_ADDR
	ddrphy_en_vtc_write(0);
#endif
	sdrlevel(1);
#if CSR_DDRPHY_EN_VTC_ADDR
	ddrphy_en_vtc_write(1);
#endif
#endif
	sdrhw();
	if(!memtest()) {
#ifdef CSR_DDRPHY_BASE
		/* show scans */
		sdrlevel(0);
#endif
		return 0;
	}

	return 1;
}

#endif
