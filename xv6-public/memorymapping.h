
// Flag defs
#define MAP_ANONYMOUS 0x001
#define MAP_SHARED	  0x010
#define MAP_FIXED	  0x100



struct mappingEntry {
	struct mappingEntry* next_entry;
	uint mapping_addr;
} mappingEntry;


struct MemoryMappings{
	int size;
};
