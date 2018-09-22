//////////////////////////////////////////////////////////////////////
//              North Carolina State University
//
//
//
//                 Copyright 2016
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

#include <linux/pid_namespace.h>

// Lock for create entry point STEP 1
static DEFINE_MUTEX(create_lock);

// Lock for container init STEP 2
static DEFINE_MUTEX(container_lock);

// Lock for p_id_to_c_id
static DEFINE_MUTEX(p_id_to_c_id_lock);

// Lock for c_id_running_p_id
static DEFINE_MUTEX(c_id_running_p_id_lock);

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
        if(col >= colSize) {
                return -1;
        }
        return row*colSize + col;
}

// Util function that gets the cid given a pid
long long unsigned get_cid_for_pid(long long unsigned pid) {
        int i;
        mutex_lock(&p_id_to_c_id_lock);
        for(i = 0; i < curr_pid_count; i++) {
                if(p_id_to_c_id[map2Dto1D(i, 0, col_size)] == pid) {
                        mutex_unlock(&p_id_to_c_id_lock);
                        return p_id_to_c_id[map2Dto1D(i, 1, col_size)];
                }
        }
        mutex_unlock(&p_id_to_c_id_lock);
        printk("Error: PID Not found for PID %llu \n", pid);
        return 0;
}

int is_container_intialized(long long unsigned cid)
{
        printk("IN is container\n");
        int idx;
        mutex_lock(&c_id_running_p_id_lock);
        for(idx = 0; idx < curr_cid_count; idx++)
        {
                if(c_id_running_p_id[map2Dto1D(idx, 0, col_size)] == cid)
                {
                        mutex_unlock(&c_id_running_p_id_lock);
                        return 1;
                }
        }
        mutex_unlock(&c_id_running_p_id_lock);
        return 0;
}

long long unsigned get_next_pid(long long unsigned curr_pid) {
        int i;
        int index = -1;
        long long unsigned curr_cid = 0;
        mutex_lock(&p_id_to_c_id_lock);
        for(i = 0; i < curr_pid_count; i++) {
                if(curr_pid == p_id_to_c_id[map2Dto1D(i,0,col_size)]) {
                        index = i;
                        curr_cid = p_id_to_c_id[map2Dto1D(i,1,col_size)];
                        break;
                }
        }
        //printk("CID: %llu, INDEX: %d", curr_cid, index);
        if(index != -1) {
                index = (index+1) % curr_pid_count;
                while(curr_cid != p_id_to_c_id[map2Dto1D(index,1,col_size)]) {
                        index = (index+1) % curr_pid_count;
                }
                // returning next pid
                mutex_unlock(&p_id_to_c_id_lock);
                return p_id_to_c_id[map2Dto1D(index,0,col_size)];
        } else {
                printk("PID not found\n");
                mutex_unlock(&p_id_to_c_id_lock);
                return 0;
        }
}


// Update the PID for the CID with paramerters passed
void update_pid_for_cid(long long unsigned cid, long long unsigned next_pid)
{
        int idx;
	printk("update_pid_for_cid trying to acquire lock\n");
        mutex_lock(&c_id_running_p_id_lock);
	printk("update_pid_for_cid acquireD lock\n");
        for(idx = 0; idx < curr_cid_count; idx++)
        {
                if(c_id_running_p_id[map2Dto1D(idx, 0, col_size)] == cid)
                {
                        c_id_running_p_id[map2Dto1D(idx, 1, col_size)] = next_pid;
                        printk("Updated CID: %llu with New PID: %llu\n", cid, next_pid);
                        break;
                }
        }
        mutex_unlock(&c_id_running_p_id_lock);
	printk("update_pid_for_cid gave up  lock\n");
}

/**
 * Delete the task in the container.
 *
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(),
 */
