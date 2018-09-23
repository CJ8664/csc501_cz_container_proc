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
#include <linux/list.h>

#include <linux/pid_namespace.h>

// // Lock for create entry point STEP 1
// static DEFINE_MUTEX(create_lock);
//
// // Lock for container init STEP 2
// static DEFINE_MUTEX(container_lock);
//
// // Lock for p_id_to_c_id
// static DEFINE_MUTEX(p_id_to_c_id_lock);
//
// // Lock for c_id_running_p_id
// static DEFINE_MUTEX(c_id_running_p_id_lock);
//
// // Array that stores the mapping pid -> cid
// long long unsigned *p_id_to_c_id;
//
// // Array that stores current process running in cid
// long long unsigned *c_id_running_p_id;
//
// // Total number of processes currently in system (from all containers)
// int curr_pid_count = 0;
//
// // Total number of containers initialized
// int curr_cid_count = 0;
//
// // Two col (key,val) const
// const int col_size = 2;
//
//
//
// // Util function to get value at arr[row][col]
// int map2Dto1D(int row, int col, int colSize){
//         if(col >= colSize) {
//                 return -1;
//         }
//         return row*colSize + col;
// }
//
// // Util function that gets the cid given a pid
// long long unsigned get_cid_for_pid(long long unsigned pid) {
//         int i;
//         mutex_lock(&p_id_to_c_id_lock);
//         for(i = 0; i < curr_pid_count; i++) {
//                 if(p_id_to_c_id[map2Dto1D(i, 0, col_size)] == pid) {
//                         mutex_unlock(&p_id_to_c_id_lock);
//                         return p_id_to_c_id[map2Dto1D(i, 1, col_size)];
//                 }
//         }
//         mutex_unlock(&p_id_to_c_id_lock);
//         printk("Error: PID Not found for PID %llu \n", pid);
//         return 0;
// }
//
// int is_container_intialized(long long unsigned cid)
// {
//         printk("IN is container\n");
//         int idx;
//         mutex_lock(&c_id_running_p_id_lock);
//         for(idx = 0; idx < curr_cid_count; idx++)
//         {
//                 if(c_id_running_p_id[map2Dto1D(idx, 0, col_size)] == cid)
//                 {
//                         mutex_unlock(&c_id_running_p_id_lock);
//                         return 1;
//                 }
//         }
//         mutex_unlock(&c_id_running_p_id_lock);
//         return 0;
// }
//
// long long unsigned get_next_pid(long long unsigned curr_pid) {
//         int i;
//         int index = -1;
//         long long unsigned curr_cid = 0;
//         mutex_lock(&p_id_to_c_id_lock);
//         for(i = 0; i < curr_pid_count; i++) {
//                 if(curr_pid == p_id_to_c_id[map2Dto1D(i,0,col_size)]) {
//                         index = i;
//                         curr_cid = p_id_to_c_id[map2Dto1D(i,1,col_size)];
//                         break;
//                 }
//         }
//         //printk("CID: %llu, INDEX: %d", curr_cid, index);
//         if(index != -1) {
//                 index = (index+1) % curr_pid_count;
//                 while(curr_cid != p_id_to_c_id[map2Dto1D(index,1,col_size)]) {
//                         index = (index+1) % curr_pid_count;
//                 }
//                 // returning next pid
//                 mutex_unlock(&p_id_to_c_id_lock);
//                 return p_id_to_c_id[map2Dto1D(index,0,col_size)];
//         } else {
//                 printk("PID not found\n");
//                 mutex_unlock(&p_id_to_c_id_lock);
//                 return 0;
//         }
// }
//
//
// // Update the PID for the CID with paramerters passed
// void update_pid_for_cid(long long unsigned cid, long long unsigned next_pid)
// {
//         int idx;
//         printk("update_pid_for_cid trying to acquire lock\n");
//         mutex_lock(&c_id_running_p_id_lock);
//         printk("update_pid_for_cid acquireD lock\n");
//         for(idx = 0; idx < curr_cid_count; idx++)
//         {
//                 if(c_id_running_p_id[map2Dto1D(idx, 0, col_size)] == cid)
//                 {
//                         c_id_running_p_id[map2Dto1D(idx, 1, col_size)] = next_pid;
//                         printk("Updated CID: %llu with New PID: %llu\n", cid, next_pid);
//                         break;
//                 }
//         }
//         mutex_unlock(&c_id_running_p_id_lock);
//         printk("update_pid_for_cid gave up  lock\n");
// }

