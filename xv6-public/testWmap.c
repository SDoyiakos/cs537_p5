#include "types.h"
#include "wmap.h"
#include "user.h"

int main(void) {
	int ret_val;
	int second_ret_val;
	ret_val = wmap(0x60000000, 8192, MAP_FIXED|MAP_SHARED|MAP_ANONYMOUS,0);
	second_ret_val = wmap(0x60001000, 4096, MAP_FIXED|MAP_SHARED|MAP_ANONYMOUS,0);
	printf(1, "Return Value of first is 0x%x\n", ret_val);
	printf(1, "Return Value of second is 0x%x\n", second_ret_val);
	exit();
}
