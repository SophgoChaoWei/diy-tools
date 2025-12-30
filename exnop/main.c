#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <stdint.h>
#include <errno.h>

#ifdef __aarch64__

#define NOP		0xd503201fU
#define RET		0xd65f03c0U
#define IFENCE()	do { asm volatile ("isb"); } while (0)

#elif defined(__riscv)

#define NOP		0x00010001U
#define RET		0x80828082U
#define IFENCE()	do { asm volatile ("fence.i"); } while (0)

#else

#error "Don't support this architecture"

#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

unsigned long str2size(char *str) {
    if (str == NULL || *str == '\0') return 0;

    char *endptr;
    unsigned long value = strtoul(str, &endptr, 10);

    // Check for conversion errors
    if (endptr == str)
	    return 0;

    // Skip trailing whitespace
    while (isspace((unsigned char)*endptr))
	    endptr++;

    // Handle suffix if present
    if (*endptr != '\0') {
        switch (toupper((unsigned char)*endptr)) {
            case 'K':
            case 'k':
                if (*(endptr+1) == '\0') return value * 1024UL;
                break;
            case 'M':
            case 'm':
                if (*(endptr+1) == '\0') return value * (1024UL * 1024UL);
                break;
            case 'G':
            case 'g':
                if (*(endptr+1) == '\0') return value * (1024UL * 1024UL * 1024UL);
                break;
        }
        return 0;  // Invalid suffix
    }

    return value;  // No suffix
}


void *memset32(void *dst, uint32_t n, size_t s)
{
	size_t i;

	for (i = 0; i < s / 4; ++i)
		((volatile uint32_t *)dst)[i] = n;

	return dst;
}

/* time stamp count in us */
unsigned long time_stamp(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return tv.tv_sec * 1000 * 1000 + tv.tv_usec;
}

void usage(void)
{
	printf("exnop SIZE-OF-INST-BUFFER\n");
}

int main(int argc, char *argv[])
{
	void *inst;
	unsigned long size, start;

	if (argc != 2) {
		printf("Invalid parameter\n");
		usage();
		return EINVAL;
	}

	size = str2size(argv[1]);

	if (size < 4) {
		printf("Instruction size too small\n");
		return EINVAL;
	}

	printf("Allocate %lu bytes instruction buffer\n", size);
	inst = mmap(NULL, size,
			PROT_EXEC | PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			0,
			0);

	if (inst == MAP_FAILED) {
		printf("Map failed\n");
		return ENOMEM;
	}

	printf("Clear instruction buffer with \'NOP\' instruction\n");
	memset32(inst, NOP, size);
	/* add an ret at the end if instruction buffer, let programm back to the caller */
	printf("Add \'RET\' at the end of instruction buffer\n");
	((volatile uint32_t *)inst)[size / 4 - 1] = RET;


	IFENCE();

	printf("Start execute instructions\n");

	start = time_stamp();

	((void (*)(void))inst)();

	printf("Execute %lu NOPs in %lu ms\n", size, (time_stamp() - start) / 1000);

	munmap(inst, size);

	return 0;
}
