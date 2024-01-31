#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <time.h>
#include <Python.h>

#include "coreio.h"

#define INTVL_SCALE             1
#define MIN_DELAY_US            100

#define REG_IRQ_EN              0x000
#define REG_IRQ_SET             0x004
#define REG_IRQ_CLR             0x008
#define REG_IRQ_STAT            0x00c
#define  REG_IRQ_STAT_TMR       (1 << 0)
#define REG_TMR_CTRL            0x100
#define  REG_TMR_CTRL_START     (1 << 0)
#define  REG_TMR_CTRL_PERIODIC  (1 << 1)
#define  REG_TMR_CTRL_IRQEN     (1 << 2)
#define  REG_TMR_CTRL_DMAEN     (1 << 3)
#define REG_TMR_INTVL           0x104
#define REG_TMR_COUNT           0x108
#define REG_DMA_CTRL            0x200
#define  REG_DMA_CTRL_ENABLE    (1 << 0)
#define  REG_DMA_CTRL_DIR_M2P   (1 << 1)
#define REG_DMA_ADDR            0x204
#define REG_DMA_BLKSZ           0x208
#define REG_DMA_BLKNUM          0x20c
#define REG_DMA_BLKIDX          0x210
#define REG_EXT_CTRL           0x300
#define  REG_EXT_CTRL_ENABLE   (1 << 0)
#define REG_EXT_CHANS          0x304
#define REG_EXT_SMPLBITS       0x308
#define REG_EXT_SMPLRATE       0x30c
#define REG_OFFSET_INDEX(reg)		((reg & 0xF) >> 2)

#define NUM_VSI 2
static struct {
    uint32_t irq_en, irq_stat;
    uint32_t tmr_ctrl, tmr_intvl, tmr_count;
    uint32_t dma_ctrl, dma_addr, dma_blksz, dma_blknum, dma_blkidx;
    uint32_t ext_ctrl, ext_chans, ext_smplbits, ext_smplrate;
    uint64_t start_us;
    FILE *fp;
    unsigned fpdir;
    PyObject *pName, *pModule, *pDict, *pValue;

} vsi[NUM_VSI];
static uint64_t now_us;


static int vsi_read(void *priv, uint64_t addr, size_t len, void *buf, unsigned flags)
{
    unsigned idx;

    idx = (addr >> 16) & (NUM_VSI - 1);
    addr &= 0xFFFF;
    switch(addr) {
    case REG_IRQ_EN:
        *(uint32_t *)buf = vsi[idx].irq_en;
        break;
    case REG_IRQ_STAT:
        *(uint32_t *)buf = vsi[idx].irq_stat;
        break;
    case REG_TMR_CTRL:
        *(uint32_t *)buf = vsi[idx].tmr_ctrl;
        break;
    case REG_TMR_INTVL:
        *(uint32_t *)buf = vsi[idx].tmr_intvl;
        break;
    case REG_TMR_COUNT:
        *(uint32_t *)buf = vsi[idx].tmr_count;
        break;
    case REG_DMA_CTRL:
        *(uint32_t *)buf = vsi[idx].dma_ctrl;
        call_python_func("rdRegs", REG_OFFSET_INDEX(addr), 99);
        break;
    case REG_DMA_ADDR:
        *(uint32_t *)buf = vsi[idx].dma_addr;
        call_python_func("rdRegs", REG_OFFSET_INDEX(addr), 99);
        break;
    case REG_DMA_BLKSZ:
        *(uint32_t *)buf = vsi[idx].dma_blksz;
        call_python_func("rdRegs",REG_OFFSET_INDEX(addr), 99);
        break;
    case REG_DMA_BLKNUM:
        *(uint32_t *)buf = vsi[idx].dma_blknum;
        break;
    case REG_DMA_BLKIDX:
        *(uint32_t *)buf = vsi[idx].dma_blkidx;
        break;
    case REG_EXT_CTRL:
        *(uint32_t *)buf = vsi[idx].ext_ctrl;
        call_python_func("rdRegs",REG_OFFSET_INDEX(addr), 99);
        break;
    case REG_EXT_CHANS:
        *(uint32_t *)buf = vsi[idx].ext_chans;
        call_python_func("rdRegs", REG_OFFSET_INDEX(addr), 99);
        break;
    case REG_EXT_SMPLBITS:
        *(uint32_t *)buf = vsi[idx].ext_smplbits;
        call_python_func("rdRegs", REG_OFFSET_INDEX(addr), 99);
        break;
    case REG_EXT_SMPLRATE:
        *(uint32_t *)buf = vsi[idx].ext_smplrate;
        call_python_func("rdRegs", REG_OFFSET_INDEX(addr), 99);
        break;
    }
    
    printf("read  %d.%03x %d %x %08x\n", idx, (unsigned)addr, (unsigned)len, flags, *(uint32_t *)buf);
    return 0;
}

