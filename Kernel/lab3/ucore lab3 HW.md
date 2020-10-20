#### [1]

```
if ((ptep = get_pte(mm->pgdir, addr, 1)) == NULL) //获取页表项
    {
        cprintf("get_pte in do_pgfault failed\n");
        goto failed;
    }

    if (*ptep == 0) //判断页表项是否为0,如果为0,则申请一个页
    {
        if (pgdir_alloc_page(mm->pgdir, addr, perm) == NULL)
        {
            cprintf("pgdir_alloc_page in do_pgfault failed\n");
            goto failed;
        }
    }
    else
    {  //如果 页表项不为0
        if (swap_init_ok)
        {
            struct Page *page = NULL;
            if ((ret = swap_in(mm, addr, &page)) != 0) //换入
            {
                cprintf("swap_in in do_pgfault failed\n");
                goto failed;
            }
            // 建立物理与虚拟的映射关系
            page_insert(mm->pgdir, page, addr, perm);
            // 将页属性设置为 可交换
            swap_map_swappable(mm, addr, page, 1);
            page->pra_vaddr = addr;
        }
        else
        {
            cprintf("no swap_init_ok but ptep is %x, failed\n", *ptep);
            goto failed;
        }
    }
```

#### [2]

```
static int
_fifo_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
	// 获取访问记录的链表头
    list_entry_t *head=(list_entry_t*) mm->sm_priv;
    // 获取要写入到双向链表的指针
    list_entry_t *entry=&(page->pra_page_link);
 
    assert(entry != NULL && head != NULL);
    list_add(head, entry); //调用list_add函数 进行双向链表的写入到队列的尾部
    return 0;
}


static int
_fifo_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick)
{
     list_entry_t *head=(list_entry_t*) mm->sm_priv; //获取链表头 mm_struct中保存的sm_priv表示访问记录的链表
     assert(head != NULL);
     assert(in_tick==0);
     list_entry_t *le = head->prev; //获取最进换入的页
     assert(head != le);
     struct Page *p = le2page(le, pra_page_link);
     list_del(le); //将此页从队列中删除
     assert(p != NULL);
     *ptr_page = p; //将获取到的页指针交给ptr_page
     return 0;
}
```

