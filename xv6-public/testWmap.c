#include "types.h"
#include "wmap.h"
#include "user.h"
#include "fcntl.h"
#include "mmu.h"

int testWmapAndUnmap(){

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


	return 0;
}

int test_va2pa(void){
	printf(1, "va2pa(x90000000). Expected: -1. Actual: %d\n", va2pa(0x90000000));
	int* my_ret_val;
	my_ret_val = (int*)wmap(0x60000000, 8192, MAP_FIXED|MAP_SHARED|MAP_ANONYMOUS, -1);
	*my_ret_val = 6;
	printf(1, "va2pa(x60000000). Expected: IDK. Actual:  %d\n", va2pa(1));
	printf(1, "val at 0x6000000: %d\n", *((int*)my_ret_val));
	return 0;
}

int test_getwmapinfo(void){
	struct wmapinfo *wminfo= malloc(1000);
	printf(1, "test_getwmapinfo() with 0 mappings: %d\n", getwmapinfo(wminfo));
	printf(1, "expected num mappings: 0 actual: %d\n", wminfo->total_mmaps);
	for(int i = 0; i < 16; i++){
		printf(1, "mapping %d expected start address: %x actual: %x\n", i, 0, wminfo->addr[i]);
		printf(1, "mapping %d expected start length: %x actual: %x\n", i, 0, wminfo->length[i]);
		printf(1, "mapping %d expectedloaded pages: %x actual: %x\n", i, 0, wminfo->n_loaded_pages[i]);
	}

	int * my_ret_val = (int*)wmap(0x60000000, 8192, MAP_FIXED|MAP_SHARED|MAP_ANONYMOUS, -1);
	*my_ret_val = 6;
	printf(1, "test_getwmapinfo() with 1 mappings: %d\n", getwmapinfo(wminfo));
	printf(1, "expected num mappings: 1 actual: %d\n", wminfo->total_mmaps);
	for(int i = 0; i < 16; i++){
		printf(1, "mapping %d expected start address: %x actual: %x\n", i, 0, wminfo->addr[i]);
		printf(1, "mapping %d expected start length: %x actual: %x\n", i, 0, wminfo->length[i]);
		printf(1, "mapping %d expectedloaded pages: %x actual: %x\n", i, 0, wminfo->n_loaded_pages[i]);
	}
	
	my_ret_val = (int*)wmap(0x70000000, 8192, MAP_FIXED|MAP_SHARED|MAP_ANONYMOUS, -1);
	*my_ret_val = 6;
	printf(1, "test_getwmapinfo() with 2 mappings: %d\n", getwmapinfo(wminfo));
	printf(1, "expected num mappings: 2 actual: %d\n", wminfo->total_mmaps);
	for(int i = 0; i < 16; i++){
		printf(1, "mapping %d expected start address: %x actual: %x\n", i, 0, wminfo->addr[i]);
		printf(1, "mapping %d expected start length: %x actual: %x\n", i, 0, wminfo->length[i]);
		printf(1, "mapping %d expectedloaded pages: %x actual: %x\n", i, 0, wminfo->n_loaded_pages[i]);
	}
	return(0);	

}

int test_fork(void) {
	int ret_val;
	ret_val = wmap(0x60000000, 4096, MAP_FIXED|MAP_SHARED|MAP_ANONYMOUS, -1);
	*(int*)ret_val = 25;
	printf(1, "Ret Val before fork %d\n", *(int*)ret_val);
	int fork_ret = fork();
	if(fork_ret > 0) {
		wait();
		printf(1, "Ret Val in parent %d\n", *(int*)ret_val);
	}
	else if(fork_ret == 0) {
		*(int*)ret_val = 672;
		printf(1, "Ret Val in child %d\n", *(int*)ret_val);
	}
	else {
		printf(1, "Fork experienced an error\n");
	}
	return 0;
}

int test_cow(void) {
	uint p = 0x00000600;
	int cow_flag;
	int cow_rw;
	cow_flag = GETCOWF(p);
	cow_rw = GETCOWRW(p);
	printf(1, "COW Flag is %d\nCOW RW is %d\n", cow_flag, cow_rw);
	return 0;
}

int main(void) {
	//testWmapAndUnmap();
	//test_va2pa();
	//test_getwmapinfo();
	test_fork();
	//test_cow();
	exit();
}