static void vsi_update_irq(unsigned idx)
{
    unsigned irql = !!(vsi[idx].irq_en & vsi[idx].irq_stat);
    coreio_irq_update(0, idx, irql);
}

static int vsi_write(void *priv, uint64_t addr, size_t len, void *buf, unsigned flags)
{
    unsigned idx;
    uint32_t val = *(uint32_t *)buf;

    idx = (addr >> 16) & (NUM_VSI - 1);
    addr &= 0xFFFF;

    printf("WRITE %d.%03x %d %x %08x\n", idx, (unsigned)addr, (unsigned)len, flags, val);

    switch(addr) {
    case REG_IRQ_EN:
        vsi[idx].irq_en = val;
        vsi_update_irq(idx);
	call_python_func("wrIRQ", val, 99);
        break;
    case REG_IRQ_SET:
        vsi[idx].irq_stat |= val;
        vsi_update_irq(idx);
        break;
    case REG_IRQ_CLR:
        vsi[idx].irq_stat &= ~val;
        vsi_update_irq(idx);
	if(val == 1) {
		call_python_func("timerEvent", 99, 99);
		call_python_func("rdDataDMA", 20, 99);
	}
        break;
    case REG_TMR_CTRL:
        if(val & ~vsi[idx].tmr_ctrl & REG_TMR_CTRL_START) {
            vsi[idx].start_us = now_us;
            vsi[idx].tmr_count = 0;
        }
        vsi[idx].tmr_ctrl = val;
        call_python_func("wrTimer",REG_OFFSET_INDEX(addr), val);
        break;
    case REG_TMR_INTVL:
        vsi[idx].tmr_intvl = val;
        call_python_func("wrTimer", REG_OFFSET_INDEX(addr), val);
        break;
    case REG_DMA_CTRL:
        if(val & ~vsi[idx].dma_ctrl & REG_DMA_CTRL_ENABLE)
            vsi[idx].dma_blkidx = 0;
        vsi[idx].dma_ctrl = val;
        call_python_func("wrDMA", REG_OFFSET_INDEX(addr), val);
        break;
    case REG_DMA_ADDR:
        vsi[idx].dma_addr = val;
        call_python_func("wrDMA",REG_OFFSET_INDEX(addr), val);
        break;
    case REG_DMA_BLKSZ:
        vsi[idx].dma_blksz = val;
        call_python_func("wrDMA",REG_OFFSET_INDEX(addr), val);
        break;
    case REG_DMA_BLKNUM:
        vsi[idx].dma_blknum = val;
        call_python_func("wrDMA",REG_OFFSET_INDEX(addr), val);
        break;
    case REG_EXT_CTRL:
        vsi[idx].ext_ctrl = val;
	call_python_func("wrRegs", REG_OFFSET_INDEX(addr), val);
        break;
    case REG_EXT_CHANS:
        vsi[idx].ext_chans = val;
	call_python_func("wrRegs", REG_OFFSET_INDEX(addr), val);
        break;
    case REG_EXT_SMPLBITS:
        vsi[idx].ext_smplbits = val;
	call_python_func("wrRegs", REG_OFFSET_INDEX(addr), val);
        break;
    case REG_EXT_SMPLRATE:
        vsi[idx].ext_smplrate = val;
	call_python_func("wrRegs", REG_OFFSET_INDEX(addr), val);
        break;
    }

    return 0;
}

