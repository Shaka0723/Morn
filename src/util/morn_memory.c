/*
Copyright (C) 2019-2020 JingWeiZhangHuai <jingweizhanghuai@163.com>
Licensed under the Apache License, Version 2.0; you may not use this file except in compliance with the License. You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0 Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
*/
 
#include "morn_ptc.h"

pthread_mutex_t debug_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef DEBUG
struct MemoryElement
{
    char info[128];
    void *ptr;
    int size;
    int state;
};

void *morn_pointer;
struct MemoryElement **morn_memory_list = NULL;
int morn_memory_num = 0;
int morn_memory_valid_num = 0;

void *MemoryListSet(int size,const char *file,int line,const char *func)
{
    pthread_mutex_lock(&debug_mutex);
    
    void *ptr = mMemAlloc(size+4);
    // printf("ptr is %p\t",ptr);
    struct MemoryElement *element;
    int row,col;
    if((morn_memory_num == 0)&&(morn_memory_list == NULL))
    {
        morn_memory_list = (struct MemoryElement **)malloc(1024*sizeof(struct MemoryElement *));
        memset(morn_memory_list,0,1024*sizeof(struct MemoryElement *));
    }
    row = morn_memory_num/1024;
    col = morn_memory_num%1024;
    if((col == 0)&&(morn_memory_list[row] == NULL))
        morn_memory_list[row] = (struct MemoryElement *)malloc(1024*sizeof(struct MemoryElement));

    element = morn_memory_list[row]+col;
    sprintf(element->info,"%s:%d %s",file,line,func);
    element->size =  *((int *)ptr -1);
    element->state = 1;
    element->ptr = ptr;
    *((int8_t *)ptr + element->size-4) = -1;
    morn_memory_valid_num++;
    morn_memory_num++;
    
    pthread_mutex_unlock(&debug_mutex);
    // printf("ptr is %p,morn_memory_num is %d\n",ptr,morn_memory_num);
    return ptr;
}

void MemoryListUnset(void *ptr,const char *file,int line,const char *func)
{
    pthread_mutex_lock(&debug_mutex);
    // printf("free morn_memory_num is %d\n",morn_memory_num);
    int i;
    int size = *((int *)ptr -1);
    for(i=0;i<morn_memory_num;i++)
    {
        int row = i/1024;
        int col = i%1024;
        struct MemoryElement *element = morn_memory_list[row]+col;
        if(element->ptr == ptr)
        {
            // printf("size is %d,element->size is %d\n",size,element->size);
            if(element->state == 1)
            {
                if((*((int8_t *)ptr + element->size-4) != -1)||(element->size != size))
                {
                    printf("[%s,line %d]Error: in function %s: memory over used\n",file,line,func);
                    exit(0);
                }
                element->state = 0;
                morn_memory_valid_num --;
                break;
            }
        }
    }
    if(i==morn_memory_num)
    {
        printf("morn_memory_num is %d\n",morn_memory_num);
        printf("[%s,line %d]Error: in function %s: free invalid memory\n",file,line,func);
        exit(0);
    }
   
    if(morn_memory_valid_num == 0)
    {
        for(i=0;i<1024;i++) 
        {
            if(morn_memory_list[i] != NULL) 
                free(morn_memory_list[i]);
        }
        free(morn_memory_list);
        morn_memory_list = NULL;
        morn_memory_num = 0;
    }
    mMemFree(ptr);
    pthread_mutex_unlock(&debug_mutex);
}
#endif
void MemoryListPrint(int state)
{
    #ifdef DEBUG
    // printf("print morn_memory_num is %d\n",morn_memory_num);
    for(int i=0;i<morn_memory_num;i++)
    {
        int row = i/1024;
        int col = i%1024;
        struct MemoryElement *element = morn_memory_list[row]+col;
        if((state<0)||(state==element->state))
            printf("%d:[%s] ptr is 0x%p,size is %d,state is %d\n",i,element->info,element->ptr,element->size-4,element->state);
    }
    
    for(int i=0;i<32;i++)
    {
        if(morn_mem_chain[i] != NULL)
            printf("morn_mem_chain[i] is %p\n",morn_mem_chain[i]);
    }
    
    printf("memory not free %d\n",morn_memory_valid_num);
    printf("memory list over\n");
    #endif
}


