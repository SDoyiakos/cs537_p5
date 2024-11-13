#include "types.h"
#include "wmap.h"
#include "user.h"

int main(void) {
	int ret_val;
	uint addr = 0x7000C000;
	ret_val = wmap(addr, 4096, MAP_FIXED|MAP_SHARED|MAP_ANONYMOUS,0);
	
	int ret_val_unmap;
	ret_val_unmap = wunmap(addr);
	printf(1, "Return Value is %d\n", ret_val);
	printf(1, "unmap return val expected: 0. Actual: %d\n", ret_val_unmap);

	ret_val_unmap = wunmap(0x90000000);
	printf(1, "unmap return val should be -1. Actual:  %d\n", ret_val_unmap);
	exit();
}
