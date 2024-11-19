#include "types.h"
#include "wmap.h"
#include "user.h"
#include "fcntl.h"

int main(void) {
  	char* my_ret_val;
	int fd = open("testFile", O_CREATE|O_RDWR);
	int write_ret;


	for(int i = 0; i < 800; i++){
		if(i < 400){
			write_ret = write(fd,"aaaaaaaaa\n", 10);
		}else {
			write_ret = write(fd,"bbbbbbbbb\n", 10);
		}
	}


	printf(1, "fd: %d\n", fd);
	printf(1, "Write return val is %d\n", write_ret);

	my_ret_val = (char*)wmap(0x60000000, 8192, MAP_FIXED|MAP_SHARED,fd);
	printf(1,"Return value is 0x%x\n", my_ret_val);
	printf(1,"The file start %c\n", *my_ret_val);

	printf(1,"The file middle %c\n", *(my_ret_val+4096));

	printf(1,"The file end %c\n", *(my_ret_val+8000));
	*my_ret_val = 'p';
	*(my_ret_val + 1) = 'h';
	*(my_ret_val + 2) = 'o';
	*(my_ret_val + 3) = 'e';
	*(my_ret_val + 4) = 'n';
	*(my_ret_val + 5) = 'i';
	*(my_ret_val + 6) = 'x';

	*(my_ret_val + 4096) = 'p';
	*(my_ret_val + 4097) = 'h';
	*(my_ret_val + 4098) = 'o';
	*(my_ret_val + 4099) = 'e';
	*(my_ret_val + 4100) = 'n';
	*(my_ret_val + 4101) = 'i';
	*(my_ret_val + 4102) = 'x';

	*(my_ret_val + 7990) = 'p';
	*(my_ret_val + 7991) = 'h';
	*(my_ret_val + 7992) = 'o';
	*(my_ret_val + 7993) = 'e';
	*(my_ret_val + 7994) = 'n';
	*(my_ret_val + 7995) = 'i';
	*(my_ret_val + 7996) = 'x';

	
	if(wunmap((uint)my_ret_val) < 0){
		printf(1, "calling wunmap() failed\n");
	}
	close(fd);
	exit();
}