struct MAFBlock
{
    void *info;
    unsigned short size;
    volatile unsigned short flag;
    volatile int idlnum;
    volatile int order;
    int info_idx;
    char data[0];
};
struct MAFInfo
{
    struct MAFBlock **block_list;
    struct MAFBlock *block;
    struct MAFBlock *spare;
    int block_num;
    int block_vol;
    int block_order;
};
__thread struct MAFInfo *morn_alloc_free_info=NULL;
short morn_alloc_free_size[32]={48,80,112,144,176,208,240,272,336,400,464,560,688,816,1008,1168,1328,1552,1840,2352,2864,3376,3888,4400,5424,6448,7472,9008,11056,13104,14736,16400};
char morn_alloc_free_size_idx[512]={0,1,2,3,4,5,6,7,8,8,9,9,10,10,11,11,11,12,12,12,12,13,13,13,13,14,14,14,14,14,14,15,15,15,15,15,16,16,16,16,16,17,17,17,17,17,17,17,18,18,18,18,18,18,18,18,18,19,19,19,19,19,19,19,19,19,19,19,19,19,19,19,19,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,23,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,30,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31};
unsigned short morn_alloc_free_ref_flag1[16]={0x0001,0x0002,0x0004,0x0008,0x0010,0x0020,0x0040,0x0080,0x0100,0x0200,0x0400,0x0800,0x1000,0x2000,0x4000,0x8000};
unsigned short morn_alloc_free_ref_flag2[16]={0xFFFE,0xFFFD,0xFFFB,0xFFF7,0xFFEF,0xFFDF,0xFFBF,0xFF7F,0xFEFF,0xFDFF,0xFBFF,0xF7FF,0xEFFF,0xDFFF,0xBFFF,0x7FFF};

void *m_Malloc(int size)
{
    intptr_t *ptr;
    size=size+16;
    if(size>16392) {ptr=malloc(size); ptr[0]=0; return (((char *)ptr)+16);}
    
    int idx=morn_alloc_free_size_idx[(size-17)/32];
    size = morn_alloc_free_size[idx];
    
    if(morn_alloc_free_info==NULL)
    {
        morn_alloc_free_info=malloc(32*sizeof(struct MAFInfo));
        memset(morn_alloc_free_info,0,32*sizeof(struct MAFInfo));
    }
    struct MAFInfo *info  = morn_alloc_free_info+idx;
    
    int invalid_block=(info->block==NULL);
    if(!invalid_block) invalid_block=(info->block->idlnum==0);
    if(invalid_block)
    {
        if(info->spare!=info->block)
        {
            info->block = info->spare;
            invalid_block=0;
        }
    }
    
    struct MAFBlock *block = info->block;
    
    if(!invalid_block)
    {
        mAtomicSub(&(block->idlnum),1);
        idx=block->order;
        while(1)
        {
            if((block->flag&morn_alloc_free_ref_flag1[idx])==0) 
            {
                mAtomicOr( &(block->flag),morn_alloc_free_ref_flag1[idx]);
                block->order = ((idx+1)&0x0000000F);
                ptr = (intptr_t *)(block->data+size*idx);
                ptr[0]=(intptr_t)(block);ptr[1]=idx;
                return (((char *)ptr)+16);
            }
            idx=((idx+1)&0x0000000F);
        }
    }
    
    block = m_Malloc(sizeof(struct MAFBlock)+16*size);
    block->size = size;
    block->info = info;
    block->flag=0x0001;block->idlnum=15;block->order=1;
    
    int n=info->block_num;
    if(n>=info->block_vol)
    {
        struct MAFBlock **list = malloc((n+64)*sizeof(struct MAFBlock *));
        if(info->block_list!=NULL)
        {
            memcpy(list,info->block_list,n*sizeof(struct MAFBlock *));
            free(info->block_list);
        }
        memset(list+n,0,64*sizeof(struct MAFBlock *));
        info->block_list=list;
        info->block_vol=n+64;
        info->block_order=n;
    }
    
    int m=info->block_order;
    while(1)
    {
        if(info->block_list[m]==NULL) 
        {
            info->block_list[m]=block;
            block->info_idx=m;
            m=m+1;if(m>=info->block_vol)m=0;
            info->block_order=m;
            info->block_num = n+1;
            break;
        }
        m=m+1;if(m>=info->block_vol) m=0;
    }
    
    ptr=(intptr_t *)(block->data);
    ptr[0]=(intptr_t)(block);ptr[1]=0;
    if(info->block==info->spare) info->spare =block;
    info->block =block;
    return (((char *)ptr)+16);
}

