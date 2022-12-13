/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <sys/__assert.h>
#include <timing/timing.h>
#include "cab.h"
#include "img_algo.h"

/* Size of stack area used by each thread (can be thread specific, if necessary)*/
#define STACK_SIZE 1024

/* Thread scheduling priority */
#define thread_NOD_prio 2	 /* Near obstacle detection thread */
#define thread_OBSC_prio 1	 /* Obstacle count thread */
#define thread_OAP_prio 1	 /* Orientation and position thread */
#define thread_OUTPUT_prio 1 /* OAP update thread */

/* Create thread stack space */
K_THREAD_STACK_DEFINE(thread_NOD_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_OBSC_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_OAP_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_OUTPUT_stack, STACK_SIZE);

/* Create variables for thread data */
struct k_thread thread_NOD_data;
struct k_thread thread_OBSC_data;
struct k_thread thread_OAP_data;
struct k_thread thread_OUTPUT_data;

/* Create task IDs */
k_tid_t thread_NOD_tid;
k_tid_t thread_OBSC_tid;
k_tid_t thread_OAP_tid;
k_tid_t thread_OUTPUT_tid;

/* Thread code prototypes */
void thread_NOD_code(void *argA, void *argB, void *argC);
void thread_OBSC_code(void *argA, void *argB, void *argC);
void thread_OAP_code(void *argA, void *argB, void *argC);
void thread_OUTPUT_code(void *argA, void *argB, void *argC);

uint8_t buffer[IMGHEIGHT][IMGWIDTH]= 
	{	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00}, 
		{0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 
		{0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
		{0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00},					
		{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00} 
	};


void main(void)
{
	int arg1 = 0, arg2 = 0, arg3 = 0; // Input args of tasks (actually not used in this case)

	/* Creation of tasks */
	thread_NOD_tid = k_thread_create(&thread_NOD_data, thread_NOD_stack,
									 K_THREAD_STACK_SIZEOF(thread_NOD_stack), thread_NOD_code,
									 &arg1, &arg2, &arg3, thread_NOD_prio, 0, K_NO_WAIT);

	thread_OBSC_tid = k_thread_create(&thread_OBSC_data, thread_OBSC_stack,
										K_THREAD_STACK_SIZEOF(thread_OBSC_stack), thread_OBSC_code,
										NULL, NULL, NULL, thread_OBSC_prio, 0, K_NO_WAIT);

	thread_OAP_tid = k_thread_create(&thread_OAP_data, thread_OAP_stack,
										K_THREAD_STACK_SIZEOF(thread_OAP_stack), thread_OAP_code,
										NULL, NULL, NULL, thread_OAP_prio, 0, K_NO_WAIT);

	thread_OUTPUT_tid = k_thread_create(&thread_OUTPUT_data, thread_OUTPUT_stack,
										K_THREAD_STACK_SIZEOF(thread_OUTPUT_stack), thread_OUTPUT_code,
										NULL, NULL, NULL, thread_OUTPUT_prio, 0, K_NO_WAIT);

	return;
}

void thread_NOD_code(void *argA , void *argB, void *argC){
	uint16_t start, end;
	uint16_t wc_exec_time = 10000;
	uint8_t flag;
	while(1){
		start = k_uptime_get();
		printk("Thread NOD released\n");
		flag = nearObstSearch(buffer);
		end = k_uptime_get();
		if(wc_exec_time > (end-start)) wc_exec_time = (end - start);
		printk("Near obstacle -> %4u\nCi -> %4u\n", flag, wc_exec_time);
		/* Periodicity of task */
		k_msleep(1000);
	}
}

void thread_OBSC_code(void *argA , void *argB, void *argC){
	/*
	obs = obsCount(buffer);
	*/
}

void thread_OAP_code(void *argA , void *argB, void *argC){
	uint16_t pos;
	float angle;
	uint16_t start, end;
	uint16_t wc_exec_time = 10000;
	
	while(1){
		start = k_uptime_get();
		printk("Thread OAP released\n");
		guideLineSearch(buffer, &pos, &angle);
		end = k_uptime_get();
		if(wc_exec_time > (end-start)) wc_exec_time = (end - start);
		printk("Angle (Radians): %f\n", angle);
    	printk("Position (Percentage): %d%%\n", pos);
		printk("Ci -> %4u\n", wc_exec_time);
		/* Periodicity of task */
		k_msleep(5000);
	}
}

void thread_OUTPUT_code(void *argA , void *argB, void *argC){
}