int call_python_init()
{
   // Set PYTHONPATH TO working directory
   setenv("PYTHONPATH",".",1);

   // Initialize the Python Interpreter
   Py_Initialize();

   // Build the name object
   vsi[0].pName = PyUnicode_FromString((char*)"arm_vsi0");

   // Load the module object
   vsi[0].pModule = PyImport_Import(vsi[0].pName);


   // pDict is a borrowed reference
   vsi[0].pDict = PyModule_GetDict(vsi[0].pModule);

   return 0;
}

//pass function API name as arguement
int call_python_func(char *funcname,int arg1, int arg2)
{
   PyObject  *pFunc, *pValue, *presult;
   // pFunc is also a borrowed reference 
   pFunc = PyDict_GetItemString(vsi[0].pDict, funcname);

   if (PyCallable_Check(pFunc))
   {
	   if(arg1 != 99)
	   {
		   if(arg2 != 99)
			   pValue=Py_BuildValue("(i : i)",arg1, arg2);
		   else
			   pValue=Py_BuildValue("(i)",arg1);
       		   PyErr_Print();
       		   presult=PyObject_CallObject(pFunc,pValue);
	   }
	   else
	   {
		   pValue=Py_BuildValue("(i)",NULL);
       		   PyErr_Print();
       		   presult=PyObject_CallObject(pFunc,NULL);
	   }
	 
       PyErr_Print();
   } else
   {
       PyErr_Print();
   }

    return 0;
}

int call_python_cleanup()
{
	Py_DECREF(vsi[0].pValue);
	
	// Clean up
  	Py_DECREF(vsi[0].pModule);
   	Py_DECREF(vsi[0].pName);
   
   	// Finish the Python Interpreter
   	Py_FinalizeEx();
	return 0;
}

static const coreio_func_t vsi_func = {
    .read = vsi_read,
    .write = vsi_write,
};

static void vsi_timer_tick(unsigned idx)
{
    void *buf;
    int res;

    if(vsi[idx].tmr_ctrl & REG_TMR_CTRL_PERIODIC)
        vsi[idx].start_us = now_us;
    else
        vsi[idx].tmr_ctrl &= ~REG_TMR_CTRL_START;

    vsi[idx].tmr_count ++;

    if((vsi[idx].tmr_ctrl & REG_TMR_CTRL_DMAEN) && (vsi[idx].dma_ctrl && REG_DMA_CTRL_ENABLE)) {
        if(vsi[idx].dma_ctrl & REG_DMA_CTRL_DIR_M2P) {
            if(vsi[idx].fp && vsi[idx].fpdir) {
                buf = calloc(1, vsi[idx].dma_blksz);
                if(buf) {
                    res = coreio_dma_read(0, 0, vsi[idx].dma_addr + vsi[idx].dma_blksz * vsi[idx].dma_blkidx, vsi[idx].dma_blksz, buf, 0, NULL, NULL);
                    if(res)
                        fprintf(stderr, "[vsi-waveio] DMA read of 0x%08x+%d returned %d.\n", vsi[idx].dma_addr + vsi[idx].dma_blksz * vsi[idx].dma_blkidx, vsi[idx].dma_blksz, res);
                    fwrite(buf, 1, vsi[idx].dma_blksz, vsi[idx].fp);
                    fflush(vsi[idx].fp);
                    free(buf);
                }
            }
        } else {
            buf = calloc(1, vsi[idx].dma_blksz);
            if(buf) {
                if(vsi[idx].fp && !vsi[idx].fpdir) {
                    if(fread(buf, 1, vsi[idx].dma_blksz, vsi[idx].fp) < vsi[idx].dma_blksz) {
                        fprintf(stderr, "[vsi-waveio] Microphone file complete.\n");
                        fclose(vsi[idx].fp);
                        vsi[idx].fp = NULL;
                    }
                }
                res = coreio_dma_write(0, 0, vsi[idx].dma_addr + vsi[idx].dma_blksz * vsi[idx].dma_blkidx, vsi[idx].dma_blksz, buf, 0, NULL, NULL);
                if(res)
                    fprintf(stderr, "[vsi-waveio] DMA write of 0x%08x+%d returned %d.\n", vsi[idx].dma_addr + vsi[idx].dma_blksz * vsi[idx].dma_blkidx, vsi[idx].dma_blksz, res);
                free(buf);
            }
        }
        vsi[idx].dma_blkidx ++;
        if(vsi[idx].dma_blkidx >= vsi[idx].dma_blknum)
            vsi[idx].dma_blkidx = 0;
    }

    if(vsi[idx].tmr_ctrl & REG_TMR_CTRL_IRQEN) {
        vsi[idx].irq_stat |= REG_IRQ_STAT_TMR;
        vsi_update_irq(idx);
    }
}

