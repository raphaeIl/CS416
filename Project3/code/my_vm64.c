#include "my_vm64.h"

vm_t vm; // global variable that contains the data needed for my vm
pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;

/*
Function responsible for allocating and setting your physical memory 
*/
void set_physical_mem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating
    vm.physical_memory = malloc(MEMSIZE);

    if (vm.physical_memory == NULL) 
    {
        printf("Not enough memory available!\n");
        exit(EXIT_FAILURE);
    }

    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
    vm.num_physical_pages = MEMSIZE / PGSIZE;
    vm.num_virtual_pages = MAX_MEMSIZE / PGSIZE;

    vm.physical_bitmap = calloc(vm.num_physical_pages, sizeof(char)); // calloc to set all initial values to 0 (unallocated)
    vm.virtual_bitmap = calloc(vm.num_virtual_pages, sizeof(char)); 

    set_bit_at_index(vm.physical_bitmap, 0); // set bit 0 to always used
    set_bit_at_index(vm.virtual_bitmap, 0);

    TLB_init();
}


/*
 * Part 2: Initialize the core struct for managing tlb
 */
void TLB_init() {
    for (int i = 0; i < TLB_ENTRIES; i++) 
    {
        vm.tlb_store.tlb_entries[i].valid = 0; // set all entries as invalid
    }
    
    vm.tlb_store.hits = 0;
    vm.tlb_store.misses = 0;
}

/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 * 
 * Note: Make sure this is thread safe by locking around critical 
 *       data structures touched when interacting with the TLB
 */
int TLB_add(void *va, void *pa)
{
    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
    unsigned long vpn, ppn;

    extract_page_number_from_address(va, &vpn);
    extract_page_number_from_address(pa, &ppn);

    int index = vpn % TLB_ENTRIES;

    tlb_entry_t new_tlb_entry;

    new_tlb_entry.virtual_page = vpn;
    new_tlb_entry.physical_page = ppn;
    new_tlb_entry.valid = 1;

    vm.tlb_store.tlb_entries[index] = new_tlb_entry;
    return 0;
}

int TLB_remove(void *va) { // this simply just make the entry invalid by setting the valid int
    unsigned long vpn;

    extract_page_number_from_address(va, &vpn);

    int index = vpn % TLB_ENTRIES;
    
    vm.tlb_store.tlb_entries[index].valid = 0;
}

/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 * 
 * Note: Make sure this is thread safe by locking around critical 
 *       data structures touched when interacting with the TLB
 */
pte_t* TLB_check(void *va) {

    /* Part 2: TLB lookup code here */

    /*This function should return a pte_t pointer*/
    unsigned long vpn; // vpn: extract the first 20 bits which is the vpn (offset removed)

    extract_page_number_from_address(va, &vpn);
    int index = vpn % TLB_ENTRIES; // calculating the index this mapping should be stored at (vpn % num_sets)

    if (vm.tlb_store.tlb_entries[index].valid && vm.tlb_store.tlb_entries[index].virtual_page == vpn) 
    {
        vm.tlb_store.hits++;

        unsigned long ppn = vm.tlb_store.tlb_entries[index].physical_page;
        unsigned long found_physical_address = ppn * PGSIZE;

        return (pte_t*)found_physical_address;
    }

    vm.tlb_store.misses++;
    return NULL;
}


/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void print_TLB_missrate()
{
    /*Part 2 Code here to calculate and print the TLB miss rate*/
    int total = vm.tlb_store.hits + vm.tlb_store.misses;

    double miss_rate = (total == 0) ? 0.0 : ((double)vm.tlb_store.misses / total) * 100; // just incase if we get no lookups which probably wont happen idk but dont wanna cause divide by 0

    printf("Hits: %d, Misses: %d\n", vm.tlb_store.hits, vm.tlb_store.misses);
    printf("TLB miss rate: %.2lf%%\n", miss_rate);
}

/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t* translate(pde_t* pgdir, void* va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */
    pte_t* pa = TLB_check(va);

    if (pa != NULL) // cached translation found inside tlb
    {
        return pa;
    }

    unsigned int indices[NUM_LEVELS];
    unsigned int offset;
    extract_data_from_va(va, indices, &offset);

    pde_t* current_table = pgdir;

    for (int i = 0; i < NUM_LEVELS - 1; i++) // walk the page table until the last level
    {
        if (!current_table[indices[i]]) 
        {
            return NULL; 
        }

        current_table = (pde_t*)current_table[indices[i]];
    }

    int num_offset_bits = (int)log2(PGSIZE); 

    // last level (real page table reached omg)
    unsigned long target_entry = current_table[indices[NUM_LEVELS - 1]];
    unsigned long physical_address = target_entry << num_offset_bits | offset;

    TLB_add(va, (void*)physical_address);

    return (pte_t *)physical_address;
}