void m_Free(void *p)
{
    if(p==NULL) return;
    intptr_t *ptr=(intptr_t *)(((char *)p)-16);
    if(ptr[0]==0) {free(ptr);return;}

    struct MAFBlock *block = (struct MAFBlock *)(ptr[0]);
    struct MAFInfo *info = block->info;
    int idx=ptr[1];
    
    if((block!=info->block)&&(block!=info->spare)) 
    {
        if(mAtomicCompare(&(block->idlnum),15,0))
        {
            int n = block->info_idx;
            info->block_num=info->block_num-1;
            info->block_list[n]=NULL;
            info->block_order=n;
            m_Free(block);
            return;
        }
        else if(block->idlnum>0)
        {
            if(!mAtomicCompare(&(info->spare),info->block,block))
            {
                if(block->idlnum>info->spare->idlnum) info->spare=block;
            }
        }
    }
    
    mAtomicAnd(&(block->flag),morn_alloc_free_ref_flag2[idx]);
    block->order = idx;
    mAtomicAdd(&(block->idlnum),1);
}

void *m_Realloc(void *p,int size)
{
    if(p==NULL) return m_Malloc(size);
    intptr_t *ptr=(intptr_t *)(((char *)p)-16);
    if(ptr[0]==0) return realloc(p,size);
    
    struct MAFBlock *block = (struct MAFBlock *)(ptr[0]);
    if(block->size>=size) return p;
    m_Free(p);
    return m_Malloc(size);
}

void endMAF()
{
    if(morn_alloc_free_info==NULL) return;
    for(int i=0;i<32;i++)
    {
        if(morn_alloc_free_info[i].spare!=morn_alloc_free_info[i].block) m_Free(morn_alloc_free_info[i].spare);
        if(morn_alloc_free_info[i].block     !=NULL) m_Free(morn_alloc_free_info[i].block);
        if(morn_alloc_free_info[i].block_list!=NULL)   free(morn_alloc_free_info[i].block_list);
    }
    free(morn_alloc_free_info);
    morn_alloc_free_info=NULL;
}

/*
MThreadSignal memory_thread_sgn=MORN_THREAD_SIGNAL;
void m_Freee(void *p)
{
    intptr_t *ptr=(intptr_t *)(((char *)p)-16);
    if(ptr[0]==0) {free(ptr);return;}

    struct MAFBlock *block = (struct MAFBlock *)(ptr[0]);
    int idx=ptr[1];

    
    block->order = idx;
    mAtomicAnd(&(block->flag),morn_alloc_free_ref_flag2[idx]);
    mThreadLockBegin(memory_thread_sgn);
    mAtomicAdd(&(block->idlnum),1);
    // block->flag &= morn_alloc_free_ref_flag2[idx];
    // block->idlnum =block->idlnum+1;
    // mThreadLockEnd(memory_thread_sgn);
    
    struct MAFInfo *info = block->info;
    if(block==info->block) {
        mThreadLockEnd(memory_thread_sgn);
        return;}
    // 

    if(info->spare==NULL) info->spare=block;
    else if(block->idlnum>info->spare->idlnum) info->spare=block;
    else {
        mThreadLockEnd(memory_thread_sgn);
        return;}

    if(mAtomicCompare(&(block->idlnum),16,0))
    {
        info->spare = NULL;
        mThreadLockEnd(memory_thread_sgn);
        struct MAFBlock *b = info->block;
        info->block = block;
        if(mAtomicCompare(&(b->idlnum),16,0))
        {
            int n = b->info_idx;
            
            info->block_list[n]=NULL;
            info->block_order=n;
            info->block_num=info->block_num-1;
            m_Freee(b);
            return;
        }
    }
    mThreadLockEnd(memory_thread_sgn);
}
*/

/*
    struct MAFBlock *spare = info->spare;
    info->spare = NULL;
    if(mAtomicCompare(&(block->idlnum),16,0))
    {
        spare = NULL;
        struct MAFBlock *b = info->block;
        info->block = block;
        info->block->idlnum=16;
        if(b->idlnum==16)
        {
            int n = b->info_idx;
            info->block_list[n]=NULL;
            info->block_order=n;
            info->block_num=info->block_num-1;
            m_Free(b);
        }
    }
    else
    {
        if(spare==NULL) spare=block;
        else if(block->idlnum>spare->idlnum) spare=block;
    }
    info->spare = spare;
}
*/
    
    
/*
    if(block->idlnum==15)
    {
        info->spare = NULL;
        struct MAFBlock *b = info->block;
        info->block = block;
        if(b->idlnum==16)
        {
            int n = b->info_idx;
            info->block_list[n]=NULL;
            info->block_order=n;
            info->block_num=info->block_num-1;
            m_Free(b);
        }
        else info->spare=b;
    }
    else //if(block->idlnum<16)
    {
             if(info->spare==NULL) info->spare=block;
        else if(block->idlnum>info->spare->idlnum) info->spare=block;
    }
}
*/
    

