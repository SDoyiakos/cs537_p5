
#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  case T_PGFLT:
  	
  	uint flt_addr = rcr2(); // The page fault addr
	char* mem; // Used to hold kalloc addr
	pte_t* pte = walkpgdir(myproc()->pgdir, (void*)flt_addr, 0);
	uint pa;
	//cprintf("PTE is  0x%x Get COW F is 0x%x\n", *pte, GETCOWF(*pte));
	// COW PGFLT
	if(GETCOWF(*pte) == 1) {
		if(GETCOWRW(*pte) == 0) {
			cprintf("COW ");
			cprintf("Segmentation Fault\n");
			exit();
		}

		pa = PTE_ADDR(*pte);
		//cprintf("PA 0x%x REF CT %d\n", pa, getRefCnt(pa));
		// If last reference just set write bit
		if(getRefCnt(pa) == 1) { 
			*pte|=PTE_W;
			lcr3(V2P(myproc()->pgdir)); // Flush TLB
			break;
		}

		// There are other refs
		else if(getRefCnt(pa) > 1) {
			mem = kalloc();
			if(mem == 0) {
				cprintf("Error with COW kalloc\n");
				exit();
			}
			//cprintf("About to map\n");
			memmove(mem, (char*)P2V(pa), PGSIZE);
			//cprintf("Beyond memmove\n");
			decreaseRefCnt(pa);
			*pte = V2P(mem) | PTE_W | PTE_P | PTE_U;
			//cprintf("PTE NOW MAPS TO 0x%x\n", PTE_ADDR(*pte));
			lcr3(V2P(myproc()->pgdir)); // Flush TLB
			break;
		}	
		else {
			cprintf("Error, ref count < 1\n");
			exit();
		}
	} 	

	/** Mapping pgflt **/
	struct ProcMapping* curr_mapping; // Pointer to current mapping
	curr_mapping = findMapping(flt_addr); // Get current mapping
	if(curr_mapping != (struct ProcMapping*)0) { // If a mapping is found

		// Allocate memory
		mem = kalloc();
		if(mem == 0) { 
			cprintf("Page fault kalloc failed\n");
			exit();
		}

		// Map the page to the mem region
		if(mappages(myproc()->pgdir, (void*)PGROUNDDOWN(flt_addr), PGSIZE, V2P(mem), PTE_W|PTE_U) <0) {
			kfree(mem);
			cprintf("Failed to map at addr 0x%x\n", PGROUNDDOWN(flt_addr));
			exit();
		}
		
		//zero -initialize the pages
		
		
		struct file* my_file = curr_mapping->f;
//		int fd = curr_mapping->fd;
		
		if(my_file != 0){
			
			my_file->off = PGROUNDDOWN(flt_addr) - curr_mapping->addr;
			
			// Read from file into the page
			if(my_file != 0 && fileread(my_file, (char*)PGROUNDDOWN(flt_addr), PGSIZE) == -1) {
				cprintf("Failed to read from file\n");
				exit();
			}
			my_file->off = 0;

		} else {
			memset(mem, 0, PGSIZE);
			}
		}
	else {
		cprintf("Segmentation Fault\n");
		exit();
	}
	break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.) 
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
