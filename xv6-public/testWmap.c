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
			write_ret = write(fd,"aaaaaaaaaa", 10);
		}else {
			write_ret = write(fd,"bbbbbbbbbb", 10);
		}
	}


	printf(1, "fd: %d\n", fd);
	printf(1, "Write return val is %d\n", write_ret);

	my_ret_val = (char*)wmap(0x60000000, 8192, MAP_FIXED|MAP_SHARED,fd);
	printf(1,"Return value is 0x%x\n", my_ret_val);
	printf(1,"The file start %c\n", *my_ret_val);

	printf(1,"The file middle %c\n", *(my_ret_val+4096));

	printf(1,"The file end %c\n", *(my_ret_val+8000));
//	*my_ret_val = 'p';
//	*(my_ret_val + 1) = 'h';
//	*(my_ret_val + 2) = 'o';
//	*(my_ret_val + 3) = 'e';
//	*(my_ret_val + 4) = 'n';
//	*(my_ret_val + 5) = 'i';
//	*(my_ret_val + 6) = 'x';
//
//
//	*(my_ret_val + 40) = 'p';
//	*(my_ret_val + 41) = 'h';
//	*(my_ret_val + 42) = 'o';
//	*(my_ret_val + 43) = 'e';
//	*(my_ret_val + 44) = 'n';
//	*(my_ret_val + 45) = 'i';
//	*(my_ret_val + 46) = 'x';
//
//	printf(1, "va: %x\n", my_ret_val + 4096);
//	*(my_ret_val + 4096) = 'p';
//	*(my_ret_val + 4097) = 'h';
//	*(my_ret_val + 4098) = 'o';
//	*(my_ret_val + 4099) = 'e';
//	*(my_ret_val + 4100) = 'n';
//	*(my_ret_val + 4101) = 'i';
//	*(my_ret_val + 4102) = 'x';


//	
//	if(wunmap((uint)my_ret_val) < 0){
//		printf(1, "calling wunmap() failed\n");
//	}
	
	//printf(1, "wunmap() test:\n my_ret_val expected: page fault.\n actual: %s", (char*)my_ret_val);
	printf(1, "wunmap() file-backed test\n");
	close(fd);
	exit();
}