// Mutex for performing any updates on pid_cid_list
static DEFINE_MUTEX(pid_cid_list_lock);

// Node of the list
typedef struct {
        int pid;
        __u64 cid;
        int is_valid;
        int cid_has_pid;
} pid_cid_map;

// List size
__u64 total_pid = 0;

// Actual list
pid_cid_map *pid_cid_map_list;

// Function to add PID-CID mapping
void add_pid_cid_mapping(int pid, __u64 cid) {
        mutex_lock(&pid_cid_list_lock);
        total_pid++;
        pid_cid_map_list = krealloc(pid_cid_map_list, total_pid * sizeof(pid_cid_map), GFP_KERNEL);
        pid_cid_map_list[total_pid - 1].pid = pid;
        pid_cid_map_list[total_pid - 1].cid = cid;
        pid_cid_map_list[total_pid - 1].is_valid = 1;
        pid_cid_map_list[total_pid - 1].cid_has_pid = 0;
        mutex_unlock(&pid_cid_list_lock);
}

// Function to remove PID-CID mapping
void remove_pid_cid_mapping(int pid, __u64 cid) {

        int idx;
        mutex_lock(&pid_cid_list_lock);
        for(idx = 0; idx < total_pid; idx++) {
                if(pid_cid_map_list[idx].pid == pid) {
                        pid_cid_map_list[idx].is_valid = 0;
                        break;
                }
        }
        mutex_unlock(&pid_cid_list_lock);
}

__u64 get_cid_for_pid(int pid){

        int idx;
        __u64 cid = -1;
        mutex_lock(&pid_cid_list_lock);
        for(idx = 0; idx < total_pid; idx++) {
                if(pid_cid_map_list[idx].pid == pid) {
                        cid = pid_cid_map_list[idx].cid;
                        break;
                }
        }
        if(cid == -1) {
                printk("PID not available");
                cid = 0;
        }
        mutex_unlock(&pid_cid_list_lock);
        return cid;
}

// Function to get next PID for a given PID
int get_next_pid(int pid) {

        int idx = -1;
        __u64 cid = -1;
        int next_pid = -1;
        mutex_lock(&pid_cid_list_lock);
        for(idx = 0; idx < total_pid; idx++) {
                if(pid_cid_map_list[idx].pid == pid) {
                        cid = pid_cid_map_list[idx].cid;
                        break;
                }
        }
        if(idx != -1) {
                idx = (idx+1) % total_pid;
                while(cid != pid_cid_map_list[idx].cid || !pid_cid_map_list[idx].is_valid) {
                        idx = (idx+1) % total_pid;
                }
                next_pid = pid_cid_map_list[idx].pid;
        } else {
                printk("PID not found\n");
        }
        mutex_unlock(&pid_cid_list_lock);
        return next_pid;
}

void assign_pid_to_cid(int pid, __u64 cid){

        int idx;
        mutex_lock(&pid_cid_list_lock);
        for(idx = 0; idx < total_pid; idx++) {
                if(pid_cid_map_list[idx].cid == cid &&
                   pid_cid_map_list[idx].is_valid &&
                   pid_cid_map_list[idx].cid_has_pid) {
                        pid_cid_map_list[idx].cid_has_pid = 0;
                        break;
                }
        }
        // Assign container to given PID
        for(idx = 0; idx < total_pid; idx++) {
                if(pid_cid_map_list[idx].cid == cid &&
                   pid_cid_map_list[idx].pid == pid &&
                   pid_cid_map_list[idx].is_valid) {
                        pid_cid_map_list[idx].cid_has_pid = 1;
                        break;
                }
        }
        mutex_unlock(&pid_cid_list_lock);
}