/*
    The function takes a page directory address, virtual address, physical address
    as an argument, and sets a page table entry. This function will walk the page
    directory to see if there is an existing mapping for a virtual address. If the
    virtual address is not present, then a new entry will be added
*/
int map_page(pde_t* pgdir, void *va, void *pa)
{
    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */
    unsigned long physical_address = (unsigned long)pa;

    unsigned int indices[NUM_LEVELS];
    unsigned int offset;
    extract_data_from_va(va, indices, &offset);

    pde_t* current_table = pgdir;
    int num_offset_bits = (int)log2(PGSIZE); 
    int num_bit_per_level = (NUM_BIT_ADDRESS_SPACE - num_offset_bits) / NUM_LEVELS;
    int page_directory_size = pow(2, num_bit_per_level); // 13 bits 2^13 = 8192

    // for each table down the walk, allocate if NULL
    for (int i = 0; i < NUM_LEVELS - 1; i++) 
    {
        if (!current_table[indices[i]]) 
        {
            current_table[indices[i]] = (pde_t)calloc(page_directory_size, sizeof(pde_t));
        }

        current_table = (pde_t*)current_table[indices[i]];
    }

    if (current_table[indices[NUM_LEVELS - 1]]) 
    {
        return -1; // page already mapped
    }

    unsigned long ppn; // extract the top bits of the physical address, this removes the offset since it is not needed to be stored (the va contains the offsets)
    
    extract_page_number_from_address(pa, &ppn);
    current_table[indices[NUM_LEVELS - 1]] = ppn;

    if (TLB_check(va) == NULL) // cached translation found inside tlb if not inside already
    {
        TLB_add(va, pa);
    }

    return 0;
}


/*
    Function that gets the next available virtual page
    Note that this will find the next available space in the virtual memory
    that has enough continous space for *num_pages*, not just any opened single space. 
*/
void* get_next_avail_virtual(int num_pages) {
    for (int i = 1; i <= vm.num_virtual_pages - num_pages; i++)
    {
        int is_available = 1;

        for (int j = i; j < i + num_pages; j++)
        {
            if (get_bit_at_index(vm.virtual_bitmap, j) == 1)
            {
                is_available = 0;
                break;
            }
        }

        if (is_available)
        {
            unsigned long available_virtual_address = i * PGSIZE;
            return (void*)available_virtual_address;
        }
    }
    return NULL;
}

/*
    Function that gets the next available physical page
    Note that this will find the next available space in the physical memory
    unlike virtual memory, this does not have to be contiguous and can be any opened single space combined together. 
*/
void* get_next_avail_physical() {
    for (int i = 1; i < vm.num_physical_pages; i++)
    {
        if (get_bit_at_index(vm.physical_bitmap, i) == 0)
        {
            unsigned long available_physical_address = i * PGSIZE;

            return (void*)available_physical_address;
        }
    }

    return NULL; 
}

