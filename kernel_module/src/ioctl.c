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
        struct pid_node *next;
};

struct cid_node {
  __u64 cid;
  struct pid_node *running_pids;
  struct cid_node *next;
};

// Actual list that stores the CIDS and in it corresponding PIDS
struct cid_node *cid_list = NULL;

// List size
__u64 total_cids = 0;

// Linked List functions

void print_ll(void) {
  struct cid_node *temp_cid_node = cid_list;
  while (temp_cid_node != NULL) {
    printk("CID %llu: \n", temp_cid_node->cid);
    struct pid_node *temp_pid_node = temp_cid_node->running_pids;

    while(temp_pid_node != NULL) {
      printk("PID %d: \n", temp_pid_node->pid);
      temp_pid_node = temp_pid_node->next;
    }

    temp_cid_node = temp_cid_node->next;
  }
}

void add_ll(__u64 new_cid, int new_pid) {
  if(cid_list == NULL) {
    cid_list = (struct cid_node *) kmalloc(sizeof(struct cid_node), GFP_KERNEL);
    cid_list->cid = new_cid;
    cid_list->next = NULL;
    cid_list->running_pids = (struct pid_node *)kmalloc(sizeof(struct pid_node), GFP_KERNEL);
    cid_list->running_pids->pid = new_pid;
    cid_list->running_pids->next = NULL;
  } else {
    struct cid_node *prev_cid_node = NULL;
    struct cid_node *temp_cid_node = cid_list;
    int found = 0;
    while (temp_cid_node != NULL) {
      if(temp_cid_node->cid == new_cid) {
        found = 1;
        break;
      }
      prev_cid_node = temp_cid_node;
      temp_cid_node = temp_cid_node->next;
    }

    if(!found) {
      struct cid_node *new_cid_node = (struct cid_node *) kmalloc(sizeof(struct cid_node), GFP_KERNEL);
      new_cid_node->cid = new_cid;
      new_cid_node->next = NULL;
      new_cid_node->running_pids = (struct pid_node *)kmalloc(sizeof(struct pid_node), GFP_KERNEL);
      new_cid_node->running_pids->pid = new_pid;
      new_cid_node->running_pids->next = NULL;

      prev_cid_node->next = new_cid_node;
    } else { // Found 1

      struct pid_node *prev_pid_node = NULL;
      struct pid_node *temp_pid_node = temp_cid_node->running_pids;

      while (temp_pid_node != NULL) {
        prev_pid_node = temp_pid_node;
        temp_pid_node = temp_pid_node->next;

      }
      struct pid_node *new_pid_node = (struct pid_node *)kmalloc(sizeof(struct pid_node), GFP_KERNEL);
      new_pid_node->next = NULL;
      new_pid_node->pid = new_pid;

      prev_pid_node->next = new_pid_node;
    }
    print_ll();
  }
}

// Function to add PID-CID mapping
void add_pid_cid_mapping(int pid, __u64 cid) {
        mutex_lock(&pid_cid_list_lock);

        add_ll(cid, pid);

        mutex_unlock(&pid_cid_list_lock);
}

// Function to remove PID-CID mapping
void remove_pid_cid_mapping(int pid, __u64 cid) {

        mutex_lock(&pid_cid_list_lock);

        mutex_unlock(&pid_cid_list_lock);
}

__u64 get_cid_for_pid(int pid_to_find){

        int idx;
        __u64 cid = -1;
        mutex_lock(&pid_cid_list_lock);

        struct cid_node *temp_cid_node = cid_list;
        while (temp_cid_node != NULL) {
          struct pid_node *temp_pid_node = temp_cid_node->running_pids;

          while(temp_pid_node != NULL) {
            if(pid_to_find == temp_pid_node->pid) {
               cid = temp_cid_node->cid;
               break;
            }
            temp_pid_node = temp_pid_node->next;
          }

          temp_cid_node = temp_cid_node->next;
        }

        mutex_unlock(&pid_cid_list_lock);
        return cid;
}

// Function to get next PID for a given PID
int get_next_pid(__u64 cid, int pid) {

        int next_pid = -1;
        mutex_lock(&pid_cid_list_lock);

        struct cid_node *temp_cid_node = cid_list;
        while (temp_cid_node != NULL) {

          if(temp_cid_node->cid == cid) {
            break;
          }

          temp_cid_node = temp_cid_node->next;
        }
        if(temp_cid_node->running_pids->next != NULL) {
          next_pid = temp_cid_node->running_pids->next->pid;
        } else {
          next_pid = pid;
        }
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
        // int next_pid = get_next_pid(current->pid);
        //
        // // To get the task_struct for next pid
        // struct pid *pid_struct;
        // struct task_struct *next_task;
        //
        // user_cmd_kernal = kmalloc(sizeof(struct processor_container_cmd), GFP_KERNEL);
        // copy_from_user(user_cmd_kernal, (void *)user_cmd, sizeof(struct processor_container_cmd));
        //
        // // Display the current PID and CID
        // printk("Calling DELETE PID: %d CID: %llu\n", current->pid, user_cmd_kernal->cid);
        //
        // // Remove PID-CID mapping
        // remove_pid_cid_mapping(current->pid, user_cmd_kernal->cid);
        //
        // // Get task struct for next pid
        // pid_struct = find_get_pid(next_pid);
        // next_task = pid_task(pid_struct, PIDTYPE_PID);
        //
        // // Schedule current process and wake up next process
        // assign_pid_to_cid(next_pid, user_cmd_kernal->cid);
        // wake_up_process(next_task);
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
        printk("CID: %llu -> PID: %d\n",  cid, current->pid);
        int next_pid = get_next_pid(cid, current->pid);
        printk("Next PID: %d\n", next_pid);
        return 0;

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