static uint64_t vsi_get_microtime(void)
{
    struct timespec tsp;
    clock_gettime(CLOCK_MONOTONIC, &tsp);
    return tsp.tv_sec * 1000000ul + (tsp.tv_nsec / 1000ul);
}

int main(int argc, char *argv[])
{
    uint64_t next_us;
    fd_set readfds, writefds;
    struct timeval tv = { 0, 0 };
    unsigned idx;
    int nfds, res;
    
    if(argc != 3) {
        fprintf(stderr, "usage: vsi-waveio <host:port> <microphone.wav>\n");
        return 1;
    }
    
    call_python_init();
    call_python_func("init", 99, 99);
    
    vsi[0].fp = fopen(argv[2], "rb");
    if(!vsi[0].fp) {
        fprintf(stderr, "[vsi-waveio] Microphone input file could not be opened.\n");
        return 1;
    }
    fseek(vsi[0].fp, 0x2C, SEEK_SET); /* skip WAV header */

    if(coreio_connect(argv[1]))
        return 1;

    coreio_register(0, &vsi_func, NULL);

    fprintf(stderr, "[vsi-waveio] Ready.\n");

    while(1) {
        now_us = vsi_get_microtime();
        next_us = -1ul;
        for(idx=0; idx<NUM_VSI; idx++)
            if((vsi[idx].tmr_ctrl & REG_TMR_CTRL_START) && vsi[idx].start_us + (uint64_t)vsi[idx].tmr_intvl * INTVL_SCALE < next_us)
                next_us = vsi[idx].start_us + vsi[idx].tmr_intvl * INTVL_SCALE;
        if(next_us < -1ul) {
            if(next_us < now_us + MIN_DELAY_US)
                next_us = MIN_DELAY_US;
            else
                next_us -= now_us;
            tv.tv_sec = next_us / 1000000ull;
            tv.tv_usec = next_us % 1000000ull;
        }
        FD_ZERO(&writefds);
        FD_ZERO(&readfds);
        nfds = coreio_preparefds(0, &readfds, &writefds);
        if(nfds < 0)
            break;
        select(nfds, &readfds, &writefds, NULL, next_us < -1ul ? &tv : NULL);
        now_us = vsi_get_microtime();
        res = coreio_processfds(&readfds, &writefds);
        if(res)
            break;
        for(idx=0; idx<NUM_VSI; idx++) {
		
            if((vsi[idx].tmr_ctrl & REG_TMR_CTRL_START) && vsi[idx].start_us + (uint64_t)vsi[idx].tmr_intvl * INTVL_SCALE < now_us)
                vsi_timer_tick(idx);
	}
    }
    
    call_python_cleanup();
    coreio_disconnect();

    return 0;
}