/* Function responsible for allocating pages and used by the benchmark
 *
 * Note: Make sure this is thread safe by locking around critical 
 *       data structures you plan on updating. Two threads calling
 *       this function at the same time should NOT get the same
 *       or overlapping allocations
*/
void *n_malloc(unsigned long num_bytes) {

    /* 
     * HINT: If the physical memory is not yet initialized, then allocate and initialize.
     */

   /* 
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will 
    * have to mark which physical pages are used. 
    */

    pthread_mutex_lock(&global_lock);
    void* ret = __n_malloc_internal(num_bytes);
    pthread_mutex_unlock(&global_lock);

    return ret;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void n_free(void *va, int size) {

    /* Part 1: Free the page table entries starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the 
     * memory from "va" to va+size is valid.
     *
     * Part 2: Also, remove the translation from the TLB
     */

    pthread_mutex_lock(&global_lock);
    __n_free_internal(va, size);
    pthread_mutex_unlock(&global_lock);
}

int put_data(void *va, void *val, int size) {

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger 
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */

    pthread_mutex_lock(&global_lock);
    int ret = __put_data_internal(va, val, size);
    pthread_mutex_unlock(&global_lock);
    /*return -1 if put_data failed and 0 if put is successfull*/

    return ret;
}

/*Given a virtual address, this function copies the contents of the page to val*/
void get_data(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */
    pthread_mutex_lock(&global_lock);
    __get_data_internal(va, val, size);
    pthread_mutex_unlock(&global_lock);
}

/*
    This function receives two matrices mat1 and mat2 as an argument with size
    argument representing the number of rows and columns. After performing matrix
    multiplication, copy the result to answer.
    Default implementation as given, didn't change anything
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
     * matrix accessed. Similar to the code in test.c, you will use get_data() to
     * load each element and perform multiplication. Take a look at test.c! In addition to 
     * getting the values from two matrices, you will perform multiplication and 
     * store the result to the "answer array"
     */
    int x, y, val_size = sizeof(int);
    int i, j, k;
    for (i = 0; i < size; i++) {
        for(j = 0; j < size; j++) {
            unsigned long a, b, c = 0;
            for (k = 0; k < size; k++) {
                int address_a = (unsigned long)mat1 + ((i * size * sizeof(int))) + (k * sizeof(int));
                int address_b = (unsigned long)mat2 + ((k * size * sizeof(int))) + (j * sizeof(int));
                get_data( (void *)address_a, &a, sizeof(int));
                get_data( (void *)address_b, &b, sizeof(int));
                printf("Values at the index: %d, %d, %d, %d, %d\n", 
                    a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned long)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            printf("This is the c: %d, address: %x!\n", c, address_c);
            put_data((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}

// Bit Util Functions from Project 1
/*
    This is a custom utils function that will extract a speific segments of bits from an value using a mask,
    from beginning (inclusive) to end (exclusive), or range: [begin, end)
    From the least sigificant bit to the most, so reversed order.
*/ 
unsigned long extract_bits(unsigned long value, int begin, int end)
{
    unsigned long mask = (1UL << (end - begin)) - 1;
    return (value >> begin) & mask;
}

unsigned long get_top_bits(unsigned long value,  int num_bits)
{
	int shift = floor(log2(value)) + 1;

    return value >> (shift - num_bits);
}

/* 
 * Function 2: SETTING A BIT AT AN INDEX 
 * Function to set a bit at "index" bitmap
 */
void set_bit_at_index(char *bitmap, int index)
{
    int byteNum = index / 8;  
    int bitNum = index % 8;  

    bitmap[byteNum] |= (1 << bitNum);
}

void clear_bit_at_index(char *bitmap, int index)
{
    int byteNum = index / 8;  
    int bitNum = index % 8;  

    bitmap[byteNum] &= ~(1 << bitNum); 
}

/* 
 * Function 3: GETTING A BIT AT AN INDEX 
 * Function to get a bit at "index"
 */
int get_bit_at_index(char *bitmap, int index)
{
    int byteNum = index / 8; 
    int bitNum = index % 8; 

    return (bitmap[byteNum] >> bitNum) & 1;
}

void extract_data_from_va(void* va, unsigned int* indices, unsigned int* offset) 
{
    unsigned long virtual_address = (unsigned long)va;
    int num_offset_bits = (int)log2(PGSIZE); 
    int num_bit_per_level = (NUM_BIT_ADDRESS_SPACE - num_offset_bits) / NUM_LEVELS;

    *offset = extract_bits(virtual_address, 0, num_offset_bits);

    // extract the index for each level, in this case 4-level 12 bit offset in a 64 bit space, (64 - 12) / 4 = 13 bits per level
    // store each corresponding index to the array passed in
    for (int i = 0; i < NUM_LEVELS; i++) 
    {
        int start_bit = num_offset_bits + (i * num_bit_per_level);
        indices[i] = extract_bits(virtual_address, start_bit, start_bit + num_bit_per_level);
    }
}

void extract_page_number_from_address(void* address, unsigned long* page_number)
{
    int num_offset_bits = (int)log2(PGSIZE); // in a 4k page size config, 12 bits reserved for the offset

    *page_number = extract_bits((unsigned long)address, num_offset_bits, NUM_BIT_ADDRESS_SPACE);
}

void* __n_malloc_internal(unsigned long num_bytes) {
    int num_offset_bits = (int)log2(PGSIZE); 
    int num_bit_per_level = (NUM_BIT_ADDRESS_SPACE - num_offset_bits) / NUM_LEVELS;
    int page_directory_size = pow(2, num_bit_per_level);

    if (vm.physical_memory == NULL)
    {
        set_physical_mem();
        vm.pgdir = calloc(page_directory_size, sizeof(pde_t)); // 2^10, 10 bits for first level
    }

    unsigned long num_pages_needed = (int)ceil((double)num_bytes / PGSIZE); // calculates the number of pages needed, rounded up since for example 4097 bytes uses 2 pages

    pte_t next_available_virtual_address = (pte_t)get_next_avail_virtual(num_pages_needed);;

    if (next_available_virtual_address == (int)NULL)
    {
        printf("No available virtual memory for allocation\n");
        return NULL;
    }

    pte_t current_virtual_address = next_available_virtual_address;

    for (int i = 0; i < num_pages_needed; i++) { // for EACH page, map the va to pa, (storing it inside the pagetable) and set the bit in the bitmap indicating it has been allocated
        unsigned long current_physical_address = (unsigned long)get_next_avail_physical(); 
        
        if (current_physical_address == (int)NULL) 
        {
            printf("Out of physical memory\n");
            return NULL;
        }

        // map the virtual page to the physical page
        int map_result = map_page(vm.pgdir, (void*)current_virtual_address, (void*)current_physical_address);
        
        if (map_result != 0) 
        {
            printf("Failed to map page for virtual address\n");
            return NULL;
        }

        int virtual_page_index = current_virtual_address / PGSIZE;
        int physical_page_index = current_physical_address / PGSIZE;

        set_bit_at_index(vm.virtual_bitmap, virtual_page_index);
        set_bit_at_index(vm.physical_bitmap, physical_page_index);
        current_virtual_address += PGSIZE;
    }

    return (void*)next_available_virtual_address; // return the start of the va
}

void __n_free_internal(void *va, int size) { // freeing the whole page even if size is not a multiple of PGSIZE
    if (!va || size <= 0)
    {
        printf("Invalid Pointer or Size\n");
        return;
    }

    unsigned long virtual_address = (unsigned long)va;
    int num_pages_freed = (int)ceil((double)size / PGSIZE);
    
    for (int i = 0; i < num_pages_freed; i++) // looping through and freeing EACH page, for each page, set the corresponding region in the physical memory to null, clear the bit in the bitmap and remove it from the TLB
    {
        unsigned long current_virtual_address = virtual_address + i * PGSIZE; 
        unsigned long current_physical_address = (unsigned long)translate(vm.pgdir, (void *)current_virtual_address);

        if (current_physical_address == (int)NULL) 
        {
            printf("Virtual address has not been mapped or allocated\n");
            return;
        }

        int virtual_page_index = current_virtual_address / PGSIZE;
        int physical_page_index = current_physical_address / PGSIZE;

        // unsigned long page_dir_index, page_table_index, offset;
        unsigned int indices[NUM_LEVELS], offset;
        extract_data_from_va((void*)current_virtual_address, indices, &offset);

        pde_t* current_table = vm.pgdir;
        
        for (int j = 0; j < NUM_LEVELS - 1; j++) 
        {
            current_table = (pde_t*)current_table[indices[j]];
        }

        current_table[indices[NUM_LEVELS - 1]] = 0; // clear the page table


        clear_bit_at_index(vm.virtual_bitmap, virtual_page_index);
        clear_bit_at_index(vm.physical_bitmap, physical_page_index); 

        TLB_remove((void*)current_virtual_address);
    }
}

int __put_data_internal(void *va, void *val, int size) {
    if (!va || !val || size <= 0) {
        printf("Invalid Pointer or Size\n");
        return -1;
    }

    if (vm.physical_memory == NULL)
    {
        printf("Please malloc first\n");
        return -1;
    }

    unsigned long virtual_address = (unsigned long)va;
    char* data = (char*)val;
    int remaining_data_size = size;

    while (remaining_data_size > 0) { // memcpy the data provided to the corresponding physical memory region, if the data doesn't fit in one page, still copy that amount of bytes into the page, but the page will not be full
        unsigned long current_physical_address = (unsigned long)translate(vm.pgdir, (void *)virtual_address);

        if (current_physical_address == (int)NULL) 
        {
            printf("Virtual address has not been mapped or allocated\n");
            return -1;
        }

        unsigned long offset = virtual_address % PGSIZE;

        int copy_size = PGSIZE;

        if (copy_size > remaining_data_size) 
        {
            copy_size = remaining_data_size; 
        }

        memcpy(vm.physical_memory + current_physical_address, data, copy_size);

        virtual_address += copy_size; // move to the next virtual address
        data += copy_size;    // move to the next source data position
        remaining_data_size -= copy_size; // decrease remaining data size
    }

    return 0;
}

void __get_data_internal(void *va, void *val, int size) { // very similar to put_data except that instead of copying (data -> memory) we copy (memory -> data), also done one page at a time
    if (!va || !val || size <= 0) {
        printf("Invalid Pointer or Size\n");
        return;
    }

    if (vm.physical_memory == NULL)
    {
        printf("Please malloc first\n");
        return;
    }

    unsigned long virtual_address = (unsigned long)va;
    char* dest = (char*)val;
    int remaining_data_size = size;

    while (remaining_data_size > 0) {

        unsigned long current_physical_address = (unsigned long)translate(vm.pgdir, (void *)virtual_address);

        if (current_physical_address == (int)NULL) 
        {
            printf("Virtual address has not been mapped or allocated\n");
            return;
        }

        unsigned long offset = virtual_address % PGSIZE;

        int copy_size = PGSIZE;

        if (copy_size > remaining_data_size) 
        {
            copy_size = remaining_data_size; 
        }

        memcpy(dest, vm.physical_memory + current_physical_address, copy_size);

        virtual_address += copy_size; 
        dest += copy_size;  
        remaining_data_size -= copy_size;
    }
}