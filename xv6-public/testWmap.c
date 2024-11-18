#include "types.h"
#include "wmap.h"
#include "user.h"

int main(void) {
	int* ret_val;
	ret_val = (void*)wmap(0x60000000, 8192, MAP_FIXED|MAP_SHARED|MAP_ANONYMOUS,0);
	*ret_val = 5;
	printf(1, "VALUE ALLOCATED IS %d\n", *ret_val);


	printf(1, "wunmap(%d). Expect a page fault\n", ret_val);
	wunmap((int)ret_val);

	printf(1, "wunmap(ret_val): %d\n", *ret_val);

	
	exit();
}
