//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2016
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for Processor Container
//
////////////////////////////////////////////////////////////////////////

#include "processor_container.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>

// Lock for create entry point STEP 1
static DEFINE_MUTEX(create_lock);

// Lock for container init STEP 2
static DEFINE_MUTEX(container_lock);

// Array that stores the mapping pid -> cid
long long unsigned *p_id_to_c_id;

// Array that stores current process running in cid
long long unsigned *c_id_running_p_id;

// Total number of processes currently in system (from all containers) 
int curr_pid_count = 0;

// Total number of containers initialized
int curr_cid_count = 0;

// Two col (key,val) const
const int col_size = 2;

// Util function to get value at arr[row][col]
int map2Dto1D(int row, int col, int colSize){
    if(col >= colSize){
        return -1;
    }
    return row*colSize + col;
}

// Util function that gets the cid given a pid
long long unsigned get_cid_for_pid(long long unsigned pid) {
    int i;
    for(i = 0; i < curr_pid_count; i++) {
	if(p_id_to_c_id[map2Dto1D(i, 0, col_size)] == pid) {
	    return p_id_to_c_id[map2Dto1D(i, 1, col_size)];
	}
    }
    printk("Error: PID Not found for PID %llu \n", pid);
    return 0;
}

int is_container_intialized(long long unsigned cid)
{
    int idx = 0;
    for(idx; idx < curr_cid_count; idx++)
    {
    	if(c_id_running_p_id[map2Dto1D(idx, 0, col_size)] == cid)
        {
	    return 1;
	}
    }
    return 0;
}

/**
 * Delete the task in the container.
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), 
 */
int processor_container_delete(struct processor_container_cmd __user *user_cmd)
{
    return 0;
}

/**
 * Create a task in the corresponding container.
 * external functions needed:
 * copy_from_user(), mutex_lock(), mutex_unlock(), set_current_state(), schedule()
 * 
 * external variables needed:
 * struct task_struct* current  
 */
int processor_container_create(struct processor_container_cmd __user *user_cmd)
{
    // STEP 1
    printk("Acquiring lock Step 1\n");
    mutex_lock(&create_lock);
    struct processor_container_cmd *user_cmd_kernal;
    curr_pid_count += 1;
    // printk("Current PID count %d", curr_pid_count);
    p_id_to_c_id = krealloc(p_id_to_c_id, curr_pid_count * 2 * sizeof(long long unsigned), GFP_KERNEL);

    user_cmd_kernal = kmalloc(sizeof(struct processor_container_cmd), GFP_KERNEL);
    copy_from_user(user_cmd_kernal, (void *)user_cmd, sizeof(struct processor_container_cmd));
    printk("\nCID value: %llu\n", user_cmd_kernal->cid);
    printk("\nPID val: %d\n", current->pid);
    p_id_to_c_id[map2Dto1D(curr_pid_count-1, 0, col_size)] = current->pid;
    p_id_to_c_id[map2Dto1D(curr_pid_count-1, 1, col_size)] = user_cmd_kernal->cid;
    printk("\nStored PID in 0,0: %llu\n", p_id_to_c_id[map2Dto1D(curr_pid_count - 1, 0, col_size)]);
    printk("\nStored CID in 0,1: %llu\n", p_id_to_c_id[map2Dto1D(curr_pid_count - 1, 1, col_size)]);
    mutex_unlock(&create_lock);
    printk("Lock Released step 1\n");


    // STEP 2
    printk("acquiring lock step 2\n");
    mutex_lock(&container_lock);
    if(curr_cid_count == 0 || !is_container_intialized(user_cmd_kernal->cid))
    {
        c_id_running_p_id = krealloc(c_id_running_p_id, curr_cid_count * 2 * sizeof(long long unsigned), GFP_KERNEL);
        curr_cid_count++;
        c_id_running_p_id[map2Dto1D(curr_cid_count - 1, 0, col_size)] = user_cmd_kernal->cid;
        c_id_running_p_id[map2Dto1D(curr_cid_count - 1, 1, col_size)] = current->pid;  
        mutex_unlock(&container_lock);
        printk("Lock released step 2\n");
    }    
    else
    {   
        // release contanier lock
        mutex_unlock(&container_lock);
        printk("Lock released step 2\n");
        // sleep
    }
    return 0;
}

/**
 * switch to the next task in the next container
 * 
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(), set_current_state(), schedule()
 */
int processor_container_switch(struct processor_container_cmd __user *user_cmd)
{
    long long unsigned current_pid = current->pid;
    printk("\nFound for PID: %llu CID: %llu \n", current_pid,  get_cid_for_pid(current_pid));
    return 0;
}

/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */
int processor_container_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg)
{
    switch (cmd)
    {
    case PCONTAINER_IOCTL_CSWITCH:
        return processor_container_switch((void __user *)arg);
    case PCONTAINER_IOCTL_CREATE:
        return processor_container_create((void __user *)arg);
    case PCONTAINER_IOCTL_DELETE:
        return processor_container_delete((void __user *)arg);
    default:
        return -ENOTTY;
    }
}
