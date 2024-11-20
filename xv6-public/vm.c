#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"
#include "wmap.h"
#include "spinlock.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } 
  else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz, uint elf_flags)
{
  uint i, pa, n;
  pte_t *pte;
  elf_flags = elf_flags & 0x2; // Mask elf flags on write perm

  
  if((uint) addr % PGSIZE != 0) // Check that the starting addr is page aligned
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){ // Iterate over every page that needs to be loaded
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0) // Retrieve the pte for the given va
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte); 
    

	
    
    if(sz - i < PGSIZE) // Only load remaining bytes if under a page
      n = sz - i;
    else // Load whole page if still need to read >= a page
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n) {
      return -1;
	}
	
	// Setting permission bits
    *pte = *pte - 2;
    *pte = *pte | elf_flags;

  }
  
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      kfree(v);
      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  return d;

bad:
  freevm(d);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//wunmap removes the mapping starting at addr from the process virtual address space.
// If it's a file-backed mapping with MAP_SHARED, it writes the memory data back to the file to ensure the file remains up-to-da
//te. So, wunmap does not partially unmap any mmap.
int wunmap(uint addr){
	struct proc* p = myproc();
	struct ProcMapping* m = 0;
	
  // check if the mapping exits
	for(int i = 0; i < 16;i++) {
		if((p->mappings[i]).addr == addr) { // Found free entry
			m = &p->mappings[i];
			break;
		}
	}

	// Assume inuse means that is an invalid mapping	
	if((m == 0) | (m->inuse != 1)){
		return -1;	
	}

 // If its a file based mapping
	int fd = -1;
	struct file *pfile = p->ofile[0];
	if((0 < m->fd) & (m->fd < NOFILE)){
		fd = m->fd;
		pfile = p->ofile[fd];
	 }



	void* va_end = (void*)(addr + m->length);
	void* va = (void*)(addr);	
	pfile->off = 0;
	while(va < va_end){

		pte_t *pte = walkpgdir(p->pgdir, va, 0);
		if(pte != 0) {
		    if(0 <= fd){
			    int fw_status =filewrite(pfile, va, PGSIZE);
				if(fw_status == -1){
					cprintf("failed filewrite()\n");
					panic("failed filewrite()");
		
				}
		    }
			uint physical_address = PTE_ADDR(*pte);
			kfree(P2V(physical_address));
			*pte = 0;
		}
		va += PGSIZE;
	}
	// update meta data
	m->inuse = 0;	
	p->mapping_count--;
	return 0;

}

uint wmap(uint addr, int length, int flags, int fd) {
	struct proc* p = myproc(); // The current process
	struct ProcMapping* m; // The mapping entry of the current proc
	pte_t* my_pte;
	void* va;
	int page_count;
	
	// Can't add anymore mappings
	if(p->mapping_count == 16) {
		return FAILED;
	}

	// Search for open mapping
	for(int i = 0; i < 16;i++) {
		m = &p->mappings[i];
		if(m->inuse == 0) { // Found free entry
			break;
		}
	}

	// We're only implementing fixed mappings
	if((flags & MAP_FIXED) != MAP_FIXED) {
		return FAILED;
	}

	// We're only considering shared mappings
	if((flags & MAP_SHARED) != MAP_SHARED) {
		return FAILED;
	}

	// Check within bounds 
	if(addr < LOWER_BOUND || addr > UPPER_BOUND) {
		return FAILED;
	}
	
	// Check for page aligned
	if(addr % PGSIZE != 0) {
		return FAILED;
	}

	// Check requested upper bound is within bounds
	if(addr + length > UPPER_BOUND) {
		return FAILED;
	}
				

	if((flags & (MAP_SHARED|MAP_FIXED)) == (MAP_SHARED|MAP_FIXED)) { // Mapping without file backing
		
		va = (void*)(addr);
		page_count = length/PGSIZE;

		// Checking each PTE we want to see if its being used
		for(int i = 0;i < page_count;i++) {
			my_pte = walkpgdir(p->pgdir, va + (i * PGSIZE), 0); // Retrieve pte

			// Check pte isnt used
			if(my_pte != 0 && (*my_pte & PTE_P)) { 
				cprintf("PTE already used at addr 0x%x\n", addr);
				return FAILED;
			}
		}

		// Check mapping isnt already found
		if(findOverlap(addr, page_count)) {
			cprintf("Mapping already found or overlapping\n");
			return FAILED;
		}

		// Check if file backed
		if((flags & MAP_ANONYMOUS) == 0) { 
			if(fd == - 1) {
				cprintf("File backed mapping with invalid fd\n");
				return FAILED;
			}
			m->fd = fd;
		}
		else {
			m->fd = -1;
		}
		
		// Add to mapping structure
		p->mapping_count++;
		m->inuse = 1;
		m->addr = addr;
		m->length = length;
	}
	return addr;
}

uint va2pa(uint va){	
	struct proc* p = myproc();
	pte_t *pte = walkpgdir(p->pgdir, (void *)va, 0);
	if(pte == 0){
		return -1;
	}
	
	if((*pte & PTE_P)== 0){
		return -1;	
	}
	
	uint physical_address = PTE_ADDR(*pte);
	return physical_address;

	}

int getwmapinfo(struct wmapinfo *wminfo){
	struct proc* p = myproc();
	wminfo->total_mmaps = p->mapping_count;
	struct ProcMapping *m;
	for(int i = 0; i < MAX_WMMAP_INFO; i++){
		m = &p->mappings[i];

		// number of loaded pages starts at 0
		wminfo->n_loaded_pages[i] = 0;
		if(m->inuse != 1){
			wminfo->addr[i] = 0;
			wminfo->length[i] = 0;
			continue;
		}

		int start_addr = m->addr;
		int length = m->length;
		wminfo->addr[i] = start_addr;
		wminfo->length[i] = length;

		for(int va = start_addr + 1; va <(start_addr + length); va+= PGSIZE){

			pte_t *pte = walkpgdir(p->pgdir, (void*)va, 0);
			if(pte != 0){
				if(*pte & PTE_P){
					wminfo->n_loaded_pages[i]++;	
				}
			}	
	} 
	}
	return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.