int processor_container_delete(struct processor_container_cmd __user *user_cmd)
{
        long long unsigned current_pid = current->pid;
        long long unsigned curr_cid = get_cid_for_pid(current_pid);
        printk("\nin Delete Found for PID: %llu CID: %llu \n", current_pid, curr_cid);

        long long unsigned next_pid = get_next_pid(current_pid);
        printk("Next PID = %lld\n", next_pid);

        // Remove the exiting process from mapping p_id_to_c_id
        mutex_lock(&p_id_to_c_id_lock);
        long long unsigned *temp_p_id_to_c_id = kmalloc((curr_pid_count -1 ) * 2 * sizeof(long long unsigned), GFP_KERNEL);
        int old;
        int new = 0;
        for(old = 0; old < curr_pid_count; old++) {
                if(p_id_to_c_id[map2Dto1D(old, 0, col_size)] == current_pid) {
                        continue;
                }
                temp_p_id_to_c_id[map2Dto1D(new, 0, col_size)] = p_id_to_c_id[map2Dto1D(old, 0, col_size)];
                temp_p_id_to_c_id[map2Dto1D(new, 1, col_size)] = p_id_to_c_id[map2Dto1D(old, 1, col_size)];
        }
        kfree(p_id_to_c_id);
        p_id_to_c_id = temp_p_id_to_c_id;
        curr_pid_count--;
        mutex_unlock(&p_id_to_c_id_lock);

        if(current_pid == next_pid) {
                // Remove entry from mapping c_id_running_p_id
                mutex_lock(&c_id_running_p_id_lock);
                long long unsigned *temp_c_id_running_p_id = kmalloc((curr_cid_count -1 ) * 2 * sizeof(long long unsigned), GFP_KERNEL);
                int old;
                int new = 0;
                for(old = 0; old < curr_cid_count; old++) {
                        if(c_id_running_p_id[map2Dto1D(old, 0, col_size)] == curr_cid) {
                                continue;
                        }
                        temp_c_id_running_p_id[map2Dto1D(new, 0, col_size)] = c_id_running_p_id[map2Dto1D(old, 0, col_size)];
                        temp_c_id_running_p_id[map2Dto1D(new, 1, col_size)] = c_id_running_p_id[map2Dto1D(old, 1, col_size)];
                }

                kfree(c_id_running_p_id);
                c_id_running_p_id = temp_c_id_running_p_id;
                curr_cid_count--;
                mutex_unlock(&c_id_running_p_id_lock);
                return 0;
        }

        // Get task struct for next pid
        struct pid *pid_struct;
        pid_struct = find_get_pid(next_pid);
        struct task_struct *task;
        task = pid_task(pid_struct,PIDTYPE_PID);

        printk("WAking up the process %d", task->pid);
        printk("WAking up the original process %d", next_pid);
        // Schedule current process
        printk("Updating map in del");
        update_pid_for_cid(curr_cid, next_pid);
        printk("Updated map in del");
        wake_up_process(task);
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
        // mutex_lock(&create_lock);
        struct processor_container_cmd *user_cmd_kernal;
        curr_pid_count += 1;
        // printk("Current PID count %d", curr_pid_count);
        mutex_lock(&p_id_to_c_id_lock);
        p_id_to_c_id = krealloc(p_id_to_c_id, curr_pid_count * 2 * sizeof(long long unsigned), GFP_KERNEL);

        user_cmd_kernal = kmalloc(sizeof(struct processor_container_cmd), GFP_KERNEL);
        copy_from_user(user_cmd_kernal, (void *)user_cmd, sizeof(struct processor_container_cmd));
        printk("\nCID value: %llu\n", user_cmd_kernal->cid);
        printk("\nPID val: %d\n", current->pid);
        p_id_to_c_id[map2Dto1D(curr_pid_count-1, 0, col_size)] = current->pid;
        p_id_to_c_id[map2Dto1D(curr_pid_count-1, 1, col_size)] = user_cmd_kernal->cid;
        printk("\nStored PID in 0,0: %llu\n", p_id_to_c_id[map2Dto1D(curr_pid_count - 1, 0, col_size)]);
        printk("\nStored CID in 0,1: %llu\n", p_id_to_c_id[map2Dto1D(curr_pid_count - 1, 1, col_size)]);
        mutex_unlock(&p_id_to_c_id_lock);

        // mutex_unlock(&create_lock);
        printk("Lock Released step 1\n");


        // STEP 2
        printk("acquiring lock step 2\n");
        // mutex_lock(&container_lock);
        printk("Lock acq %llu", current->pid);

        mutex_lock(&c_id_running_p_id_lock);
        if(curr_cid_count == 0)
        {
                printk("Enterd in if\n");
                curr_cid_count++;
                c_id_running_p_id = krealloc(c_id_running_p_id, curr_cid_count * 2 * sizeof(long long unsigned), GFP_KERNEL);
                c_id_running_p_id[map2Dto1D(curr_cid_count - 1, 0, col_size)] = user_cmd_kernal->cid;
                c_id_running_p_id[map2Dto1D(curr_cid_count - 1, 1, col_size)] = current->pid;
                // mutex_unlock(&container_lock);
                mutex_unlock(&c_id_running_p_id_lock);
                printk("Lock released step 2\n");
        } else if(!is_container_intialized(user_cmd_kernal->cid)) {
                printk("Enterd in if\n");
                curr_cid_count++;
                c_id_running_p_id = krealloc(c_id_running_p_id, curr_cid_count * 2 * sizeof(long long unsigned), GFP_KERNEL);
                c_id_running_p_id[map2Dto1D(curr_cid_count - 1, 0, col_size)] = user_cmd_kernal->cid;
                c_id_running_p_id[map2Dto1D(curr_cid_count - 1, 1, col_size)] = current->pid;
                // mutex_unlock(&container_lock);
                mutex_unlock(&c_id_running_p_id_lock);
                printk("Lock released step 2\n");
        }
        else
        {
                // release contanier lock
                // mutex_unlock(&container_lock);
                mutex_unlock(&c_id_running_p_id_lock);
                printk("Lock released step 2\n");
                // sleep
                set_current_state(TASK_UNINTERRUPTIBLE);
                schedule();
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
        long long unsigned curr_cid = get_cid_for_pid(current_pid);
        printk("\nFound for PID: %llu CID: %llu \n", current_pid, curr_cid);

        long long unsigned next_pid = get_next_pid(current_pid);
        printk("Next PID = %lld\n", next_pid);

        // Get task struct for next pid
        struct pid *pid_struct;
        pid_struct = find_get_pid(next_pid);
        struct task_struct *task;
        task = pid_task(pid_struct,PIDTYPE_PID);

        // Schedule current process
        set_current_state(TASK_UNINTERRUPTIBLE);
        printk("Updating map in switch");
        update_pid_for_cid(curr_cid, next_pid);
        printk("Updatid map in switch");
        wake_up_process(task);
        schedule();

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
