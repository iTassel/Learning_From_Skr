###  [1] 

```
static void
default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    list_add_before(&free_list, &(base->page_link)); //list_add_after和list_add_before的区别在于页添加到free_list链表中的位置前后
}
```

初始化,将管理的内存页的Page Header 加入到free_list的双向链表里面

```
static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
    // 遍历链表 找到符合条件的Page结构
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            page = p;
            break;
        }
    }
    if (page != NULL) {
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p); //设置页p的flags表示占用中,类似于加锁?
            list_add_after(&(page->page_link), &(p->page_link)); //这里应该是将page与p切割开,并在双向链表中构造进去
    }
        list_del(&(page->page_link)); //从链表中删除这个链表
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}
```

申请内存页,遍历链表,找到适合的大小,如果大于就切分,剩下的last reminder就依旧存在原先的链表位置,

然后返回Page Header的指针

```
static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p)); //判断每一个内存页是否可以修改或者释放
        p->flags = 0;
        set_page_ref(p, 0); //页引用计数置0
    }
    base->property = n;
    SetPageProperty(base);
    list_entry_t *le = list_next(&free_list);
    while (le != &free_list) {  //循环然后将内存页合并到相邻的页中,修改Page Header
        p = le2page(le, page_link);
        le = list_next(le);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
        else if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            base = p;
            list_del(&(p->page_link));
        }
    }
    nr_free += n;
    le = list_next(&free_list);
    while (le != &free_list) //遍历检测,需释放的这段内存页 是否与空间内存页相邻
    {
        p = le2page(le, page_link);
        if (base + base->property <= p)
        {
            assert(base + base->property != p);
            break;
        }
        le = list_next(le);
    }
    list_add_before(&free_list, &(base->page_link)); //将上述内存页加入到free_list的链表中
}
```

首先将需要释放的一连串的内存页的对应的Page结构参数修改

然后检测前后向合并

如果不与空间内存页相邻 就不合并 并将这段内存加入到free_list链表中管理

### [2]

```
//get_pte()
    pde_t *pdep = &pgdir[PDX(la)]; //获取pgdir中索引页目录项地址
    if (!(*pdep & PTE_P)) //检测标志位
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)//如果create为0,则return,如果不为0,则需要alloc一个内存页
        {
            return NULL;
        }
        set_page_ref(page, 1); //设置ref为1,表示该页引用一次
        uintptr_t pa = page2pa(page); //获取物理页地址
        memset(KADDR(pa), 0, PGSIZE);//将返回的页表初始化
        *pdep = pa | PTE_U | PTE_W | PTE_P; //修改页的权限 读/写/存在标志位,并填入页目录项
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep)))[PTX(la)]; //返回页表项地址
 
```

### [3]

```
//page_remove_pte()
if (*ptep & PTE_P) //检测页表是否存在
    {
        struct Page *page = pte2page(*ptep); //获取管理页表的Page结构地址
        if (page_ref_dec(page) == 0) //取消页的引用
        {
            free_page(page); //释放页表所在的页
        }
        *ptep = 0; //页目录项设置为0
        tlb_invalidate(pgdir, la); //传入页目录地址 与 线性地址 (刷新TLB，保证TLB中的缓存不会有错误的映射关系)
    }
```

太菜了,只能抄实验答案了,后面的抄都抄不会
