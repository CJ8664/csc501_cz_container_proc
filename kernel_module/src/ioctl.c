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

// Mutex for performing any updates on pid_cid_list
static DEFINE_MUTEX(pid_cid_list_lock);

// Node that stores PID
struct pid_node {
        int pid;
        struct list_head list;
};

struct cid_node {
  __u64 cid;
  struct pid_node *running_pids;
  struct list_head list;
};

// Actual list that stores the CIDS and in it corresponding PIDS
struct cid_node *cid_list;

// List size
__u64 total_cids = 0;

// Function to add PID-CID mapping
void add_pid_cid_mapping(int pid, __u64 cid) {
        mutex_lock(&pid_cid_list_lock);
        if(total_cids == 0) {

          // Initalize the First ever Container with CID
          // Temp CID node
          cid_list = (struct cid_node *)kmalloc(sizeof(struct cid_node), GFP_KERNEL);
          cid_list->cid = cid;

          // Init head for PID list within the
          cid_list->running_pids = (struct pid_node *)kmalloc(sizeof(struct pid_node), GFP_KERNEL);
          // Temp PID node
          cid_list->running_pids->pid = pid;
          total_cids++;
          INIT_LIST_HEAD(&(cid_list->running_pids)->list);
          INIT_LIST_HEAD(&cid_list->list);
          printk("Created new CID: %llu and PID: %d", cid_list->cid, cid_list->running_pids->pid);
        } else {
          printk("Else\n");
          struct cid_node *temp_cid_node;

          int cid_node_exists = 0;

          list_for_each_entry(temp_cid_node, &(cid_list->list), list) {
            printk("Iterating over CID: %llu \n", temp_cid_node->cid);
            if(temp_cid_node->cid == cid) {

              // Add PID node to existing Container
              struct pid_node *temp_pid_node = (struct pid_node *)kmalloc(sizeof(struct pid_node), GFP_KERNEL);
              temp_pid_node->pid = pid;
              // Adding to Internal PID list
              list_add_tail(&(temp_pid_node->list), &(temp_cid_node->running_pids->list));
              cid_node_exists = 1;
              break;
            }
          }
          if(!cid_node_exists) {

            // Temp CID node
            struct cid_node *temp_cid_node = (struct cid_node *)kmalloc(sizeof(struct cid_node), GFP_KERNEL);
            temp_cid_node->cid = cid;

            // Init head for PID list within the
            temp_cid_node->running_pids = (struct pid_node *)kmalloc(sizeof(struct pid_node), GFP_KERNEL);
            // Temp PID node
            temp_cid_node->running_pids->pid = pid;

            INIT_LIST_HEAD(&(cid_list->running_pids)->list);

            // Addting to Main CID list
            list_add_tail(&(temp_cid_node->list), &(cid_list->list));
            total_cids++;
          }

        }
        mutex_unlock(&pid_cid_list_lock);
}

// Function to remove PID-CID mapping
void remove_pid_cid_mapping(int pid, __u64 cid) {

        mutex_lock(&pid_cid_list_lock);

        mutex_unlock(&pid_cid_list_lock);
}

__u64 get_cid_for_pid(int pid){

        int idx;
        __u64 cid = -1;
        printk("In get_cid_for_pid %d", total_cids);
        mutex_lock(&pid_cid_list_lock);

        struct cid_node *temp_cid_node;
        list_for_each_entry(temp_cid_node, &cid_list->list, list) {
          printk("CID %llu: ", temp_cid_node->cid);

          struct pid_node *temp_pid_node;
          list_for_each_entry(temp_pid_node, &(temp_cid_node->running_pids)->list, list) {
            printk("PID %d: ", temp_pid_node->pid);
            if(temp_pid_node->pid == pid){
              printk("CID %llu for PID %d: ", temp_cid_node->cid, pid);
            }
          }
          printk("\n");
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

        mutex_unlock(&pid_cid_list_lock);
        return next_pid;
}

void assign_pid_to_cid(int pid, __u64 cid){

        int idx;
        mutex_lock(&pid_cid_list_lock);

        mutex_unlock(&pid_cid_list_lock);
}

int is_container_available(int pid, __u64 cid) {

        int idx;
        int available = 1;
        int pid_assigned_to = -1;
        mutex_lock(&pid_cid_list_lock);

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
        return 0;
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

        // Schedule current process and wake up next process
        assign_pid_to_cid(next_pid, user_cmd_kernal->cid);
        wake_up_process(next_task);
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
        return 0;

        if(is_container_available(current->pid, user_cmd_kernal->cid)) {
                assign_pid_to_cid(current->pid, user_cmd_kernal->cid);
        } else {
                // The container is occupied by some other processes
                set_current_state(TASK_UNINTERRUPTIBLE);
                schedule();
        }
        kfree(user_cmd_kernal);
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
        return 0;
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

        // Schedule current process and wake up next process
        set_current_state(TASK_UNINTERRUPTIBLE);
        assign_pid_to_cid(next_pid, cid);
        wake_up_process(next_task);
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
