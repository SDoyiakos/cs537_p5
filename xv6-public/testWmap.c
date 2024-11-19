#include "types.h"
#include "wmap.h"
#include "user.h"
#include "fcntl.h"

int main(void) {
  	int my_ret_val;
	int fd = open("testFile", O_CREATE|O_RDWR);
	int write_ret;
	write_ret = write(fd, "I am a file\n", 20);
	printf(1, "Write return val is %d\n", write_ret);
	my_ret_val = wmap(0x60002000, 4096, MAP_FIXED|MAP_SHARED,fd);
	printf(1,"Return value is 0x%x\n", my_ret_val);
	printf(1,"The file contains %s\n", (char*)my_ret_val);
	exit();
}
