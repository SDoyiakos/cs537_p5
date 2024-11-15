#include "types.h"
#include "wmap.h"
#include "user.h"

int main(void) {
	int* ret_val;
	ret_val = (void*)wmap(0x60000000, 8192, MAP_FIXED|MAP_SHARED|MAP_ANONYMOUS,0);
	*ret_val = 5;
	printf(1, "VALUE ALLOCATED IS %d\n", *ret_val);
	wmap(0x60002000, 4096, MAP_FIXED|MAP_SHARED|MAP_ANONYMOUS,0);
	exit();
}
