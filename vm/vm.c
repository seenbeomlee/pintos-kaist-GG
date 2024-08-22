/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/** Project 3-Memory Management */
#include "threads/mmu.h"
static struct list frame_table; 

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	/** Project 3-Memory Management */
	list_init(&frame_table);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/** #project3-Anonymous Page */
		struct page *page = malloc(sizeof(struct page));

		if (!page)
			goto err;

		typedef bool (*initializer_by_type)(struct page *, enum vm_type, void *);
		initializer_by_type initializer = NULL;

		switch (VM_TYPE(type))
		{
		case VM_ANON:
			initializer = anon_initializer;
			break;
		case VM_FILE:
			initializer = file_backed_initializer;
			break;
		}

		uninit_new(page, upage, init, type, aux, initializer);
		page->writable = writable;

		return spt_insert_page(spt, page);
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	/* TODO: Fill this function. */
	/** Project 3-Memory Management */
	struct page *page = (struct page *)malloc(sizeof(struct page));     // 가상 주소에 대응하는 해시 값 도출을 위해 새로운 페이지 할당
	page->va = pg_round_down(va);                                       // 가상 주소의 시작 주소를 페이지의 va에 복제
	struct hash_elem *e = hash_find(&spt->spt_hash, &page->hash_elem);  // spt hash 테이블에서 hash_elem과 같은 hash를 갖는 페이지를 찾아서 return
	free(page);                                                         // 복제한 페이지 삭제

	return e != NULL ? hash_entry(e, struct page, hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	/* TODO: Fill this function. */

	return hash_insert(&spt->spt_hash, &page->hash_elem) ? false : true;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	/** Project 3-Memory Mapped Files */
	hash_delete(&thread_current()->spt.spt_hash, &page->hash_elem);
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	/** Project 3-Memory Management */
	struct frame *frame = (struct frame *)malloc(sizeof(struct frame));
	ASSERT (frame != NULL);

	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);  

    if (frame->kva == NULL)
        frame = vm_evict_frame();  
    else
        list_push_back(&frame_table, &frame->frame_elem);

    frame->page = NULL;

	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
		/** Project 3-Stack Growth*/
    bool success = false;
		addr = pg_round_down(addr);
    if (vm_alloc_page(VM_ANON | VM_MARKER_0, addr, true)) {
        success = vm_claim_page(addr);

        if (success) {
            thread_current()->stack_bottom -= PGSIZE;
        }
    }
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;

	/** Project 3-Anonymous Page */
	struct page *page = NULL;

    if (addr == NULL || is_kernel_vaddr(addr))
        return false;

    if (not_present)
    {
		/** Project 3-Stack Growth*/
		void *rsp = user ?  f->rsp : thread_current()->stack_pointer;
		if (STACK_LIMIT <= rsp - 8 && rsp - 8 == addr && addr <= USER_STACK){
			vm_stack_growth(addr);
			return true;
		}
		else if (STACK_LIMIT <= rsp && rsp <= addr && addr <= USER_STACK){
			vm_stack_growth(addr);
			return true;
		}
		page = spt_find_page(spt, addr);

		if (!page || (write && !page->writable))
			return false;
		
		return vm_do_claim_page(page);
    }
    return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
	/** Project 3-Memory Management */
	struct page *page = spt_find_page(&thread_current()->spt, va);

	if (page == NULL)
		return false;
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	if (!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable))
		return false;

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	/** Project 3-Memory Management */
	hash_init(spt, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
	
	/** Project 3-Memory Mapped Files */
 	struct hash_iterator iter;
    hash_first(&iter, &src->spt_hash);
    while (hash_next(&iter)) {
        struct page *src_page = hash_entry(hash_cur(&iter), struct page, hash_elem);
        enum vm_type type = src_page->operations->type;
        void *upage = src_page->va;
        bool writable = src_page->writable;

        if (type == VM_UNINIT) {
            void *aux = src_page->uninit.aux;
            vm_alloc_page_with_initializer(page_get_type(src_page), upage, writable, src_page->uninit.init, aux);
        }

        else if (type == VM_FILE) {
            struct vm_load_arg *aux = malloc(sizeof(struct vm_load_arg));
            aux->file = src_page->file.file;
            aux->ofs = src_page->file.offset;
            aux->read_bytes = src_page->file.page_read_bytes;

            if (!vm_alloc_page_with_initializer(type, upage, writable, NULL, aux))
                return false;

            struct page *dst_page = spt_find_page(dst, upage);
            file_backed_initializer(dst_page, type, NULL);
            dst_page->frame = src_page->frame;
            pml4_set_page(thread_current()->pml4, dst_page->va, src_page->frame->kva, src_page->writable);
        }

        else {                                          
            if (!vm_alloc_page(type, upage, writable)) 
                return false;

            if (!vm_claim_page(upage))  
                return false;

            struct page *dst_page = spt_find_page(dst, upage); 
            memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
        }
    }

    return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	/** Project 3-Memory Management */
	hash_clear(&spt->spt_hash, hash_page_destroy);
}

uint64_t 
page_hash(const struct hash_elem *e, void *aux)
{
	struct page *page = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&page->va, sizeof page->va);
}

bool 
page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
	struct page *page_a = hash_entry(a, struct page, hash_elem);
	struct page *page_b = hash_entry(b, struct page, hash_elem);

	return page_a->va < page_b->va;
}

/** Project 3-Memory Management */
void 
hash_page_destroy(struct hash_elem *e, void *aux)
{
    struct page *page = hash_entry(e, struct page, hash_elem);
    destroy(page);
    free(page);
}