int is_container_available(int pid, __u64 cid) {

        int idx;
        int available = 1;
        int pid_assigned_to = -1;
        mutex_lock(&pid_cid_list_lock);
        for(idx = 0; idx < total_pid; idx++) {
                if(pid_cid_map_list[idx].cid == cid &&
                   pid_cid_map_list[idx].is_valid &&
                   pid_cid_map_list[idx].cid_has_pid) {
                        pid_assigned_to = pid_cid_map_list[idx].pid;
                        available = 0;
                }
        }
        if(!available && pid_assigned_to == pid) {
                available = 1;
        }
        mutex_unlock(&pid_cid_list_lock);
        return available;
}

/**
 * Delete the task in the container.
 *
 * external functions needed:
 * mutex_lock(), mutex_unlock(), wake_up_process(),
 */
int processor_container_delete(struct processor_container_cmd __user *user_cmd)
{
        // Get the current PID and CID
        struct processor_container_cmd *user_cmd_kernal;
        int next_pid = get_next_pid(current->pid);

        // To get the task_struct for next pid
        struct pid *pid_struct;
        struct task_struct *next_task;

        user_cmd_kernal = kmalloc(sizeof(struct processor_container_cmd), GFP_KERNEL);
        copy_from_user(user_cmd_kernal, (void *)user_cmd, sizeof(struct processor_container_cmd));

        // Display the current PID and CID
        printk("Calling DELETE PID: %d CID: %llu\n", current->pid, user_cmd_kernal->cid);

        // Remove PID-CID mapping
        remove_pid_cid_mapping(current->pid, user_cmd_kernal->cid);

        // Get task struct for next pid
        pid_struct = find_get_pid(next_pid);
        next_task = pid_task(pid_struct, PIDTYPE_PID);

        // // Schedule current process and wake up next process
        // assign_pid_to_cid(next_pid, user_cmd_kernal->cid);
        // wake_up_process(task);
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
        // Get the current PID and CID
        struct processor_container_cmd *user_cmd_kernal;
        user_cmd_kernal = kmalloc(sizeof(struct processor_container_cmd), GFP_KERNEL);
        copy_from_user(user_cmd_kernal, (void *)user_cmd, sizeof(struct processor_container_cmd));

        // Display the current PID and CID
        printk("Calling CREATE PID: %d CID: %llu\n", current->pid, user_cmd_kernal->cid);

        // Add the PID-CID to mapping
        add_pid_cid_mapping(current->pid, user_cmd_kernal->cid);

        // if(is_container_available(current->pid, user_cmd_kernal->cid)) {
        //         assign_pid_to_cid(current->pid, user_cmd_kernal->cid);
        // } else {
        //         // The container is occupied by some other processes
        //         set_current_state(TASK_UNINTERRUPTIBLE);
        //         schedule();
        // }
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
        // Get the current PID, CID and next PID
        __u64 cid = get_cid_for_pid(current->pid);
        int next_pid = get_next_pid(current->pid);

        // To get the task_struct for next pid
        struct pid *pid_struct;
        struct task_struct *next_task;

        // Display the current PID and CID
        printk("Calling SWITCH PID: %d CID: %llu\n", current->pid, cid);
        printk("Next PID: %d\n", next_pid);

        // Get task struct for next pid
        pid_struct = find_get_pid(next_pid);
        next_task = pid_task(pid_struct, PIDTYPE_PID);

        // // Schedule current process and wake up next process
        // set_current_state(TASK_UNINTERRUPTIBLE);
        // assign_pid_to_cid(next_pid, curr_cid);
        // wake_up_process(task);
        // schedule();

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
