#include "types.h"
#include "wmap.h"
#include "user.h"

int main(void) {
	int ret_val;
	ret_val = wmap(0x7000C000, 4096, MAP_FIXED|MAP_SHARED|MAP_ANONYMOUS,0);

	printf(1, "Return Value is %d\n", ret_val);
	exit();
}
